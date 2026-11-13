#include "mainwindow.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QTouchEvent>
#include <cmath>
#include <cstring>

using namespace td;

namespace {
const QColor BG(0x10, 0x0e, 0x0c), PANEL(0x1a, 0x17, 0x14), PANEL2(0x24, 0x20, 0x1b), PANEL3(0x33, 0x2d, 0x26);
const QColor LINE(0x3d, 0x36, 0x2e), TEXT(0xef, 0xe7, 0xd6), DIM(0x93, 0x8a, 0x7b);
const QColor AMBER(0xff, 0xae, 0x35), TEAL(0x55, 0xe3, 0xc6), RED(0xff, 0x55, 0x44);
const qreal CW = 720;
enum { HK_STRING, HK_ACT, HK_NONE };

QString sinkDescription; // refreshed asynchronously via wpctl
bool sinkIsDummy = false, sinkIsBluetooth = false;
int sinkGeneration = 0; // bumps when the output device type changes
} // namespace

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

MainWindow::MainWindow(Instrument &i, AudioOut &a) : inst(i), audio(a) {
    setWindowTitle("Tendril");
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    resize(720, 720);
    fDisplay = QFont("URW Gothic");
    fLabel = QFont("Roboto Condensed");
    fMono = QFont("Ubuntu Sans Mono");
    fMono.setStyleHint(QFont::Monospace);
    fLabel.setStyleHint(QFont::SansSerif);
    fDisplay.setStyleHint(QFont::SansSerif);
    rng.s = (uint32_t)QDateTime::currentMSecsSinceEpoch() | 1u;
    clock.start();

    store::ensureDirs();
    km = KeyMap::loadOrCreate(store::baseDir() + "/keymap.json");
    for (int k = 0; k < 10; ++k) bank[k] = factoryPatch(k);
    lead = bank[0];
    SessionData sd;
    QJsonObject st;
    if (store::readJson(store::baseDir() + "/state.json", st)) {
        sd.settings = store::settingsFromJson(st["settings"].toObject());
        QJsonArray sl = st["slots"].toArray();
        for (int k = 0; k < 10 && k < sl.size(); ++k) bank[k] = store::patchFromJson(sl[k].toObject());
        curSlot = qBound(-1, st["slot"].toInt(0), 9);
        lead = st.contains("lead") ? store::patchFromJson(st["lead"].toObject()) : bank[qMax(0, curSlot)];
        chordRow = st["chordRow"].toBool(false);
    }
    sd.lead = lead; sd.backing = lead;
    inst.restore(sd);
    refreshLists();

    connect(&frameTimer, &QTimer::timeout, this, [this] {
        // 25 fps while anything moves; a slow tick when the instrument is idle (battery).
        // Key and touch input call update() themselves, so response stays instant.
        static int skip = 0;
        UiState s = inst.uiState();
        bool busy = s.voices > 0 || !captured.isEmpty() || s.backPlaying || s.loopState != L_EMPTY || s.peak > 0.002f ||
                    clock.elapsed() < toastUntil + 200 || listenState == 1 || wav.isOpen() || !held.isEmpty();
        if (busy || ++skip >= 12) { skip = 0; update(); }
    });
    frameTimer.start(40);
    connect(&recTimer, &QTimer::timeout, this, [this] { drainRecording(); });

    // which sink is PipeWire really sending us to? (tells the player when it is the dummy device)
    auto *sinkTimer = new QTimer(this);
    auto probe = [this] {
        auto *pr = new QProcess(this);
        connect(pr, &QProcess::finished, this, [pr](int, QProcess::ExitStatus) {
            QString out = QString::fromUtf8(pr->readAllStandardOutput());
            QString desc; bool dummy = false, bt = false;
            for (const QString &ln : out.split('\n')) {
                if (ln.contains("node.description")) desc = ln.section('"', 1, 1);
                if (ln.contains("node.name") && ln.contains("auto_null")) dummy = true;
                if (ln.contains("node.name") && ln.contains("bluez_output")) bt = true;
            }
            if (!out.isEmpty()) {
                if (bt != sinkIsBluetooth || sinkGeneration == 0) ++sinkGeneration;
                sinkDescription = desc; sinkIsDummy = dummy; sinkIsBluetooth = bt;
            }
            pr->deleteLater();
        });
        connect(pr, &QProcess::errorOccurred, this, [pr](QProcess::ProcessError) { pr->deleteLater(); });
        pr->start("wpctl", {"inspect", "@DEFAULT_AUDIO_SINK@"});
    };
    connect(sinkTimer, &QTimer::timeout, this, probe);
    sinkTimer->start(4000);
    probe();
}

MainWindow::~MainWindow() {
    if (listenThread.joinable()) listenThread.join();
    if (wav.isOpen()) stopRecording();
}

void MainWindow::closeEvent(QCloseEvent *e) {
    saveState();
    inst.cmd(EV_PANIC);
    e->accept();
}

void MainWindow::saveState() {
    SessionData sd;
    inst.snapshot(sd);
    QJsonObject o;
    o["settings"] = store::settingsToJson(sd.settings);
    QJsonArray sl;
    for (const Patch &p : bank) sl.append(store::patchToJson(p));
    o["slots"] = sl;
    o["slot"] = curSlot;
    o["lead"] = store::patchToJson(lead);
    o["chordRow"] = chordRow;
    store::writeJson(store::baseDir() + "/state.json", o);
}

void MainWindow::refreshLists() {
    favs = store::listPatches("favourites");
    sessions = store::listFiles("sessions", "json");
}

void MainWindow::toast(const QString &s, int ms) {
    toastText = s;
    toastUntil = clock.elapsed() + ms;
}

// --------------------------------------------------------------- actions

void MainWindow::pushUndo() {
    undo.push_back(lead);
    if (undo.size() > 24) undo.erase(undo.begin());
}

void MainWindow::applyLead(bool resetSlot) {
    if (resetSlot) curSlot = -1;
    inst.setPatch(0, lead);
}

void MainWindow::selectSlot(int i) {
    if (i < 0 || i > 9) return;
    if (slotSave) {
        bank[i] = lead; curSlot = i; slotSave = false;
        toast(QString("Stored in slot %1").arg(i + 1));
        saveState();
        return;
    }
    pushUndo();
    lead = bank[i]; curSlot = i;
    applyLead(false);
}

void MainWindow::randomise() {
    pushUndo();
    lead = randomPatch(rng);
    applyLead(true);
    toast(QString("New sound: %1").arg(lead.name), 1400);
}

void MainWindow::mutate() {
    pushUndo();
    QByteArray nm(lead.name);
    lead = mutatePatch(lead, rng, 0.3f);
    if (!nm.endsWith("*") && nm.size() < 26) nm += "*";
    lead.setName(nm.constData());
    applyLead(true);
}

void MainWindow::undoSound() {
    if (undo.empty()) { toast("Nothing to undo"); return; }
    lead = undo.back(); undo.pop_back();
    applyLead(true);
    toast(QString("Back to: %1").arg(lead.name), 1200);
}

void MainWindow::saveFavourite() {
    QString path = store::savePatch("favourites", lead);
    refreshLists();
    toast(path.isEmpty() ? "Could not save favourite" : QString("Favourite saved: %1").arg(lead.name));
}

void MainWindow::setParam(int p, float v) {
    lead.p[p] = v;
    Event e; e.type = EV_PATCHPARAM; e.a = (int16_t)p; e.f = v;
    inst.push(e);
}

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

QString MainWindow::startRecording() {
    store::ensureDirs();
    QString path = store::baseDir() + "/recordings/" + store::timestampName("tendril", "wav");
    if (!wav.open(path, audio.sampleRate())) { toast("Could not create WAV file"); return QString(); }
    float junk[1024];
    while (inst.readRecorded(junk, 512) > 0) {} // discard stale frames
    inst.setRecording(true);
    recTimer.start(40);
    return path;
}

QString MainWindow::stopRecording() {
    inst.setRecording(false);
    recTimer.stop();
    drainRecording();
    lastRecording = wav.path();
    double secs = wav.frames() / (double)audio.sampleRate();
    wav.close();
    toast(QString("Saved %1 (%2 s)").arg(QFileInfo(lastRecording).fileName()).arg(secs, 0, 'f', 1), 2600);
    return lastRecording;
}

void MainWindow::toggleRecord() {
    if (wav.isOpen()) stopRecording();
    else if (!startRecording().isEmpty()) toast("Recording audio…", 1200);
}

void MainWindow::drainRecording() {
    static float buf[4096 * 2];
    int n;
    while ((n = inst.readRecorded(buf, 4096)) > 0) wav.write(buf, n);
}

bool MainWindow::saveSession(const QString &path) {
    SessionData sd;
    inst.snapshot(sd);
    bool ok = store::writeJson(path, store::sessionToJson(sd));
    refreshLists();
    return ok;
}

bool MainWindow::loadSession(const QString &path) {
    QJsonObject o;
    SessionData sd;
    if (!store::readJson(path, o) || !store::sessionFromJson(o, sd)) return false;
    releaseAllKeys();
    inst.restore(sd);
    lead = sd.lead; curSlot = -1;
    return true;
}

