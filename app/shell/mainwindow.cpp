#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include "app/ui/palette.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
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

// ------------------------------------------------------------ param text

float paramToNorm(int p, float v) {
    const ParamInfo &m = paramInfo(p);
    if (m.logScale) return std::log(v / m.min) / std::log(m.max / m.min);
    return (v - m.min) / (m.max - m.min);
}
float normToParam(int p, float n) {
    const ParamInfo &m = paramInfo(p);
    n = n < 0 ? 0 : (n > 1 ? 1 : n);
    float v = m.logScale ? m.min * std::pow(m.max / m.min, n) : m.min + n * (m.max - m.min);
    if (m.integer) v = std::round(v);
    return v;
}
QString paramText(int p, float v) {
    switch (p) {
    case P_WAVE1: case P_WAVE2: return waveName((int)v);
    case P_FMODE: return filterName((int)v);
    case P_DLYDIV: return delayDivName((int)v);
    case P_SEMI2: return QString("%1%2 st").arg(v > 0 ? "+" : "").arg((int)v);
    case P_CUTOFF: return v >= 1000 ? QString("%1 kHz").arg(v / 1000, 0, 'f', 1) : QString("%1 Hz").arg((int)v);
    case P_DETUNE: case P_SPREAD: case P_VIBDEPTH: return QString("%1 ct").arg((int)v);
    case P_GLIDE: return QString("%1 ms").arg((int)v);
    case P_ATK: case P_DEC: case P_REL: case P_FATK: case P_FDEC: case P_FREL: case P_VIBDELAY:
        return v < 1 ? QString("%1 ms").arg((int)(v * 1000)) : QString("%1 s").arg(v, 0, 'f', 1);
    case P_VIBRATE: case P_LFORATE: return QString("%1 Hz").arg(v, 0, 'f', v < 1 ? 2 : 1);
    case P_FENV: case P_LFOCUT: return QString("%1 oct").arg(v, 0, 'f', 1);
    case P_FM: return QString::number(v, 'f', 1);
    case P_LEVEL: return QString("%1 dB").arg(20 * std::log10(v), 0, 'f', 1);
    default: {
        const ParamInfo &m = paramInfo(p);
        return QString("%1%").arg((int)std::round((v - m.min) / (m.max - m.min) * 100));
    }
    }
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
        if (st.contains("track")) store::trackFromJson(st["track"].toObject(), track);
        chat.load(st);
        tourSeenFlag = st["tourSeen"].toBool(false);
        tourLastPage = st["tourPage"].toString();
    }
    beatsLoad(st, sd);
    sd.lead = sounds.lead; sd.backing = sounds.lead;
    inst.restore(sd);
    inst.setTrack(std::make_shared<const TrackData>(track));
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
                    s.beatPlaying || recorder.playingBack() || clock.elapsed() < chat.tickerEnds() + 200 || pg == PG_CHAT || guide.dockOpen || clock.elapsed() < guide.hl.until + 200 || s.trackPlaying || exportState.load() != 0 || tourOpen() || (tourView && tourView->promptShowing());
        if (busy || ++skip >= 12) { skip = 0; update(); }
    });
    frameTimer.start(40);
    sink.start();
}

