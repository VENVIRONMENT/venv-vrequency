#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include "app/ui/palette.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QStandardPaths>
#include <QTouchEvent>
#include <cmath>
#include <cstring>
#include <memory>

using namespace td;

using namespace pal;

namespace {
const qreal CW = 720;

} // namespace

QString nice(const char *label) {
    QString s = QString(label).toLower();
    if (!s.isEmpty()) s[0] = s[0].toUpper();
    for (int i = 1; i + 1 <= s.size(); ++i) // a lone letter is a name: "Wave A", "B mix"
        if (s[i - 1] == ' ' && (i + 1 == s.size() || s[i + 1] == ' ')) s[i] = s[i].toUpper();
    s.replace("Lfo", "LFO").replace("Fm", "FM");
    return s;
}

// ------------------------------------------------------------- lifecycle

MainWindow::MainWindow(Instrument &i, AudioOut &a, bool scratch, SinkProbe *sharedSink)
    : Shell(i, a), scratchWindow(scratch), sink(sharedSink ? *sharedSink : ownSink) {
    setWindowTitle("venv-vrequency");
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    resize(720, 720);
    clock.start();
    pageOf[PG_CHAT] = &chatPage;
    pageOf[PG_TRACK] = &trackPage;
    pageOf[PG_PLAY] = &synthPage;
    pageOf[PG_SOUND] = &soundPage;
    chat.attach(guide);
    chat.onUserMessage = [this] { emit userChatArrived(); };
    guide.rowLabels = &helpDesc;

    if (!scratch) store::ensureDirs();
    km = scratch ? KeyMap::defaults() : KeyMap::loadOrCreate(store::baseDir() + "/keymap.json");
    SessionData sd;
    QJsonObject st;
    if (!scratch && store::readJson(store::baseDir() + "/state.json", st)) {
        sd.settings = store::settingsFromJson(st["settings"].toObject());
        sounds.load(st);
        keys.chordRow = st["chordRow"].toBool(false);
        store::beatFromJson(st["beat"].toArray(), sd.beat);
        if (st.contains("track")) store::trackFromJson(st["track"].toObject(), trackPage.track);
        chat.load(st);
        tourSeenFlag = st["tourSeen"].toBool(false);
        tourLastPage = st["tourPage"].toString();
    }
    beatsLoad(st, sd);
    sd.lead = sounds.lead; sd.backing = sounds.lead;
    inst.restore(sd);
    inst.setTrack(std::make_shared<const TrackData>(trackPage.track));
    saveTimer.setSingleShot(true);
    connect(&saveTimer, &QTimer::timeout, this, [this] { saveState(); }); // an edited track is on disk a moment later
    if (scratch) {
        sounds.extrasSeeded = kExtraCount;
        return;                                              // a picture-drawing copy: no timers, no probes, no files
    }
    if (sounds.extrasSeeded < kExtraCount) sounds.addExtraSounds(sounds.extrasSeeded); // once: deleting one keeps it gone
    refreshLists();
    tourView = std::make_unique<TourView>(*this);

    connect(&frameTimer, &QTimer::timeout, this, [this] {
        // 25 fps while anything moves; a slow tick when the instrument is idle (battery).
        // Key and touch input call update() themselves, so response stays instant.
        static int skip = 0;
        UiState s = inst.uiState();
        bool busy = s.voices > 0 || ui.touching() || s.backPlaying || s.loopState != L_EMPTY || s.peak > 0.002f ||
                    clock.elapsed() < toastUntil + 200 || listener.state == 1 || recorder.recording() || !keys.held.isEmpty() ||
                    s.beatPlaying || recorder.playingBack() || clock.elapsed() < chat.tickerEnds() + 200 || pg == PG_CHAT || guide.dockOpen || clock.elapsed() < guide.hl.until + 200 || s.trackPlaying || trackPage.exportStatus() != 0 || tourOpen() || (tourView && tourView->promptShowing());
        if (busy || ++skip >= 12) { skip = 0; update(); }
    });
    frameTimer.start(40);
    sink.start();
}

MainWindow::~MainWindow() {
    recorder.shutdown();
    trackPage.stopExport();
}

void MainWindow::closeEvent(QCloseEvent *e) {
    saveState();
    inst.cmd(EV_PANIC);
    e->accept();
}

void MainWindow::saveState() {
    if (scratchWindow) return;
    SessionData sd;
    inst.snapshot(sd);
    QJsonObject o;
    o["settings"] = store::settingsToJson(sd.settings);
    sounds.save(o);
    o["chordRow"] = keys.chordRow;
    o["beat"] = store::beatToJson(sd.beat);
    o["track"] = store::trackToJson(trackPage.track);
    beatsSave(o);
    chat.save(o);
    o["tourSeen"] = tourSeenFlag;
    if (!tourLastPage.isEmpty()) o["tourPage"] = tourLastPage;
    store::writeJson(store::baseDir() + "/state.json", o);
}

void MainWindow::refreshLists() {
    sounds.favs = store::listPatches("favourites");
    sessions = store::listFiles("sessions", "json");
}

void MainWindow::toast(const QString &s, int ms) {
    toastText = s;
    toastUntil = clock.elapsed() + ms;
}

// --------------------------------------------------------------- actions

void MainWindow::tapTempo() {
    qint64 now = clock.elapsed();
    if (!taps.empty() && now - taps.back() > 2000) taps.clear();
    taps.push_back(now);
    if (taps.size() > 6) taps.erase(taps.begin());
    if (taps.size() >= 2) {
        double avg = (taps.back() - taps.front()) / double(taps.size() - 1);
        if (avg > 0) set(S_BPM, (float)(60000.0 / avg));
    }
}

bool MainWindow::saveSession(const QString &path) {
    SessionData sd;
    inst.snapshot(sd);
    QJsonObject o = store::sessionToJson(sd);
    o["track"] = store::trackToJson(trackPage.track);
    bool ok = store::writeJson(path, o);
    refreshLists();
    return ok;
}

bool MainWindow::loadSession(const QString &path) {
    QJsonObject o;
    SessionData sd;
    if (!store::readJson(path, o) || !store::sessionFromJson(o, sd)) return false;
    releaseAllKeys();
    inst.restore(sd);
    sounds.lead = sd.lead; sounds.curSlot = -1;
    if (o.contains("track")) { // sessions saved before the track existed leave the track alone
        trackPage.loadJson(o["track"].toObject());
    }
    return true;
}

void MainWindow::updateBend() {
    Event e; e.type = EV_BEND; e.f = (keys.bendUp ? 1.0f : 0.0f) - (keys.bendDown ? 1.0f : 0.0f);
    inst.push(e);
}

void MainWindow::releaseAllKeys() {
    for (auto it = keys.held.begin(); it != keys.held.end(); ++it)
        if (it.value().kind == HK_STRING) inst.key(it.value().a, it.value().b, false);
    keys.held.clear();
    synthPage.releaseTouches();
    keys.chromatic = keys.chord = keys.command = false;
    if (keys.bendUp || keys.bendDown) { keys.bendUp = keys.bendDown = false; updateBend(); }
    if (keys.susKey) { keys.susKey = false; inst.cmd(EV_SUSTAIN, keys.susLatch ? 1 : 0); }
}