void MainWindow::startListen() {
    if (listenState == 1) return;
    if (listenThread.joinable()) listenThread.join();
    listenState = 1;
    listenStarted = clock.elapsed();
    listenGuess = 0;
    int rate = audio.sampleRate();
    listenThread = std::thread([this, rate] {
        std::vector<float> buf;
        std::string err;
        int n = captureMono(buf, rate, 6.0, err);
        ListenResult r;
        if (n > 0) r = analyseAudio(buf.data(), n, (float)rate);
        std::lock_guard<std::mutex> lk(listenMx);
        if (n <= 0) { listenError = "No microphone or audio input delivered sound (" + QString::fromStdString(err) + ")"; listenState = 3; }
        else if (!r.ok) { listenError = QString("Heard something, but it was %1").arg(r.problem); listenState = 3; }
        else { listenResult = r; listenState = 2; }
    });
}

void MainWindow::applyListen(bool key, bool tempo) {
    std::lock_guard<std::mutex> lk(listenMx);
    if (listenState != 2) return;
    const KeyGuess &k = listenResult.keys[listenGuess % 3];
    if (key) {
        set(S_ROOT, (float)k.root);
        // stay pentatonic if the player was on a pentatonic; otherwise full major/minor
        bool pent = u.set.tuning.scale == SC_MAJPENT || u.set.tuning.scale == SC_MINPENT;
        set(S_SCALE, (float)(k.minor ? (pent ? SC_MINPENT : SC_MINOR) : (pent ? SC_MAJPENT : SC_MAJOR)));
    }
    if (tempo && listenResult.bpm > 0) set(S_BPM, listenResult.bpm);
}

void MainWindow::updateBend() {
    Event e; e.type = EV_BEND; e.f = (bendUp ? 1.0f : 0.0f) - (bendDown ? 1.0f : 0.0f);
    inst.push(e);
}

void MainWindow::releaseAllKeys() {
    for (auto it = held.begin(); it != held.end(); ++it)
        if (it.value().kind == HK_STRING) inst.key(it.value().a, it.value().b, false);
    held.clear();
    for (int s = 0; s < kStrings; ++s)
        if (touchCol[s] >= 0) { inst.key(s, touchCol[s] | 0x100, false); touchCol[s] = -1; }
    modChromatic = modChord = modCommand = false;
    if (bendUp || bendDown) { bendUp = bendDown = false; updateBend(); }
    if (susKey) { susKey = false; inst.cmd(EV_SUSTAIN, susLatch ? 1 : 0); }
}

void MainWindow::perform(int act, bool shift) {
    switch (act) {
    case A_PANIC:
        inst.cmd(EV_PANIC);
        held.clear(); susLatch = false;
        toast("All notes off", 900);
        break;
    case A_LOOP: inst.cmd(EV_LOOP, LC_PEDAL); break;
    case A_OCT_DOWN: inst.cmd(EV_OCTAVE, -1); break;
    case A_OCT_UP: inst.cmd(EV_OCTAVE, 1); break;
    case A_CHORD_ROW:
        chordRow = !chordRow;
        toast(chordRow ? "Bottom row plays chords" : "Bottom row is a string again");
        break;
    case A_PAGE_NEXT: pg = (pg + 1) % PG_COUNT; deepEdit = false; break;
    case A_HOME: pg = PG_PLAY; keyTest = false; deepEdit = false; break;
    case A_KEY_UP: set(S_ROOT, (float)((u.set.tuning.root + (shift ? 11 : 1)) % 12)); break;
    case A_SCALE_NEXT: set(S_SCALE, (float)((u.set.tuning.scale + (shift ? SC_COUNT - 1 : 1)) % SC_COUNT)); break;
    case A_RANDOM: randomise(); break;
    case A_MUTATE: mutate(); break;
    case A_UNDO_SOUND: undoSound(); break;
    case A_FAVOURITE: saveFavourite(); break;
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
    case A_RECORD: toggleRecord(); break;
    case A_HELP: pg = pg == PG_HELP ? PG_PLAY : PG_HELP; break;
    case A_XYMAP: set(S_XYMAP, (float)((u.set.xyMap + 1) % XY_COUNT)); break;
    case A_QUIT: close(); break;
    default:
        if (act >= A_SOUND_1 && act <= A_SOUND_10) selectSlot(act - A_SOUND_1);
        break;
    }
}

// ------------------------------------------------------------------ keys

void MainWindow::handleKey(int key, bool down) {
    update();
    // bookkeeping for the key-test view
    if (down) {
        keyLog.prepend(QString("DOWN  %1").arg(keyName(key)));
    } else {
        keyLog.prepend(QString("up    %1").arg(keyName(key)));
    }
    while (keyLog.size() > 12) keyLog.removeLast();

    if (!down) {
        auto it = held.find(key);
        if (it == held.end()) return;
        Held h = it.value();
        held.erase(it);
        if (h.kind == HK_STRING) inst.key(h.a, h.b, false);
        else if (h.kind == HK_ACT) {
            switch (h.a) {
            case A_SUSTAIN: susKey = false; inst.cmd(EV_SUSTAIN, susLatch ? 1 : 0); break;
            case A_CHROMATIC: modChromatic = false; break;
            case A_CHORD_LAYER: modChord = false; break;
            case A_COMMAND: modCommand = false; break;
            case A_BEND_UP: bendUp = false; updateBend(); break;
            case A_BEND_DOWN: bendDown = false; updateBend(); break;
            }
        }
        return;
    }
    if (held.contains(key)) return; // already down (missed release or repeat)
    if ((int)held.size() + 1 > maxHeld) maxHeld = held.size() + 1;

    int plainAct = km.plain.value(key, A_NONE);
    // modifiers first, so they work in any layer
    if (plainAct == A_CHROMATIC) { modChromatic = true; held[key] = {HK_ACT, A_CHROMATIC, 0}; return; }
    if (plainAct == A_CHORD_LAYER) { modChord = true; held[key] = {HK_ACT, A_CHORD_LAYER, 0}; return; }
    if (plainAct == A_COMMAND) { modCommand = true; held[key] = {HK_ACT, A_COMMAND, 0}; return; }

    if (modCommand) {
        perform(km.command.value(key, A_NONE), modChromatic);
        held[key] = {HK_NONE, 0, 0};
        return;
    }
    int row, col;
    if (km.stringKey(key, row, col)) {
        if (row == 0 && (chordRow || modChord)) {
            if (col < 7) inst.cmd(EV_CHORD, col);
            held[key] = {HK_NONE, 0, 0};
            return;
        }
        inst.key(row, col, true, modChromatic);
        held[key] = {HK_STRING, row, col};
        return;
    }
    held[key] = {HK_ACT, plainAct, 0};
    switch (plainAct) {
    case A_SUSTAIN: susKey = true; inst.cmd(EV_SUSTAIN, 1); break;
    case A_BEND_UP: bendUp = true; updateBend(); break;
    case A_BEND_DOWN: bendDown = true; updateBend(); break;
    default: perform(plainAct, modChromatic); break;
    }
}

void MainWindow::keyPressEvent(QKeyEvent *e) {
    if (e->isAutoRepeat()) return; // a held key is one note, not many
    handleKey(e->key(), true);
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
    if (phase == PH_PRESS) {
        for (int i = (int)zones.size() - 1; i >= 0; --i)
            if (zones[i].r.contains(c)) {
                captured[id] = zones[i];
                downNames.insert(zones[i].name);
                zones[i].fn(c, PH_PRESS);
                break;
            }
    } else {
        auto it = captured.find(id);
        if (it == captured.end()) return;
        Zone z = it.value();
        if (phase == PH_RELEASE) { captured.erase(it); downNames.remove(z.name); }
        z.fn(c, phase);
    }
    update();
}

void MainWindow::mousePressEvent(QMouseEvent *e) { pointer(-1, toCanvas(e->position()), PH_PRESS); }
void MainWindow::mouseMoveEvent(QMouseEvent *e) { if (e->buttons()) pointer(-1, toCanvas(e->position()), PH_MOVE); }
void MainWindow::mouseReleaseEvent(QMouseEvent *e) { pointer(-1, toCanvas(e->position()), PH_RELEASE); }

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

QRectF MainWindow::zoneRect(const QString &name) const {
    for (const Zone &z : zones) if (z.name == name) return z.r;
    return QRectF();
}

QImage MainWindow::shot() {
    QImage img(720, 720, QImage::Format_RGB32); // the instrument's native canvas, whatever the window size
    QPainter p(&img);
    drawAll(p, 720, 720);
    return img;
}

// ------------------------------------------------------- drawing helpers

QFont MainWindow::font(const QFont &base, int px, int weight, qreal spacing) const {
    // building a QFont is surprisingly costly; every label reuses a cached one
    int baseId = &base == &fDisplay ? 0 : (&base == &fLabel ? 1 : 2);
    quint32 key = (quint32)baseId << 28 | (quint32)px << 18 | (quint32)weight << 6 | (quint32)(spacing * 10 + 0.5);
    auto it = fontCache.constFind(key);
    if (it != fontCache.constEnd()) return it.value();
    QFont f(base);
    f.setPixelSize(px);
    f.setWeight((QFont::Weight)weight);
    if (spacing != 0) f.setLetterSpacing(QFont::AbsoluteSpacing, spacing);
    fontCache.insert(key, f);
    return f;
}

