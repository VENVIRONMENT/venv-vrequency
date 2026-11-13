#include "mainwindow.h"
#include "hw.h"
#include "shared.h"
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
#include <QWheelEvent>
#include <cmath>
#include <cstring>

using namespace td;
using namespace ui;

namespace ui {
QString sinkDescription; // refreshed asynchronously via wpctl
bool sinkIsDummy = false, sinkIsBluetooth = false;
int sinkGeneration = 0;
const float TB_MS[TB_COUNT] = {0.2f, 0.5f, 1, 2, 5, 10, 20};
const float VD_V[VD_COUNT] = {0.05f, 0.1f, 0.2f, 0.5f, 1.0f};
QString tbText(int i) {
    float v = TB_MS[qBound(0, i, TB_COUNT - 1)];
    return v < 1 ? QString("%1us/").arg((int)(v * 1000)) : QString("%1ms/").arg(v, 0, 'f', 2);
}
QString vdText(int i) {
    float v = VD_V[qBound(0, i, VD_COUNT - 1)];
    return v < 1 ? QString("%1mV/").arg((int)(v * 1000)) : QString("%1V/").arg(v, 0, 'f', 2);
}
QString nice(const char *label) {
    QString s = QString(label).toLower();
    if (!s.isEmpty()) s[0] = s[0].toUpper();
    for (int i = 1; i + 1 <= s.size(); ++i) // a lone letter is a name: "Wave A", "B mix"
        if (s[i - 1] == ' ' && (i + 1 == s.size() || s[i + 1] == ' ')) s[i] = s[i].toUpper();
    s.replace("Lfo", "LFO").replace("Fm", "FM").replace("Dly", "Dly");
    return s;
}
} // namespace ui

namespace {
enum { HK_STRING, HK_ACT, HK_NONE };
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

MainWindow::MainWindow(Instrument &i, AudioOut &a) : inst(i), audio(a) {
    setWindowTitle("Tendril");
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    resize(720, 720);
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
        tbIdx = qBound(0, st["timebase"].toInt(3), TB_COUNT - 1);
        vdIdx = qBound(0, st["vdiv"].toInt(1), VD_COUNT - 1);
    }
    sd.lead = lead; sd.backing = lead;
    inst.restore(sd);
    refreshLists();