void MainWindow::perform(int act, bool shift) {
    switch (act) {
    case A_PANIC:
        inst.cmd(EV_PANIC);
        keys.held.clear(); keys.susLatch = false;
        toast("All notes off", 900);
        break;
    case A_LOOP: inst.cmd(EV_LOOP, LC_PEDAL); break;
    case A_OCT_DOWN: inst.cmd(EV_OCTAVE, -1); break;
    case A_OCT_UP: inst.cmd(EV_OCTAVE, 1); break;
    case A_CHORD_ROW:
        keys.chordRow = !keys.chordRow;
        toast(keys.chordRow ? "Bottom row plays chords" : "Bottom row is a string again");
        break;
    case A_PAGE_NEXT: goPage((pg + 1) % PG_COUNT); break;
    case A_HOME: goPage(PG_PLAY); keys.testOpen = false; break;
    case A_KEY_UP: set(S_ROOT, (float)((u.set.tuning.root + (shift ? 11 : 1)) % 12)); break;
    case A_SCALE_NEXT: set(S_SCALE, (float)((u.set.tuning.scale + (shift ? SC_COUNT - 1 : 1)) % SC_COUNT)); break;
    case A_RANDOM: sounds.randomise(); break;
    case A_MUTATE: sounds.mutate(); break;
    case A_UNDO_SOUND: sounds.undoSound(); break;
    case A_FAVOURITE: sounds.saveFavourite(); break;
    case A_METRO: set(S_METRO, u.set.metro ? 0 : 1); toast(u.set.metro ? "Metronome off" : "Metronome on", 900); break;
    case A_TAP: tapTempo(); break;
    case A_BACK_STYLE: set(S_BACKSTYLE, (float)((u.set.backStyle + 1) % BS_COUNT)); toast(QString("Backing: %1").arg(backStyleName((u.set.backStyle + 1) % BS_COUNT)), 1000); break;
    case A_BACK_MODE: set(S_BACKMODE, (float)((u.set.backMode + 1) % BM_COUNT)); toast(QString("Chord taps: %1").arg(backModeName((u.set.backMode + 1) % BM_COUNT)), 1000); break;
    case A_BACK_CLEAR: inst.cmd(EV_BACK, BC_CLEAR); toast("Backing cleared", 900); break;
    case A_BACK_PLAY: inst.cmd(EV_BACK, BC_PLAYSTOP); break;
    case A_BACK_REMOVE: inst.cmd(EV_BACK, BC_REMOVE_LAST); break;
    case A_LOOP_UNDO: inst.cmd(EV_LOOP, LC_UNDO); break;
    case A_LOOP_PLAY: inst.cmd(EV_LOOP, LC_PLAYSTOP); break;
    case A_LOOP_CLEAR: inst.cmd(EV_LOOP, LC_CLEAR); toast("Loop cleared", 900); break;
    case A_RECORD: recorder.toggle(); break;
    case A_BEAT_PLAY: inst.beatDo(BT_PLAYSTOP); toast(u.beatPlaying ? "Beat stopped" : "Beat playing", 900); break;
    case A_BEAT_REC: inst.beatDo(BT_REC); toast(u.beatRec ? "Beat recording off" : "Beat recording: pads write hits", 1200); break;
    case A_PLAY_LAST: recorder.togglePlayback(); break;
    case A_TRACK_PLAY: trackPage.trackPlayStop(); break;
    case A_CHAT: // Ctrl+Y: the chat dock on this page; with Shift (or on the Chat page itself) the full Chat page
        if (shift || pg == PG_CHAT) chat.toggle();
        else if (!guide.dockOpen) guide.dockOpenFocus();
        else guide.dockFocus = !guide.dockFocus;
        break;
    case A_HELP: goPage(pg == PG_HELP ? PG_PLAY : PG_HELP); break;
    case A_TOUR: if (tourView) { if (tourView->isOpen()) tourView->close(); else tourView->open(QString(), QString(), false, nullptr, true); } break;
    case A_XYMAP: set(S_XYMAP, (float)((u.set.xyMap + 1) % XY_COUNT)); break;
    case A_QUIT: close(); break;
    default:
        if (act >= A_SOUND_1 && act <= A_SOUND_10) sounds.selectSlot(act - A_SOUND_1);
        break;
    }
}

// ------------------------------------------------------------------ keys

void MainWindow::handleKey(int key, bool down) {
    update();
    // bookkeeping for the key-test view
    if (down) {
        keys.log.prepend(QString("DOWN  %1").arg(keyName(key)));
    } else {
        keys.log.prepend(QString("up    %1").arg(keyName(key)));
    }
    while (keys.log.size() > 12) keys.log.removeLast();

    if (!down) {
        auto it = keys.held.find(key);
        if (it == keys.held.end()) return;
        Held h = it.value();
        keys.held.erase(it);
        if (h.kind == HK_STRING) inst.key(h.a, h.b, false);
        else if (h.kind == HK_ACT) {
            switch (h.a) {
            case A_SUSTAIN: keys.susKey = false; inst.cmd(EV_SUSTAIN, keys.susLatch ? 1 : 0); break;
            case A_CHROMATIC: keys.chromatic = false; break;
            case A_CHORD_LAYER: keys.chord = false; break;
            case A_COMMAND: keys.command = false; break;
            case A_BEND_UP: keys.bendUp = false; updateBend(); break;
            case A_BEND_DOWN: keys.bendDown = false; updateBend(); break;
            }
        }
        return;
    }
    if (keys.held.contains(key)) return; // already down (missed release or repeat)
    if ((int)keys.held.size() + 1 > keys.maxHeld) keys.maxHeld = keys.held.size() + 1;

    int plainAct = km.plain.value(key, A_NONE);
    // modifiers first, so they work in any layer
    if (plainAct == A_CHROMATIC) { keys.chromatic = true; keys.held[key] = {HK_ACT, A_CHROMATIC, 0}; return; }
    if (plainAct == A_CHORD_LAYER) { keys.chord = true; keys.held[key] = {HK_ACT, A_CHORD_LAYER, 0}; return; }
    if (plainAct == A_COMMAND) { keys.command = true; keys.held[key] = {HK_ACT, A_COMMAND, 0}; return; }

    if (tourView && tourView->promptKey(key)) { keys.held[key] = {HK_NONE, 0, 0}; return; } // Enter / Esc answer the first-run offer
    if (tourOpen()) { // the tour is a picture: its keys never reach the synth
        tourView->key(key, keys.text, keys.chromatic);
        keys.text.clear();
        keys.held[key] = {HK_NONE, 0, 0};
        return;
    }
    if (guide.dockOpen && pg != PG_CHAT && !keys.testOpen && !keys.command) { // the chat dock
        if (guide.dockFocus && chat.key(key, plainAct, true)) { keys.held[key] = {HK_NONE, 0, 0}; return; }
        if (!guide.dockFocus && plainAct == A_HOME) { guide.dockClose(); keys.held[key] = {HK_NONE, 0, 0}; return; } // Esc again closes it
    }
    if (pageOf[pg] && !keys.testOpen && pageOf[pg]->key(key, plainAct)) { keys.held[key] = {HK_NONE, 0, 0}; return; }
    if (keys.command) {
        perform(km.command.value(key, A_NONE), keys.chromatic);
        keys.held[key] = {HK_NONE, 0, 0};
        return;
    }
    int row, col;
    if (pg == PG_BEAT && !keys.testOpen) { // on the Beat page the two upper rows are the pads
        int drum;
        if (km.drumKey(key, drum)) { hitPad(drum); keys.held[key] = {HK_NONE, 0, 0}; return; }
        if (km.stringKey(key, row, col) && row != 0) { keys.held[key] = {HK_NONE, 0, 0}; return; } // between the pad keys: nothing
        if (plainAct == A_SUSTAIN) { inst.beatDo(BT_PLAYSTOP); keys.held[key] = {HK_NONE, 0, 0}; return; }
        if (plainAct == A_LOOP) { inst.beatDo(BT_REC); keys.held[key] = {HK_NONE, 0, 0}; return; }
        if (plainAct == A_BEND_DOWN || plainAct == A_BEND_UP) { // the beat before or after
            beatSwitchTo(qBound(0, beatCur + (plainAct == A_BEND_UP ? 1 : -1), (int)beatSlots.size() - 1));
            keys.held[key] = {HK_NONE, 0, 0};
            return;
        }
    }
    if (km.stringKey(key, row, col)) {
        if (row == 0 && (keys.chordRow || keys.chord)) {
            if (col < 7) inst.cmd(EV_CHORD, col);
            keys.held[key] = {HK_NONE, 0, 0};
            return;
        }
        inst.key(row, col, true, keys.chromatic);
        keys.held[key] = {HK_STRING, row, col};
        return;
    }
    keys.held[key] = {HK_ACT, plainAct, 0};
    switch (plainAct) {
    case A_SUSTAIN: keys.susKey = true; inst.cmd(EV_SUSTAIN, 1); break;
    case A_BEND_UP: keys.bendUp = true; updateBend(); break;
    case A_BEND_DOWN: keys.bendDown = true; updateBend(); break;
    default: perform(plainAct, keys.chromatic); break;
    }
}