void MainWindow::txt(QRectF r, const QString &s, const QFont &f, QColor c, int align) {
    g->setFont(f);
    g->setPen(c);
    g->drawText(r, align | Qt::TextSingleLine, s);
}

void MainWindow::panel(QRectF r, QColor fill, QColor line, qreal radius) {
    g->setPen(line.isValid() ? QPen(line, 1.5) : Qt::NoPen);
    g->setBrush(fill);
    g->drawRoundedRect(r, radius, radius);
}

void MainWindow::zone(QRectF r, const QString &name, std::function<void(QPointF, int)> fn) {
    zones.push_back({r, name, std::move(fn)});
}

void MainWindow::button(QRectF r, const QString &name, const QString &label, bool on, std::function<void()> fn,
                        const QString &sub, QColor accent) {
    bool dn = downNames.contains(name);
    QColor ac = accent.isValid() ? accent : AMBER;
    panel(r, on ? ac : (dn ? PANEL3 : PANEL2), on ? ac : (dn ? ac : LINE), 8);
    QColor tc = on ? BG : TEXT;
    int px = r.height() >= 50 ? 17 : 15;
    if (sub.isEmpty()) {
        txt(r, label, font(fLabel, px, QFont::Medium, 1.0), tc);
    } else {
        txt(QRectF(r.x(), r.y() + r.height() * 0.12, r.width(), r.height() * 0.48), label, font(fLabel, px, QFont::Medium, 1.0), tc);
        txt(QRectF(r.x() + 2, r.y() + r.height() * 0.56, r.width() - 4, r.height() * 0.34), sub, font(fLabel, 12, QFont::Normal, 0.5), on ? BG : DIM);
    }
    zone(r, name, [fn](QPointF, int ph) { if (ph == PH_PRESS && fn) fn(); });
}

void MainWindow::slider(QRectF r, const QString &name, const QString &label, float norm, const QString &value,
                        std::function<void(float)> fn) {
    bool dn = downNames.contains(name);
    panel(r, PANEL2, dn ? AMBER : LINE, 8);
    norm = norm < 0 ? 0 : (norm > 1 ? 1 : norm);
    QRectF bar(r.x() + 10, r.bottom() - 16, r.width() - 20, 6);
    g->setPen(Qt::NoPen);
    g->setBrush(PANEL3);
    g->drawRoundedRect(bar, 3, 3);
    g->setBrush(AMBER);
    g->drawRoundedRect(QRectF(bar.x(), bar.y(), bar.width() * norm, bar.height()), 3, 3);
    g->setBrush(TEXT);
    g->drawEllipse(QPointF(bar.x() + bar.width() * norm, bar.center().y()), 6, 6);
    txt(QRectF(r.x() + 10, r.y() + 5, r.width() * 0.6, 20), label, font(fLabel, 14, QFont::Medium, 1.2), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    txt(QRectF(r.x() + r.width() * 0.4, r.y() + 5, r.width() * 0.6 - 10, 20), value, font(fMono, 14), TEXT, Qt::AlignRight | Qt::AlignVCenter);
    zone(r, name, [fn, bar](QPointF p, int ph) {
        if (ph == PH_RELEASE) return;
        fn((float)((p.x() - bar.x()) / bar.width()));
    });
}

void MainWindow::stepper(QRectF r, const QString &name, const QString &label, const QString &value,
                         std::function<void()> minus, std::function<void()> plus) {
    panel(r, PANEL2, LINE, 8);
    qreal bw = 56;
    QRectF lm(r.x(), r.y(), bw, r.height()), rp(r.right() - bw, r.y(), bw, r.height());
    bool dm = downNames.contains(name + "-"), dp = downNames.contains(name + "+");
    if (dm) panel(lm, PANEL3, AMBER, 8);
    if (dp) panel(rp, PANEL3, AMBER, 8);
    txt(lm, QString::fromUtf8("−"), font(fMono, 26), dm ? AMBER : TEXT);
    txt(rp, "+", font(fMono, 26), dp ? AMBER : TEXT);
    txt(QRectF(r.x() + bw, r.y() + 5, r.width() - 2 * bw, 18), label, font(fLabel, 13, QFont::Medium, 1.2), DIM);
    txt(QRectF(r.x() + bw, r.y() + 22, r.width() - 2 * bw, r.height() - 26), value, font(fMono, 20, QFont::Medium), TEXT);
    zone(lm, name + "-", [minus](QPointF, int ph) { if (ph == PH_PRESS) minus(); });
    zone(rp, name + "+", [plus](QPointF, int ph) { if (ph == PH_PRESS) plus(); });
}

void MainWindow::paramSlider(QRectF r, int p) {
    slider(r, QString("param%1").arg(p), paramInfo(p).label, paramToNorm(p, lead.p[p]), paramText(p, lead.p[p]),
           [this, p](float n) { setParam(p, normToParam(p, n)); });
}

// ------------------------------------------------------------------ paint

extern qint64 tendrilMsSinceStart();

void MainWindow::paintEvent(QPaintEvent *) {
    static bool first = true;
    if (first) {
        first = false;
        if (qEnvironmentVariableIsSet("TENDRIL_TIMING")) fprintf(stderr, "tendril: first frame %lld ms after launch\n", (long long)tendrilMsSinceStart());
    }
    QPainter painter(this);
    drawAll(painter, width(), height());
}

void MainWindow::drawAll(QPainter &painter, qreal W, qreal H) {
    u = inst.uiState();
    if (sinkGeneration != seenSinkGeneration) { // output device changed: pick a sensible loop timing offset
        seenSinkGeneration = sinkGeneration;
        set(S_LATENCY, sinkIsBluetooth ? 200.0f : 0.0f);
    }
    g = &painter;
    painter.fillRect(QRectF(0, 0, W, H), BG);
    qreal s = qMin(W, H) / CW;
    painter.translate((W - CW * s) / 2, (H - CW * s) / 2);
    painter.scale(s, s);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    zones.clear();

    switch (pg) {
    case PG_PLAY: paintPlay(); break;
    case PG_SOUND: paintTabs(); paintSound(); break;
    case PG_BACK: paintTabs(); paintBacking(); break;
    case PG_TUNE: paintTabs(); paintTune(); break;
    case PG_SYS: paintTabs(); paintSystem(); break;
    case PG_HELP: paintTabs(); paintHelp(); break;
    }
    if (keyTest) paintKeyTest();

    if (clock.elapsed() < toastUntil && !toastText.isEmpty()) {
        QFont f = font(fLabel, 17, QFont::Medium, 0.8);
        int w = QFontMetrics(f).horizontalAdvance(toastText) + 44;
        if (w > 680) w = 680;
        QRectF r((CW - w) / 2, 84, w, 40);
        panel(r, TEXT, QColor(), 20);
        txt(r, toastText, f, BG);
    }
    g = nullptr;
}

void MainWindow::paintTabs() {
    static const char *N[PG_COUNT] = {"PLAY", "SOUND", "BACKING", "TUNE", "SYSTEM", "HELP"};
    qreal w = (688 - 5 * 6) / 6.0;
    for (int i = 0; i < PG_COUNT; ++i)
        button(QRectF(16 + i * (w + 6), 8, w, 50), QString("tab%1").arg(i), N[i], pg == i,
               [this, i] { pg = i; deepEdit = false; slotSave = false; });
}

void MainWindow::paintLoopButton(QRectF r) {
    bool dn = downNames.contains("loop");
    QString label = "LOOP", sub = "tap to record";
    QColor fill = dn ? PANEL3 : PANEL2, line = LINE, tc = TEXT;
    float prog = 0;
    bool blink = (clock.elapsed() / 300) % 2 == 0;
    switch (u.loopState) {
    case L_COUNTIN: label = QString::number(qMax(1, u.countBeat)); sub = "count-in"; line = AMBER; tc = AMBER; break;
    case L_REC: label = "REC"; sub = QString("bar %1").arg((int)u.loopPos + 1); fill = RED; line = RED; tc = BG; break;
    case L_PLAY:
        if (u.loopClosing) { label = "REC"; sub = "closing bar…"; fill = RED; line = RED; tc = BG; }
        else { label = QString("LOOP %1").arg(u.layers > 1 ? QString::fromUtf8("×%1").arg(u.layers) : QString()); sub = QString("%1 bar%2 · tap = overdub").arg(u.loopBars).arg(u.loopBars == 1 ? "" : "s"); line = AMBER; tc = AMBER; }
        prog = u.loopPos; break;
    case L_OVERDUB: label = "OVERDUB"; sub = u.loopFull ? "memory full" : "tap to finish"; fill = blink ? RED : QColor(0xb0, 0x30, 0x25); line = RED; tc = BG; prog = u.loopPos; break;
    case L_STOPPED: label = "LOOP"; sub = "stopped · tap = play"; tc = DIM; break;
    default: break;
    }
    panel(r, fill, line, 10);
    txt(QRectF(r.x(), r.y() + 8, r.width(), 34), label, font(fDisplay, 24, QFont::DemiBold, 1), tc);
    txt(QRectF(r.x(), r.y() + 44, r.width(), 18), sub, font(fLabel, 12, QFont::Normal, 0.5), tc == BG ? BG : DIM);
    if (prog > 0) {
        g->setPen(Qt::NoPen);
        g->setBrush(tc == BG ? BG : AMBER);
        g->drawRoundedRect(QRectF(r.x() + 8, r.bottom() - 10, (r.width() - 16) * prog, 4), 2, 2);
    }
    zone(r, "loop", [this](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_LOOP, LC_PEDAL); });
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
            panel(r, cur ? AMBER : PANEL, sel ? TEAL : (has ? LINE : PANEL2), 5);
            if (has) txt(r, buildChord(t, u.prog[i], u.set.seventh).name, font(fMono, 13, QFont::Medium), cur ? BG : TEXT);
            if (has) zone(r.adjusted(0, -6, 0, 6), QString("pslot%1").arg(i), [this, i](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_BACK, BC_SELECT_SLOT, i); });
        }
        QString info;
        if (u.backPlaying && (u.progLen || u.holdChord >= 0 || u.droneMask)) info = QString::fromUtf8("♪ %1").arg(u.backName);
        else if (u.progLen) info = "stopped";
        else info = chordRow ? "Z‥M = chords" : "tap chords";
        txt(QRectF(area.x() + 8 * (sw + 4), y, area.right() - area.x() - 8 * (sw + 4), 22), info, font(fLabel, 13, QFont::Normal, 0.5), DIM, Qt::AlignRight | Qt::AlignVCenter);
        y += 28;
    }
    qreal h = area.bottom() - y;
    qreal ctl = 92, pw = (area.width() - ctl - 7 * 6) / 7.0;
    for (int d = 0; d < 7; ++d) {
        QRectF r(area.x() + d * (pw + 6), y, pw, h);
        QString name = QString("chord%1").arg(d);
        bool cur = u.curChord == d, dn = downNames.contains(name);
        bool drone = (u.droneMask >> d) & 1;
        QColor fill = cur ? AMBER : (drone ? QColor(0x1d, 0x4a, 0x42) : (dn ? PANEL3 : PANEL2));
        panel(r, fill, cur ? AMBER : (drone ? TEAL : LINE), 8);
        Chord c = buildChord(t, d, u.set.seventh);
        txt(QRectF(r.x(), r.y() + h * 0.10, pw, h * 0.52), c.name, font(fDisplay, c.name[3] ? 17 : 22, QFont::DemiBold), cur ? BG : TEXT);
        bool small = std::strchr(c.name + 1, 'm') && !std::strstr(c.name, "maj");
        txt(QRectF(r.x(), r.y() + h * 0.62, pw, h * 0.30), romanNumeral(d, small), font(fMono, 12), cur ? BG : DIM);
        zone(r, name, [this, d](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_CHORD, d); });
    }
    QRectF c1(area.right() - ctl, y, ctl, h / 2 - 3), c2(area.right() - ctl, y + h / 2 + 3, ctl, h / 2 - 3);
    static const char *MN[BM_COUNT] = {"PROG", "HOLD", "DRONE"};
    button(c1, "backmode", MN[u.set.backMode], false, [this] { perform(A_BACK_MODE); });
    button(c2, "backplay", u.backPlaying ? QString::fromUtf8("■ STOP") : QString::fromUtf8("▶ PLAY"), u.backPlaying,
           [this] { inst.cmd(EV_BACK, BC_PLAYSTOP); }, QString(), TEAL);
}