MainWindow::~MainWindow() {
    recorder.shutdown();
    if (exportThread.joinable()) exportThread.join();
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
    o["track"] = store::trackToJson(track);
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
    o["track"] = store::trackToJson(track);
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
        trackUndo.push_back(track);
        trackRedo.clear();
        store::trackFromJson(o["track"].toObject(), track);
        selLane = selIdx = -1; handOn = false; chopOpen = false; dashCache.clear();
        playBar = viewBar = 0;
        trackSync();
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
    for (int s = 0; s < kStrings; ++s)
        if (touchCol[s] >= 0) { inst.key(s, touchCol[s] | 0x100, false); touchCol[s] = -1; }
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
    case A_TRACK_PLAY: trackPlayStop(); break;
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
    if (pg == PG_TRACK && !keys.testOpen && trackKey(key, plainAct)) { keys.held[key] = {HK_NONE, 0, 0}; return; }
    if (pg == PG_CHAT && !keys.testOpen && chat.key(key, plainAct)) { keys.held[key] = {HK_NONE, 0, 0}; return; }
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

void MainWindow::paramSlider(QRectF r, int p) {
    ui.slider(r, QString("param%1").arg(p), nice(paramInfo(p).label), paramToNorm(p, sounds.lead.p[p]), paramText(p, sounds.lead.p[p]),
           [this, p](float n) { sounds.setParam(p, normToParam(p, n)); });
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
    pollExport();
    hearWatch();
    beatWatch();

    if (tourOpen()) tourView->paint(); // the tour draws a picture instead of the page: nothing on it is live
    else switch (pg) {
    case PG_TRACK: paintTabs(); paintTrack(); break;
    case PG_PLAY: paintPlay(); break;
    case PG_SOUND: paintTabs(); paintSound(); break;
    case PG_BACK: paintTabs(); paintBacking(); break;
    case PG_BEAT: paintTabs(); paintBeat(); break;
    case PG_TUNE: paintTabs(); paintTune(); break;
    case PG_SYS: paintTabs(); paintSystem(); break;
    case PG_HELP: paintTabs(); paintHelp(); break;
    case PG_CHAT: paintTabs(); chatPage.paint(); break;
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

void MainWindow::paintLoopButton(QRectF r) {
    bool dn = ui.isDown("loop_pedal");
    QString label = "Loop", sub = "tap to record";
    bool inv = dn, armed = false;
    float prog = 0;
    bool blink = (clock.elapsed() / 300) % 2 == 0;
    switch (u.loopState) {
    case L_COUNTIN: label = QString::number(qMax(1, u.countBeat)); sub = "count-in"; armed = true; break;
    case L_REC: label = "REC"; sub = QString("bar %1").arg((int)u.loopPos + 1); inv = true; break;
    case L_PLAY:
        if (u.loopClosing) { label = "REC"; sub = QString::fromUtf8("closing bar…"); inv = true; }
        else { label = u.layers > 1 ? QString::fromUtf8("Loop ×%1").arg(u.layers) : QString("Loop"); sub = QString("%1 bar%2, tap = dub").arg(u.loopBars).arg(u.loopBars == 1 ? "" : "s"); armed = true; }
        prog = u.loopPos; break;
    case L_OVERDUB: label = "DUB"; sub = u.loopFull ? "memory full" : "tap to finish"; inv = blink; armed = true; prog = u.loopPos; break;
    case L_STOPPED: label = "Loop"; sub = "stopped, tap = play"; break;
    default: break;
    }
    ui.frame(r, inv || armed ? INK : LOW, inv ? INK : BG);
    if (armed && !inv) ui.frame(r.adjusted(3, 3, -3, -3), INK);
    QColor tc = inv ? BG : INK;
    ui.txt(QRectF(r.x(), r.y() + 9, r.width(), 32), label, 24, u.loopState == L_STOPPED ? MID : tc, Qt::AlignCenter, true);
    ui.txt(QRectF(r.x(), r.y() + 45, r.width(), 16), ui.fit(sub, 11, r.width() - 10), 11, inv ? BG : MID);
    if (prog > 0) { // position in the loop
        QRectF b(r.x() + 9, r.bottom() - 14, r.width() - 18, 5);
        ui.frame(b, inv ? BG : MID);
        ui.g->fillRect(QRectF(b.x(), b.y(), b.width() * qMin(1.0f, prog), b.height()), tc);
    }
    ui.zone(r, "loop_pedal", [this](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_LOOP, LC_PEDAL); });
}

void MainWindow::paintChordPads(QRectF area, bool withSlots) {
    const Tuning &t = u.set.tuning;
    qreal y = area.y();
    if (withSlots) {
        // progression strip
        qreal sw = 62;
        for (int i = 0; i < kProgMax; ++i) {
            QRectF r(area.x() + i * (sw + 4), y, sw, 22);
            bool has = i < u.progLen, cur = has && i == u.progPos && u.backPlaying, sel = i == u.selSlot;
            ui.frame(r, sel || cur ? INK : (has ? MID : GRAT), cur ? INK : BG);
            if (sel && !cur) ui.frame(r.adjusted(2, 2, -2, -2), INK);
            if (has) ui.txt(r, buildChord(t, u.prog[i], u.set.seventh).name, 13, cur ? BG : INK, Qt::AlignCenter, true);
            else ui.txt(r, QString::number(i + 1), 11, GRAT);
            if (has) ui.zone(r.adjusted(0, -6, 0, 6), QString("pslot%1").arg(i), [this, i](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_BACK, BC_SELECT_SLOT, i); });
        }
        QString info;
        if (u.backPlaying && (u.progLen || u.holdChord >= 0 || u.droneMask)) info = QString::fromUtf8("♪ %1").arg(u.backName);
        else if (u.progLen) info = "stopped";
        else info = keys.chordRow ? QString::fromUtf8("Z‥M = chords") : QString("tap chords");
        qreal ix = area.x() + 8 * (sw + 4);
        ui.txt(QRectF(ix, y, area.right() - ix, 22), ui.fit(info, 12, area.right() - ix), 12, MID, Qt::AlignRight | Qt::AlignVCenter);
        y += 28;
    }
    qreal h = area.bottom() - y;
    qreal ctl = 92, pw = (area.width() - ctl - 7 * 6) / 7.0;
    for (int d = 0; d < 7; ++d) {
        QRectF r(std::floor(area.x() + d * (pw + 6)), y, std::floor(pw), h);
        QString name = QString("chord%1").arg(d);
        bool cur = u.curChord == d, dn = ui.isDown(name);
        bool drone = (u.droneMask >> d) & 1;
        bool inv = cur || dn;
        ui.frame(r, inv || drone ? INK : LOW, inv ? INK : BG);
        if (drone && !inv) ui.frame(r.adjusted(3, 3, -3, -3), INK);
        Chord c = buildChord(t, d, u.set.seventh);
        ui.txt(QRectF(r.x(), r.y() + h * 0.10, r.width(), h * 0.52), c.name, c.name[3] ? 16 : 20, inv ? BG : INK, Qt::AlignCenter, true);
        bool small = std::strchr(c.name + 1, 'm') && !std::strstr(c.name, "maj");
        ui.txt(QRectF(r.x(), r.y() + h * 0.62, r.width(), h * 0.30), romanNumeral(d, small), 12, inv ? BG : MID);
        ui.zone(r, name, [this, d](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_CHORD, d); });
    }
    QRectF c1(area.right() - ctl, y, ctl, h / 2 - 3), c2(area.right() - ctl, y + h / 2 + 3, ctl, h / 2 - 3);
    static const char *MN[BM_COUNT] = {"Prog", "Hold", "Drone"};
    ui.button(c1, "backmode", MN[u.set.backMode], false, [this] { perform(A_BACK_MODE); });
    ui.button(c2, "backplay", u.backPlaying ? QString::fromUtf8("■ Stop") : QString::fromUtf8("▶ Play"), u.backPlaying,
           [this] { inst.cmd(EV_BACK, BC_PLAYSTOP); });
}