void MainWindow::keyPressEvent(QKeyEvent *e) {
    if (e->isAutoRepeat()) return; // a held key is one note, not many
    keys.text = e->text();
    handleKey(e->key(), true);
    keys.text.clear();
}

void MainWindow::keyReleaseEvent(QKeyEvent *e) {
    if (e->isAutoRepeat()) return;
    handleKey(e->key(), false);
}

void MainWindow::focusOutEvent(QFocusEvent *) { releaseAllKeys(); }

// --------------------------------------------------------------- pointer

QPointF MainWindow::toCanvas(QPointF p) const {
    qreal s = qMin(width(), height()) / CW;
    qreal ox = (width() - CW * s) / 2, oy = (height() - CW * s) / 2;
    return QPointF((p.x() - ox) / s, (p.y() - oy) / s);
}

void MainWindow::pointer(int id, QPointF c, int phase) {
    const bool taken = ui.touch(id, c, phase, [this](const QString &name) {
        if (!tourOpen()) guide.noteTap(name);
    });
    if (taken) update();
}

void MainWindow::mousePressEvent(QMouseEvent *e) { pointer(-1, toCanvas(e->position()), PH_PRESS); }
void MainWindow::mouseMoveEvent(QMouseEvent *e) { if (e->buttons()) pointer(-1, toCanvas(e->position()), PH_MOVE); }
void MainWindow::mouseReleaseEvent(QMouseEvent *e) { pointer(-1, toCanvas(e->position()), PH_RELEASE); }

bool MainWindow::tourOpen() const { return tourView && tourView->isOpen() && !tourSuspended; }

bool MainWindow::event(QEvent *ev) {
    switch (ev->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel: {
        auto *te = static_cast<QTouchEvent *>(ev);
        for (const QEventPoint &pt : te->points()) {
            QPointF c = toCanvas(pt.position());
            if (ev->type() == QEvent::TouchCancel || pt.state() == QEventPoint::Released) pointer(pt.id(), c, PH_RELEASE);
            else if (pt.state() == QEventPoint::Pressed) pointer(pt.id(), c, PH_PRESS);
            else if (pt.state() == QEventPoint::Updated) pointer(pt.id(), c, PH_MOVE);
        }
        ev->accept();
        return true;
    }
    default: return QWidget::event(ev);
    }
}

QImage MainWindow::shot() {
    QImage img(720, 720, QImage::Format_RGB32); // the instrument's native canvas, whatever the window size
    QPainter p(&img);
    drawAll(p, 720, 720);
    return img;
}

// ------------------------------------------------------------------ paint

extern qint64 msSinceStart();

void MainWindow::paintEvent(QPaintEvent *) {
    static bool first = true;
    if (first) {
        first = false;
        if (qEnvironmentVariableIsSet("VREQUENCY_TIMING")) fprintf(stderr, "vrequency: first frame %lld ms after launch\n", (long long)msSinceStart());
    }
    QElapsedTimer pt; pt.start();
    {
        QPainter painter(this);
        drawAll(painter, width(), height());
    }
    // VREQUENCY_TIMING=1 prints how often the window is drawn and how long a frame takes
    static const bool timing = qEnvironmentVariableIsSet("VREQUENCY_TIMING");
    static int frames = 0; static qint64 spent = 0, since = 0;
    ++frames; spent += pt.nsecsElapsed();
    if (timing && clock.elapsed() - since >= 2000) {
        fprintf(stderr, "vrequency: page %d, %.1f frames/s, %.1f ms each\n", pg, frames * 1000.0 / (clock.elapsed() - since), spent / 1e6 / frames);
        frames = 0; spent = 0; since = clock.elapsed();
    }
}

void MainWindow::drawAll(QPainter &painter, qreal W, qreal H) {
    u = inst.uiState();
    if (sink.generation != seenSinkGeneration) { // output device changed: pick a sensible loop timing offset
        seenSinkGeneration = sink.generation;
        set(S_LATENCY, sink.bluetooth ? 200.0f : 0.0f);
    }
    ui.beginFrame(painter);
    painter.fillRect(QRectF(0, 0, W, H), BG);
    qreal s = qMin(W, H) / CW;
    painter.translate((W - CW * s) / 2, (H - CW * s) / 2);
    painter.scale(s, s);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    ui.recordTexts(true);
    guide.beginFrame();
    beatWatch();

    for (Page *p : pageOf) if (p) p->tick();

    if (tourOpen()) tourView->paint(); // the tour draws a picture instead of the page: nothing on it is live
    else if (Page *p = pageOf[pg]) { if (p->tabs()) paintTabs(); p->paint(); }
    else switch (pg) {
    case PG_BACK: paintTabs(); paintBacking(); break;
    case PG_BEAT: paintTabs(); paintBeat(); break;
    case PG_TUNE: paintTabs(); paintTune(); break;
    case PG_SYS: paintTabs(); paintSystem(); break;
    case PG_HELP: paintTabs(); paintHelp(); break;
    }
    ui.recordTexts(false);
    if (!tourOpen()) {
        guide.layoutDock();
        guide.paintHighlights();
        chat.paintTicker();
        guide.paintDock();
        if (keys.testOpen) paintKeyTest();
        if (tourView) tourView->paintPrompt();
    }

    if (clock.elapsed() < toastUntil && !toastText.isEmpty()) { // a message box, as the scope reports "Auto-scale"
        QString t = ui.fit(toastText, 14, 640);
        int w = t.size() * ui.chW(14) + 28;
        QRectF r(std::floor((CW - w) / 2), pg == PG_TRACK ? 398 : 86, w, 30); // on the Track page, clear of the bars
        ui.frame(r.adjusted(-4, -4, 4, 4), INK, BG);
        ui.tag(r, t, 14);
    }

    // tube face: faint scan lines and darker corners over everything
    if (facePix.isNull()) {
        QImage im(720, 720, QImage::Format_ARGB32_Premultiplied);
        im.fill(Qt::transparent);
        QPainter q(&im);
        for (int y = 1; y < 720; y += 2) q.fillRect(0, y, 720, 1, QColor(0, 0, 0, 34));
        QRadialGradient v(QPointF(360, 360), 520);
        v.setColorAt(0.0, QColor(0, 0, 0, 0));
        v.setColorAt(0.80, QColor(0, 0, 0, 0));
        v.setColorAt(1.0, QColor(0, 0, 0, 70));
        q.fillRect(0, 0, 720, 720, v);
        facePix = QPixmap::fromImage(im);
    }
    painter.drawPixmap(0, 0, facePix);
    ui.endFrame();
}