void MainWindow::paintPlay() {
    const Tuning &t = u.set.tuning;
    // ---------------- header ----------------
    txt(QRectF(18, 6, 430, 46), lead.name, font(fDisplay, 33, QFont::DemiBold), AMBER, Qt::AlignLeft | Qt::AlignVCenter);
    QString slotTxt = curSlot >= 0 ? QString("SOUND %1").arg(curSlot + 1) : QString("UNSAVED SOUND");
    QString sub = QString("%1   ·   %2 VOICES").arg(slotTxt).arg(u.voices);
    txt(QRectF(20, 52, 300, 20), sub, font(fLabel, 13, QFont::Normal, 1.6), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    if (wav.isOpen()) {
        bool blink = (clock.elapsed() / 400) % 2 == 0;
        txt(QRectF(250, 52, 190, 20), QString::fromUtf8("● REC %1 s").arg(wav.frames() / audio.sampleRate()), font(fLabel, 13, QFont::Medium, 1.2), blink ? RED : DIM, Qt::AlignLeft | Qt::AlignVCenter);
    } else if (sinkIsDummy) {
        txt(QRectF(250, 52, 200, 20), "NO AUDIO DEVICE", font(fLabel, 13, QFont::Medium, 1.2), RED, Qt::AlignLeft | Qt::AlignVCenter);
    }
    zone(QRectF(16, 4, 430, 72), "hdr_sound", [this](QPointF, int ph) { if (ph == PH_PRESS) pg = PG_SOUND; });

    txt(QRectF(452, 4, 60, 50), noteName(t.root), font(fDisplay, 36, QFont::DemiBold), TEXT, Qt::AlignLeft | Qt::AlignVCenter);
    txt(QRectF(452, 52, 130, 20), scaleDef(t.scale).shortName, font(fLabel, 13, QFont::Medium, 1.6), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    txt(QRectF(560, 8, 78, 42), QString::number((int)std::round(u.set.bpm)), font(fMono, 30, QFont::Medium), TEXT, Qt::AlignRight | Qt::AlignVCenter);
    txt(QRectF(560, 52, 78, 20), "BPM", font(fLabel, 13, QFont::Medium, 1.6), DIM, Qt::AlignRight | Qt::AlignVCenter);
    double frac = u.beat - std::floor(u.beat);
    bool running = u.backPlaying || u.loopState != L_EMPTY;
    g->setPen(Qt::NoPen);
    g->setBrush(running && frac < 0.25 ? AMBER : PANEL3);
    g->drawEllipse(QPointF(550, 30), 5, 5);
    txt(QRectF(644, 8, 60, 42), QString("%1%2").arg(t.octave > 0 ? "+" : "").arg(t.octave), font(fMono, 30, QFont::Medium), t.octave ? TEAL : TEXT, Qt::AlignRight | Qt::AlignVCenter);
    txt(QRectF(644, 52, 60, 20), "OCT", font(fLabel, 13, QFont::Medium, 1.6), DIM, Qt::AlignRight | Qt::AlignVCenter);
    zone(QRectF(450, 4, 254, 72), "hdr_tune", [this](QPointF, int ph) { if (ph == PH_PRESS) pg = PG_TUNE; });

    // level line
    g->setBrush(PANEL2);
    g->drawRect(QRectF(16, 80, 688, 3));
    g->setBrush(u.limiter < 0.85f ? RED : AMBER);
    g->drawRect(QRectF(16, 80, 688 * qMin(1.0f, u.peak), 3));

    // ---------------- XY pad + scope ----------------
    QRectF xy(16, 92, 688, 226);
    panel(xy, PANEL, downNames.contains("xy") ? TEAL : LINE, 12);
    g->setPen(QPen(PANEL3, 1));
    for (int i = 1; i < 8; ++i) g->drawLine(QPointF(xy.x() + xy.width() * i / 8, xy.y() + 8), QPointF(xy.x() + xy.width() * i / 8, xy.bottom() - 8));
    for (int i = 1; i < 4; ++i) g->drawLine(QPointF(xy.x() + 8, xy.y() + xy.height() * i / 4), QPointF(xy.right() - 8, xy.y() + xy.height() * i / 4));
    {
        static float sc[1400];
        inst.readScope(sc, 1400);
        int trig = 0;
        for (int i = 1; i < 700; ++i)
            if (sc[i - 1] < 0 && sc[i] >= 0) { trig = i; break; }
        QPainterPath path;
        const int N = 340;
        for (int i = 0; i < N; ++i) {
            qreal x = xy.x() + 4 + (xy.width() - 8) * i / (N - 1);
            qreal v = qBound(-1.0f, sc[trig + i * 2] * 1.6f, 1.0f);
            qreal y = xy.center().y() - v * (xy.height() / 2 - 12);
            if (i == 0) path.moveTo(x, y); else path.lineTo(x, y);
        }
        g->setBrush(Qt::NoBrush);
        g->setPen(QPen(AMBER, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        g->drawPath(path);
    }
    bool xyActive = u.xyTouch || u.set.xyHold;
    QPointF puck(xy.x() + xy.width() * u.xyX, xy.bottom() - xy.height() * u.xyY);
    if (xyActive) {
        g->setPen(QPen(QColor(TEAL.red(), TEAL.green(), TEAL.blue(), 110), 1));
        g->drawLine(QPointF(puck.x(), xy.y() + 6), QPointF(puck.x(), xy.bottom() - 6));
        g->drawLine(QPointF(xy.x() + 6, puck.y()), QPointF(xy.right() - 6, puck.y()));
        g->setPen(QPen(TEAL, 2.5));
        g->setBrush(QColor(TEAL.red(), TEAL.green(), TEAL.blue(), u.xyTouch ? 120 : 40));
        g->drawEllipse(puck, 20, 20);
    }
    txt(QRectF(xy.x() + 14, xy.bottom() - 26, 300, 20), QString::fromUtf8("X → %1").arg(xyAxisName(u.set.xyMap, false)), font(fLabel, 13, QFont::Medium, 1.5), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    txt(QRectF(xy.x() + 14, xy.y() + 8, 300, 20), QString::fromUtf8("Y ↑ %1").arg(xyAxisName(u.set.xyMap, true)), font(fLabel, 13, QFont::Medium, 1.5), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    zone(xy, "xy", [this, xy](QPointF p, int ph) {
        Event e; e.type = EV_XY; e.a = ph != PH_RELEASE;
        e.f = (float)qBound(0.0, (p.x() - xy.x()) / xy.width(), 1.0);
        e.g = (float)qBound(0.0, (xy.bottom() - p.y()) / xy.height(), 1.0);
        inst.push(e);
    });
    button(QRectF(xy.right() - 196, xy.y() + 8, 118, 40), "xymap", xyMapName(u.set.xyMap), false, [this] { perform(A_XYMAP); });
    button(QRectF(xy.right() - 72, xy.y() + 8, 64, 40), "xyhold", "HOLD", u.set.xyHold, [this] { set(S_XYHOLD, u.set.xyHold ? 0 : 1); }, QString(), TEAL);

    // ---------------- strings ----------------
    QRectF sp(16, 328, 688, 176);
    panel(sp, PANEL, LINE, 12);
    const qreal x0 = sp.x() + 40, dx = (sp.width() - 80) / 9.0;
    qint64 now = clock.elapsed();
    for (int row = 2; row >= 0; --row) {
        qreal y = sp.y() + 34 + (2 - row) * 54;
        int count = km.rows[row].size();
        int shift = row == 0 ? 1 : 0; // the Q20's bottom letter row starts one key in
        auto X = [&](qreal col) { return x0 + (col + shift) * dx; };
        const StringUi &su = u.str[row];
        // string line (wiggles while sounding)
        QPainterPath line;
        qreal amp = su.gate ? 3.0 : 0.0;
        for (int i = 0; i <= 60; ++i) {
            qreal x = sp.x() + 12 + (sp.width() - 24) * i / 60.0;
            qreal yy = y + amp * std::sin(i * 0.9 + now * 0.03) * std::sin(3.14159 * i / 60.0);
            if (i == 0) line.moveTo(x, yy); else line.lineTo(x, yy);
        }
        g->setBrush(Qt::NoBrush);
        g->setPen(QPen(su.gate ? AMBER : LINE, su.gate ? 2 : 1.5));
        g->drawPath(line);
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
        for (int c = 0; c < count; ++c) {
            int n = t.noteFor(row, c, false);
            bool isHeldTop = su.held > 0 && su.topCol == c;
            bool isRoot = ((n - t.root) % 12 + 12) % 12 == 0;
            QPointF ctr(X(c), y);
            g->setPen(QPen(isHeldTop ? AMBER : (isRoot ? DIM : LINE), 1.5));
            g->setBrush(isHeldTop ? AMBER : PANEL2);
            g->drawEllipse(ctr, 16, 16);
            QString nn = QString("%1%2").arg(noteName(n)).arg(n / 12 - 1);
            txt(QRectF(ctr.x() - 18, y - 9, 36, 18), nn, font(fMono, nn.size() > 2 ? 10 : 12, QFont::Medium), isHeldTop ? BG : (isRoot ? TEXT : DIM));
            txt(QRectF(ctr.x() - 14, y - 34, 28, 14), QString(km.rows[row][c]), font(fLabel, 11, QFont::Normal, 0.5), row == 0 && (chordRow || modChord) ? TEAL : QColor(0x6a, 0x62, 0x56));
        }
        // gliding pitch marker + fading trail
        std::deque<float> &tr = trail[row];
        if (su.gate) {
            float px = (float)pitchX(su.note + u.bend);
            tr.push_back(px);
            if (tr.size() > 10) tr.pop_front();
            bool sliding = std::fabs(su.note - su.target) > 0.02f;
            g->setPen(Qt::NoPen);
            for (size_t i = 0; i + 1 < tr.size(); ++i) {
                if (std::fabs(tr[i] - px) < 1.0f) continue;
                g->setBrush(QColor(TEAL.red(), TEAL.green(), TEAL.blue(), (int)(18 + 12 * i)));
                g->drawEllipse(QPointF(tr[i], y), 5 + i * 0.8, 5 + i * 0.8);
            }
            g->setPen(QPen(TEAL, sliding ? 3.5 : 2.5));
            g->setBrush(Qt::NoBrush);
            g->drawEllipse(QPointF(px, y), 21, 21);
        } else if (!tr.empty()) tr.clear();
        // touch: press plays, dragging along the string slides
        QRectF zr(sp.x(), y - 27, sp.width(), 54);
        zone(zr, QString("string%1").arg(row), [this, row, count, x0, dx, shift](QPointF p, int ph) {
            int col = (int)std::floor((p.x() - x0) / dx - shift + 0.5);
            col = qBound(0, col, count - 1);
            int &cur = touchCol[row];
            if (ph == PH_RELEASE) { if (cur >= 0) inst.key(row, cur | 0x100, false); cur = -1; return; }
            if (col == cur) return;
            inst.key(row, col | 0x100, true, modChromatic);
            if (cur >= 0) inst.key(row, cur | 0x100, false);
            cur = col;
        });
    }

    // ---------------- chords ----------------
    paintChordPads(QRectF(16, 512, 688, 106), true);

    // ---------------- bottom bar ----------------
    qreal x = 16, y = 628, h = 80;
    paintLoopButton(QRectF(x, y, 150, h)); x += 158;
    button(QRectF(x, y, 66, h), "loopundo", "UNDO", false, [this] { inst.cmd(EV_LOOP, LC_UNDO); }, "layer"); x += 74;
    bool lp = u.loopState == L_PLAY || u.loopState == L_OVERDUB;
    button(QRectF(x, y, 66, h), "loopstop", lp ? QString::fromUtf8("■") : QString::fromUtf8("▶"), false, [this] { inst.cmd(EV_LOOP, LC_PLAYSTOP); }, "loop"); x += 74;
    button(QRectF(x, y, 66, h), "sus", "SUS", u.sustain, [this] { susLatch = !susLatch; inst.cmd(EV_SUSTAIN, susLatch || susKey ? 1 : 0); }, "sustain", TEAL); x += 74;
    button(QRectF(x, y, 54, h), "octdn", QString::fromUtf8("−"), false, [this] { inst.cmd(EV_OCTAVE, -1); }, "oct"); x += 62;
    button(QRectF(x, y, 54, h), "octup", "+", false, [this] { inst.cmd(EV_OCTAVE, 1); }, "oct"); x += 62;
    button(QRectF(x, y, 86, h), "dice", "NEW", false, [this] { randomise(); }, "sound"); x += 94;
    button(QRectF(x, y, 704 - x, h), "menu", "MENU", false, [this] { pg = PG_SOUND; }, QString::fromUtf8("≡"));
}

void MainWindow::paintSound() {
    qreal w = (688 - 4 * 8) / 5.0;
    for (int i = 0; i < 10; ++i) {
        QRectF r(16 + (i % 5) * (w + 8), 70 + (i / 5) * 56, w, 50);
        QString name = QString("slot%1").arg(i);
        bool on = curSlot == i && !slotSave;
        panel(r, on ? AMBER : (downNames.contains(name) ? PANEL3 : PANEL2), slotSave ? TEAL : (on ? AMBER : LINE), 8);
        txt(QRectF(r.x() + 8, r.y(), 22, r.height()), QString::number((i + 1) % 10), font(fMono, 15, QFont::Medium), on ? BG : DIM, Qt::AlignLeft | Qt::AlignVCenter);
        QFont nf = font(fLabel, 15, QFont::Medium, 0.3);
        txt(QRectF(r.x() + 26, r.y(), r.width() - 30, r.height()), QFontMetrics(nf).elidedText(bank[i].name, Qt::ElideRight, (int)r.width() - 32), nf, on ? BG : TEXT, Qt::AlignLeft | Qt::AlignVCenter);
        zone(r, name, [this, i](QPointF, int ph) { if (ph == PH_PRESS) selectSlot(i); });
    }
    qreal y = 186;
    button(QRectF(16, y, w, 54), "newsound", "NEW SOUND", false, [this] { randomise(); }, "randomise");
    button(QRectF(16 + (w + 8), y, w, 54), "mutate", "MUTATE", false, [this] { mutate(); }, "nudge it");
    button(QRectF(16 + 2 * (w + 8), y, w, 54), "undosound", "UNDO", false, [this] { undoSound(); }, QString("%1 step%2").arg(undo.size()).arg(undo.size() == 1 ? "" : "s"));
    button(QRectF(16 + 3 * (w + 8), y, w, 54), "slotsave", "STORE", slotSave, [this] { slotSave = !slotSave; }, slotSave ? "pick a slot" : "to a slot 1-0", TEAL);
    button(QRectF(16 + 4 * (w + 8), y, w, 54), "fav", QString::fromUtf8("♥ SAVE"), false, [this] { saveFavourite(); }, "favourite");

    txt(QRectF(18, 248, 440, 40), lead.name, font(fDisplay, 26, QFont::DemiBold), AMBER, Qt::AlignLeft | Qt::AlignVCenter);
    button(QRectF(704 - 220, 250, 220, 38), "deep", deepEdit ? QString::fromUtf8("‹ QUICK CONTROLS") : QString::fromUtf8("ALL PARAMETERS ›"), deepEdit, [this] { deepEdit = !deepEdit; });

    if (deepEdit) {
        const int per = 12, pages = (P_COUNT + per - 1) / per;
        deepPage = qBound(0, deepPage, pages - 1);
        for (int k = 0; k < per; ++k) {
            int p = deepPage * per + k;
            if (p >= P_COUNT) break;
            paramSlider(QRectF(16 + (k % 2) * 348, 296 + (k / 2) * 58, 340, 52), p);
        }
        button(QRectF(16, 650, 160, 54), "deepprev", QString::fromUtf8("‹ PREV"), false, [this, pages] { deepPage = (deepPage + pages - 1) % pages; });
        txt(QRectF(176, 650, 368, 54), QString("PAGE %1 / %2").arg(deepPage + 1).arg(pages), font(fLabel, 15, QFont::Medium, 1.5), DIM);
        button(QRectF(544, 650, 160, 54), "deepnext", QString::fromUtf8("NEXT ›"), false, [this, pages] { deepPage = (deepPage + 1) % pages; });
        return;
    }
    static const int macro[8] = {P_CUTOFF, P_RESO, P_ATK, P_REL, P_GLIDE, P_DRIVE, P_DLYMIX, P_REVMIX};
    for (int k = 0; k < 8; ++k) paramSlider(QRectF(16 + (k % 2) * 348, 296 + (k / 2) * 58, 340, 52), macro[k]);

    // favourites
    const int per = 6, pages = qMax(1, ((int)favs.size() + per - 1) / per);
    favPage = qBound(0, favPage, pages - 1);
    txt(QRectF(18, 532, 400, 24), QString("FAVOURITES  ·  %1").arg(favs.size()), font(fLabel, 13, QFont::Medium, 1.6), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    qreal cw = (688 - 110 - 3 * 8) / 3.0;
    for (int k = 0; k < per; ++k) {
        int idx = favPage * per + k;
        QRectF r(16 + (k % 3) * (cw + 8), 560 + (k / 3) * 74, cw, 66);
        if (idx >= favs.size()) { panel(r, PANEL, PANEL2, 8); continue; }
        button(r, QString("favitem%1").arg(k), favs[idx].patch.name, false, [this, idx] {
            if (idx >= favs.size()) return;
            if (favDelete) { QFile::remove(favs[idx].file); refreshLists(); favDelete = false; toast("Favourite removed"); return; }
            pushUndo(); lead = favs[idx].patch; applyLead(true);
        }, favDelete ? "tap to delete" : "recall", favDelete ? RED : QColor());
    }
    qreal bx = 704 - 102;
    button(QRectF(bx, 560, 102, 30), "favprev", QString::fromUtf8("‹  %1/%2  ›").arg(favPage + 1).arg(pages), false, [this, pages] { favPage = (favPage + 1) % pages; });
    button(QRectF(bx, 598, 102, 102), "favdel", "DELETE", favDelete, [this] { favDelete = !favDelete; }, favDelete ? "pick one" : "a favourite", RED);
}

void MainWindow::paintBacking() {
    txt(QRectF(18, 64, 500, 22), "A CHORD TAP…", font(fLabel, 13, QFont::Medium, 1.6), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    static const char *MS[BM_COUNT] = {"adds to the loop", "latches one chord", "latches a drone note"};
    qreal w3 = (688 - 2 * 8) / 3.0;
    for (int i = 0; i < BM_COUNT; ++i)
        button(QRectF(16 + i * (w3 + 8), 88, w3, 54), QString("bmode%1").arg(i), backModeName(i), u.set.backMode == i, [this, i] { set(S_BACKMODE, (float)i); }, MS[i]);
    qreal w5 = (688 - 4 * 6) / 5.0;
    for (int i = 0; i < BS_COUNT; ++i)
        button(QRectF(16 + i * (w5 + 6), 150, w5, 46), QString("bstyle%1").arg(i), backStyleName(i), u.set.backStyle == i, [this, i] { set(S_BACKSTYLE, (float)i); });

    // progression slots
    txt(QRectF(18, 202, 500, 22), "PROGRESSION  ·  tap a slot, then a chord, to replace it", font(fLabel, 13, QFont::Medium, 1.2), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    qreal sw = (688 - 7 * 6) / 8.0;
    for (int i = 0; i < kProgMax; ++i) {
        QRectF r(16 + i * (sw + 6), 226, sw, 58);
        bool has = i < u.progLen, cur = has && i == u.progPos && u.backPlaying, sel = i == u.selSlot;
        panel(r, cur ? AMBER : (has ? PANEL2 : PANEL), sel ? TEAL : (has ? LINE : PANEL2), 8);
        txt(QRectF(r.x(), r.y() + 3, sw, 16), QString::number(i + 1), font(fMono, 11), cur ? BG : DIM);
        if (has) txt(QRectF(r.x(), r.y() + 16, sw, 38), buildChord(u.set.tuning, u.prog[i], u.set.seventh).name, font(fDisplay, 19, QFont::DemiBold), cur ? BG : TEXT);
        if (has) zone(r, QString("bslot%1").arg(i), [this, i](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_BACK, BC_SELECT_SLOT, i); });
    }
    qreal w4 = (688 - 3 * 8) / 4.0;
    button(QRectF(16, 292, w4, 52), "bremove", "REMOVE LAST", false, [this] { inst.cmd(EV_BACK, BC_REMOVE_LAST); });
    button(QRectF(16 + (w4 + 8), 292, w4, 52), "bclear", "CLEAR ALL", false, [this] { inst.cmd(EV_BACK, BC_CLEAR); });
    button(QRectF(16 + 2 * (w4 + 8), 292, w4, 52), "bseventh", "7TH CHORDS", u.set.seventh, [this] { set(S_SEVENTH, u.set.seventh ? 0 : 1); });
    button(QRectF(16 + 3 * (w4 + 8), 292, w4, 52), "bmetro", "METRONOME", u.set.metro, [this] { set(S_METRO, u.set.metro ? 0 : 1); });

    paintChordPads(QRectF(16, 356, 688, 84), false);

    stepper(QRectF(16, 452, 340, 60), "chordbeats", "BEATS PER CHORD", QString::number(u.set.chordBeats),
            [this] { set(S_CHORDBEATS, (float)(u.set.chordBeats - 1)); }, [this] { set(S_CHORDBEATS, (float)(u.set.chordBeats + 1)); });
    stepper(QRectF(364, 452, 340, 60), "arprate", "ARP / PULSE STEPS PER BEAT", QString::number(u.set.arpRate),
            [this] { set(S_ARPRATE, (float)(u.set.arpRate - 1)); }, [this] { set(S_ARPRATE, (float)(u.set.arpRate + 1)); });
    stepper(QRectF(16, 520, 340, 60), "bpm2", "TEMPO", QString("%1 BPM").arg((int)std::round(u.set.bpm)),
            [this] { set(S_BPM, std::round(u.set.bpm) - 2); }, [this] { set(S_BPM, std::round(u.set.bpm) + 2); });
    slider(QRectF(364, 520, 340, 60), "backvol", "BACKING VOLUME", u.set.backVol, QString("%1%").arg((int)(u.set.backVol * 100)), [this](float n) { set(S_BACKVOL, n); });
    button(QRectF(16, 590, 688, 56), "bresync", "USE THE CURRENT LEAD SOUND FOR THE BACKING", false, [this] { inst.cmd(EV_BACK, BC_RESYNC_SOUND); },
           QString("backing now plays: %1   ·   lead: %2").arg(u.backName[0] ? u.backName : "(nothing yet)").arg(lead.name));
    txt(QRectF(18, 654, 684, 22), "The backing keeps the sound it started with, so you can change the lead freely.", font(fLabel, 14, QFont::Normal, 0.3), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    txt(QRectF(18, 678, 684, 22), QString("Keyboard: hold %1 + Z‥M for chords, or press $ to turn the bottom row into chord keys.").arg(km.keyFor(A_CHORD_LAYER, false)), font(fLabel, 14, QFont::Normal, 0.3), DIM, Qt::AlignLeft | Qt::AlignVCenter);
}

void MainWindow::paintTune() {
    const Tuning &t = u.set.tuning;
    txt(QRectF(18, 62, 300, 22), "KEY", font(fLabel, 13, QFont::Medium, 1.6), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    qreal w6 = (688 - 5 * 6) / 6.0;
    for (int i = 0; i < 12; ++i)
        button(QRectF(16 + (i % 6) * (w6 + 6), 86 + (i / 6) * 52, w6, 46), QString("root%1").arg(i), noteName(i), t.root == i, [this, i] { set(S_ROOT, (float)i); });
    txt(QRectF(18, 190, 300, 22), "SCALE", font(fLabel, 13, QFont::Medium, 1.6), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    qreal w4 = (688 - 3 * 6) / 4.0;
    for (int i = 0; i < SC_COUNT; ++i)
        button(QRectF(16 + (i % 4) * (w4 + 6), 214 + (i / 4) * 52, w4, 46), QString("scale%1").arg(i), QString(scaleDef(i).name).toUpper(), t.scale == i, [this, i] { set(S_SCALE, (float)i); });

    qreal w3 = (688 - 2 * 8) / 3.0;
    stepper(QRectF(16, 326, w3, 60), "oct", "OCTAVE", QString("%1%2").arg(t.octave > 0 ? "+" : "").arg(t.octave),
            [this] { inst.cmd(EV_OCTAVE, -1); }, [this] { inst.cmd(EV_OCTAVE, 1); });
    stepper(QRectF(16 + w3 + 8, 326, w3, 60), "rowint", "STRING GAP", QString("%1 st").arg(t.rowInterval),
            [this] { set(S_ROWINT, (float)(u.set.tuning.rowInterval - 1)); }, [this] { set(S_ROWINT, (float)(u.set.tuning.rowInterval + 1)); });
    stepper(QRectF(16 + 2 * (w3 + 8), 326, w3, 60), "bendr", "BEND RANGE", QString("%1 st").arg((int)u.set.bendRange),
            [this] { set(S_BENDRANGE, u.set.bendRange - 1); }, [this] { set(S_BENDRANGE, u.set.bendRange + 1); });

    stepper(QRectF(16, 394, w3, 60), "bpm", "TEMPO", QString("%1 BPM").arg((int)std::round(u.set.bpm)),
            [this] { set(S_BPM, std::round(u.set.bpm) - 1); }, [this] { set(S_BPM, std::round(u.set.bpm) + 1); });
    button(QRectF(16 + w3 + 8, 394, w3 / 2 - 4, 60), "tap", "TAP", false, [this] { tapTempo(); }, "tempo");
    button(QRectF(16 + w3 + 8 + w3 / 2 + 4, 394, w3 / 2 - 4, 60), "metro", "CLICK", u.set.metro, [this] { set(S_METRO, u.set.metro ? 0 : 1); }, "metronome");
    slider(QRectF(16 + 2 * (w3 + 8), 394, w3, 60), "glidescale", "GLIDE SPEED", u.set.glideScale / 4.0f,
           u.set.glideScale < 0.02f ? QString("instant") : QString::fromUtf8("×%1").arg(u.set.glideScale, 0, 'f', 2), [this](float n) { set(S_GLIDESCALE, n * 4.0f); });

    // listen
    QRectF lp(16, 466, 688, 238);
    panel(lp, PANEL, LINE, 12);
    txt(QRectF(32, 474, 400, 24), "LISTEN  ·  match the key and tempo of music around you", font(fLabel, 13, QFont::Medium, 1.2), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    int ls = listenState;
    button(QRectF(32, 506, 200, 60), "listen", ls == 1 ? "LISTENING…" : "LISTEN", ls == 1, [this] { startListen(); }, ls == 1 ? QString("%1 s").arg(qMax<qint64>(0, 6 - (clock.elapsed() - listenStarted) / 1000)) : "6 seconds, needs a mic");
    if (ls == 0) {
        txt(QRectF(248, 506, 440, 60), "Uses the system's default audio input, if there is one.", font(fLabel, 15, QFont::Normal, 0.3), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    } else if (ls == 3) {
        std::lock_guard<std::mutex> lk(listenMx);
        g->setFont(font(fLabel, 15, QFont::Normal, 0.3));
        g->setPen(RED);
        g->drawText(QRectF(248, 506, 440, 60), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, listenError);
    } else if (ls == 2) {
        std::lock_guard<std::mutex> lk(listenMx);
        const ListenResult &r = listenResult;
        const KeyGuess &k = r.keys[listenGuess % 3];
        float kc = listenGuess == 0 ? r.keyConfidence : r.keyConfidence * 0.5f;
        QString kq = kc > 0.6f ? "likely" : (kc > 0.3f ? "a guess" : "very unsure");
        txt(QRectF(248, 500, 250, 40), QString("%1 %2").arg(noteName(k.root)).arg(k.minor ? "minor" : "major"), font(fDisplay, 28, QFont::DemiBold), TEXT, Qt::AlignLeft | Qt::AlignVCenter);
        txt(QRectF(248, 540, 250, 22), QString("key: %1 (%2%)").arg(kq).arg((int)(kc * 100)), font(fLabel, 14, QFont::Normal, 0.5), kc > 0.6f ? TEAL : (kc > 0.3f ? AMBER : RED), Qt::AlignLeft | Qt::AlignVCenter);
        QString tq = r.bpmConfidence > 0.6f ? "likely" : (r.bpmConfidence > 0.3f ? "a guess" : "very unsure");
        txt(QRectF(500, 500, 190, 40), r.bpm > 0 ? QString("%1 BPM").arg((int)std::round(r.bpm)) : QString("no pulse"), font(fDisplay, 28, QFont::DemiBold), TEXT, Qt::AlignLeft | Qt::AlignVCenter);
        txt(QRectF(500, 540, 190, 22), QString("tempo: %1 (%2%)").arg(tq).arg((int)(r.bpmConfidence * 100)), font(fLabel, 14, QFont::Normal, 0.5), r.bpmConfidence > 0.6f ? TEAL : (r.bpmConfidence > 0.3f ? AMBER : RED), Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (ls == 2) {
        qreal bw = (656 - 3 * 8) / 4.0;
        button(QRectF(32, 580, bw, 56), "lkey", "USE KEY", false, [this] { applyListen(true, false); toast("Key applied"); });
        button(QRectF(32 + bw + 8, 580, bw, 56), "ltempo", "USE TEMPO", false, [this] { applyListen(false, true); toast("Tempo applied"); });
        button(QRectF(32 + 2 * (bw + 8), 580, bw, 56), "lboth", "USE BOTH", false, [this] { applyListen(true, true); toast("Key and tempo applied"); });
        button(QRectF(32 + 3 * (bw + 8), 580, bw, 56), "lnext", "OTHER KEY", false, [this] { listenGuess = (listenGuess + 1) % 3; }, QString("guess %1 of 3").arg(listenGuess % 3 + 1));
        txt(QRectF(32, 646, 656, 44), "Wrong? Pick the key and scale by hand above — detection is only an estimate.", font(fLabel, 14, QFont::Normal, 0.3), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    }
}

void MainWindow::paintSystem() {
    slider(QRectF(16, 70, 340, 58), "volume", "MASTER VOLUME", u.set.volume, QString("%1%").arg((int)(u.set.volume * 100)), [this](float n) { set(S_VOLUME, n); });
    slider(QRectF(364, 70, 340, 58), "loopvol", "LOOP VOLUME", u.set.loopVol, QString("%1%").arg((int)(u.set.loopVol * 100)), [this](float n) { set(S_LOOPVOL, n); });

    txt(QRectF(18, 132, 400, 22), "TOUCH PAD MAPPING", font(fLabel, 13, QFont::Medium, 1.6), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    qreal w6 = (688 - 5 * 6) / 6.0;
    for (int i = 0; i < XY_COUNT; ++i)
        button(QRectF(16 + i * (w6 + 6), 156, w6, 54), QString("xymap%1").arg(i), xyMapName(i), u.set.xyMap == i, [this, i] { set(S_XYMAP, (float)i); },
               QString("%1 / %2").arg(xyAxisName(i, false)).arg(xyAxisName(i, true)).toLower());
    button(QRectF(16 + 5 * (w6 + 6), 156, w6, 54), "xyhold2", "HOLD", u.set.xyHold, [this] { set(S_XYHOLD, u.set.xyHold ? 0 : 1); }, "keep position", TEAL);

    qreal w4 = (688 - 3 * 8) / 4.0;
    button(QRectF(16, 220, w4, 58), "chordrow", "CHORD ROW", chordRow, [this] { perform(A_CHORD_ROW); }, "bottom row = chords", TEAL);
    button(QRectF(16 + (w4 + 8), 220, w4, 58), "recwav", wav.isOpen() ? QString::fromUtf8("■ STOP REC") : QString::fromUtf8("● RECORD WAV"), wav.isOpen(), [this] { toggleRecord(); },
           wav.isOpen() ? QString("%1 s").arg(wav.frames() / audio.sampleRate()) : "export what you play", RED);
    button(QRectF(16 + 2 * (w4 + 8), 220, w4, 58), "savesession", "SAVE SESSION", false, [this] {
        QString path = store::baseDir() + "/sessions/" + store::timestampName("session", "json");
        toast(saveSession(path) ? QString("Saved %1").arg(QFileInfo(path).fileName()) : QString("Could not save session"), 2200);
    }, "sounds, backing, loops");
    button(QRectF(16 + 3 * (w4 + 8), 220, w4, 58), "keytest", "KEY TEST", keyTest, [this] { keyTest = !keyTest; maxHeld = 0; }, "check the keyboard");

    txt(QRectF(18, 284, 500, 22), QString("SAVED SESSIONS  ·  %1  ·  tap to load").arg(sessions.size()), font(fLabel, 13, QFont::Medium, 1.6), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    for (int k = 0; k < 4; ++k) {
        QRectF r(16 + (k % 2) * 348, 308 + (k / 2) * 54, 340, 48);
        if (k >= sessions.size()) { panel(r, PANEL, PANEL2, 8); continue; }
        QString path = sessions[k];
        button(r, QString("session%1").arg(k), QFileInfo(path).completeBaseName(), false, [this, path] {
            toast(loadSession(path) ? QString("Loaded %1 — press play on the loop / backing").arg(QFileInfo(path).completeBaseName()) : QString("Could not read that session"), 2600);
        });
    }

    QRectF ip(16, 424, 688, 206);
    panel(ip, PANEL, LINE, 12);
    QStringList L;
    QString sink = sinkDescription.isEmpty() ? QString("unknown") : sinkDescription;
    L << QString("AUDIO      %1 · %2 Hz · %3-frame blocks (%4 ms)").arg(QString::fromStdString(audio.backend())).arg(audio.sampleRate()).arg(audio.quantum()).arg(1000.0 * audio.quantum() / audio.sampleRate(), 0, 'f', 1);
    L << QString("OUTPUT     %1%2%3").arg(sink).arg(sinkIsBluetooth ? "  · Bluetooth: about 0.2 s behind your fingers" : "").arg(audio.streaming() ? "" : "  (idle)");
    L << QString("ENGINE     load %1%  ·  peak %2%  ·  %3 late blocks  ·  %4 voices").arg((int)(audio.load() * 100)).arg((int)(audio.peakLoad() * 100)).arg(audio.overruns()).arg(u.voices);
    L << QString("FILES      %1").arg(store::baseDir());
    L << QString("LAST WAV   %1").arg(lastRecording.isEmpty() ? QString("-") : QFileInfo(lastRecording).fileName());
    L << QString("KEYS       edit keymap.json in the folder above to remap");
    for (int i = 0; i < L.size(); ++i)
        txt(QRectF(32, 434 + i * 26, 656, 24), L[i], font(fMono, 14), i == 1 && sinkIsDummy ? RED : (i == 1 && sinkIsBluetooth ? AMBER : TEXT), Qt::AlignLeft | Qt::AlignVCenter);
    if (sinkIsDummy)
        txt(QRectF(32, 434 + 6 * 26, 656, 24), "No real audio device: connect headphones or a speaker to hear Tendril.", font(fLabel, 15, QFont::Medium, 0.3), RED, Qt::AlignLeft | Qt::AlignVCenter);

    button(QRectF(16, 642, w4, 60), "panic", "PANIC", false, [this] { perform(A_PANIC); }, "all notes off");
    stepper(QRectF(16 + (w4 + 8), 642, w4, 60), "latency", "LOOP TIMING FIX", QString("%1 ms").arg((int)u.set.latencyMs),
            [this] { set(S_LATENCY, u.set.latencyMs - 10); }, [this] { set(S_LATENCY, u.set.latencyMs + 10); });
    button(QRectF(16 + 2 * (w4 + 8), 642, w4, 60), "factory", "FACTORY SOUNDS", false, [this] {
        for (int k = 0; k < 10; ++k) bank[k] = factoryPatch(k);
        toast("Sound slots 1-0 restored");
    }, "restore slots 1-0");
    button(QRectF(16 + 3 * (w4 + 8), 642, w4, 60), "quit", "QUIT", false, [this] { close(); }, "saves your setup", RED);
}

void MainWindow::paintHelp() {
    struct Row { QString k, d; };
    auto K = [this](int act, bool cmd = false) { return (cmd ? km.keyFor(A_COMMAND, false) + " + " : QString()) + km.keyFor(act, cmd); };
    std::vector<Row> rows;
    if (helpPage == 0) {
        rows = {
            {km.rows[2].left(1) + QString::fromUtf8(" ‥ ") + km.rows[2].right(1), "high string — left to right goes up the scale"},
            {km.rows[1].left(1) + QString::fromUtf8(" ‥ ") + km.rows[1].right(1), "middle string"},
            {km.rows[0].left(1) + QString::fromUtf8(" ‥ ") + km.rows[0].right(1), "low string (or chords, see below)"},
            {"hold + press", "hold a key, press another on the same row: the note SLIDES"},
            {"let go", "release the newer key: it slides back to the held one"},
            {K(A_CHROMATIC) + " (hold)", "chromatic: keys step in semitones, any note"},
            {K(A_SUSTAIN) + " (hold)", "sustain — released notes keep ringing"},
            {K(A_PANIC), "panic: silence everything"},
            {K(A_LOOP), "loop pedal: count-in → record → play → overdub"},
            {"Left / Right", "pitch bend down / up (hold)"},
            {"Up / Down", "octave up / down — held notes sweep"},
            {"1 ‥ 9, 0", "sounds 1 to 10"},
            {K(A_CHORD_LAYER) + " + " + km.rows[0].left(1) + QString::fromUtf8("‥") + km.rows[0].right(1), "backing chord I ‥ VII"},
            {K(A_CHORD_ROW), "toggle: bottom row plays chords"},
            {"Tab  /  Esc", "next page  /  back to the play page"},
            {"screen", "drag the scope = XY sound control · drag a string = slide"},
        };
    } else {
        rows = {
            {K(A_RANDOM, true), "NEW SOUND — invent an instrument"},
            {K(A_MUTATE, true), "mutate the current sound a little"},
            {K(A_UNDO_SOUND, true), "undo: back to the previous sound"},
            {K(A_FAVOURITE, true), "save the sound as a favourite"},
            {K(A_KEY_UP, true), "key up a semitone (+ " + K(A_CHROMATIC) + ": down)"},
            {K(A_SCALE_NEXT, true), "next scale (+ " + K(A_CHROMATIC) + ": previous)"},
            {K(A_TAP, true) + "   " + K(A_METRO, true), "tap tempo   ·   metronome"},
            {K(A_BACK_MODE, true), "chord taps: progression / hold / drone"},
            {K(A_BACK_STYLE, true), "backing style: sustain / pulse / arpeggio"},
            {K(A_BACK_PLAY, true) + "   " + K(A_BACK_CLEAR, true), "backing start-stop   ·   clear backing"},
            {K(A_BACK_REMOVE, true), "remove the last chord of the progression"},
            {K(A_LOOP_UNDO, true), "undo the last loop layer"},
            {K(A_LOOP_PLAY, true) + "   " + K(A_LOOP_CLEAR, true), "loop start-stop   ·   clear the loop"},
            {K(A_XYMAP, true), "next touch-pad mapping"},
            {K(A_RECORD, true), "record what you hear to a WAV file"},
            {K(A_HELP, true) + "   " + K(A_QUIT, true), "this guide   ·   quit"},
        };
    }
    txt(QRectF(18, 64, 500, 30), helpPage == 0 ? "PLAYING" : QString("COMMANDS  ·  hold %1").arg(km.keyFor(A_COMMAND, false)), font(fDisplay, 20, QFont::DemiBold, 1), AMBER, Qt::AlignLeft | Qt::AlignVCenter);
    for (size_t i = 0; i < rows.size(); ++i) {
        qreal y = 100 + i * 33.5;
        QRectF kr(16, y, 190, 29);
        panel(kr, PANEL2, LINE, 6);
        txt(kr, rows[i].k, font(fMono, 14, QFont::Medium), TEXT);
        txt(QRectF(218, y, 486, 29), rows[i].d, font(fLabel, 16, QFont::Normal, 0.2), i % 2 ? DIM : TEXT, Qt::AlignLeft | Qt::AlignVCenter);
    }
    txt(QRectF(18, 640, 440, 22), "Q20: digits and arrows live on the keyboard's number / symbol layers.", font(fLabel, 13, QFont::Normal, 0.3), DIM, Qt::AlignLeft | Qt::AlignVCenter);
    button(QRectF(16, 664, 340, 44), "helppage", helpPage == 0 ? QString::fromUtf8("COMMAND KEYS ›") : QString::fromUtf8("‹ PLAYING KEYS"), false, [this] { helpPage = 1 - helpPage; });
    button(QRectF(364, 664, 340, 44), "helpplay", "BACK TO PLAYING", false, [this] { pg = PG_PLAY; });
}

void MainWindow::paintKeyTest() {
    QRectF r(40, 96, 640, 520);
    panel(r, QColor(0x0b, 0x0a, 0x09, 245), TEAL, 14);
    txt(QRectF(60, 108, 400, 30), "KEY TEST", font(fDisplay, 22, QFont::DemiBold, 1), TEAL, Qt::AlignLeft | Qt::AlignVCenter);
    QStringList names;
    for (auto it = held.begin(); it != held.end(); ++it) names << keyName(it.key());
    names.sort();
    txt(QRectF(60, 146, 600, 24), QString("HELD NOW (%1):  %2").arg(names.size()).arg(names.join("  ")), font(fMono, 16, QFont::Medium), TEXT, Qt::AlignLeft | Qt::AlignVCenter);
    txt(QRectF(60, 174, 600, 24), QString("MOST KEYS HELD AT ONCE: %1").arg(maxHeld), font(fMono, 16, QFont::Medium), AMBER, Qt::AlignLeft | Qt::AlignVCenter);
    for (int i = 0; i < keyLog.size(); ++i)
        txt(QRectF(60, 210 + i * 24, 600, 22), keyLog[i], font(fMono, 15), keyLog[i].startsWith("DOWN") ? TEXT : DIM, Qt::AlignLeft | Qt::AlignVCenter);
    g->setFont(font(fLabel, 15, QFont::Normal, 0.3));
    g->setPen(DIM);
    g->drawText(QRectF(60, 506, 600, 60), Qt::AlignLeft | Qt::TextWordWrap,
                "Hold one key on each row, then add more. Every key should show DOWN once and stay in HELD until you let go. "
                "A key you did not press appearing here, or a pressed key missing, is a keyboard-matrix limit.");
    button(QRectF(500, 104, 160, 44), "keytestclose", "CLOSE", false, [this] { keyTest = false; });
}
