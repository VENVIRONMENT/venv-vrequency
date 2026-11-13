#include "mainwindow.h"
#include "palette.h"
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

using namespace pal;

namespace {
const qreal CW = 720;
enum { HK_STRING, HK_ACT, HK_NONE };

QString sinkDescription; // refreshed asynchronously via wpctl
bool sinkIsDummy = false, sinkIsBluetooth = false;
int sinkGeneration = 0; // bumps when the output device type changes
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

MainWindow::MainWindow(Instrument &i, AudioOut &a) : inst(i), audio(a) {
    setWindowTitle("venv vrequency");
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
    QString path = store::baseDir() + "/recordings/" + store::timestampName("vrequency", "wav");
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

QStringList MainWindow::zoneNames() const {
    QStringList n;
    for (const Zone &z : zones) n << z.name;
    return n;
}

QImage MainWindow::shot() {
    QImage img(720, 720, QImage::Format_RGB32); // the instrument's native canvas, whatever the window size
    QPainter p(&img);
    drawAll(p, 720, 720);
    return img;
}

// ------------------------------------------------------- drawing helpers
//
// The whole screen is drawn the way a bench oscilloscope draws its own display:
// white phosphor on black, one-pixel rules, raster type, and inverse video for
// whatever is selected. There is no colour; state is shown by inversion, by a
// double frame, or by blinking.

QFont MainWindow::font(int px, bool bold) const {
    // building a QFont is surprisingly costly; every label reuses a cached one
    quint32 key = (quint32)px << 1 | (bold ? 1u : 0u);
    auto it = fontCache.constFind(key);
    if (it != fontCache.constEnd()) return it.value();
    QFont f("DejaVu Sans Mono");
    f.setStyleHint(QFont::Monospace);
    f.setStyleStrategy(QFont::NoAntialias); // raster text, like the tube's character generator
    f.setHintingPreference(QFont::PreferFullHinting);
    f.setPixelSize(px);
    f.setBold(bold);
    fontCache.insert(key, f);
    return f;
}

int MainWindow::chW(int px) const {
    static QHash<int, int> w;
    auto it = w.constFind(px);
    if (it != w.constEnd()) return it.value();
    int v = QFontMetrics(font(px, true)).horizontalAdvance(QLatin1Char('M'));
    w.insert(px, v);
    return v;
}

QString MainWindow::fit(const QString &s, int px, qreal width) const {
    int n = qMax(1, (int)(width / chW(px)));
    return s.size() <= n ? s : s.left(n - 1) + QString::fromUtf8("…");
}

void MainWindow::txt(QRectF r, const QString &s, int px, QColor c, int align, bool bold) {
    g->setFont(font(px, bold));
    g->setPen(c);
    g->drawText(r, align | Qt::TextSingleLine, s);
}

// a one-pixel frame on the pixel grid, optionally filled
void MainWindow::frame(QRectF r, QColor line, QColor fill) {
    if (fill.isValid()) g->fillRect(r, fill);
    if (!line.isValid()) return;
    g->fillRect(QRectF(r.x(), r.y(), r.width(), 1), line);
    g->fillRect(QRectF(r.x(), r.bottom() - 1, r.width(), 1), line);
    g->fillRect(QRectF(r.x(), r.y(), 1, r.height()), line);
    g->fillRect(QRectF(r.right() - 1, r.y(), 1, r.height()), line);
}

// inverse-video label: a white block with black text
void MainWindow::tag(QRectF r, const QString &s, int px, int align) {
    g->fillRect(r, INK);
    txt(r.adjusted(4, 0, -4, 0), s, px, BG, align, true);
}

// small heading with a rule running to the right edge
void MainWindow::section(qreal y, const QString &s) {
    txt(QRectF(18, y, 680, 20), s, 13, MID, Qt::AlignLeft | Qt::AlignVCenter);
    qreal x = 18 + s.size() * chW(13) + 10;
    if (x < 700) g->fillRect(QRectF(x, y + 10, 704 - x, 1), GRAT);
}

void MainWindow::zone(QRectF r, const QString &name, std::function<void(QPointF, int)> fn) {
    zones.push_back({r, name, std::move(fn)});
}

void MainWindow::button(QRectF r, const QString &name, const QString &label, bool on, std::function<void()> fn,
                        const QString &sub, int look) {
    bool inv = on || downNames.contains(name);
    frame(r, inv || look == LOOK_ARMED ? INK : LOW, inv ? INK : BG);
    if (look == LOOK_ARMED && !inv) frame(r.adjusted(3, 3, -3, -3), INK);
    QColor tc = inv ? BG : INK;
    int px = r.height() >= 50 ? 16 : 14;
    while (px > 11 && label.size() * chW(px) > r.width() - 6) --px;
    if (sub.isEmpty()) {
        txt(r, fit(label, px, r.width() - 4), px, tc, Qt::AlignCenter, true);
    } else {
        txt(QRectF(r.x(), r.y() + r.height() * 0.13, r.width(), r.height() * 0.46), fit(label, px, r.width() - 4), px, tc, Qt::AlignCenter, true);
        txt(QRectF(r.x() + 2, r.y() + r.height() * 0.58, r.width() - 4, r.height() * 0.32), fit(sub, 11, r.width() - 6), 11, inv ? BG : MID);
    }
    zone(r, name, [fn](QPointF, int ph) { if (ph == PH_PRESS && fn) fn(); });
}

// a level readout: value bar over a printed tick scale, with a marker
void MainWindow::slider(QRectF r, const QString &name, const QString &label, float norm, const QString &value,
                        std::function<void(float)> fn) {
    bool dn = downNames.contains(name);
    frame(r, dn ? INK : LOW, BG);
    norm = norm < 0 ? 0 : (norm > 1 ? 1 : norm);
    QRectF bar(r.x() + 10, r.bottom() - 19, r.width() - 20, 6);
    for (int i = 0; i <= 20; ++i) {
        qreal x = std::floor(bar.x() + (bar.width() - 1) * i / 20.0);
        bool major = i % 5 == 0;
        g->fillRect(QRectF(x, bar.bottom() + 2, 1, major ? 6 : 3), major ? MID : LOW);
    }
    g->fillRect(QRectF(bar.x(), bar.bottom() - 1, bar.width(), 1), LOW);
    qreal vx = std::floor(bar.x() + (bar.width() - 1) * norm);
    g->fillRect(QRectF(bar.x(), bar.y(), vx - bar.x(), bar.height()), INK);
    g->fillRect(QRectF(vx - 1, bar.y() - 4, 3, bar.height() + 8), INK);
    txt(QRectF(r.x() + 10, r.y() + 5, r.width() * 0.6, 18), label, 13, MID, Qt::AlignLeft | Qt::AlignVCenter);
    txt(QRectF(r.x() + r.width() * 0.4, r.y() + 5, r.width() * 0.6 - 10, 18), value, 13, INK, Qt::AlignRight | Qt::AlignVCenter, true);
    zone(r, name, [fn, bar](QPointF p, int ph) {
        if (ph == PH_RELEASE) return;
        fn((float)((p.x() - bar.x()) / bar.width()));
    });
}

void MainWindow::stepper(QRectF r, const QString &name, const QString &label, const QString &value,
                         std::function<void()> minus, std::function<void()> plus) {
    frame(r, LOW, BG);
    qreal bw = r.width() < 200 ? 44 : 56;
    QRectF lm(r.x(), r.y(), bw, r.height()), rp(r.right() - bw, r.y(), bw, r.height());
    bool dm = downNames.contains(name + "-"), dp = downNames.contains(name + "+");
    if (dm) g->fillRect(lm, INK);
    if (dp) g->fillRect(rp, INK);
    g->fillRect(QRectF(lm.right() - 1, r.y() + 1, 1, r.height() - 2), LOW);
    g->fillRect(QRectF(rp.x(), r.y() + 1, 1, r.height() - 2), LOW);
    txt(lm, QString::fromUtf8("−"), 24, dm ? BG : INK, Qt::AlignCenter, true);
    txt(rp, "+", 24, dp ? BG : INK, Qt::AlignCenter, true);
    txt(QRectF(r.x() + bw, r.y() + 6, r.width() - 2 * bw, 16), fit(label, 12, r.width() - 2 * bw - 4), 12, MID);
    txt(QRectF(r.x() + bw, r.y() + 22, r.width() - 2 * bw, r.height() - 26), value, 18, INK, Qt::AlignCenter, true);
    zone(lm, name + "-", [minus](QPointF, int ph) { if (ph == PH_PRESS) minus(); });
    zone(rp, name + "+", [plus](QPointF, int ph) { if (ph == PH_PRESS) plus(); });
}

void MainWindow::paramSlider(QRectF r, int p) {
    slider(r, QString("param%1").arg(p), nice(paramInfo(p).label), paramToNorm(p, lead.p[p]), paramText(p, lead.p[p]),
           [this, p](float n) { setParam(p, normToParam(p, n)); });
}

// ------------------------------------------------------------------ paint

extern qint64 msSinceStart();

void MainWindow::paintEvent(QPaintEvent *) {
    static bool first = true;
    if (first) {
        first = false;
        if (qEnvironmentVariableIsSet("VREQUENCY_TIMING")) fprintf(stderr, "vrequency: first frame %lld ms after launch\n", (long long)msSinceStart());
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
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
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

    if (clock.elapsed() < toastUntil && !toastText.isEmpty()) { // a message box, as the scope reports "Auto-scale"
        QString t = fit(toastText, 14, 640);
        int w = t.size() * chW(14) + 28;
        QRectF r(std::floor((CW - w) / 2), 86, w, 30);
        frame(r.adjusted(-4, -4, 4, 4), INK, BG);
        tag(r, t, 14);
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
    g = nullptr;
}

void MainWindow::paintTabs() {
    static const char *N[PG_COUNT] = {"Play", "Sound", "Backing", "Tune", "System", "Help"};
    qreal w = 688 / 6.0;
    for (int i = 0; i < PG_COUNT; ++i) // one joined menu strip: neighbours share their edge
        button(QRectF(std::floor(16 + i * w), 8, std::floor(16 + (i + 1) * w) - std::floor(16 + i * w) + 1, 46), QString("tab%1").arg(i), N[i], pg == i,
               [this, i] { pg = i; deepEdit = false; slotSave = false; });
}

void MainWindow::paintLoopButton(QRectF r) {
    bool dn = downNames.contains("loop");
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
    frame(r, inv || armed ? INK : LOW, inv ? INK : BG);
    if (armed && !inv) frame(r.adjusted(3, 3, -3, -3), INK);
    QColor tc = inv ? BG : INK;
    txt(QRectF(r.x(), r.y() + 9, r.width(), 32), label, 24, u.loopState == L_STOPPED ? MID : tc, Qt::AlignCenter, true);
    txt(QRectF(r.x(), r.y() + 45, r.width(), 16), fit(sub, 11, r.width() - 10), 11, inv ? BG : MID);
    if (prog > 0) { // position in the loop
        QRectF b(r.x() + 9, r.bottom() - 14, r.width() - 18, 5);
        frame(b, inv ? BG : MID);
        g->fillRect(QRectF(b.x(), b.y(), b.width() * qMin(1.0f, prog), b.height()), tc);
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
            frame(r, sel || cur ? INK : (has ? MID : GRAT), cur ? INK : BG);
            if (sel && !cur) frame(r.adjusted(2, 2, -2, -2), INK);
            if (has) txt(r, buildChord(t, u.prog[i], u.set.seventh).name, 13, cur ? BG : INK, Qt::AlignCenter, true);
            else txt(r, QString::number(i + 1), 11, GRAT);
            if (has) zone(r.adjusted(0, -6, 0, 6), QString("pslot%1").arg(i), [this, i](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_BACK, BC_SELECT_SLOT, i); });
        }
        QString info;
        if (u.backPlaying && (u.progLen || u.holdChord >= 0 || u.droneMask)) info = QString::fromUtf8("♪ %1").arg(u.backName);
        else if (u.progLen) info = "stopped";
        else info = chordRow ? QString::fromUtf8("Z‥M = chords") : QString("tap chords");
        qreal ix = area.x() + 8 * (sw + 4);
        txt(QRectF(ix, y, area.right() - ix, 22), fit(info, 12, area.right() - ix), 12, MID, Qt::AlignRight | Qt::AlignVCenter);
        y += 28;
    }
    qreal h = area.bottom() - y;
    qreal ctl = 92, pw = (area.width() - ctl - 7 * 6) / 7.0;
    for (int d = 0; d < 7; ++d) {
        QRectF r(std::floor(area.x() + d * (pw + 6)), y, std::floor(pw), h);
        QString name = QString("chord%1").arg(d);
        bool cur = u.curChord == d, dn = downNames.contains(name);
        bool drone = (u.droneMask >> d) & 1;
        bool inv = cur || dn;
        frame(r, inv || drone ? INK : LOW, inv ? INK : BG);
        if (drone && !inv) frame(r.adjusted(3, 3, -3, -3), INK);
        Chord c = buildChord(t, d, u.set.seventh);
        txt(QRectF(r.x(), r.y() + h * 0.10, r.width(), h * 0.52), c.name, c.name[3] ? 16 : 20, inv ? BG : INK, Qt::AlignCenter, true);
        bool small = std::strchr(c.name + 1, 'm') && !std::strstr(c.name, "maj");
        txt(QRectF(r.x(), r.y() + h * 0.62, r.width(), h * 0.30), romanNumeral(d, small), 12, inv ? BG : MID);
        zone(r, name, [this, d](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_CHORD, d); });
    }
    QRectF c1(area.right() - ctl, y, ctl, h / 2 - 3), c2(area.right() - ctl, y + h / 2 + 3, ctl, h / 2 - 3);
    static const char *MN[BM_COUNT] = {"Prog", "Hold", "Drone"};
    button(c1, "backmode", MN[u.set.backMode], false, [this] { perform(A_BACK_MODE); });
    button(c2, "backplay", u.backPlaying ? QString::fromUtf8("■ Stop") : QString::fromUtf8("▶ Play"), u.backPlaying,
           [this] { inst.cmd(EV_BACK, BC_PLAYSTOP); });
}

void MainWindow::paintPlay() {
    const Tuning &t = u.set.tuning;
    const int L = Qt::AlignLeft | Qt::AlignVCenter, R = Qt::AlignRight | Qt::AlignVCenter;
    // ---------------- header: readouts ----------------
    txt(QRectF(18, 6, 430, 44), fit(lead.name, 30, 428), 30, INK, L, true);
    QString slotTxt = curSlot >= 0 ? QString("Sound %1").arg(curSlot + 1) : QString("Unsaved sound");
    txt(QRectF(20, 52, 230, 20), QString("%1   %2 voices").arg(slotTxt).arg(u.voices), 13, MID, L);
    if (wav.isOpen()) {
        if ((clock.elapsed() / 400) % 2 == 0) tag(QRectF(250, 54, 104, 17), QString("REC %1 s").arg(wav.frames() / audio.sampleRate()), 12);
    } else if (sinkIsDummy) {
        tag(QRectF(250, 54, 150, 17), "No audio device", 12);
    } else if (u.limiter < 0.85f) {
        tag(QRectF(250, 54, 60, 17), "Limit", 12);
    }
    zone(QRectF(16, 4, 430, 72), "hdr_sound", [this](QPointF, int ph) { if (ph == PH_PRESS) pg = PG_SOUND; });

    g->fillRect(QRectF(444, 10, 1, 60), GRAT);
    txt(QRectF(456, 6, 60, 44), noteName(t.root), 32, INK, L, true);
    txt(QRectF(456, 52, 110, 20), QString(scaleDef(t.scale).shortName).toLower(), 13, MID, L);
    txt(QRectF(552, 6, 86, 44), QString::number((int)std::round(u.set.bpm)), 28, INK, R, true);
    txt(QRectF(552, 52, 86, 20), "bpm", 13, MID, R);
    double frac = u.beat - std::floor(u.beat);
    bool running = u.backPlaying || u.loopState != L_EMPTY;
    frame(QRectF(552, 24, 9, 9), running ? INK : LOW, running && frac < 0.25 ? INK : BG); // beat lamp
    txt(QRectF(644, 6, 60, 44), QString("%1%2").arg(t.octave > 0 ? "+" : "").arg(t.octave), 28, INK, R, true);
    txt(QRectF(644, 52, 60, 20), "oct", 13, MID, R);
    zone(QRectF(450, 4, 254, 72), "hdr_tune", [this](QPointF, int ph) { if (ph == PH_PRESS) pg = PG_TUNE; });

    // output level: a row of segments
    {
        int lit = (int)std::ceil(qMin(1.0f, u.peak) * 86);
        for (int i = 0; i < 86; ++i) g->fillRect(QRectF(16 + i * 8, 80, 6, 4), i < lit ? INK : GRAT);
    }

    // ---------------- scope + X-Y pad ----------------
    QRectF xy(16, 92, 688, 226);
    drawScope(xy);
    zone(xy, "xy", [this, xy](QPointF p, int ph) {
        Event e; e.type = EV_XY; e.a = ph != PH_RELEASE;
        e.f = (float)qBound(0.0, (p.x() - xy.x()) / xy.width(), 1.0);
        e.g = (float)qBound(0.0, (xy.bottom() - p.y()) / xy.height(), 1.0);
        inst.push(e);
    });
    button(QRectF(xy.right() - 196, xy.y() + 8, 118, 40), "xymap", nice(xyMapName(u.set.xyMap)), false, [this] { perform(A_XYMAP); });
    button(QRectF(xy.right() - 72, xy.y() + 8, 64, 40), "xyhold", "Hold", u.set.xyHold, [this] { set(S_XYHOLD, u.set.xyHold ? 0 : 1); });

    // ---------------- strings ----------------
    QRectF sp(16, 328, 688, 176);
    frame(sp, LOW, BG);
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
            g->setRenderHint(QPainter::Antialiasing, true);
            g->setBrush(Qt::NoBrush);
            g->setPen(QPen(QColor(255, 255, 255, 60), 5));
            g->drawPath(line);
            g->setPen(QPen(INK, 1.6));
            g->drawPath(line);
            g->setRenderHint(QPainter::Antialiasing, false);
        } else {
            g->fillRect(QRectF(sp.x() + 8, y, sp.width() - 16, 1), LOW);
        }
        txt(QRectF(sp.x() + 4, y - 26, 20, 12), QString("S%1").arg(row + 1), 10, su.gate ? INK : LOW, L);
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
        bool chordKeys = row == 0 && (chordRow || modChord);
        for (int c = 0; c < count; ++c) {
            int n = t.noteFor(row, c, false);
            bool isHeldTop = su.held > 0 && su.topCol == c;
            bool isRoot = ((n - t.root) % 12 + 12) % 12 == 0;
            QRectF box(std::floor(X(c)) - 17, y - 11, 35, 23);
            frame(box, isHeldTop || isRoot ? INK : LOW, isHeldTop ? INK : BG);
            QString nn = QString("%1%2").arg(noteName(n)).arg(n / 12 - 1);
            txt(box, nn, 12, isHeldTop ? BG : (isRoot ? INK : MID), Qt::AlignCenter, isRoot || isHeldTop);
            QRectF kr(box.center().x() - 8, y - 27, 16, 13);
            if (chordKeys) tag(kr.adjusted(1, 0, -1, 0), QString(km.rows[row][c]), 10, Qt::AlignCenter);
            else txt(kr, QString(km.rows[row][c]), 11, LOW);
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
                g->fillRect(QRectF(std::floor(tr[i]), y + 13, 1, 9), QColor(v, v, v));
            }
            qreal cx = std::floor(px);
            int wd = sliding ? 3 : 2;
            g->fillRect(QRectF(cx - 21, y - 15, wd, 31), INK); // [ ] brackets round the sounding pitch
            g->fillRect(QRectF(cx + 21 - wd + 1, y - 15, wd, 31), INK);
            g->fillRect(QRectF(cx - 21, y - 15, 7, wd), INK);
            g->fillRect(QRectF(cx - 21, y + 16 - wd, 7, wd), INK);
            g->fillRect(QRectF(cx + 15, y - 15, 7, wd), INK);
            g->fillRect(QRectF(cx + 15, y + 16 - wd, 7, wd), INK);
            QPainterPath m; // trigger-style marker under the string
            m.moveTo(cx - 4, y + 23); m.lineTo(cx + 5, y + 23); m.lineTo(cx + 0.5, y + 17); m.closeSubpath();
            g->setRenderHint(QPainter::Antialiasing, true);
            g->setPen(Qt::NoPen);
            g->setBrush(INK);
            g->drawPath(m);
            g->setRenderHint(QPainter::Antialiasing, false);
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