void MainWindow::paintTabs() {
    qreal w = 688.0 / PG_COUNT;
    for (int i = 0; i < PG_COUNT; ++i) // one joined menu strip: neighbours share their edge
        ui.button(QRectF(std::floor(16 + i * w), 8, std::floor(16 + (i + 1) * w) - std::floor(16 + i * w) + 1, 46), QString("tab_") + pageName(i), (i == PG_CHAT && chat.unread > 0 ? QString("Chat %1").arg(qMin(chat.unread, 9)) : QString(pageTitle(i))), pg == i,
               [this, i] { goPage(i); });
}

void MainWindow::paintBacking() {
    ui.section(62, QString::fromUtf8("A chord tap…"));
    static const char *MS[BM_COUNT] = {"adds to the loop", "latches one chord", "latches a drone note"};
    qreal w3 = (688 - 2 * 8) / 3.0;
    for (int i = 0; i < BM_COUNT; ++i)
        ui.button(QRectF(std::floor(16 + i * (w3 + 8)), 86, std::floor(w3), 54), QString("bmode%1").arg(i), nice(backModeName(i)), u.set.backMode == i, [this, i] { set(S_BACKMODE, (float)i); }, MS[i]);
    qreal w5 = (688 - 4 * 6) / 5.0;
    for (int i = 0; i < BS_COUNT; ++i)
        ui.button(QRectF(std::floor(16 + i * (w5 + 6)), 148, std::floor(w5), 46), QString("bstyle%1").arg(i), nice(backStyleName(i)), u.set.backStyle == i, [this, i] { set(S_BACKSTYLE, (float)i); });

    // progression slots
    ui.section(202, "Progression  tap a slot, then a chord, to replace it");
    qreal sw = (688 - 7 * 6) / 8.0;
    for (int i = 0; i < kProgMax; ++i) {
        QRectF r(std::floor(16 + i * (sw + 6)), 226, std::floor(sw), 58);
        bool has = i < u.progLen, cur = has && i == u.progPos && u.backPlaying, sel = i == u.selSlot;
        ui.frame(r, sel || cur ? INK : (has ? MID : GRAT), cur ? INK : BG);
        if (sel && !cur) ui.frame(r.adjusted(3, 3, -3, -3), INK);
        ui.txt(QRectF(r.x(), r.y() + 5, r.width(), 14), QString::number(i + 1), 11, cur ? BG : (has ? MID : LOW));
        if (has) ui.txt(QRectF(r.x(), r.y() + 18, r.width(), 34), buildChord(u.set.tuning, u.prog[i], u.set.seventh).name, 18, cur ? BG : INK, Qt::AlignCenter, true);
        if (has) ui.zone(r, QString("bslot%1").arg(i), [this, i](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_BACK, BC_SELECT_SLOT, i); });
    }
    qreal w4 = (688 - 3 * 8) / 4.0;
    auto c4 = [w4](int i, qreal y, qreal h) { return QRectF(std::floor(16 + i * (w4 + 8)), y, std::floor(w4), h); };
    ui.button(c4(0, 292, 52), "bremove", "Remove last", false, [this] { inst.cmd(EV_BACK, BC_REMOVE_LAST); });
    ui.button(c4(1, 292, 52), "bclear", "Clear all", false, [this] { inst.cmd(EV_BACK, BC_CLEAR); });
    ui.button(c4(2, 292, 52), "bseventh", "7th chords", u.set.seventh, [this] { set(S_SEVENTH, u.set.seventh ? 0 : 1); });
    ui.button(c4(3, 292, 52), "bmetro", "Metronome", u.set.metro, [this] { set(S_METRO, u.set.metro ? 0 : 1); });

    chordPads.paint(QRectF(16, 356, 688, 84), false);

    ui.stepper(QRectF(16, 452, 340, 60), "chordbeats", "Beats per chord", QString::number(u.set.chordBeats),
            [this] { set(S_CHORDBEATS, (float)(u.set.chordBeats - 1)); }, [this] { set(S_CHORDBEATS, (float)(u.set.chordBeats + 1)); });
    ui.stepper(QRectF(364, 452, 340, 60), "arprate", "Arp steps per beat", QString::number(u.set.arpRate),
            [this] { set(S_ARPRATE, (float)(u.set.arpRate - 1)); }, [this] { set(S_ARPRATE, (float)(u.set.arpRate + 1)); });
    ui.stepper(QRectF(16, 520, 340, 60), "bpm2", "Tempo", QString("%1 bpm").arg((int)std::round(u.set.bpm)),
            [this] { set(S_BPM, std::round(u.set.bpm) - 1); }, [this] { set(S_BPM, std::round(u.set.bpm) + 1); });
    ui.slider(QRectF(364, 520, 340, 60), "backvol", "Backing volume", u.set.backVol, QString("%1%").arg((int)(u.set.backVol * 100)), [this](float n) { set(S_BACKVOL, n); });
    ui.button(QRectF(16, 590, 688, 56), "bresync", "Use the current lead sound for the backing", false, [this] { inst.cmd(EV_BACK, BC_RESYNC_SOUND); },
           QString("backing now plays: %1    lead: %2").arg(u.backName[0] ? u.backName : "(nothing yet)").arg(sounds.lead.name));
    ui.txt(QRectF(18, 654, 684, 20), "The backing keeps the sound it started with, so the lead can change freely.", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.txt(QRectF(18, 676, 684, 20), QString::fromUtf8("Keyboard: hold %1 + Z‥M for chords, or press $ for a chord row.").arg(km.keyFor(A_CHORD_LAYER, false)), 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
}

// ------------------------------------------------------------------ beat

void MainWindow::hitPad(int drum) {
    if (drum < 0 || drum >= kDrums) return;
    if (beatErase) {
        inst.beatDo(BT_ERASE, drum);
        beatErase = false;
        toast(QString("%1 erased").arg(nice(drumName(drum))), 900);
        return;
    }
    inst.hit(drum);
    padLit[drum] = clock.elapsed();
    update();
}

void MainWindow::paintBeat() {
    const qint64 now = clock.elapsed();
    for (int d = 0; d < kDrums; ++d)
        if (u.drumHits[d] != seenHits[d]) { seenHits[d] = u.drumHits[d]; padLit[d] = now; }

    // the whole bar for all eight drums
    ui.section(60, QString("%1  %2 bpm%3").arg(beatSlots.empty() ? QString("Pattern") : beatSlots[beatCur].name).arg((int)std::round(u.set.bpm)).arg(u.beatRec ? "  recording" : (u.beatPlaying ? "  playing" : "")));
    const qreal ox = 104, cw = 37.5, sy = 84; const int ch = 12;
    for (int d = 0; d < kDrums; ++d) {
        qreal y = sy + d * (ch + 3);
        ui.txt(QRectF(16, y - 3, 84, ch + 6), nice(drumName(d)), 11, u.beatPat[d] ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter);
        for (int s = 0; s < kDrumSteps; ++s) {
            QRectF c(std::floor(ox + s * cw), y, std::floor(ox + (s + 1) * cw) - std::floor(ox + s * cw) - 4, ch);
            if ((u.beatPat[d] >> s) & 1) ui.g->fillRect(c, INK);
            else ui.frame(c, s % 4 == 0 ? LOW : GRAT);
        }
    }
    const qreal sb = sy + kDrums * (ch + 3);
    if (u.beatStep >= 0)
        ui.frame(QRectF(std::floor(ox + u.beatStep * cw) - 3, sy - 4, std::floor(ox + (u.beatStep + 1) * cw) - std::floor(ox + u.beatStep * cw) + 2, sb - sy + 5), INK);
    for (int b = 0; b < 4; ++b)
        ui.txt(QRectF(std::floor(ox + b * 4 * cw), sb + 1, cw - 4, 14), QString::number(b + 1), 11, MID);

    // the pads
    ui.section(222, beatErase ? "Pads  tap the drum to erase" : "Pads  tap to play");
    const qreal pw = (688 - 3 * 8) / 4.0;
    for (int d = 0; d < kDrums; ++d) {
        QRectF r(std::floor(16 + (d % 4) * (pw + 8)), 244 + (d / 4) * 108, std::floor(pw), 100);
        const QString name = QString("pad%1").arg(d);
        const bool lit = now - padLit[d] < 110 || ui.isDown(name);
        ui.frame(r, INK, lit ? INK : BG);
        if (!lit) {
            ui.frame(r.adjusted(1, 1, -1, -1), INK);
            ui.frame(r.adjusted(6, 6, -6, -6), beatErase ? INK : GRAT);
        }
        const QColor fg = lit ? BG : INK, dim = lit ? BG : MID;
        ui.txt(QRectF(r.x() + 12, r.y() + 10, 30, 18), QString::number(d + 1), 12, lit ? BG : LOW, Qt::AlignLeft | Qt::AlignVCenter, true);
        ui.txt(QRectF(r.x() + 40, r.y() + 10, r.width() - 52, 18), km.drumKeys(d), 12, dim, Qt::AlignRight | Qt::AlignVCenter, true);
        ui.txt(QRectF(r.x(), r.bottom() - 40, r.width(), 26), nice(drumName(d)), 18, fg, Qt::AlignCenter, true);
        if (beatErase && !lit) ui.txt(QRectF(r.x(), r.y() + 34, r.width(), 18), "erase", 12, MID);
        ui.zone(r, name, [this, d](QPointF, int ph) { if (ph == PH_PRESS) hitPad(d); });
    }

    paintKeptBeats(460);

    // transport
    auto c4 = [pw](int i, qreal y, qreal h) { return QRectF(std::floor(16 + i * (pw + 8)), y, std::floor(pw), h); };
    ui.button(c4(0, 540, 56), "beatrec", QString::fromUtf8("● Rec"), u.beatRec, [this] { inst.beatDo(BT_REC); beatErase = false; },
           u.beatRec ? "pads write hits" : "write what you play");
    ui.button(c4(1, 540, 56), "beatplay", u.beatPlaying ? QString::fromUtf8("■ Stop") : QString::fromUtf8("▶ Play"), u.beatPlaying,
           [this] { inst.beatDo(BT_PLAYSTOP); }, u.beatPlaying ? "beat is playing" : "start the beat");
    ui.button(c4(2, 540, 56), "beaterase", "Erase", false, [this] { beatErase = !beatErase; }, beatErase ? "now tap a pad" : "one drum's hits",
           beatErase ? LOOK_ARMED : LOOK_PLAIN);
    ui.stepper(c4(3, 540, 56), "kit", "Kit", nice(kitName(u.set.kit)),
            [this] { set(S_KIT, (float)((u.set.kit + kKits - 1) % kKits)); }, [this] { set(S_KIT, (float)((u.set.kit + 1) % kKits)); });
    ui.slider(QRectF(16, 604, 340, 54), "beatvol", "Beat volume", u.set.beatVol, QString("%1%").arg((int)(u.set.beatVol * 100)), [this](float n) { set(S_BEATVOL, n); });
    ui.stepper(QRectF(364, 604, 340, 54), "bpm3", "Tempo", QString("%1 bpm").arg((int)std::round(u.set.bpm)),
            [this] { set(S_BPM, std::round(u.set.bpm) - 1); }, [this] { set(S_BPM, std::round(u.set.bpm) + 1); });
    ui.txt(QRectF(18, 666, 684, 20), QString("Keys: %1 play the top pads, %2 the bottom ones. Hits snap to the nearest step.")
            .arg(km.rows[2].left(4).split("", Qt::SkipEmptyParts).join(" ")).arg(km.rows[1].left(4).split("", Qt::SkipEmptyParts).join(" ")),
        12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.txt(QRectF(18, 688, 684, 20), QString("%1 starts and stops the beat, %2 is Rec, left right the beat before or after.")
            .arg(km.keyFor(A_SUSTAIN, false)).arg(km.keyFor(A_LOOP, false)), 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
}

void MainWindow::paintTune() {
    const Tuning &t = u.set.tuning;
    ui.section(60, "Key");
    qreal w6 = (688 - 5 * 6) / 6.0;
    for (int i = 0; i < 12; ++i)
        ui.button(QRectF(std::floor(16 + (i % 6) * (w6 + 6)), 84 + (i / 6) * 52, std::floor(w6), 46), QString("root%1").arg(i), noteName(i), t.root == i, [this, i] { set(S_ROOT, (float)i); });
    ui.section(190, "Scale");
    qreal w4 = (688 - 3 * 6) / 4.0;
    for (int i = 0; i < SC_COUNT; ++i)
        ui.button(QRectF(std::floor(16 + (i % 4) * (w4 + 6)), 214 + (i / 4) * 52, std::floor(w4), 46), QString("scale%1").arg(i), scaleDef(i).name, t.scale == i, [this, i] { set(S_SCALE, (float)i); });

    qreal w3 = (688 - 2 * 8) / 3.0;
    auto c3 = [w3](int i, qreal y) { return QRectF(std::floor(16 + i * (w3 + 8)), y, std::floor(w3), 60); };
    ui.stepper(c3(0, 326), "oct", "Octave", QString("%1%2").arg(t.octave > 0 ? "+" : "").arg(t.octave),
            [this] { inst.cmd(EV_OCTAVE, -1); }, [this] { inst.cmd(EV_OCTAVE, 1); });
    ui.stepper(c3(1, 326), "rowint", "String gap", QString("%1 st").arg(t.rowInterval),
            [this] { set(S_ROWINT, (float)(u.set.tuning.rowInterval - 1)); }, [this] { set(S_ROWINT, (float)(u.set.tuning.rowInterval + 1)); });
    ui.stepper(c3(2, 326), "bendr", "Bend range", QString("%1 st").arg((int)u.set.bendRange),
            [this] { set(S_BENDRANGE, u.set.bendRange - 1); }, [this] { set(S_BENDRANGE, u.set.bendRange + 1); });

    ui.stepper(c3(0, 394), "bpm", "Tempo", QString("%1 bpm").arg((int)std::round(u.set.bpm)),
            [this] { set(S_BPM, std::round(u.set.bpm) - 1); }, [this] { set(S_BPM, std::round(u.set.bpm) + 1); });
    QRectF mid = c3(1, 394);
    ui.button(QRectF(mid.x(), 394, std::floor(mid.width() / 2) - 4, 60), "tap", "Tap", false, [this] { tapTempo(); }, "tempo");
    ui.button(QRectF(mid.x() + std::floor(mid.width() / 2) + 4, 394, mid.right() - (mid.x() + std::floor(mid.width() / 2) + 4), 60), "metro", "Click", u.set.metro, [this] { set(S_METRO, u.set.metro ? 0 : 1); }, "metronome");
    ui.slider(c3(2, 394), "glidescale", "Glide speed", u.set.glideScale / 4.0f,
           u.set.glideScale < 0.02f ? QString("instant") : QString::fromUtf8("×%1").arg(u.set.glideScale, 0, 'f', 2), [this](float n) { set(S_GLIDESCALE, n * 4.0f); });

    // listen
    QRectF lp(16, 466, 688, 238);
    ui.frame(lp, LOW, BG);
    ui.tag(QRectF(lp.x() + 1, lp.y() + 1, 62, 18), "Listen", 12);
    ui.txt(QRectF(88, 466, 600, 20), "match the key and tempo of music around you", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    int ls = listener.state;
    ui.button(QRectF(32, 506, 200, 60), "listen", ls == 1 ? QString::fromUtf8("Listening…") : QString("Listen"), ls == 1, [this] { listener.start(); }, ls == 1 ? QString("%1 s").arg(qMax<qint64>(0, 6 - (clock.elapsed() - listener.started) / 1000)) : "6 seconds, needs a mic");
    if (ls == 0) {
        ui.txt(QRectF(248, 506, 440, 60), "Uses the system's default audio input, if any.", 13, MID, Qt::AlignLeft | Qt::AlignVCenter);
    } else if (ls == 3) {
        std::lock_guard<std::mutex> lk(listener.mx);
        ui.g->setFont(ui.font(13, false));
        ui.g->setPen(INK);
        ui.g->drawText(QRectF(248, 506, 440, 60), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, listener.error);
    } else if (ls == 2) {
        std::lock_guard<std::mutex> lk(listener.mx);
        const ListenResult &r = listener.result;
        const KeyGuess &k = r.keys[listener.guess % 3];
        float kc = listener.guess == 0 ? r.keyConfidence : r.keyConfidence * 0.5f;
        QString kq = kc > 0.6f ? "likely" : (kc > 0.3f ? "a guess" : "very unsure");
        ui.txt(QRectF(248, 500, 250, 40), QString("%1 %2").arg(noteName(k.root)).arg(k.minor ? "minor" : "major"), 24, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
        ui.txt(QRectF(248, 540, 250, 22), QString("key: %1 (%2%)").arg(kq).arg((int)(kc * 100)), 13, kc > 0.6f ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter);
        QString tq = r.bpmConfidence > 0.6f ? "likely" : (r.bpmConfidence > 0.3f ? "a guess" : "very unsure");
        ui.txt(QRectF(500, 500, 190, 40), r.bpm > 0 ? QString("%1 bpm").arg((int)std::round(r.bpm)) : QString("no pulse"), 24, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
        ui.txt(QRectF(500, 540, 190, 22), QString("tempo: %1 (%2%)").arg(tq).arg((int)(r.bpmConfidence * 100)), 13, r.bpmConfidence > 0.6f ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (ls == 2) {
        qreal bw = (656 - 3 * 8) / 4.0;
        ui.button(QRectF(32, 580, bw, 56), "lkey", "Use key", false, [this] { listener.apply(true, false); toast("Key applied"); });
        ui.button(QRectF(32 + bw + 8, 580, bw, 56), "ltempo", "Use tempo", false, [this] { listener.apply(false, true); toast("Tempo applied"); });
        ui.button(QRectF(32 + 2 * (bw + 8), 580, bw, 56), "lboth", "Use both", false, [this] { listener.apply(true, true); toast("Key and tempo applied"); });
        ui.button(QRectF(32 + 3 * (bw + 8), 580, bw, 56), "lnext", "Other key", false, [this] { listener.guess = (listener.guess + 1) % 3; }, QString("guess %1 of 3").arg(listener.guess % 3 + 1));
        ui.txt(QRectF(32, 646, 656, 44), "Wrong? Pick the key and scale by hand above: detection is an estimate.", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    }
}

void MainWindow::paintSystem() {
    ui.slider(QRectF(16, 66, 340, 58), "volume", "Master volume", u.set.volume, QString("%1%").arg((int)(u.set.volume * 100)), [this](float n) { set(S_VOLUME, n); });
    ui.slider(QRectF(364, 66, 340, 58), "loopvol", "Loop volume", u.set.loopVol, QString("%1%").arg((int)(u.set.loopVol * 100)), [this](float n) { set(S_LOOPVOL, n); });

    ui.section(130, "Touch pad mapping");
    qreal w6 = (688 - XY_COUNT * 6) / (XY_COUNT + 1.0);
    auto c6 = [w6](int i) { return QRectF(std::floor(16 + i * (w6 + 6)), 154, std::floor(w6), 54); };
    for (int i = 0; i < XY_COUNT; ++i)
        ui.button(c6(i), QString("xymap%1").arg(i), nice(xyMapName(i)), u.set.xyMap == i, [this, i] { set(S_XYMAP, (float)i); },
               QString("%1/%2").arg(xyAxisName(i, false)).arg(xyAxisName(i, true)).toLower().replace("resonance", "reso").replace("vibrato", "vib"));
    ui.button(c6(XY_COUNT), "xyhold2", "Hold", u.set.xyHold, [this] { set(S_XYHOLD, u.set.xyHold ? 0 : 1); }, "keep it on");

    qreal w4 = (688 - 3 * 8) / 4.0;
    auto c4 = [w4](int i, qreal y, qreal h) { return QRectF(std::floor(16 + i * (w4 + 8)), y, std::floor(w4), h); };
    qreal w5 = (688 - 4 * 6) / 5.0;
    auto c5 = [w5](int i) { return QRectF(std::floor(16 + i * (w5 + 6)), 218, std::floor(w5), 58); };
    ui.button(c5(0), "chordrow", "Chord row", keys.chordRow, [this] { perform(A_CHORD_ROW); }, "low row = chords");
    ui.button(c5(1), "recwav", recorder.recording() ? QString::fromUtf8("■ Stop rec") : QString::fromUtf8("● Record"), recorder.recording(), [this] { recorder.toggle(); },
           recorder.recording() ? QString("%1 s").arg(recorder.seconds()) : "what you hear");
    ui.button(c5(2), "playlast", recorder.playingBack() ? QString::fromUtf8("■ Stop") : QString::fromUtf8("▶ Play last"), recorder.playingBack(), [this] { recorder.togglePlayback(); },
           recorder.playingBack() ? "playing it back" : "listen back");
    ui.button(c5(3), "savesession", "Save session", false, [this] {
        QString path = store::baseDir() + "/sessions/" + store::timestampName("session", "json");
        toast(saveSession(path) ? QString("Saved %1").arg(QFileInfo(path).fileName()) : QString("Could not save session"), 2200);
    }, "sounds and loops");
    ui.button(c5(4), "keytest", "Key test", keys.testOpen, [this] { keys.testOpen = !keys.testOpen; keys.maxHeld = 0; }, "check the keys");

    ui.section(284, QString("Saved sessions  %1  tap to load").arg(sessions.size()));
    for (int k = 0; k < 4; ++k) {
        QRectF r(16 + (k % 2) * 348, 308 + (k / 2) * 54, 340, 48);
        if (k >= sessions.size()) { ui.frame(r, GRAT); continue; }
        QString path = sessions[k];
        ui.button(r, QString("session%1").arg(k), QFileInfo(path).completeBaseName(), false, [this, path] {
            toast(loadSession(path) ? QString("Loaded %1: press play on the loop / backing").arg(QFileInfo(path).completeBaseName()) : QString("Could not read that session"), 2600);
        });
    }

    QRectF ip(16, 424, 688, 206);
    ui.frame(ip, LOW, BG);
    ui.tag(QRectF(ip.x() + 1, ip.y() + 1, 62, 18), "Status", 12);
    QStringList L;
    const QString output = sink.description.isEmpty() ? QString("unknown") : sink.description;
    L << QString("Audio    %1, %2 Hz, %3-frame blocks (%4 ms)").arg(QString::fromStdString(audio.backend())).arg(audio.sampleRate()).arg(audio.quantum()).arg(1000.0 * audio.quantum() / audio.sampleRate(), 0, 'f', 1);
    L << QString("Output   %1%2").arg(output.left(52)).arg(audio.streaming() ? "" : " (idle)");
    L << (sink.bluetooth ? QString("         Bluetooth: about 0.2 s behind your fingers") : QString());
    L << QString("Engine   load %1%, peak %2%, %3 late blocks, %4 voices").arg((int)(audio.load() * 100)).arg((int)(audio.peakLoad() * 100)).arg(audio.overruns()).arg(u.voices);
    L << QString("Files    %1").arg(store::baseDir());
    L << QString("Last WAV %1").arg(recorder.lastPath().isEmpty() ? QString("-") : QFileInfo(recorder.lastPath()).fileName());
    L << QString("Keys     edit keymap.json in the folder above to remap");
    for (int i = 0; i < L.size(); ++i)
        ui.txt(QRectF(32, 448 + i * 22, 656, 20), ui.fit(L[i], 13, 656), 13, i == 2 ? MID : INK, Qt::AlignLeft | Qt::AlignVCenter);
    if (sink.dummy)
        ui.tag(QRectF(32, 448 + 7 * 22 + 2, 560, 20), "No real audio device: connect headphones or a speaker.", 13, Qt::AlignLeft | Qt::AlignVCenter);

    ui.button(c4(0, 642, 60), "panic", "Panic", false, [this] { perform(A_PANIC); }, "all notes off");
    ui.stepper(c4(1, 642, 60), "latency", "Loop fix", QString("%1 ms").arg((int)u.set.latencyMs),
            [this] { set(S_LATENCY, u.set.latencyMs - 10); }, [this] { set(S_LATENCY, u.set.latencyMs + 10); });
    ui.button(c4(2, 642, 60), "factory", "Factory sounds", false, [this] {
        sounds.factoryReset();
        int back = sounds.addExtraSounds();
        refreshLists();
        toast(back ? QString("Slots 1-0 restored, %1 favourite%2 put back").arg(back).arg(back == 1 ? "" : "s") : QString("Sound slots 1-0 restored"));
    }, "restore slots 1-0");
    ui.button(c4(3, 642, 60), "quit", "Quit", false, [this] { close(); }, "saves your setup");
}

void MainWindow::paintHelp() {
    struct Row { QString k, d; };
    auto K = [this](int act, bool cmd = false) { return (cmd ? km.keyFor(A_COMMAND, false) + "+" : QString()) + km.keyFor(act, cmd); };
    std::vector<Row> rows;
    if (helpPage == 0) {
        rows = {
            {km.rows[2].left(1) + QString::fromUtf8(" ‥ ") + km.rows[2].right(1), "high string: left to right goes up the scale"},
            {km.rows[1].left(1) + QString::fromUtf8(" ‥ ") + km.rows[1].right(1), "middle string"},
            {km.rows[0].left(1) + QString::fromUtf8(" ‥ ") + km.rows[0].right(1), "low string (or chords, see below)"},
            {"hold + press", "hold a key, press another on its row: it SLIDES"},
            {"let go", "release the newer key: it slides back"},
            {K(A_CHROMATIC) + " (hold)", "chromatic: keys step in semitones"},
            {K(A_SUSTAIN) + " (hold)", "sustain: released notes keep ringing"},
            {K(A_PANIC), "panic: silence everything"},
            {K(A_LOOP), "loop pedal: count-in, record, play, overdub"},
            {"Left / Right", "pitch bend down / up (hold)"},
            {"Up / Down", "octave up / down, held notes sweep"},
            {QString::fromUtf8("1 ‥ 9, 0"), "sounds 1 to 10"},
            {K(A_CHORD_LAYER) + "+" + km.rows[0].left(1) + QString::fromUtf8("‥") + km.rows[0].right(1), QString::fromUtf8("backing chord I ‥ VII")},
            {K(A_CHORD_ROW), "toggle: bottom row plays chords"},
            {"Tab / Esc", "next page / back to the Synth page"},
            {"screen", "drag the scope = X-Y control, drag a string = slide"},
        };
    } else if (helpPage == 2) {
        rows = {
            {km.rows[2].left(4).split("", Qt::SkipEmptyParts).join(" "), "Beat page: pads 1-4 (kick, snare, hat, open hat)"},
            {km.rows[1].left(4).split("", Qt::SkipEmptyParts).join(" "), "Beat page: pads 5-8 (clap, rim, tom, cowbell)"},
            {km.rows[2].mid(6, 4).split("", Qt::SkipEmptyParts).join(" ") + " / " + km.rows[1].mid(5, 4).split("", Qt::SkipEmptyParts).join(" "), "the same pads again, for the other hand"},
            {K(A_SUSTAIN), "Beat page: start / stop the beat"},
            {K(A_LOOP), "Beat page: Rec on / off (pads write hits)"},
            {km.rows[0].left(1) + QString::fromUtf8(" ‥ ") + km.rows[0].right(1), "Beat page: the low string still plays"},
            {K(A_BEAT_PLAY, true), "start / stop the beat from any page"},
            {K(A_BEAT_REC, true), "beat Rec on / off from any page"},
            {K(A_PLAY_LAST, true), "play the last recording back / stop it"},
            {"Rec + pads", "hits snap to the nearest of 16 steps in the bar"},
            {"Erase, pad", "tap Erase, then a pad: that drum's hits are removed"},
            {"Kit", "soft, crunchy or glitchy drum sounds"},
            {"Left / Right", "Beat page: the kept beat before / after"},
            {"+ New", "a fresh empty beat; the others are kept"},
        };
    } else if (helpPage == 3) {
        const QString c = km.keyFor(A_COMMAND, false) + "+";
        rows = {
            {K(A_SUSTAIN) + " / " + K(A_LOOP), "Track page: play or stop / back to bar 1"},
            {"Left / Right", "choose the clip before / after"},
            {"Up / Down", "choose a clip on the lane above / below"},
            {"Alt + arrows", "move the chosen clip a bar, or to the other loop lane"},
            {"Shift + L / R", "move the play line a bar (also , and .)"},
            {QString::fromUtf8("1 ‥ 5"), "take a clip from the shelf in hand"},
            {c + "X / C / V", "cut / copy the chosen clip / paste at the play line"},
            {c + "T / " + c + "J", "split the chosen clip at the play line / join"},
            {c + "R", "repeat: a copy right after the chosen clip"},
            {c + "Z", "undo (with " + K(A_CHROMATIC) + ": redo)"},
            {K(A_PANIC), "delete the chosen clip from the track"},
            {"M / C", "mute the chosen clip / cycle on or off"},
            {"E / I", "chop the chosen clip / pick another sound for it"},
            {"K / X", "keep what is live as clips / open the mixer"},
            {c + "B", "export the track as an audio file"},
            {K(A_TRACK_PLAY, true), "play or stop the track from any page"},
        };
    } else {
        rows = {
            {K(A_RANDOM, true), "new sound: invent an instrument"},
            {K(A_MUTATE, true), "mutate the current sound a little"},
            {K(A_UNDO_SOUND, true), "undo: back to the previous sound"},
            {K(A_FAVOURITE, true), "save the sound as a favourite"},
            {K(A_KEY_UP, true), "key up a semitone (with " + K(A_CHROMATIC) + ": down)"},
            {K(A_SCALE_NEXT, true), "next scale (with " + K(A_CHROMATIC) + ": previous)"},
            {K(A_TAP, true) + " " + K(A_METRO, true), "tap tempo / metronome"},
            {K(A_BACK_MODE, true), "chord taps: progression / hold / drone"},
            {K(A_BACK_STYLE, true), "backing style: sustain / pulse / arpeggio"},
            {K(A_BACK_PLAY, true) + " " + K(A_BACK_CLEAR, true), "backing start-stop / clear backing"},
            {K(A_BACK_REMOVE, true), "remove the last chord of the progression"},
            {K(A_LOOP_UNDO, true), "undo the last loop layer"},
            {K(A_LOOP_PLAY, true) + " " + K(A_LOOP_CLEAR, true), "loop start-stop / clear the loop"},
            {K(A_XYMAP, true), "next touch-pad mapping"},
            {K(A_RECORD, true), "record what you hear to a WAV file"},
            {K(A_HELP, true) + " " + K(A_CHAT, true) + " " + K(A_QUIT, true), "this guide / chat dock (with Shift: Chat page) / quit"},
        };
    }
    const QString title = helpPage == 0 ? QString("Playing") : (helpPage == 2 ? QString("Beat and recording") : (helpPage == 3 ? QString("Track: letters = commands") : QString("Commands: hold %1").arg(km.keyFor(A_COMMAND, false))));
    // header: what this page of the guide is, the tour, the next page, and the way to ask
    ui.tag(QRectF(16, 62, 242, 30), ui.fit(title, 14, 230), 14, Qt::AlignLeft | Qt::AlignVCenter);
    ui.zone(QRectF(16, 62, 242, 30), "help_section", [](QPointF, int) {});
    ui.button(QRectF(264, 62, 84, 30), "help_tour", "Tour", false, [this] { if (tourView) tourView->open(QString(), QString(), false, nullptr, true); });
    ui.button(QRectF(354, 62, 178, 30), "help_next", helpPage == 0 ? QString::fromUtf8("Command keys ›") : (helpPage == 1 ? QString::fromUtf8("Beat keys ›") : (helpPage == 2 ? QString::fromUtf8("Track keys ›") : QString::fromUtf8("‹ Playing keys"))), false, [this] { helpPage = (helpPage + 1) % 4; helpSel.clear(); });
    ui.button(QRectF(538, 62, 166, 30), "help_ask", "Ask Rook", false, [this] { helpAsk(); });
    // every row is a zone with a stable name built from its description, so Rook can point at it and a tap asks about it
    helpDesc.clear();
    QSet<QString> used;
    const qreal pitch = 29.5;
    for (size_t i = 0; i < rows.size(); ++i) {
        qreal y = 100 + i * pitch;
        QString slug = rows[i].d.toLower();
        slug.replace(QRegularExpression("[^a-z0-9]+"), "_");
        slug = slug.left(34); while (slug.endsWith('_')) slug.chop(1);
        QString nm = QString("help_row_%1").arg(slug), base = nm;
        for (int n = 2; used.contains(nm); ++n) nm = QString("%1_%2").arg(base).arg(n);
        used.insert(nm);
        helpDesc[nm] = QString("%1: %2").arg(rows[i].k, rows[i].d);
        const bool sel = helpSel == nm;
        QRectF kr(16, std::floor(y), 176, 25);
        ui.frame(kr, sel ? INK : LOW, sel ? INK : BG);
        ui.txt(kr, ui.fit(rows[i].k, 13, 172), 13, sel ? BG : INK, Qt::AlignCenter, true);
        ui.txt(QRectF(204, std::floor(y), 500, 25), ui.fit(rows[i].d, 13, 500), 13, sel || i % 2 == 0 ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter, sel);
        ui.zone(QRectF(16, std::floor(y), 688, 25), nm, [this, nm](QPointF, int ph) { if (ph == PH_PRESS) { helpSel = nm; helpAsk(); } });
    }
    ui.txt(QRectF(18, 586, 680, 18), "Q20: digits and arrows live on the keyboard's number / symbol layers.", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.txt(QRectF(18, 606, 680, 18), QString("Ask Rook: tap a row, or press %1+%2. Rook answers here and can point at things.").arg(km.keyFor(A_COMMAND, false)).arg(km.keyFor(A_CHAT, true)), 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.txt(QRectF(18, 626, 680, 18), QString("Tour: tap Tour (or press %1+%2) for a picture of every page. Tap any control to read what it does.").arg(km.keyFor(A_COMMAND, false)).arg(km.keyFor(A_TOUR, true)), 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.button(QRectF(16, 664, 688, 44), "help_back", "Back to playing", false, [this] { goPage(PG_PLAY); });
}

// Ask Rook from the Help page: opens the dock with the keyboard, and starts the question with the
// row that was tapped (or the part of the guide being read).
void MainWindow::helpAsk() {
    static const char *parts[4] = {"the playing keys", "the command keys", "the beat keys", "the track keys"};
    if (chat.draft.trimmed().isEmpty() || chat.draft == helpPrefill) {
        helpPrefill = helpSel.isEmpty() ? QString("About %1: ").arg(parts[helpPage]) : QString("What does \"%1\" do? ").arg(helpDesc.value(helpSel));
        chat.draft = helpPrefill.left(380);
    }
    guide.dockOpenFocus();
}

void MainWindow::paintKeyTest() {
    ui.zone(QRectF(0, 0, CW, CW), "keytestblock", [](QPointF, int) {}); // the panel swallows every touch but Close
    QRectF r(40, 96, 640, 520);
    ui.frame(r.adjusted(-3, -3, 3, 3), INK, BG);
    ui.frame(r, INK, BG);
    ui.tag(QRectF(r.x() + 1, r.y() + 1, 100, 22), "Key test", 14, Qt::AlignLeft | Qt::AlignVCenter);
    QStringList names;
    for (auto it = keys.held.begin(); it != keys.held.end(); ++it) names << keyName(it.key());
    names.sort();
    ui.txt(QRectF(60, 150, 600, 22), ui.fit(QString("Held now (%1): %2").arg(names.size()).arg(names.join(" ")), 14, 600), 14, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    ui.tag(QRectF(60, 176, 300, 22), QString("Most keys held at once: %1").arg(keys.maxHeld), 14, Qt::AlignLeft | Qt::AlignVCenter);
    for (int i = 0; i < keys.log.size(); ++i)
        ui.txt(QRectF(60, 210 + i * 24, 600, 22), keys.log[i], 14, keys.log[i].startsWith("DOWN") ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.g->setFont(ui.font(13, false));
    ui.g->setPen(MID);
    ui.g->drawText(QRectF(60, 506, 600, 80), Qt::AlignLeft | Qt::TextWordWrap,
                "Hold one key on each row, then add more. Every key should show DOWN once and stay in Held until you let go. "
                "A key you did not press appearing here, or a pressed key missing, is a keyboard-matrix limit.");
    ui.button(QRectF(516, 100, 160, 44), "keytestclose", "Close", false, [this] { keys.testOpen = false; });
}