    connect(&frameTimer, &QTimer::timeout, this, [this] {
        // 25 fps while anything moves; a slow tick when the instrument is idle (battery).
        // Only the tube is redrawn every frame. The panel and the key deck repaint when
        // something on them changes (a lamp, a key, the glide strip); input repaints at once.
        static int skip = 0;
        UiState s = inst.uiState();
        u = s;
        bool busy = s.voices > 0 || !captured.isEmpty() || s.backPlaying || s.loopState != L_EMPTY || s.peak > 0.002f ||
                    clock.elapsed() < toastUntil + 200 || clock.elapsed() < readoutUntil + 200 || listenState == 1 || wav.isOpen() ||
                    !held.isEmpty();
        if (!busy && ++skip < 12) return;
        skip = 0;
        quint64 ps = panelSignature(), ds = deckSignature();
        if (ps != panelSig) { panelSig = ps; update(toWidget(PANEL)); update(toWidget(UNDER)); }
        if (ds != deckSigDyn) { deckSigDyn = ds; update(toWidget(DECK)); }
        update(toWidget(TUBE));
    });
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
    o["timebase"] = tbIdx;
    o["vdiv"] = vdIdx;
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
        toast(chordRow ? "Bottom row plays chords" : "Bottom row is a string");
        break;
    case A_PAGE_NEXT: setMenu((mnu + 1) % M_COUNT); break;
    case A_HOME: setMenu(M_SOUND); slotSave = false; break;
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
    case A_HELP: setView(vw == V_HELP ? V_SCOPE : V_HELP); break;
    case A_XYMAP: set(S_XYMAP, (float)((u.set.xyMap + 1) % XY_COUNT)); toast(QString("Touch X-Y: %1").arg(xyMapName((u.set.xyMap + 1) % XY_COUNT)), 1000); break;
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
        int act = km.command.value(key, A_NONE);
        if (act == A_NONE) { // front-panel shortcuts: soft keys 1-6, and the selected knob
            if (key >= Qt::Key_1 && key <= Qt::Key_6) pressSoft(key - Qt::Key_1);
            else if (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up || key == Qt::Key_Down) {
                bool coarse = key == Qt::Key_Up || key == Qt::Key_Down, up = key == Qt::Key_Right || key == Qt::Key_Up;
                if (selKnob == "entry") turnEntry((up ? 1 : -1) * (coarse ? 4.0f : 1.0f));
                else turnKnob(selKnob, (up ? 1 : -1) * (coarse ? 0.05f : 0.005f));
            }
        }
        perform(act, modChromatic);
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
        int hit = -1;
        for (int i = (int)zones.size() - 1; i >= 0 && hit < 0; --i)
            if (zones[i].r.contains(c)) hit = i;
        if (hit < 0) { // the controls are small: a near miss still counts
            qreal best = 14 * 14;
            for (int i = 0; i < (int)zones.size(); ++i) {
                const QRectF &r = zones[i].r;
                qreal dx = qMax(qMax(r.left() - c.x(), 0.0), c.x() - r.right()), dy = qMax(qMax(r.top() - c.y(), 0.0), c.y() - r.bottom());
                if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; hit = i; }
            }
        }
        if (hit >= 0) {
            captured[id] = zones[hit];
            downNames.insert(zones[hit].name);
            zones[hit].fn(c, PH_PRESS);
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

QStringList MainWindow::zoneNames() const {
    QStringList n;
    for (const Zone &z : zones) n << z.name;
    return n;
}

QImage MainWindow::shot() {
    QImage img(720, 720, QImage::Format_RGB32); // the instrument's native canvas, whatever the window size
    QPainter p(&img);
    allDirty = true;
    drawAll(p, 720, 720);
    return img;
}

QImage MainWindow::shotPartial(const QImage &base, QRect area) {
    QImage img = base;
    QPainter p(&img);
    p.setClipRect(area);
    allDirty = false;
    dirty = QRegion(area);
    drawAll(p, 720, 720);
    allDirty = true;
    return img;
}

void MainWindow::wheelEvent(QWheelEvent *e) {
    float d = e->angleDelta().y() / 120.0f;
    if (selKnob == "entry") turnEntry(d);
    else turnKnob(selKnob, d * 0.01f);
    update();
}

// --------------------------------------------------------- panel plumbing

QRect MainWindow::toWidget(QRectF c) const {
    qreal s = qMin(width(), height()) / CW;
    qreal ox = (width() - CW * s) / 2, oy = (height() - CW * s) / 2;
    return QRectF(ox + c.x() * s, oy + c.y() * s, c.width() * s, c.height() * s).toAlignedRect().adjusted(-2, -2, 2, 2);
}

bool MainWindow::vis(const QRectF &r) const {
    return allDirty || dirty.intersects(r.toAlignedRect().adjusted(-8, -8, 8, 8));
}

void MainWindow::zone(QRectF r, const QString &name, std::function<void(QPointF, int)> fn) {
    zones.push_back({r, name, std::move(fn)});
}

void MainWindow::readout(const QString &label, const QString &value) {
    readoutLabel = label; readoutValue = value;
    readoutUntil = clock.elapsed() + 1500;
}

void MainWindow::setMenu(int m, int page) {
    mnu = qBound(0, m, (int)M_COUNT - 1);
    menuPage[mnu] = page;
    vw = V_SCOPE;
    selSoft = -1;
    stepAcc = 0;
    buildMenu();
    update();
}

void MainWindow::setView(int v) {
    vw = qBound(0, v, (int)V_COUNT - 1);
    selSoft = -1;
    buildMenu();
    update();
}

void MainWindow::pressSoft(int i) {
    if (i < 0 || i > 5) return;
    buildMenu();
    Soft &k = soft[i];
    if (k.turn && selSoft != i) { // first tap hands the entry knob to this key
        selSoft = i; stepAcc = 0; selKnob = "entry";
    } else if (k.press) {
        k.press();
    }
    buildMenu();
    update();
}

void MainWindow::turnEntry(float detents) {
    entryAngle += detents * 9.0f;
    buildMenu();
    if (selSoft >= 0 && soft[selSoft].turn) soft[selSoft].turn(detents);
    buildMenu();
    update();
}

void MainWindow::turnKnob(const QString &name, float amount) {
    auto it = knobTurn.constFind(name);
    if (it != knobTurn.constEnd() && it.value()) it.value()(amount);
    update();
}

QString MainWindow::softTitle(int i) { buildMenu(); return soft[qBound(0, i, 5)].title; }
QString MainWindow::softValue(int i) { buildMenu(); return soft[qBound(0, i, 5)].value; }

void MainWindow::sustainLatch() {
    susLatch = !susLatch;
    inst.cmd(EV_SUSTAIN, susLatch || susKey ? 1 : 0);
}

extern qint64 tendrilMsSinceStart();

void MainWindow::paintEvent(QPaintEvent *e) {
    static bool first = true;
    if (first) {
        first = false;
        if (qEnvironmentVariableIsSet("TENDRIL_TIMING")) fprintf(stderr, "tendril: first frame %lld ms after launch\n", (long long)tendrilMsSinceStart());
    }
    QPainter painter(this);
    // partial repaints (the tube every frame) only make sense when the canvas maps 1:1
    allDirty = width() != 720 || height() != 720 || e->region().boundingRect() == rect();
    dirty = e->region();
    drawAll(painter, width(), height());
}