void MainWindow::paintPlay() {
    const Tuning &t = u.set.tuning;
    const int L = Qt::AlignLeft | Qt::AlignVCenter, R = Qt::AlignRight | Qt::AlignVCenter;
    // ---------------- header: readouts ----------------
    ui.txt(QRectF(18, 6, 430, 44), ui.fit(sounds.lead.name, 30, 428), 30, INK, L, true);
    QString slotTxt = sounds.curSlot >= 0 ? QString("Sound %1").arg(sounds.curSlot + 1) : QString("Unsaved sound");
    ui.txt(QRectF(20, 52, 230, 20), QString("%1   %2 voices").arg(slotTxt).arg(u.voices), 13, MID, L);
    if (recorder.recording()) {
        if ((clock.elapsed() / 400) % 2 == 0) ui.tag(QRectF(250, 54, 104, 17), QString("REC %1 s").arg(recorder.seconds()), 12);
    } else if (sink.dummy) {
        ui.tag(QRectF(250, 54, 150, 17), "No audio device", 12);
    } else if (u.limiter < 0.85f) {
        ui.tag(QRectF(250, 54, 60, 17), "Limit", 12);
    }
    ui.zone(QRectF(16, 4, 430, 72), "hdr_sound", [this](QPointF, int ph) { if (ph == PH_PRESS) goPage(PG_SOUND); });

    ui.g->fillRect(QRectF(444, 10, 1, 60), GRAT);
    ui.txt(QRectF(456, 6, 60, 44), noteName(t.root), 32, INK, L, true);
    ui.txt(QRectF(456, 52, 110, 20), QString(scaleDef(t.scale).shortName).toLower(), 13, MID, L);
    ui.txt(QRectF(552, 6, 86, 44), QString::number((int)std::round(u.set.bpm)), 28, INK, R, true);
    ui.txt(QRectF(552, 52, 86, 20), "bpm", 13, MID, R);
    double frac = u.beat - std::floor(u.beat);
    bool running = u.backPlaying || u.loopState != L_EMPTY || u.beatPlaying || u.trackPlaying;
    ui.frame(QRectF(552, 24, 9, 9), running ? INK : LOW, running && frac < 0.25 ? INK : BG); // beat lamp
    ui.txt(QRectF(644, 6, 60, 44), QString("%1%2").arg(t.octave > 0 ? "+" : "").arg(t.octave), 28, INK, R, true);
    ui.txt(QRectF(644, 52, 60, 20), "oct", 13, MID, R);
    ui.zone(QRectF(450, 4, 254, 72), "hdr_tune", [this](QPointF, int ph) { if (ph == PH_PRESS) goPage(PG_TUNE); });
    // the readouts inside it, named so they can be pointed at; they open the Tune page too
    for (const auto &rd : {std::make_pair(QRectF(452, 4, 96, 72), "key_readout"), std::make_pair(QRectF(550, 4, 90, 72), "tempo_readout"), std::make_pair(QRectF(642, 4, 62, 72), "octave_readout")})
        ui.zone(rd.first, rd.second, [this](QPointF, int ph) { if (ph == PH_PRESS) goPage(PG_TUNE); });

    // output level: a row of segments
    {
        int lit = (int)std::ceil(qMin(1.0f, u.peak) * 86);
        for (int i = 0; i < 86; ++i) ui.g->fillRect(QRectF(16 + i * 8, 80, 6, 4), i < lit ? INK : GRAT);
    }

    // ---------------- scope + X-Y pad ----------------
    QRectF xy(16, 92, 688, 226);
    drawScope(xy);
    ui.zone(xy, "xy", [this, xy](QPointF p, int ph) {
        Event e; e.type = EV_XY; e.a = ph != PH_RELEASE;
        e.f = (float)qBound(0.0, (p.x() - xy.x()) / xy.width(), 1.0);
        e.g = (float)qBound(0.0, (xy.bottom() - p.y()) / xy.height(), 1.0);
        inst.push(e);
    });
    ui.button(QRectF(xy.right() - 196, xy.y() + 8, 118, 40), "xymap", nice(xyMapName(u.set.xyMap)), false, [this] { perform(A_XYMAP); });
    ui.button(QRectF(xy.right() - 72, xy.y() + 8, 64, 40), "xyhold", "Hold", u.set.xyHold, [this] { set(S_XYHOLD, u.set.xyHold ? 0 : 1); });

    // ---------------- strings ----------------
    QRectF sp(16, 328, 688, 176);
    ui.frame(sp, LOW, BG);
    const qreal x0 = sp.x() + 40, dx = (sp.width() - 80) / 9.0;
    qint64 now = clock.elapsed();
    for (int row = 2; row >= 0; --row) {
        qreal y = sp.y() + 36 + (2 - row) * 54;
        int count = km.rows[row].size();
        int shift = row == 0 ? 1 : 0; // the Q20's bottom letter row starts one key in
        auto X = [&](qreal col) { return x0 + (col + shift) * dx; };
        const StringUi &su = u.str[row];
        // the string is a trace: flat at rest, standing wave while it sounds
        if (su.gate) {
            QPainterPath line;
            for (int i = 0; i <= 80; ++i) {
                qreal x = sp.x() + 8 + (sp.width() - 16) * i / 80.0;
                qreal yy = y + 3.5 * std::sin(i * 0.7 + now * 0.03) * std::sin(3.14159 * i / 80.0);
                if (i == 0) line.moveTo(x, yy); else line.lineTo(x, yy);
            }
            ui.g->setRenderHint(QPainter::Antialiasing, true);
            ui.g->setBrush(Qt::NoBrush);
            ui.g->setPen(QPen(QColor(255, 255, 255, 60), 5));
            ui.g->drawPath(line);
            ui.g->setPen(QPen(INK, 1.6));
            ui.g->drawPath(line);
            ui.g->setRenderHint(QPainter::Antialiasing, false);
        } else {
            ui.g->fillRect(QRectF(sp.x() + 8, y, sp.width() - 16, 1), LOW);
        }
        ui.txt(QRectF(sp.x() + 4, y - 26, 20, 12), QString("S%1").arg(row + 1), 10, su.gate ? INK : LOW, L);
        // pitch -> x (piecewise linear across the row's notes)
        auto pitchX = [&](float pitch) {
            float pbase = pitch - u.bend;
            int prev = t.noteFor(row, 0, false);
            if (pbase <= prev) return X(0) - qMin(0.6, (double)(prev - pbase) / 6.0) * dx;
            for (int c = 1; c < count; ++c) {
                int n = t.noteFor(row, c, false);
                if (pbase <= n) return X(c - 1) + dx * (pbase - prev) / qMax(1, n - prev);
                prev = n;
            }
            return X(count - 1) + qMin(0.6, (double)(pbase - prev) / 6.0) * dx;
        };
        bool chordKeys = row == 0 && (keys.chordRow || keys.chord);
        for (int c = 0; c < count; ++c) {
            int n = t.noteFor(row, c, false);
            bool isHeldTop = su.held > 0 && su.topCol == c;
            bool isRoot = ((n - t.root) % 12 + 12) % 12 == 0;
            QRectF box(std::floor(X(c)) - 17, y - 11, 35, 23);
            ui.frame(box, isHeldTop || isRoot ? INK : LOW, isHeldTop ? INK : BG);
            QString nn = QString("%1%2").arg(noteName(n)).arg(n / 12 - 1);
            ui.txt(box, nn, 12, isHeldTop ? BG : (isRoot ? INK : MID), Qt::AlignCenter, isRoot || isHeldTop);
            QRectF kr(box.center().x() - 8, y - 27, 16, 13);
            if (chordKeys) ui.tag(kr.adjusted(1, 0, -1, 0), QString(km.rows[row][c]), 10, Qt::AlignCenter);
            else ui.txt(kr, QString(km.rows[row][c]), 11, LOW);
        }
        // where the pitch really is: a cursor that travels during a slide, leaving a fading trail
        std::deque<float> &tr = trail[row];
        if (su.gate) {
            float px = (float)pitchX(su.note + u.bend);
            tr.push_back(px);
            if (tr.size() > 10) tr.pop_front();
            bool sliding = std::fabs(su.note - su.target) > 0.02f;
            for (size_t i = 0; i + 1 < tr.size(); ++i) {
                if (std::fabs(tr[i] - px) < 1.0f) continue;
                int v = 40 + 14 * (int)i;
                ui.g->fillRect(QRectF(std::floor(tr[i]), y + 13, 1, 9), QColor(v, v, v));
            }
            qreal cx = std::floor(px);
            int wd = sliding ? 3 : 2;
            ui.g->fillRect(QRectF(cx - 21, y - 15, wd, 31), INK); // [ ] brackets round the sounding pitch
            ui.g->fillRect(QRectF(cx + 21 - wd + 1, y - 15, wd, 31), INK);
            ui.g->fillRect(QRectF(cx - 21, y - 15, 7, wd), INK);
            ui.g->fillRect(QRectF(cx - 21, y + 16 - wd, 7, wd), INK);
            ui.g->fillRect(QRectF(cx + 15, y - 15, 7, wd), INK);
            ui.g->fillRect(QRectF(cx + 15, y + 16 - wd, 7, wd), INK);
            QPainterPath m; // trigger-style marker under the string
            m.moveTo(cx - 4, y + 23); m.lineTo(cx + 5, y + 23); m.lineTo(cx + 0.5, y + 17); m.closeSubpath();
            ui.g->setRenderHint(QPainter::Antialiasing, true);
            ui.g->setPen(Qt::NoPen);
            ui.g->setBrush(INK);
            ui.g->drawPath(m);
            ui.g->setRenderHint(QPainter::Antialiasing, false);
        } else if (!tr.empty()) tr.clear();
        // touch: press plays, dragging along the string slides
        QRectF zr(sp.x(), y - 27, sp.width(), 54);
        ui.zone(zr, QString("string%1").arg(row), [this, row, count, x0, dx, shift](QPointF p, int ph) {
            int col = (int)std::floor((p.x() - x0) / dx - shift + 0.5);
            col = qBound(0, col, count - 1);
            int &cur = touchCol[row];
            if (ph == PH_RELEASE) { if (cur >= 0) inst.key(row, cur | 0x100, false); cur = -1; return; }
            if (col == cur) return;
            inst.key(row, col | 0x100, true, keys.chromatic);
            if (cur >= 0) inst.key(row, cur | 0x100, false);
            cur = col;
        });
    }

    // ---------------- chords ----------------
    paintChordPads(QRectF(16, 512, 688, 106), true);

    // ---------------- bottom bar: the soft keys ----------------
    qreal x = 16, y = 628, h = 80;
    paintLoopButton(QRectF(x, y, 150, h)); x += 158;
    ui.button(QRectF(x, y, 66, h), "loop_undo", "Undo", false, [this] { inst.cmd(EV_LOOP, LC_UNDO); }, "layer"); x += 74;
    bool lp = u.loopState == L_PLAY || u.loopState == L_OVERDUB;
    ui.button(QRectF(x, y, 66, h), "loop_playstop", lp ? QString::fromUtf8("■") : QString::fromUtf8("▶"), false, [this] { inst.cmd(EV_LOOP, LC_PLAYSTOP); }, "loop"); x += 74;
    ui.button(QRectF(x, y, 66, h), "sustain", "Sus", u.sustain, [this] { keys.susLatch = !keys.susLatch; inst.cmd(EV_SUSTAIN, keys.susLatch || keys.susKey ? 1 : 0); }, "sustain"); x += 74;
    ui.button(QRectF(x, y, 54, h), "octave_down", QString::fromUtf8("−"), false, [this] { inst.cmd(EV_OCTAVE, -1); }, "oct"); x += 62;
    ui.button(QRectF(x, y, 54, h), "octave_up", "+", false, [this] { inst.cmd(EV_OCTAVE, 1); }, "oct"); x += 62;
    ui.button(QRectF(x, y, 86, h), "new_sound", "New", false, [this] { sounds.randomise(); }, "sound"); x += 94;
    ui.button(QRectF(x, y, 704 - x, h), "menu", "Menu", false, [this] { goPage(PG_SOUND); }, QString::fromUtf8("≡"));
}