    // ---------------- bottom bar: the soft keys ----------------
    qreal x = 16, y = 628, h = 80;
    paintLoopButton(QRectF(x, y, 150, h)); x += 158;
    button(QRectF(x, y, 66, h), "loopundo", "Undo", false, [this] { inst.cmd(EV_LOOP, LC_UNDO); }, "layer"); x += 74;
    bool lp = u.loopState == L_PLAY || u.loopState == L_OVERDUB;
    button(QRectF(x, y, 66, h), "loopstop", lp ? QString::fromUtf8("■") : QString::fromUtf8("▶"), false, [this] { inst.cmd(EV_LOOP, LC_PLAYSTOP); }, "loop"); x += 74;
    button(QRectF(x, y, 66, h), "sus", "Sus", u.sustain, [this] { susLatch = !susLatch; inst.cmd(EV_SUSTAIN, susLatch || susKey ? 1 : 0); }, "sustain"); x += 74;
    button(QRectF(x, y, 54, h), "octdn", QString::fromUtf8("−"), false, [this] { inst.cmd(EV_OCTAVE, -1); }, "oct"); x += 62;
    button(QRectF(x, y, 54, h), "octup", "+", false, [this] { inst.cmd(EV_OCTAVE, 1); }, "oct"); x += 62;
    button(QRectF(x, y, 86, h), "dice", "New", false, [this] { randomise(); }, "sound"); x += 94;
    button(QRectF(x, y, 704 - x, h), "menu", "Menu", false, [this] { pg = PG_SOUND; }, QString::fromUtf8("≡"));
}

void MainWindow::paintSound() {
    qreal w = (688 - 4 * 8) / 5.0;
    for (int i = 0; i < 10; ++i) {
        QRectF r(std::floor(16 + (i % 5) * (w + 8)), 66 + (i / 5) * 56, std::floor(w), 50);
        QString name = QString("slot%1").arg(i);
        bool on = curSlot == i && !slotSave;
        bool inv = on || downNames.contains(name);
        frame(r, inv || slotSave ? INK : LOW, inv ? INK : BG);
        if (slotSave && !inv) frame(r.adjusted(3, 3, -3, -3), INK);
        // slot number in a corner tab, name beneath: laid out like a soft-key label
        g->fillRect(QRectF(r.x() + 1, r.y() + 1, 18, 16), inv ? BG : LOW);
        txt(QRectF(r.x() + 1, r.y() + 1, 18, 16), QString::number((i + 1) % 10), 12, inv ? INK : BG, Qt::AlignCenter, true);
        txt(QRectF(r.x() + 4, r.y() + 20, r.width() - 8, 24), fit(bank[i].name, 13, r.width() - 8), 13, inv ? BG : INK, Qt::AlignLeft | Qt::AlignVCenter, true);
        zone(r, name, [this, i](QPointF, int ph) { if (ph == PH_PRESS) selectSlot(i); });
    }
    qreal y = 182;
    auto col = [w](int i) { return QRectF(std::floor(16 + i * (w + 8)), 182, std::floor(w), 54); };
    Q_UNUSED(y);
    button(col(0), "newsound", "New sound", false, [this] { randomise(); }, "randomise");
    button(col(1), "mutate", "Mutate", false, [this] { mutate(); }, "nudge it");
    button(col(2), "undosound", "Undo", false, [this] { undoSound(); }, QString("%1 step%2").arg(undo.size()).arg(undo.size() == 1 ? "" : "s"));
    button(col(3), "slotsave", "Store", slotSave, [this] { slotSave = !slotSave; }, slotSave ? "pick a slot" : "to a slot 1-0");
    button(col(4), "fav", QString::fromUtf8("♥ Save"), false, [this] { saveFavourite(); }, "favourite");

    txt(QRectF(18, 246, 440, 40), fit(lead.name, 24, 440), 24, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    button(QRectF(704 - 220, 248, 220, 38), "deep", deepEdit ? QString::fromUtf8("‹ Quick controls") : QString::fromUtf8("All parameters ›"), deepEdit, [this] { deepEdit = !deepEdit; });

    if (deepEdit) {
        const int per = 12, pages = (P_COUNT + per - 1) / per;
        deepPage = qBound(0, deepPage, pages - 1);
        for (int k = 0; k < per; ++k) {
            int p = deepPage * per + k;
            if (p >= P_COUNT) break;
            paramSlider(QRectF(16 + (k % 2) * 348, 296 + (k / 2) * 58, 340, 52), p);
        }
        button(QRectF(16, 650, 160, 54), "deepprev", QString::fromUtf8("‹ Prev"), false, [this, pages] { deepPage = (deepPage + pages - 1) % pages; });
        txt(QRectF(176, 650, 368, 54), QString("Page %1 / %2").arg(deepPage + 1).arg(pages), 14, MID);
        button(QRectF(544, 650, 160, 54), "deepnext", QString::fromUtf8("Next ›"), false, [this, pages] { deepPage = (deepPage + 1) % pages; });
        return;
    }
    static const int macro[8] = {P_CUTOFF, P_RESO, P_ATK, P_REL, P_GLIDE, P_DRIVE, P_DLYMIX, P_REVMIX};
    for (int k = 0; k < 8; ++k) paramSlider(QRectF(16 + (k % 2) * 348, 296 + (k / 2) * 58, 340, 52), macro[k]);

    // favourites
    const int per = 6, pages = qMax(1, ((int)favs.size() + per - 1) / per);
    favPage = qBound(0, favPage, pages - 1);
    section(534, QString("Favourites  %1").arg(favs.size()));
    qreal cw = (688 - 110 - 3 * 8) / 3.0;
    for (int k = 0; k < per; ++k) {
        int idx = favPage * per + k;
        QRectF r(std::floor(16 + (k % 3) * (cw + 8)), 560 + (k / 3) * 74, std::floor(cw), 66);
        if (idx >= favs.size()) { frame(r, GRAT); continue; }
        button(r, QString("favitem%1").arg(k), favs[idx].patch.name, false, [this, idx] {
            if (idx >= favs.size()) return;
            if (favDelete) { QFile::remove(favs[idx].file); refreshLists(); favDelete = false; toast("Favourite removed"); return; }
            pushUndo(); lead = favs[idx].patch; applyLead(true);
        }, favDelete ? "tap to delete" : "recall", favDelete ? LOOK_ARMED : LOOK_PLAIN);
    }
    qreal bx = 704 - 102;
    button(QRectF(bx, 560, 102, 30), "favprev", QString::fromUtf8("‹ %1/%2 ›").arg(favPage + 1).arg(pages), false, [this, pages] { favPage = (favPage + 1) % pages; });
    button(QRectF(bx, 598, 102, 102), "favdel", "Delete", favDelete, [this] { favDelete = !favDelete; }, favDelete ? "pick one" : "a favourite");
}

void MainWindow::paintBacking() {
    section(62, QString::fromUtf8("A chord tap…"));
    static const char *MS[BM_COUNT] = {"adds to the loop", "latches one chord", "latches a drone note"};
    qreal w3 = (688 - 2 * 8) / 3.0;
    for (int i = 0; i < BM_COUNT; ++i)
        button(QRectF(std::floor(16 + i * (w3 + 8)), 86, std::floor(w3), 54), QString("bmode%1").arg(i), nice(backModeName(i)), u.set.backMode == i, [this, i] { set(S_BACKMODE, (float)i); }, MS[i]);
    qreal w5 = (688 - 4 * 6) / 5.0;
    for (int i = 0; i < BS_COUNT; ++i)
        button(QRectF(std::floor(16 + i * (w5 + 6)), 148, std::floor(w5), 46), QString("bstyle%1").arg(i), nice(backStyleName(i)), u.set.backStyle == i, [this, i] { set(S_BACKSTYLE, (float)i); });

    // progression slots
    section(202, "Progression  tap a slot, then a chord, to replace it");
    qreal sw = (688 - 7 * 6) / 8.0;
    for (int i = 0; i < kProgMax; ++i) {
        QRectF r(std::floor(16 + i * (sw + 6)), 226, std::floor(sw), 58);
        bool has = i < u.progLen, cur = has && i == u.progPos && u.backPlaying, sel = i == u.selSlot;
        frame(r, sel || cur ? INK : (has ? MID : GRAT), cur ? INK : BG);
        if (sel && !cur) frame(r.adjusted(3, 3, -3, -3), INK);
        txt(QRectF(r.x(), r.y() + 5, r.width(), 14), QString::number(i + 1), 11, cur ? BG : (has ? MID : LOW));
        if (has) txt(QRectF(r.x(), r.y() + 18, r.width(), 34), buildChord(u.set.tuning, u.prog[i], u.set.seventh).name, 18, cur ? BG : INK, Qt::AlignCenter, true);
        if (has) zone(r, QString("bslot%1").arg(i), [this, i](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_BACK, BC_SELECT_SLOT, i); });
    }
    qreal w4 = (688 - 3 * 8) / 4.0;
    auto c4 = [w4](int i, qreal y, qreal h) { return QRectF(std::floor(16 + i * (w4 + 8)), y, std::floor(w4), h); };
    button(c4(0, 292, 52), "bremove", "Remove last", false, [this] { inst.cmd(EV_BACK, BC_REMOVE_LAST); });
    button(c4(1, 292, 52), "bclear", "Clear all", false, [this] { inst.cmd(EV_BACK, BC_CLEAR); });
    button(c4(2, 292, 52), "bseventh", "7th chords", u.set.seventh, [this] { set(S_SEVENTH, u.set.seventh ? 0 : 1); });
    button(c4(3, 292, 52), "bmetro", "Metronome", u.set.metro, [this] { set(S_METRO, u.set.metro ? 0 : 1); });

    paintChordPads(QRectF(16, 356, 688, 84), false);

    stepper(QRectF(16, 452, 340, 60), "chordbeats", "Beats per chord", QString::number(u.set.chordBeats),
            [this] { set(S_CHORDBEATS, (float)(u.set.chordBeats - 1)); }, [this] { set(S_CHORDBEATS, (float)(u.set.chordBeats + 1)); });
    stepper(QRectF(364, 452, 340, 60), "arprate", "Arp / pulse steps per beat", QString::number(u.set.arpRate),
            [this] { set(S_ARPRATE, (float)(u.set.arpRate - 1)); }, [this] { set(S_ARPRATE, (float)(u.set.arpRate + 1)); });
    stepper(QRectF(16, 520, 340, 60), "bpm2", "Tempo", QString("%1 bpm").arg((int)std::round(u.set.bpm)),
            [this] { set(S_BPM, std::round(u.set.bpm) - 2); }, [this] { set(S_BPM, std::round(u.set.bpm) + 2); });
    slider(QRectF(364, 520, 340, 60), "backvol", "Backing volume", u.set.backVol, QString("%1%").arg((int)(u.set.backVol * 100)), [this](float n) { set(S_BACKVOL, n); });
    button(QRectF(16, 590, 688, 56), "bresync", "Use the current lead sound for the backing", false, [this] { inst.cmd(EV_BACK, BC_RESYNC_SOUND); },
           QString("backing now plays: %1    lead: %2").arg(u.backName[0] ? u.backName : "(nothing yet)").arg(lead.name));
    txt(QRectF(18, 654, 684, 20), "The backing keeps the sound it started with, so the lead can change freely.", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    txt(QRectF(18, 676, 684, 20), QString::fromUtf8("Keyboard: hold %1 + Z‥M for chords, or press $ for a chord row.").arg(km.keyFor(A_CHORD_LAYER, false)), 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
}

void MainWindow::paintTune() {
    const Tuning &t = u.set.tuning;
    section(60, "Key");
    qreal w6 = (688 - 5 * 6) / 6.0;
    for (int i = 0; i < 12; ++i)
        button(QRectF(std::floor(16 + (i % 6) * (w6 + 6)), 84 + (i / 6) * 52, std::floor(w6), 46), QString("root%1").arg(i), noteName(i), t.root == i, [this, i] { set(S_ROOT, (float)i); });
    section(190, "Scale");
    qreal w4 = (688 - 3 * 6) / 4.0;
    for (int i = 0; i < SC_COUNT; ++i)
        button(QRectF(std::floor(16 + (i % 4) * (w4 + 6)), 214 + (i / 4) * 52, std::floor(w4), 46), QString("scale%1").arg(i), scaleDef(i).name, t.scale == i, [this, i] { set(S_SCALE, (float)i); });

    qreal w3 = (688 - 2 * 8) / 3.0;
    auto c3 = [w3](int i, qreal y) { return QRectF(std::floor(16 + i * (w3 + 8)), y, std::floor(w3), 60); };
    stepper(c3(0, 326), "oct", "Octave", QString("%1%2").arg(t.octave > 0 ? "+" : "").arg(t.octave),
            [this] { inst.cmd(EV_OCTAVE, -1); }, [this] { inst.cmd(EV_OCTAVE, 1); });
    stepper(c3(1, 326), "rowint", "String gap", QString("%1 st").arg(t.rowInterval),
            [this] { set(S_ROWINT, (float)(u.set.tuning.rowInterval - 1)); }, [this] { set(S_ROWINT, (float)(u.set.tuning.rowInterval + 1)); });
    stepper(c3(2, 326), "bendr", "Bend range", QString("%1 st").arg((int)u.set.bendRange),
            [this] { set(S_BENDRANGE, u.set.bendRange - 1); }, [this] { set(S_BENDRANGE, u.set.bendRange + 1); });

    stepper(c3(0, 394), "bpm", "Tempo", QString("%1 bpm").arg((int)std::round(u.set.bpm)),
            [this] { set(S_BPM, std::round(u.set.bpm) - 1); }, [this] { set(S_BPM, std::round(u.set.bpm) + 1); });
    QRectF mid = c3(1, 394);
    button(QRectF(mid.x(), 394, std::floor(mid.width() / 2) - 4, 60), "tap", "Tap", false, [this] { tapTempo(); }, "tempo");
    button(QRectF(mid.x() + std::floor(mid.width() / 2) + 4, 394, mid.right() - (mid.x() + std::floor(mid.width() / 2) + 4), 60), "metro", "Click", u.set.metro, [this] { set(S_METRO, u.set.metro ? 0 : 1); }, "metronome");
    slider(c3(2, 394), "glidescale", "Glide speed", u.set.glideScale / 4.0f,
           u.set.glideScale < 0.02f ? QString("instant") : QString::fromUtf8("×%1").arg(u.set.glideScale, 0, 'f', 2), [this](float n) { set(S_GLIDESCALE, n * 4.0f); });

    // listen
    QRectF lp(16, 466, 688, 238);
    frame(lp, LOW, BG);
    tag(QRectF(lp.x() + 1, lp.y() + 1, 62, 18), "Listen", 12);
    txt(QRectF(88, 466, 600, 20), "match the key and tempo of music around you", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    int ls = listenState;
    button(QRectF(32, 506, 200, 60), "listen", ls == 1 ? QString::fromUtf8("Listening…") : QString("Listen"), ls == 1, [this] { startListen(); }, ls == 1 ? QString("%1 s").arg(qMax<qint64>(0, 6 - (clock.elapsed() - listenStarted) / 1000)) : "6 seconds, needs a mic");
    if (ls == 0) {
        txt(QRectF(248, 506, 440, 60), "Uses the system's default audio input, if any.", 13, MID, Qt::AlignLeft | Qt::AlignVCenter);
    } else if (ls == 3) {
        std::lock_guard<std::mutex> lk(listenMx);
        g->setFont(font(13, false));
        g->setPen(INK);
        g->drawText(QRectF(248, 506, 440, 60), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, listenError);
    } else if (ls == 2) {
        std::lock_guard<std::mutex> lk(listenMx);
        const ListenResult &r = listenResult;
        const KeyGuess &k = r.keys[listenGuess % 3];
        float kc = listenGuess == 0 ? r.keyConfidence : r.keyConfidence * 0.5f;
        QString kq = kc > 0.6f ? "likely" : (kc > 0.3f ? "a guess" : "very unsure");
        txt(QRectF(248, 500, 250, 40), QString("%1 %2").arg(noteName(k.root)).arg(k.minor ? "minor" : "major"), 24, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
        txt(QRectF(248, 540, 250, 22), QString("key: %1 (%2%)").arg(kq).arg((int)(kc * 100)), 13, kc > 0.6f ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter);
        QString tq = r.bpmConfidence > 0.6f ? "likely" : (r.bpmConfidence > 0.3f ? "a guess" : "very unsure");
        txt(QRectF(500, 500, 190, 40), r.bpm > 0 ? QString("%1 bpm").arg((int)std::round(r.bpm)) : QString("no pulse"), 24, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
        txt(QRectF(500, 540, 190, 22), QString("tempo: %1 (%2%)").arg(tq).arg((int)(r.bpmConfidence * 100)), 13, r.bpmConfidence > 0.6f ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (ls == 2) {
        qreal bw = (656 - 3 * 8) / 4.0;
        button(QRectF(32, 580, bw, 56), "lkey", "Use key", false, [this] { applyListen(true, false); toast("Key applied"); });
        button(QRectF(32 + bw + 8, 580, bw, 56), "ltempo", "Use tempo", false, [this] { applyListen(false, true); toast("Tempo applied"); });
        button(QRectF(32 + 2 * (bw + 8), 580, bw, 56), "lboth", "Use both", false, [this] { applyListen(true, true); toast("Key and tempo applied"); });
        button(QRectF(32 + 3 * (bw + 8), 580, bw, 56), "lnext", "Other key", false, [this] { listenGuess = (listenGuess + 1) % 3; }, QString("guess %1 of 3").arg(listenGuess % 3 + 1));
        txt(QRectF(32, 646, 656, 44), "Wrong? Pick the key and scale by hand above: detection is an estimate.", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    }
}

void MainWindow::paintSystem() {
    slider(QRectF(16, 66, 340, 58), "volume", "Master volume", u.set.volume, QString("%1%").arg((int)(u.set.volume * 100)), [this](float n) { set(S_VOLUME, n); });
    slider(QRectF(364, 66, 340, 58), "loopvol", "Loop volume", u.set.loopVol, QString("%1%").arg((int)(u.set.loopVol * 100)), [this](float n) { set(S_LOOPVOL, n); });

    section(130, "Touch pad mapping");
    qreal w6 = (688 - 5 * 6) / 6.0;
    auto c6 = [w6](int i) { return QRectF(std::floor(16 + i * (w6 + 6)), 154, std::floor(w6), 54); };
    for (int i = 0; i < XY_COUNT; ++i)
        button(c6(i), QString("xymap%1").arg(i), nice(xyMapName(i)), u.set.xyMap == i, [this, i] { set(S_XYMAP, (float)i); },
               QString("%1/%2").arg(xyAxisName(i, false)).arg(xyAxisName(i, true)).toLower().replace("resonance", "reso"));
    button(c6(5), "xyhold2", "Hold", u.set.xyHold, [this] { set(S_XYHOLD, u.set.xyHold ? 0 : 1); }, "keep position");

    qreal w4 = (688 - 3 * 8) / 4.0;
    auto c4 = [w4](int i, qreal y, qreal h) { return QRectF(std::floor(16 + i * (w4 + 8)), y, std::floor(w4), h); };
    button(c4(0, 218, 58), "chordrow", "Chord row", chordRow, [this] { perform(A_CHORD_ROW); }, "bottom row = chords");
    button(c4(1, 218, 58), "recwav", wav.isOpen() ? QString::fromUtf8("■ Stop rec") : QString::fromUtf8("● Record WAV"), wav.isOpen(), [this] { toggleRecord(); },
           wav.isOpen() ? QString("%1 s").arg(wav.frames() / audio.sampleRate()) : "export what you play");
    button(c4(2, 218, 58), "savesession", "Save session", false, [this] {
        QString path = store::baseDir() + "/sessions/" + store::timestampName("session", "json");
        toast(saveSession(path) ? QString("Saved %1").arg(QFileInfo(path).fileName()) : QString("Could not save session"), 2200);
    }, "sounds, backing, loops");
    button(c4(3, 218, 58), "keytest", "Key test", keyTest, [this] { keyTest = !keyTest; maxHeld = 0; }, "check the keyboard");

    section(284, QString("Saved sessions  %1  tap to load").arg(sessions.size()));
    for (int k = 0; k < 4; ++k) {
        QRectF r(16 + (k % 2) * 348, 308 + (k / 2) * 54, 340, 48);
        if (k >= sessions.size()) { frame(r, GRAT); continue; }
        QString path = sessions[k];
        button(r, QString("session%1").arg(k), QFileInfo(path).completeBaseName(), false, [this, path] {
            toast(loadSession(path) ? QString("Loaded %1: press play on the loop / backing").arg(QFileInfo(path).completeBaseName()) : QString("Could not read that session"), 2600);
        });
    }

    QRectF ip(16, 424, 688, 206);
    frame(ip, LOW, BG);
    tag(QRectF(ip.x() + 1, ip.y() + 1, 62, 18), "Status", 12);
    QStringList L;
    QString sink = sinkDescription.isEmpty() ? QString("unknown") : sinkDescription;
    L << QString("Audio    %1, %2 Hz, %3-frame blocks (%4 ms)").arg(QString::fromStdString(audio.backend())).arg(audio.sampleRate()).arg(audio.quantum()).arg(1000.0 * audio.quantum() / audio.sampleRate(), 0, 'f', 1);
    L << QString("Output   %1%2").arg(sink.left(52)).arg(audio.streaming() ? "" : " (idle)");
    L << (sinkIsBluetooth ? QString("         Bluetooth: about 0.2 s behind your fingers") : QString());
    L << QString("Engine   load %1%, peak %2%, %3 late blocks, %4 voices").arg((int)(audio.load() * 100)).arg((int)(audio.peakLoad() * 100)).arg(audio.overruns()).arg(u.voices);
    L << QString("Files    %1").arg(store::baseDir());
    L << QString("Last WAV %1").arg(lastRecording.isEmpty() ? QString("-") : QFileInfo(lastRecording).fileName());
    L << QString("Keys     edit keymap.json in the folder above to remap");
    for (int i = 0; i < L.size(); ++i)
        txt(QRectF(32, 448 + i * 22, 656, 20), fit(L[i], 13, 656), 13, i == 2 ? MID : INK, Qt::AlignLeft | Qt::AlignVCenter);
    if (sinkIsDummy)
        tag(QRectF(32, 448 + 7 * 22 + 2, 560, 20), "No real audio device: connect headphones or a speaker.", 13, Qt::AlignLeft | Qt::AlignVCenter);

    button(c4(0, 642, 60), "panic", "Panic", false, [this] { perform(A_PANIC); }, "all notes off");
    stepper(c4(1, 642, 60), "latency", "Loop fix", QString("%1 ms").arg((int)u.set.latencyMs),
            [this] { set(S_LATENCY, u.set.latencyMs - 10); }, [this] { set(S_LATENCY, u.set.latencyMs + 10); });
    button(c4(2, 642, 60), "factory", "Factory sounds", false, [this] {
        for (int k = 0; k < 10; ++k) bank[k] = factoryPatch(k);
        toast("Sound slots 1-0 restored");
    }, "restore slots 1-0");
    button(c4(3, 642, 60), "quit", "Quit", false, [this] { close(); }, "saves your setup");
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
            {"Tab / Esc", "next page / back to the play page"},
            {"screen", "drag the scope = X-Y control, drag a string = slide"},
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
            {K(A_HELP, true) + " " + K(A_QUIT, true), "this guide / quit"},
        };
    }
    tag(QRectF(16, 64, 688, 22), helpPage == 0 ? QString("Playing") : QString("Commands: hold %1").arg(km.keyFor(A_COMMAND, false)), 14, Qt::AlignLeft | Qt::AlignVCenter);
    for (size_t i = 0; i < rows.size(); ++i) {
        qreal y = 96 + i * 33.5;
        QRectF kr(16, std::floor(y), 176, 28);
        frame(kr, LOW, BG);
        txt(kr, fit(rows[i].k, 13, 172), 13, INK, Qt::AlignCenter, true);
        txt(QRectF(204, std::floor(y), 500, 28), fit(rows[i].d, 13, 500), 13, i % 2 ? MID : INK, Qt::AlignLeft | Qt::AlignVCenter);
    }
    txt(QRectF(18, 638, 680, 20), "Q20: digits and arrows live on the keyboard's number / symbol layers.", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    button(QRectF(16, 664, 340, 44), "helppage", helpPage == 0 ? QString::fromUtf8("Command keys ›") : QString::fromUtf8("‹ Playing keys"), false, [this] { helpPage = 1 - helpPage; });
    button(QRectF(364, 664, 340, 44), "helpplay", "Back to playing", false, [this] { pg = PG_PLAY; });
}

void MainWindow::paintKeyTest() {
    QRectF r(40, 96, 640, 520);
    frame(r.adjusted(-3, -3, 3, 3), INK, BG);
    frame(r, INK, BG);
    tag(QRectF(r.x() + 1, r.y() + 1, 100, 22), "Key test", 14, Qt::AlignLeft | Qt::AlignVCenter);
    QStringList names;
    for (auto it = held.begin(); it != held.end(); ++it) names << keyName(it.key());
    names.sort();
    txt(QRectF(60, 150, 600, 22), fit(QString("Held now (%1): %2").arg(names.size()).arg(names.join(" ")), 14, 600), 14, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    tag(QRectF(60, 176, 300, 22), QString("Most keys held at once: %1").arg(maxHeld), 14, Qt::AlignLeft | Qt::AlignVCenter);
    for (int i = 0; i < keyLog.size(); ++i)
        txt(QRectF(60, 210 + i * 24, 600, 22), keyLog[i], 14, keyLog[i].startsWith("DOWN") ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter);
    g->setFont(font(13, false));
    g->setPen(MID);
    g->drawText(QRectF(60, 506, 600, 80), Qt::AlignLeft | Qt::TextWordWrap,
                "Hold one key on each row, then add more. Every key should show DOWN once and stay in Held until you let go. "
                "A key you did not press appearing here, or a pressed key missing, is a keyboard-matrix limit.");
    button(QRectF(516, 100, 160, 44), "keytestclose", "Close", false, [this] { keyTest = false; });
}