void MainWindow::paintSound() {
    qreal w = (688 - 4 * 8) / 5.0;
    for (int i = 0; i < 10; ++i) {
        QRectF r(std::floor(16 + (i % 5) * (w + 8)), 66 + (i / 5) * 56, std::floor(w), 50);
        QString name = QString("slot%1").arg(i);
        bool on = sounds.curSlot == i && !sounds.slotSave;
        bool inv = on || ui.isDown(name);
        ui.frame(r, inv || sounds.slotSave ? INK : LOW, inv ? INK : BG);
        if (sounds.slotSave && !inv) ui.frame(r.adjusted(3, 3, -3, -3), INK);
        // slot number in a corner tab, name beneath: laid out like a soft-key label
        ui.g->fillRect(QRectF(r.x() + 1, r.y() + 1, 18, 16), inv ? BG : LOW);
        ui.txt(QRectF(r.x() + 1, r.y() + 1, 18, 16), QString::number((i + 1) % 10), 12, inv ? INK : BG, Qt::AlignCenter, true);
        ui.txt(QRectF(r.x() + 4, r.y() + 20, r.width() - 8, 24), ui.fit(sounds.bank[i].name, 13, r.width() - 8), 13, inv ? BG : INK, Qt::AlignLeft | Qt::AlignVCenter, true);
        ui.zone(r, name, [this, i](QPointF, int ph) { if (ph == PH_PRESS) sounds.selectSlot(i); });
    }
    qreal y = 182;
    auto col = [w](int i) { return QRectF(std::floor(16 + i * (w + 8)), 182, std::floor(w), 54); };
    Q_UNUSED(y);
    ui.button(col(0), "newsound", "New sound", false, [this] { sounds.randomise(); }, "randomise");
    ui.button(col(1), "mutate", "Mutate", false, [this] { sounds.mutate(); }, "nudge it");
    ui.button(col(2), "undosound", "Undo", false, [this] { sounds.undoSound(); }, QString("%1 step%2").arg(sounds.undo.size()).arg(sounds.undo.size() == 1 ? "" : "s"));
    ui.button(col(3), "slotsave", "Store", sounds.slotSave, [this] { sounds.slotSave = !sounds.slotSave; }, sounds.slotSave ? "pick a slot" : "to a slot 1-0");
    ui.button(col(4), "fav", QString::fromUtf8("♥ Save"), false, [this] { sounds.saveFavourite(); }, "favourite");

    ui.txt(QRectF(18, 246, 440, 40), ui.fit(sounds.lead.name, 24, 440), 24, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    ui.button(QRectF(704 - 220, 248, 220, 38), "deep", deepEdit ? QString::fromUtf8("‹ Quick controls") : QString::fromUtf8("All parameters ›"), deepEdit, [this] { deepEdit = !deepEdit; });

    if (deepEdit) {
        const int per = 12, pages = (P_COUNT + per - 1) / per;
        deepPage = qBound(0, deepPage, pages - 1);
        for (int k = 0; k < per; ++k) {
            int p = deepPage * per + k;
            if (p >= P_COUNT) break;
            paramSlider(QRectF(16 + (k % 2) * 348, 296 + (k / 2) * 58, 340, 52), p);
        }
        ui.button(QRectF(16, 650, 160, 54), "deepprev", QString::fromUtf8("‹ Prev"), false, [this, pages] { deepPage = (deepPage + pages - 1) % pages; });
        ui.txt(QRectF(176, 650, 368, 54), QString("Page %1 / %2").arg(deepPage + 1).arg(pages), 14, MID);
        ui.button(QRectF(544, 650, 160, 54), "deepnext", QString::fromUtf8("Next ›"), false, [this, pages] { deepPage = (deepPage + 1) % pages; });
        return;
    }
    static const int macro[8] = {P_CUTOFF, P_RESO, P_ATK, P_REL, P_GLIDE, P_DRIVE, P_DLYMIX, P_REVMIX};
    for (int k = 0; k < 8; ++k) paramSlider(QRectF(16 + (k % 2) * 348, 296 + (k / 2) * 58, 340, 52), macro[k]);

    // favourites
    const int per = 6, pages = qMax(1, ((int)sounds.favs.size() + per - 1) / per);
    favPage = qBound(0, favPage, pages - 1);
    ui.section(534, QString("Favourites  %1").arg(sounds.favs.size()));
    qreal cw = (688 - 110 - 3 * 8) / 3.0;
    for (int k = 0; k < per; ++k) {
        int idx = favPage * per + k;
        QRectF r(std::floor(16 + (k % 3) * (cw + 8)), 560 + (k / 3) * 74, std::floor(cw), 66);
        if (idx >= sounds.favs.size()) { ui.frame(r, GRAT); continue; }
        ui.button(r, QString("favitem%1").arg(k), sounds.favs[idx].patch.name, false, [this, idx] {
            if (idx >= sounds.favs.size()) return;
            if (favDelete) { QFile::remove(sounds.favs[idx].file); refreshLists(); favDelete = false; toast("Favourite removed"); return; }
            sounds.recall(sounds.favs[idx].patch);
        }, favDelete ? "tap to delete" : "recall", favDelete ? LOOK_ARMED : LOOK_PLAIN);
    }
    qreal bx = 704 - 102;
    { // the arrows say which way: the left half goes back a page, the right half forward, both wrapping
        const QRectF r(bx, 560, 102, 30);
        const bool inv = ui.isDown("favprev");
        ui.frame(r, inv ? INK : LOW, inv ? INK : BG);
        ui.txt(r, QString::fromUtf8("‹ %1/%2 ›").arg(favPage + 1).arg(pages), 14, inv ? BG : INK, Qt::AlignCenter, true);
        ui.zone(r, "favprev", [this, r, pages](QPointF p, int ph) {
            if (ph == PH_PRESS) favPage = (favPage + (p.x() < r.center().x() ? pages - 1 : 1)) % pages;
        });
    }
    ui.button(QRectF(bx, 598, 102, 102), "favdel", "Delete", favDelete, [this] { favDelete = !favDelete; }, favDelete ? "pick one" : "a favourite");
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

    paintChordPads(QRectF(16, 356, 688, 84), false);

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
