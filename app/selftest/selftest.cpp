// venv-vrequency — built-in self-test. Drives the real window with real Qt key and
// pointer events while the real audio thread runs, then checks what the
// engine did. (It cannot press the physical keys or listen to the speaker:
// those two checks need a human.)
#include "audio/audio.h"
#include "app/control/control.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "core/presets/presets.h"
#include "core/presets/sound_shelf.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QScreen>
#include <QThread>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>

using namespace td;

void runTourTests(MainWindow &w, Instrument &inst, AudioOut &audio, const std::function<void(bool, const QString &)> &check); // tourtest.cpp

namespace {
int fails = 0, passes = 0;
void check(bool ok, const QString &what) {
    if (ok) ++passes; else ++fails;
    std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", what.toUtf8().constData());
    std::fflush(stdout);
}
void wait(int ms) {
    QElapsedTimer t; t.start();
    while (t.elapsed() < ms) { QCoreApplication::processEvents(QEventLoop::AllEvents, 5); QThread::msleep(2); }
}
void key(MainWindow &w, int k, bool down, bool repeat = false) {
    QKeyEvent e(down ? QEvent::KeyPress : QEvent::KeyRelease, k, Qt::NoModifier, QString(), repeat);
    QCoreApplication::sendEvent(&w, &e);
}
void tap(MainWindow &w, const QString &zone) {
    QRectF r = w.zoneRect(zone);
    w.pointer(7, r.center(), PH_PRESS);
    w.pointer(7, r.center(), PH_RELEASE);
}

// ---- the control socket: a real client in this process, talking over the real socket ----
struct Client {
    QLocalSocket s;
    QByteArray buf;
    bool open(const QString &path) { s.connectToServer(path); return s.waitForConnected(1000); }
    // the next line the server sent, or empty after `ms`
    QByteArray line(int ms) {
        QElapsedTimer t; t.start();
        while (t.elapsed() < ms) {
            const int nl = buf.indexOf('\n');
            if (nl >= 0) { QByteArray l = buf.left(nl); buf.remove(0, nl + 1); return l; }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
            buf += s.readAll();
            QThread::msleep(1);
        }
        return QByteArray();
    }
    void send(const QByteArray &raw) { s.write(raw + "\n"); s.flush(); }
    QJsonObject call(const QByteArray &raw, int ms = 3000) {
        send(raw);
        const QByteArray l = line(ms);
        return QJsonDocument::fromJson(l).object();
    }
};
// zero crossings of the output's tail: the pitch of a plain sine
float scopeHz(Instrument &inst) {
    static float x[8192];
    inst.readScope(x, 8192);
    int first = -1, last = -1, n = 0;
    for (int i = 1; i < 8192; ++i)
        if (x[i - 1] < 0 && x[i] >= 0) { if (first < 0) first = i; last = i; ++n; }
    return n < 3 ? 0.0f : (n - 1) * 48000.0f / (last - first);
}
} // namespace

int runSelfTest(MainWindow &w, Instrument &inst, AudioOut &audio, const QString &shotsDir) {
    std::printf("venv-vrequency self-test\n");
    inst.set(S_VOLUME, 0.06f); // keep it quiet: this plays through the real output
    wait(400);
    std::printf("- audio\n");
    check(audio.backend() != "none (silent)", QString("audio backend opened: %1").arg(QString::fromStdString(audio.backend())));
    uint64_t f0 = inst.uiState().frames; wait(500); uint64_t f1 = inst.uiState().frames;
    double rate = (f1 - f0) / 0.5;
    check(rate > 40000 && rate < 56000, QString("audio callback runs in real time (%1 frames/s)").arg((int)rate));

    std::printf("- layout\n");
    {
        QScreen *sc = w.screen();
        std::printf("  info  screen %dx%d logical, device pixel ratio %.2f, window %dx%d, platform %s\n", sc->geometry().width(), sc->geometry().height(),
                    sc->devicePixelRatio(), w.width(), w.height(), QGuiApplication::platformName().toUtf8().constData());
    }
    QImage img = w.shot();
    check(img.width() == 720 && img.height() == 720, "interface renders at 720x720");
    static const char *files[] = {"10-track", "0-play", "1-sound", "9-beat", "2-backing", "3-tune", "4-system", "5-help", "14-chat"};
    static const char *names[] = {"track", "synth", "sound", "beat", "backing", "tune", "system", "help", "chat"};
    if (!shotsDir.isEmpty()) QDir().mkpath(shotsDir);
    for (int p = 0; p < PG_COUNT; ++p) {
        w.setPage(p);
        wait(120);
        QImage im = w.shot();
        bool inside = true; int count = 0;
        for (const QString &z : w.zoneNames()) { // every control on the page, not a sample
            QRectF r = w.zoneRect(z);
            ++count;
            if (r.left() < 0 || r.top() < 0 || r.right() > 720 || r.bottom() > 720) inside = false;
        }
        check(inside && count > 5, QString("%1 page: all %2 controls fit inside 720x720 (no scrolling)").arg(names[p]).arg(count));
        if (!shotsDir.isEmpty()) im.save(QString("%1/%2.png").arg(shotsDir).arg(files[p]));
    }
    w.setPage(PG_PLAY);
    wait(100);
    {
        QElapsedTimer pt; pt.start();
        for (int i = 0; i < 40; ++i) w.shot();
        std::printf("  info  play page redraw: %.1f ms per frame\n", pt.elapsed() / 40.0);
    }
    { // a busy wave on the scope once cost 40 to 60 ms a frame: a whole processor core at 25 frames a second
        static const char *pn[] = {"Track", "Synth", "Sound", "Beat", "Backing", "Tune", "System", "Help", "Chat"};
        key(w, Qt::Key_Z, true); key(w, Qt::Key_S, true); key(w, Qt::Key_E, true); wait(300);
        double worst = 0; int worstPage = 0;
        for (int p = 0; p < PG_COUNT; ++p) {
            w.setPage(p); w.shot();
            QElapsedTimer t; t.start();
            for (int i = 0; i < 20; ++i) w.shot();
            const double ms = t.nsecsElapsed() / 20e6;
            if (ms > worst) { worst = ms; worstPage = p; }
        }
        key(w, Qt::Key_E, false); key(w, Qt::Key_S, false); key(w, Qt::Key_Z, false);
        inst.cmd(EV_PANIC); // no release tails left for the checks that follow
        w.setPage(PG_PLAY); wait(400);
        check(worst < 25.0, QString("with three notes sounding every page redraws well inside a 40 ms frame (slowest: %1, %2 ms)").arg(pn[worstPage]).arg(worst, 0, 'f', 1));
    }

    std::printf("- keyboard events -> notes\n");
    key(w, Qt::Key_Z, true); wait(120);
    UiState u = inst.uiState();
    check(u.str[0].gate && std::fabs(u.str[0].note - 45) < 0.01f, "Z sounds the low string's first note (A2)");
    int v0 = u.voices;
    for (int i = 0; i < 6; ++i) key(w, Qt::Key_Z, true, true); // keyboard auto-repeat
    wait(80);
    check(inst.uiState().voices == v0, "auto-repeat events do not retrigger the note");
    key(w, Qt::Key_C, true); wait(40);
    u = inst.uiState();
    check(u.str[0].target == 50 && u.str[0].note > 45.0f && u.str[0].note < 50.0f, QString("holding Z and pressing C slides (pitch in transit: %1)").arg(u.str[0].note));
    wait(300);
    u = inst.uiState();
    check(std::fabs(u.str[0].note - 50) < 0.01f && u.voices == v0, "slide arrives on D3 with the same single voice");
    key(w, Qt::Key_C, false); wait(400);
    u = inst.uiState();
    check(u.str[0].gate && std::fabs(u.str[0].note - 45) < 0.01f, "releasing C slides back to the held Z note");
    key(w, Qt::Key_A, true); key(w, Qt::Key_Q, true); wait(120);
    u = inst.uiState();
    check(u.str[0].gate && u.str[1].gate && u.str[2].gate, "three strings sound together");
    key(w, Qt::Key_Z, false); key(w, Qt::Key_A, false); key(w, Qt::Key_Q, false); wait(120);
    u = inst.uiState();
    check(!u.str[0].gate && !u.str[1].gate && !u.str[2].gate, "key releases stop the notes");
    key(w, Qt::Key_Shift, true); key(w, Qt::Key_X, true); wait(80);
    check(std::fabs(inst.uiState().str[0].target - 46) < 0.01f, "Shift = chromatic override (X gives A#2)");
    key(w, Qt::Key_X, false); key(w, Qt::Key_Shift, false);
    key(w, Qt::Key_Space, true); key(w, Qt::Key_S, true); wait(60); key(w, Qt::Key_S, false); wait(150);
    check(inst.uiState().str[1].gate, "Space sustains a released note");
    key(w, Qt::Key_Space, false); wait(100);
    check(!inst.uiState().str[1].gate, "releasing Space lets it go");
    key(w, Qt::Key_Up, true); key(w, Qt::Key_Up, false); wait(60);
    check(inst.uiState().set.tuning.octave == 1, "Up arrow raises the octave");
    key(w, Qt::Key_Down, true); key(w, Qt::Key_Down, false); wait(60);

    std::printf("- sounds\n");
    key(w, Qt::Key_2, true); key(w, Qt::Key_2, false); wait(80);
    check(QString(inst.uiState().leadName) == "Glass Bells", "key 2 selects sound 2");
    QString before = w.sounds.lead.name;
    key(w, Qt::Key_Control, true); key(w, Qt::Key_R, true); key(w, Qt::Key_R, false); wait(80);
    QString rnd = inst.uiState().leadName;
    check(rnd != before && !rnd.isEmpty(), QString("Ctrl+R generates a new sound (%1)").arg(rnd));
    QDir favDir(store::baseDir() + "/favourites");
    int favBefore = favDir.entryList({"*.json"}).size();
    check(favBefore == kExtraCount && favDir.exists("haze.json") && favDir.exists("grain.json"),
          QString("first start puts the %1 dream sounds in favourites").arg(kExtraCount));
    key(w, Qt::Key_F, true); key(w, Qt::Key_F, false); wait(60);
    check(favDir.entryList({"*.json"}).size() == favBefore + 1, "Ctrl+F saves it as a favourite file");
    key(w, Qt::Key_Z, true); key(w, Qt::Key_Z, false); wait(80);
    check(QString(inst.uiState().leadName) == before, "Ctrl+Z undoes back to the previous sound");
    key(w, Qt::Key_Control, false);
    w.setPage(PG_SOUND); wait(100);
    tap(w, "favitem0"); wait(80);
    check(QString(inst.uiState().leadName) == rnd, "favourite recalled from the sound page");
    tap(w, "favitem1"); wait(80);
    check(QString(inst.uiState().leadName) == "Haze", "the dream sounds follow in order: Haze is next");
    tap(w, "favitem5"); wait(80);
    check(QString(inst.uiState().leadName) == "Grain", "and Grain is last on the first page");
    check(w.sounds.addExtraSounds() == 0 && favDir.entryList({"*.json"}).size() == favBefore + 1, "they are not added twice");
    w.setPage(PG_PLAY); wait(100);
    key(w, Qt::Key_1, true); key(w, Qt::Key_1, false); wait(60);

    std::printf("- backing\n");
    inst.set(S_BPM, 240); wait(50);
    key(w, Qt::Key_Alt, true);
    for (int k : {Qt::Key_Z, Qt::Key_V, Qt::Key_B, Qt::Key_V}) { key(w, k, true); key(w, k, false); }
    key(w, Qt::Key_Alt, false);
    wait(150);
    u = inst.uiState();
    check(u.backPlaying && u.progLen == 4 && u.prog[1] == 3, "Alt + bottom row taps a four-chord progression");
    key(w, Qt::Key_3, true); key(w, Qt::Key_3, false);
    key(w, Qt::Key_W, true); wait(1200);
    u = inst.uiState();
    check(QString(u.backName) == "Warm Lead" && QString(u.leadName) == "Soft Pad" && u.str[2].gate && u.backPlaying, "backing keeps its sound while the lead changes and plays");
    check(u.progPos != 0 || u.beat > 4, "progression advances with the clock");
    key(w, Qt::Key_W, false);

    std::printf("- looper\n");
    key(w, Qt::Key_Return, true); key(w, Qt::Key_Return, false); wait(100);
    check(inst.uiState().loopState == L_COUNTIN, "Enter starts the count-in");
    for (int i = 0; i < 40 && inst.uiState().loopState != L_REC; ++i) wait(50);
    check(inst.uiState().loopState == L_REC, "recording starts on the bar");
    wait(150); key(w, Qt::Key_E, true); wait(250); key(w, Qt::Key_T, true); wait(200); key(w, Qt::Key_T, false); key(w, Qt::Key_E, false);
    wait(250);
    key(w, Qt::Key_Return, true); key(w, Qt::Key_Return, false);
    for (int i = 0; i < 40 && (inst.uiState().layers < 1); ++i) wait(50);
    u = inst.uiState();
    check(u.loopState == L_PLAY && u.layers == 1, QString("loop closes on the bar and plays (%1 bar)").arg(u.loopBars));
    key(w, Qt::Key_Return, true); key(w, Qt::Key_Return, false); wait(100);
    key(w, Qt::Key_H, true); wait(200); key(w, Qt::Key_H, false); wait(100);
    key(w, Qt::Key_Return, true); key(w, Qt::Key_Return, false); wait(100);
    check(inst.uiState().layers == 2, "overdub adds a second layer");
    key(w, Qt::Key_Control, true); key(w, Qt::Key_U, true); key(w, Qt::Key_U, false); wait(80);
    check(inst.uiState().layers == 1, "Ctrl+U undoes the overdub");
    key(w, Qt::Key_Control, false);

    std::printf("- touch\n");
    QRectF xy = w.zoneRect("xy");
    w.pointer(1, xy.center(), PH_PRESS); wait(60);
    w.pointer(1, QPointF(xy.left() + xy.width() * 0.9, xy.top() + xy.height() * 0.2), PH_MOVE); wait(80);
    u = inst.uiState();
    check(u.xyTouch && u.xyX > 0.85f && u.xyY > 0.75f, "dragging the pad moves the XY control");
    check(inst.part(0).modCut > 2.0f, "which modulates the filter live");
    // second finger plays a string while the first stays on the pad
    QRectF s1 = w.zoneRect("string1");
    w.pointer(2, QPointF(s1.left() + s1.width() * 0.3, s1.center().y()), PH_PRESS); wait(80);
    check(inst.uiState().str[1].gate, "a second touch plays a string at the same time");
    float n0 = inst.uiState().str[1].target;
    w.pointer(2, QPointF(s1.left() + s1.width() * 0.6, s1.center().y()), PH_MOVE); wait(80);
    check(inst.uiState().str[1].target > n0 && inst.uiState().str[1].gate, "dragging along the string slides the pitch");
    w.pointer(2, s1.center(), PH_RELEASE);
    w.pointer(1, xy.center(), PH_RELEASE); wait(80);
    check(!inst.uiState().xyTouch && std::fabs(inst.part(0).modCut) < 0.01f, "lifting the finger returns the sound to normal");
    inst.set(S_XYMAP, XY_GLITCH); wait(60);
    w.pointer(1, xy.center(), PH_PRESS); wait(80);
    check(inst.uiState().glitch, "the Glitch mapping crushes the sound while the pad is touched");
    w.pointer(1, xy.center(), PH_RELEASE); wait(80);
    check(!inst.uiState().glitch, "and stops when the finger lifts");
    inst.set(S_XYMAP, XY_FILTER); wait(60);
    tap(w, "chord4"); wait(60);
    check(inst.uiState().progLen == 5, "touch chord pads work");

    std::printf("- saving\n");
    QString wavPath = w.recorder.start();
    key(w, Qt::Key_D, true); wait(700); key(w, Qt::Key_D, false); wait(200);
    w.recorder.stop();
    QFile wf(wavPath);
    bool wavOk = wf.open(QIODevice::ReadOnly);
    QByteArray wd = wavOk ? wf.readAll() : QByteArray();
    int nz = 0;
    for (int i = 44; i + 1 < wd.size(); i += 2) if (wd[i] != 0 || wd[i + 1] != 0) ++nz;
    check(wavOk && wd.size() > 44 + 48000 * 2 && wd.startsWith("RIFF") && nz > 1000, QString("WAV export written with audio in it (%1 kB)").arg(wd.size() / 1024));
    QString sp = store::baseDir() + "/sessions/selftest.json";
    SessionData a; inst.snapshot(a);
    check(w.saveSession(sp), "session saved");
    check(a.layers.size() == 1 && !a.layers[0].ev.empty() && a.loopBeats >= 4, "the session holds the recorded loop");
    key(w, Qt::Key_Control, true);
    key(w, Qt::Key_X, true); key(w, Qt::Key_X, false);   // Ctrl+X clear backing
    key(w, Qt::Key_D, true); key(w, Qt::Key_D, false);   // Ctrl+D clear loop
    key(w, Qt::Key_Control, false);
    inst.set(S_ROOT, 3); wait(100);
    check(inst.uiState().progLen == 0 && inst.uiState().layers == 0, "backing and loop cleared");
    check(w.loadSession(sp), "session loaded");
    wait(100);
    SessionData b; inst.snapshot(b);
    bool same = b.settings.tuning.root == a.settings.tuning.root && b.progLen == a.progLen && b.layers.size() == a.layers.size() &&
                b.loopBeats == a.loopBeats && QString(b.lead.name) == QString(a.lead.name) && QString(b.backing.name) == QString(a.backing.name);
    if (same && !a.layers.empty()) same = a.layers[0].ev.size() == b.layers[0].ev.size();
    check(same, "loaded session matches what was saved (key, sounds, progression, loop)");
    inst.set(S_VOLUME, 0.06f);
    w.saveState();
    check(QFile::exists(store::baseDir() + "/state.json") && QFile::exists(store::baseDir() + "/keymap.json"), "state and keymap files written");

    std::printf("- beat\n");
    w.setPage(PG_BEAT); wait(100);
    inst.set(S_BEATVOL, 0.3f); inst.set(S_BPM, 120); wait(60);
    uint32_t k0 = inst.uiState().drumHits[D_KICK];
    tap(w, "pad0"); wait(80);
    check(inst.uiState().drumHits[D_KICK] == k0 + 1 && !inst.uiState().beatPlaying, "tapping the first pad plays the kick");
    key(w, Qt::Key_Q, true); key(w, Qt::Key_Q, false); wait(80);
    key(w, Qt::Key_U, true); key(w, Qt::Key_U, false); wait(80);
    check(inst.uiState().drumHits[D_KICK] == k0 + 3 && inst.uiState().voices == 0, "on the Beat page Q and U play the kick, not a string");
    uint32_t c0 = inst.uiState().drumHits[D_CLAP];
    key(w, Qt::Key_A, true); key(w, Qt::Key_A, false); wait(80);
    check(inst.uiState().drumHits[D_CLAP] == c0 + 1, "A plays the clap (pad 5)");
    key(w, Qt::Key_Return, true); key(w, Qt::Key_Return, false); wait(150);
    check(inst.uiState().beatRec && inst.uiState().beatPlaying && inst.uiState().loopState == L_STOPPED, "Enter switches Rec on and starts the beat (not the loop pedal)");
    key(w, Qt::Key_W, true); key(w, Qt::Key_W, false); wait(120);
    tap(w, "pad2"); wait(120);
    UiState bu = inst.uiState();
    check(bu.beatPat[D_SNARE] != 0 && bu.beatPat[D_HAT] != 0, "hits played while Rec is on are written into the bar");
    uint32_t s0 = bu.drumHits[D_SNARE];
    wait(2100);
    check(inst.uiState().drumHits[D_SNARE] > s0, "and the bar plays them back");
    tap(w, "beaterase"); wait(60); tap(w, "pad1"); wait(100);
    check(inst.uiState().beatPat[D_SNARE] == 0 && inst.uiState().beatPat[D_HAT] != 0, "Erase, then a pad, clears that drum only");
    int kit0 = inst.uiState().set.kit;
    tap(w, "kit+"); wait(80);
    check(inst.uiState().set.kit == (kit0 + 1) % kKits, "the kit stepper changes the kit");
    key(w, Qt::Key_Space, true); key(w, Qt::Key_Space, false); wait(100);
    check(!inst.uiState().beatPlaying && !inst.uiState().beatRec && !inst.uiState().sustain, "Space stops the beat on this page");
    w.setPage(PG_PLAY); wait(80);
    key(w, Qt::Key_Control, true); key(w, Qt::Key_J, true); key(w, Qt::Key_J, false); key(w, Qt::Key_Control, false); wait(100);
    check(inst.uiState().beatPlaying, "Ctrl+J starts the beat from the Synth page");
    key(w, Qt::Key_Control, true); key(w, Qt::Key_J, true); key(w, Qt::Key_J, false); key(w, Qt::Key_Control, false); wait(100);
    w.saveState();
    QJsonObject sj; store::readJson(store::baseDir() + "/state.json", sj);
    check(!inst.uiState().beatPlaying && sj["beat"].toArray().size() == kDrums && sj["beat"].toArray()[D_HAT].toString().contains('x'),
          "the beat is kept in the saved state");

    std::printf("- kept beats\n");
    {
        w.setPage(PG_BEAT); wait(80);
        auto press = [&](int k) { key(w, k, true); key(w, k, false); };
        const UiState b0 = inst.uiState();
        const uint16_t hat0 = b0.beatPat[D_HAT];
        const int kit0b = b0.set.kit, n0 = w.keptBeats();
        check(n0 >= 1 && w.keptBeatName(w.keptBeatShown()).startsWith("Beat"), QString("the beat you have is a kept beat (%1 of %2)").arg(w.keptBeatShown() + 1).arg(n0));
        tap(w, "beatnew"); wait(120);
        UiState bn = inst.uiState();
        bool empty = true;
        for (int d = 0; d < kDrums; ++d) empty = empty && bn.beatPat[d] == 0;
        check(w.keptBeats() == n0 + 1 && w.keptBeatShown() == n0 && empty, "+ New makes an empty beat and shows it; the others are kept");
        tap(w, "beatnew"); wait(80);
        check(w.keptBeats() == n0 + 1, "+ New on an empty beat makes no second empty one");
        tap(w, "pad0"); wait(40); // not recording: plays, writes nothing
        key(w, Qt::Key_Return, true); key(w, Qt::Key_Return, false); wait(120);
        tap(w, "pad5"); wait(120);
        key(w, Qt::Key_Space, true); key(w, Qt::Key_Space, false); wait(120);
        const uint16_t rim = inst.uiState().beatPat[D_RIM];
        check(rim != 0 && !inst.uiState().beatPlaying, "the new beat records hits of its own");
        press(Qt::Key_Left); wait(150);
        UiState bl = inst.uiState();
        check(w.keptBeatShown() == n0 - 1 && bl.beatPat[D_HAT] == hat0 && bl.beatPat[D_RIM] == 0 && bl.set.kit == kit0b,
              "Left goes back to the beat before, exactly as it was, kit included");
        press(Qt::Key_Right); wait(150);
        check(w.keptBeatShown() == n0 && inst.uiState().beatPat[D_RIM] == rim, "Right comes back to the new one, hits kept");
        // while playing, a switch waits for the bar line
        inst.set(S_BPM, 60); wait(40);
        key(w, Qt::Key_Space, true); key(w, Qt::Key_Space, false); wait(300);
        tap(w, QString("beatslot%1").arg(n0 - 1)); wait(150);
        UiState bw = inst.uiState();
        check(bw.beatSwitching && bw.beatPat[D_RIM] == rim && w.keptBeatShown() == n0 - 1, "playing, the beat tapped waits for the next bar");
        wait(4200); // a bar at 60 bpm is 4 s
        bw = inst.uiState();
        check(!bw.beatSwitching && bw.beatPat[D_HAT] == hat0 && bw.beatPat[D_RIM] == 0, "and takes over on the bar line");
        key(w, Qt::Key_Space, true); key(w, Qt::Key_Space, false); wait(80);
        inst.set(S_BPM, 120);
        for (int i = 0; i < 8; ++i) { tap(w, "beatnew"); wait(40); tap(w, "pad3"); key(w, Qt::Key_Return, true); key(w, Qt::Key_Return, false); wait(60); tap(w, "pad3"); wait(60); key(w, Qt::Key_Space, true); key(w, Qt::Key_Space, false); wait(60); }
        check(w.keptBeats() > 8 && w.zoneRect("beatmore").width() > 0, QString("with %1 beats the row has a More page").arg(w.keptBeats()));
        w.saveState(); wait(40);
        QJsonObject kj; store::readJson(store::baseDir() + "/state.json", kj);
        const QJsonArray ka = kj["beats"].toArray();
        check(ka.size() == w.keptBeats() && kj["beatShown"].toInt() == w.keptBeatShown()
              && ka[n0].toObject()["beat"].toArray()[D_RIM].toString().contains('x'), "every kept beat is in the state file");
        for (int i = 0; i < 12; ++i) { press(Qt::Key_Left); wait(30); } // back to the first beat
        wait(120);
        check(w.keptBeatShown() == 0 && inst.uiState().beatPat[D_HAT] == hat0, "Left all the way comes back to Beat 1");
    }

    std::printf("- track\n");
    {
        auto press = [&](int k) { key(w, k, true); key(w, k, false); };
        auto ctrl = [&](int k) { key(w, Qt::Key_Control, true); press(k); key(w, Qt::Key_Control, false); };
        auto T = [&]() -> const TrackData & { return w.trackData(); };
        w.setPage(PG_TRACK); wait(100);
        check(w.page() == PG_TRACK && PG_TRACK == 0 && PG_PLAY == 1, "Track is the first tab, Synth the second");
        check(T().clips.empty(), "the track starts empty");
        press(Qt::Key_Space); wait(80);
        check(!inst.uiState().trackPlaying, "Space on an empty track plays nothing");
        tap(w, "track_keep"); wait(80);
        check(T().clips.size() == 3 && T().clips[0]->kind == CK_BEAT && T().clips[1]->kind == CK_CHORDS && T().clips[2]->kind == CK_LOOP,
              "+ Keep copies the beat, the chords and the loop onto the shelf");
        check(w.clipInHand(), "and the newest clip is in hand, ready to place");
        wait(3200);
        QJsonObject aj; store::readJson(store::baseDir() + "/state.json", aj);
        check(aj["track"].toObject()["clips"].toArray().size() == 3, "a moment after an edit the track is in the state file without being asked");
        tap(w, "track_keep"); wait(60);
        check(T().clips.size() == 3, "keeping the same things again adds nothing");
        QRectF lanes = w.zoneRect("track_lanes");
        auto cell = [&](int lane, double bar) { return QPointF(lanes.left() + (bar + 0.5) * lanes.width() / 8.0, lanes.top() + 2 + lane * 70 + 33); };
        auto touch = [&](QPointF a) { w.pointer(3, a, PH_PRESS); w.pointer(3, a, PH_RELEASE); };
        if (!w.clipInHand()) press(Qt::Key_3);
        touch(cell(LN_LOOPA, 0)); wait(60);
        check(T().lane[LN_LOOPA].size() == 1 && T().lane[LN_LOOPA][0].bar == 0 && w.chosenLane() == LN_LOOPA, "with a clip in hand, tapping a bar places it there");
        press(Qt::Key_1);
        key(w, Qt::Key_Control, true); press(Qt::Key_V); press(Qt::Key_V); key(w, Qt::Key_Control, false); wait(60);
        check(T().lane[LN_BEAT].size() == 2 && T().lane[LN_BEAT][1].bar == 1 && w.playLineBar() == 2, "1 takes the first clip; Ctrl+V places it at the play line, twice");
        press(Qt::Key_Left);
        check(w.chosenLane() == LN_BEAT && w.chosenIndex() == 0, "Left chooses the clip before");
        ctrl(Qt::Key_J);
        check(T().lane[LN_BEAT].size() == 1 && T().lane[LN_BEAT][0].bars == 2, "Ctrl+J joins a clip with its repeat");
        ctrl(Qt::Key_R);
        check(T().lane[LN_BEAT].size() == 2 && T().lane[LN_BEAT][1].bar == 2 && w.chosenIndex() == 1, "Ctrl+R repeats it to the right");
        press(Qt::Key_Return);
        key(w, Qt::Key_Shift, true); press(Qt::Key_Right); key(w, Qt::Key_Shift, false);
        check(w.playLineBar() == 1, "Enter sends the play line to bar 1, Shift+Right moves it a bar");
        press(Qt::Key_Left);
        ctrl(Qt::Key_T);
        check(T().lane[LN_BEAT].size() == 3 && T().lane[LN_BEAT][0].bars == 1 && T().lane[LN_BEAT][1].bar == 1, "Ctrl+T splits the chosen clip at the play line");
        int depth = w.trackUndoDepth();
        ctrl(Qt::Key_Z);
        check(T().lane[LN_BEAT].size() == 2 && w.trackUndoDepth() == depth - 1, "Ctrl+Z undoes the split");
        key(w, Qt::Key_Control, true); key(w, Qt::Key_Shift, true); press(Qt::Key_Z); key(w, Qt::Key_Shift, false); key(w, Qt::Key_Control, false);
        check(T().lane[LN_BEAT].size() == 3, "Shift+Ctrl+Z does it again");
        ctrl(Qt::Key_Z);
        depth = w.trackUndoDepth();
        w.pointer(3, cell(LN_BEAT, 2), PH_PRESS);
        w.pointer(3, cell(LN_BEAT, 3.6), PH_MOVE);
        w.pointer(3, cell(LN_BEAT, 5), PH_MOVE);
        w.pointer(3, cell(LN_BEAT, 5), PH_RELEASE);
        check(T().lane[LN_BEAT].size() == 2 && T().lane[LN_BEAT][1].bar == 5 && w.trackUndoDepth() == depth + 1, "dragging a clip moves it by whole bars, as one undo step");
        press(Qt::Key_M);
        check(T().lane[LN_BEAT][1].mute, "M mutes the chosen clip");
        press(Qt::Key_M);
        key(w, Qt::Key_Alt, true); press(Qt::Key_Right); key(w, Qt::Key_Alt, false);
        check(!T().lane[LN_BEAT][1].mute && T().lane[LN_BEAT][1].bar == 6, "Alt+Right moves it a bar");
        press(Qt::Key_2);
        tap(w, "track_place"); wait(60);
        check(T().lane[LN_CHORDS].size() == 1 && T().lane[LN_CHORDS][0].bar == 1 && T().lane[LN_CHORDS][0].bars == T().clips[1]->bars, "Place puts the chords clip on the Chords lane at the play line");
        press(Qt::Key_Escape);
        check(w.page() == PG_TRACK && !w.clipInHand(), "Esc puts the clip in hand down (and stays on the page)");

        touch(cell(LN_LOOPA, 0));
        press(Qt::Key_E); wait(60);
        check(w.chopping() && w.chosenLane() == LN_LOOPA, "E opens Chop for the chosen clip");
        const double Lc = T().clips[2]->beats();
        tap(w, "chop_halves"); wait(40);
        check(T().lane[LN_LOOPA][0].pieces.size() == 2, "Halves cuts it in two");
        tap(w, "chop_move_right"); wait(40);
        check(T().lane[LN_LOOPA][0].pieces[0].src == Lc / 2 && T().lane[LN_LOOPA][0].pieces[1].src == 0, "Move swaps the halves");
        tap(w, "chop_mute_piece"); wait(40);
        check(T().lane[LN_LOOPA][0].pieces[1].mute, "Mute piece silences the chosen piece");
        tap(w, "chop_reset"); wait(40);
        check(T().lane[LN_LOOPA][0].pieces.size() == 1 && !T().lane[LN_LOOPA][0].pieces[0].mute, "Reset undoes every cut");
        QRectF strip = w.zoneRect("chop_strip");
        const QPointF quarter(strip.left() + strip.width() / 4 + 3, strip.center().y());
        touch(quarter); wait(40);
        check(T().lane[LN_LOOPA][0].pieces.size() == 2 && T().lane[LN_LOOPA][0].pieces[0].len == Lc / 4, "tapping a dotted line cuts there");
        touch(quarter); wait(40);
        check(T().lane[LN_LOOPA][0].pieces.size() == 1, "tapping the cut mends it");
        tap(w, "chop_done"); wait(40);
        check(!w.chopping(), "Done closes Chop");

        press(Qt::Key_Up); press(Qt::Key_Up);
        check(w.chosenLane() == LN_BEAT && w.chosenIndex() == 0, "Up chooses a clip on the lane above");
        press(Qt::Key_Right);
        check(w.chosenLane() == LN_BEAT && w.chosenIndex() == 1, "Right chooses the clip after");
        press(Qt::Key_Down);
        check(w.chosenLane() == LN_CHORDS, "Down chooses a clip on the lane below");
        press(Qt::Key_Up);
        const int line0 = w.playLineBar();
        press(Qt::Key_Period);
        const int line1 = w.playLineBar();
        press(Qt::Key_Comma);
        key(w, Qt::Key_Shift, true); press(Qt::Key_Left); key(w, Qt::Key_Shift, false);
        check(line1 == line0 + 1 && w.playLineBar() == line0 - 1, ". and , (and Shift+Left) move the play line");
        press(Qt::Key_Period);
        ctrl(Qt::Key_C);
        check(w.clipInHand() && T().lane[LN_BEAT].size() == 2, "Ctrl+C takes a copy of the chosen clip in hand");
        ctrl(Qt::Key_V);
        check(T().lane[LN_BEAT].size() == 3 && T().lane[LN_BEAT][2].bar == 8 && T().bars == 10, "Ctrl+V puts it on the first free bars after the line, and the track grows to fit");
        ctrl(Qt::Key_X);
        check(T().lane[LN_BEAT].size() == 2 && w.clipInHand(), "Ctrl+X takes the chosen clip off the track and into your hand");
        press(Qt::Key_Escape);
        ctrl(Qt::Key_Z); ctrl(Qt::Key_Z);
        check(T().lane[LN_BEAT].size() == 2 && T().bars == 8, "two undos take the paste back, and the track's length with it");
        press(Qt::Key_K);
        check(T().clips.size() == 3 && w.lastToast().startsWith("Already kept"), "K is Keep (nothing new to keep this time)");

        press(Qt::Key_Return); w.shot(); // the paste scrolled the bars: back to bar 1
        touch(cell(LN_LOOPA, 0));
        press(Qt::Key_E);
        ctrl(Qt::Key_T);
        check(w.chopping() && T().lane[LN_LOOPA][0].pieces.size() == 2, "in Chop, Ctrl+T cuts the chosen piece in half");
        press(Qt::Key_Right); press(Qt::Key_M);
        check(T().lane[LN_LOOPA][0].pieces[1].mute && !T().lane[LN_LOOPA][0].pieces[0].mute, "Right chooses the next piece, M mutes it");
        key(w, Qt::Key_Alt, true); press(Qt::Key_Left); key(w, Qt::Key_Alt, false);
        check(T().lane[LN_LOOPA][0].pieces[0].mute && T().lane[LN_LOOPA][0].pieces[0].src > 0, "Alt+Left moves the piece earlier");
        press(Qt::Key_M);
        key(w, Qt::Key_Alt, true); press(Qt::Key_Right); key(w, Qt::Key_Alt, false);
        press(Qt::Key_Left);
        ctrl(Qt::Key_J);
        check(T().lane[LN_LOOPA][0].pieces.size() == 1, "Ctrl+J mends the piece with the one after it");
        ctrl(Qt::Key_T); ctrl(Qt::Key_R);
        check(T().lane[LN_LOOPA][0].pieces.size() == 2 && T().lane[LN_LOOPA][0].pieces[1].src == 0, "Ctrl+R makes the next piece repeat it");
        ctrl(Qt::Key_Z); ctrl(Qt::Key_Z);
        press(Qt::Key_E);
        check(!w.chopping() && T().lane[LN_LOOPA][0].pieces.size() == 1, "E closes Chop again");
        touch(cell(LN_LOOPA, 0));

        press(Qt::Key_C);
        check(T().cycleTo > T().cycleFrom && T().cycleFrom == 0, "C cycles round the chosen clip");
        press(Qt::Key_C);
        check(T().cycleTo == 0, "C again switches the cycle off");

        press(Qt::Key_Return);
        uint32_t h0 = inst.uiState().drumHits[D_HAT];
        press(Qt::Key_Space); wait(200);
        UiState tu = inst.uiState();
        check(tu.trackPlaying && !tu.sustain, "Space plays the track");
        bool voiced = false, hat = false;
        for (int i = 0; i < 50 && !(voiced && hat); ++i) { wait(50); tu = inst.uiState(); voiced = voiced || tu.voices > 0; hat = hat || tu.drumHits[D_HAT] > h0; }
        check(voiced && hat, "the loop clip's notes and the beat clip's hats sound");
        const double p0 = inst.uiState().trackPos; wait(200);
        check(inst.uiState().trackPos > p0, "the play line moves");
        key(w, Qt::Key_Q, true); wait(80);
        check(!inst.uiState().str[2].gate, "on the Track page letters are commands: Q plays no string");
        key(w, Qt::Key_Q, false);
        w.setPage(PG_PLAY); wait(60);
        key(w, Qt::Key_Q, true); wait(80);
        check(inst.uiState().str[2].gate && inst.uiState().trackPlaying, "on the Synth page you play over the track");
        key(w, Qt::Key_Q, false);
        ctrl(Qt::Key_Space); wait(120);
        check(!inst.uiState().trackPlaying, "Ctrl+Space stops the track from any page");
        w.setPage(PG_TRACK); wait(60);

        tap(w, "track_export");
        check(w.exportStatus() == 1, "Export starts writing in the background");
        QElapsedTimer et; et.start();
        while (w.exportStatus() != 0 && et.elapsed() < 60000) { wait(100); w.shot(); }
        QFile ef(w.recorder.lastPath());
        const bool eok = ef.open(QIODevice::ReadOnly);
        const QByteArray ed = eok ? ef.readAll() : QByteArray();
        int enz = 0;
        for (int i = 44; i + 1 < ed.size(); i += 2) if (ed[i] != 0 || ed[i + 1] != 0) ++enz;
        const double esec = (ed.size() - 44) / 4.0 / audio.sampleRate(), want = T().bars * 4 * 60.0 / inst.uiState().set.bpm;
        std::printf("  info  export: %d bars, %.1f s of audio, written in %.1f s\n", T().bars, esec, et.elapsed() / 1000.0);
        check(eok && w.recorder.lastPath().contains("/track-") && ed.startsWith("RIFF") && enz > 1000, "the track is exported as a WAV file with audio in it");
        check(esec >= want && esec < want + 9, QString("it lasts the track plus the dying echoes (%1 s for a %2 s track)").arg(esec, 0, 'f', 1).arg(want, 0, 'f', 1));

        touch(cell(LN_BEAT, 0));
        press(Qt::Key_Backspace); wait(60);
        check(T().lane[LN_BEAT].size() == 1, "Backspace deletes the chosen clip");
        press(Qt::Key_1);
        tap(w, "track_remove"); wait(40);
        check(T().clips.size() == 3, "Remove on a shelf clip asks twice");
        tap(w, "track_remove"); wait(40);
        check(T().clips.size() == 2 && T().lane[LN_BEAT].empty() && T().lane[LN_CHORDS][0].clip == 0 && T().lane[LN_LOOPA][0].clip == 1,
              "the second tap deletes the clip and every copy of it; the others keep pointing at the right clips");
        ctrl(Qt::Key_Z);
        check(T().clips.size() == 3 && T().lane[LN_BEAT].size() == 1, "and that can be undone too");

        // the sound picker
        press(Qt::Key_Return); w.shot();
        touch(cell(LN_LOOPA, 0)); w.shot();
        const int lci = T().lane[LN_LOOPA][0].clip;
        auto lp = [&]() -> const Placement & { return T().lane[LN_LOOPA][0]; };
        auto lc = [&]() -> const Clip & { return *T().clips[lci]; };
        press(Qt::Key_I); w.shot();
        check(w.pickingSound() && w.soundKindShown() == MainWindow::PK_SYNTH, "I opens the sound picker on the kind the clip plays now");
        press(Qt::Key_Right); w.shot();
        check(w.soundKindShown() == MainWindow::PK_GUITAR, "right arrow moves to Guitar");
        const int undo0 = w.trackUndoDepth();
        press(Qt::Key_1); w.shot();
        check(lc().pick.on && QString(lc().pick.patch.name) == "Pluck" && !lp().pick.any() && lc().layers[0].patches[0].name != QString("Pluck"),
              "1 picks Pluck for every placement of the clip; the recorded sound stays inside it");
        check(w.trackUndoDepth() == undo0 + 1, "and that is one undo step");
        check(w.hearingAlone(), "picking a sound while nothing plays starts the clip alone");
        bool hv = false;
        for (int i = 0; i < 60 && !hv; ++i) { wait(50); w.shot(); hv = inst.uiState().trackPlaying && inst.uiState().voices > 0; }
        const uint32_t hh = inst.uiState().drumHits[D_HAT];
        wait(700);
        check(hv && w.hearingAlone() && inst.uiState().drumHits[D_HAT] == hh, "it is heard, and the beat lane is silent meanwhile");
        press(Qt::Key_Right); press(Qt::Key_Right); w.shot();
        check(w.soundKindShown() == MainWindow::PK_BASS, "right again is Bass, and it stops there");
        press(Qt::Key_1); w.shot();
        check(QString(lc().pick.patch.name) == "Sub" && lc().pick.octave == -1, "a bass sound puts the clip an octave down");
        press(Qt::Key_Up); w.shot();
        check(lc().pick.octave == 0, "up arrow lifts it an octave");
        tap(w, "sound_octave-"); tap(w, "sound_octave-"); tap(w, "sound_octave-"); w.shot();
        check(lc().pick.octave == -2, "the Octave stepper stops two octaves down");
        tap(w, "sound_octave+"); tap(w, "sound_octave+"); w.shot();
        tap(w, "sound_scope"); w.shot();
        check(lp().pick.own && QString(lp().pick.patch.name) == "Sub" && QString(lc().pick.patch.name) == "Sub", "Only this one: the placement takes the sound as its own");
        tap(w, "sound_kind0"); w.shot(); tap(w, "sound_cell2"); w.shot();
        check(QString(lp().pick.patch.name) == "Organ" && QString(lc().pick.patch.name) == "Sub", "now a pick changes this placement and leaves the clip's other copies alone");
        press(Qt::Key_Backspace); w.shot();
        check(lp().pick.own && !lp().pick.any() && QString(soundName(lc(), pickFor(lc(), lp()))) == QString(recordedSoundName(lc())),
              "Backspace: this one plays as recorded again, the others keep Sub");
        tap(w, "sound_kind2"); w.shot(); tap(w, "sound_cell0"); w.shot();
        check(w.hearingAlone() && inst.uiState().trackPlaying, "all the while the clip keeps going round alone");
        press(Qt::Key_Space); wait(200); w.shot();
        check(!w.hearingAlone() && !inst.uiState().trackPlaying, "Space stops it");
        press(Qt::Key_Return); w.shot();
        check(!w.pickingSound(), "Enter closes the picker");

        touch(cell(LN_BEAT, T().lane[LN_BEAT][0].bar)); w.shot();
        const int bci = T().lane[LN_BEAT][0].clip;
        tap(w, "track_sound"); w.shot();
        check(w.pickingSound() && w.soundKindShown() == MainWindow::PK_DRUMS, "the Sound button on a beat clip offers the kits");
        const int otherKit = (T().clips[bci]->kit + 1) % kKits;
        press(Qt::Key_1 + otherKit); w.shot();
        check(T().clips[bci]->pick.kit == otherKit && !T().clips[bci]->pick.on, "a number picks another kit for the beat");
        press(Qt::Key_Up); press(Qt::Key_Right); w.shot();
        check(T().clips[bci]->pick.octave == 0 && w.soundKindShown() == MainWindow::PK_DRUMS, "a beat has no octave and no other kinds");
        tap(w, "sound_as_recorded"); w.shot();
        check(!T().clips[bci]->pick.any(), "As recorded gives the beat its own kit back");
        press(Qt::Key_1 + otherKit); w.shot();
        tap(w, "sound_done"); wait(250); w.shot();
        check(!w.pickingSound() && !w.hearingAlone() && !inst.uiState().trackPlaying, "Done closes the picker and stops the clip");
        ctrl(Qt::Key_Z);
        check(!T().clips[bci]->pick.any(), "undo takes the kit back");
        key(w, Qt::Key_Shift, true); ctrl(Qt::Key_Z); key(w, Qt::Key_Shift, false);
        check(T().clips[bci]->pick.kit == otherKit, "and redo picks it again");

        std::printf("- lane mixer\n");
        press(Qt::Key_X); w.shot();
        check(w.mixing() && !w.pickingSound() && !w.chopping(), "X opens the mixer in place of the tools");
        press(Qt::Key_2); press(Qt::Key_Down); w.shot();
        check(w.mixLaneShown() == LN_CHORDS && T().mix[LN_CHORDS].level == 75, "2 chooses Chords and Down lowers it to 75");
        press(Qt::Key_Right); press(Qt::Key_Up); press(Qt::Key_Up); w.shot();
        check(w.mixLaneShown() == LN_LOOPA && T().mix[LN_LOOPA].level == 90, "Right moves to Loop A and Up twice raises it to 90");
        press(Qt::Key_0);
        check(T().mix[LN_LOOPA].level == kLaneLevelNormal, "0 puts it back to 80");
        tap(w, "mixer_mute3"); w.shot();
        check(T().mix[LN_LOOPB].mute && !laneHeard(T(), LN_LOOPB) && w.mixLaneShown() == LN_LOOPB, "tapping M on Loop B mutes it");
        ctrl(Qt::Key_Z);
        check(!T().mix[LN_LOOPB].mute, "undo unmutes it");
        key(w, Qt::Key_Shift, true); ctrl(Qt::Key_Z); key(w, Qt::Key_Shift, false);
        check(T().mix[LN_LOOPB].mute, "and redo mutes it again");
        press(Qt::Key_1); press(Qt::Key_S); w.shot();
        check(T().mix[LN_BEAT].solo && !laneHeard(T(), LN_CHORDS) && laneHeard(T(), LN_BEAT), "1 then S solos the beat: the other lanes go quiet");
        press(Qt::Key_S);
        {
            QRectF fr = w.zoneRect("mixer_fader1");
            const int mixBeforeDrag = T().mix[LN_CHORDS].level;
            w.pointer(3, QPointF(fr.center().x(), fr.bottom() - 30), PH_PRESS);
            for (int i = 1; i <= 8; ++i) w.pointer(3, QPointF(fr.center().x(), fr.bottom() - 30 - i * 6), PH_MOVE);
            w.pointer(3, QPointF(fr.center().x(), fr.bottom() - 78), PH_RELEASE);
            w.shot();
            const int lv = T().mix[LN_CHORDS].level, before = mixBeforeDrag;
            check(lv > 50 && lv < 75 && w.mixLaneShown() == LN_CHORDS, QString("dragging the Chords fader sets its level (%1)").arg(lv));
            ctrl(Qt::Key_Z);
            const int undone = T().mix[LN_CHORDS].level;
            key(w, Qt::Key_Shift, true); ctrl(Qt::Key_Z); key(w, Qt::Key_Shift, false);
            check(undone == before && T().mix[LN_CHORDS].level == lv, QString("the whole drag is one undo step (%1 back to %2)").arg(lv).arg(undone));
        }
        tap(w, "mixer_reset"); w.shot();
        bool flat = true;
        for (int ln = 0; ln < kLanes; ++ln) flat = flat && sameMix(T().mix[ln], LaneMix());
        check(flat, "Reset puts every lane back to 80, unmuted");
        ctrl(Qt::Key_Z);
        check(T().mix[LN_LOOPB].mute && T().mix[LN_CHORDS].level != kLaneLevelNormal, "and undo brings the mix back");
        press(Qt::Key_Escape); w.shot();
        check(!w.mixing(), "Esc closes the mixer");
        if (w.chosenLane() < 0) press(Qt::Key_Right);                                  // choose a clip
        press(Qt::Key_X); press(Qt::Key_I); w.shot();
        check(w.chosenLane() >= 0 && w.pickingSound() && !w.mixing(), "opening the sound picker closes the mixer");
        press(Qt::Key_Return); w.shot();

        w.saveState();
        QJsonObject tj; store::readJson(store::baseDir() + "/state.json", tj);
        TrackData rt; store::trackFromJson(tj["track"].toObject(), rt);
        bool rsame = rt.clips.size() == T().clips.size() && rt.bars == T().bars;
        for (int ln = 0; rsame && ln < kLanes; ++ln) {
            rsame = rt.lane[ln].size() == T().lane[ln].size();
            for (size_t i = 0; rsame && i < rt.lane[ln].size(); ++i)
                rsame = rt.lane[ln][i].bar == T().lane[ln][i].bar && rt.lane[ln][i].bars == T().lane[ln][i].bars && rt.lane[ln][i].clip == T().lane[ln][i].clip
                        && samePick(rt.lane[ln][i].pick, T().lane[ln][i].pick);
        }
        for (size_t i = 0; rsame && i < rt.clips.size(); ++i) rsame = sameClip(*rt.clips[i], *T().clips[i]) && samePick(rt.clips[i]->pick, T().clips[i]->pick);
        for (int ln = 0; rsame && ln < kLanes; ++ln) rsame = sameMix(rt.mix[ln], T().mix[ln]);
        check(rsame, "the track read back from the saved state is the same track, picked sounds and the mix included");
        check(rt.lane[LN_LOOPA][0].pick.own && QString(rt.lane[LN_LOOPA][0].pick.patch.name) == "Pluck" && QString(rt.clips[lci]->pick.patch.name) == "Sub"
              && rt.clips[bci]->pick.kit == otherKit, "a placement's own sound, a clip's sound and a beat's kit all come back");
        QString sp2 = store::baseDir() + "/sessions/selftest-track.json";
        check(w.saveSession(sp2), "a session saves the track with it");
        const int bars0 = T().bars;
        tap(w, "track_length+"); wait(40);
        check(T().bars == bars0 + 1, "the Length stepper makes the track longer");
        check(w.loadSession(sp2) && T().bars == bars0 && T().clips.size() == 3, "loading the session brings the track back");
        inst.set(S_VOLUME, 0.06f);
        w.setPage(PG_PLAY); wait(60);
    }

    std::printf("- listening back\n");
    check(w.recorder.togglePlayback() && w.recorder.playingBack(), "Play last starts playing the newest recording");
    wait(300);
    check(w.recorder.togglePlayback(), "and the same button stops it");
    wait(300);
    check(!w.recorder.playingBack(), "playback has stopped");

    std::printf("- control socket and chat\n");
    {
        QTemporaryDir cdir;
        const QString sock = cdir.path() + "/vrequency.sock";
        auto press = [&](int k) { key(w, k, true); key(w, k, false); };
        auto B = [](const char *s) { return QByteArray(s); };
        auto okOf = [](const QJsonObject &o) { return o["ok"].toBool(); };

        { // a leftover file from a crashed run does not block start-up
            QFile stale(sock);
            stale.open(QIODevice::WriteOnly); stale.write("not a socket"); stale.close();
            ControlServer probe(w, inst);
            check(probe.start(sock) && probe.running(), "a stale socket file is replaced at start-up");
        }
        ControlServer ctl(w, inst);
        check(ctl.start(sock) && QFile::exists(sock), "the control socket starts");
        check((QFile::permissions(sock) & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther | QFileDevice::ExeGroup)) == 0,
              "and is private to the user (no group or other access)");
        {
            ControlServer second(w, inst);
            check(!second.start(sock), "a second app does not take over a live socket");
        }
        Client c, c2;
        check(c.open(sock) && c2.open(sock), "two clients connect at once");
        QJsonObject r = c.call(B(R"({"id":"abc","cmd":"state"})"));
        check(okOf(r) && r["id"].toString() == "abc", "replies carry the request id back");
        check(r["result"].toObject().contains("sound") && r["result"].toObject().contains("backing") && r["result"].toObject().contains("track") &&
              r["result"].toObject().contains("chat_tail") && r["result"].toObject()["sound"].toObject()["params"].toObject().contains("cutoff"),
              "state holds the sound, parameters, backing, track and chat");

        // a plain sine, no effects, so the pitch can be measured
        for (const char *p : {R"({"cmd":"set_param","name":"wave1","value":0})", R"({"cmd":"set_param","name":"mix2","value":0})",
                              R"({"cmd":"set_param","name":"unison","value":0})", R"({"cmd":"set_param","name":"sub","value":0})",
                              R"({"cmd":"set_param","name":"fm","value":0})", R"({"cmd":"set_param","name":"vibdepth","value":0})",
                              R"({"cmd":"set_param","name":"delaymix","value":0})", R"({"cmd":"set_param","name":"reverbmix","value":0})",
                              R"({"cmd":"set_param","name":"chorus","value":0})", R"({"cmd":"set_param","name":"cutoff","value":16000})",
                              R"({"cmd":"set_param","name":"fenv","value":0})", R"({"cmd":"set_param","name":"sustain","value":1})"})
            c.call(B(p));
        wait(100);
        inst.set(S_VOLUME, 0.3f);
        r = c.call(B(R"({"cmd":"note","string":0,"degree":0,"dur_ms":1500})"));
        wait(450);
        float hz = scopeHz(inst);
        check(okOf(r) && std::fabs(hz - 110.0f) < 2.0f && inst.uiState().str[0].gate, QString("a note over the socket: string 0 degree 0 is A2 (measured %1 Hz)").arg(hz, 0, 'f', 1));
        key(w, Qt::Key_X, true); wait(60); // the player's own key on the same string slides it: both interleave
        check(inst.uiState().str[0].target == 48, "the player's keys still play over a socket note");
        key(w, Qt::Key_X, false);
        c.call(B(R"({"cmd":"release"})")); wait(300);
        r = c.call(B(R"({"cmd":"note","string":1,"degree":3,"dur_ms":1500})"));
        wait(450);
        hz = scopeHz(inst);
        const float want = (float)midiToHz(inst.uiState().set.tuning.noteFor(1, 3, false));
        check(okOf(r) && std::fabs(hz - want) < want * 0.02f, QString("degree 3 of the middle string follows the scale (%1 Hz, wanted %2)").arg(hz, 0, 'f', 1).arg(want, 0, 'f', 1));
        c.call(B(R"({"cmd":"panic"})")); wait(250);
        r = c.call(B(R"({"cmd":"note","string":2,"midi":81,"dur_ms":1500})"));
        wait(450);
        hz = scopeHz(inst);
        check(okOf(r) && std::fabs(hz - 880.0f) < 12.0f, QString("an exact MIDI note: 81 is 880 Hz (measured %1 Hz)").arg(hz, 0, 'f', 1));
        r = c.call(B(R"({"cmd":"note","string":2,"degree":-2,"dur_ms":200})"));
        check(okOf(r), "a negative scale degree is accepted");
        c.call(B(R"({"cmd":"panic"})")); wait(250);
        check(inst.uiState().voices == 0, "panic over the socket silences everything");
        r = c.call(B(R"({"cmd":"note","string":0,"degree":0,"dur_ms":150})"));
        wait(120);
        check(inst.uiState().str[0].gate, "a note sounds for its duration...");
        wait(500);
        check(!inst.uiState().str[0].gate, "...and lets go by itself");
        c.call(B(R"({"cmd":"slide","string":0,"from":0,"to":3,"ms":900})"));
        wait(200);
        const float fromNote = inst.uiState().str[0].note;
        wait(500);
        check(inst.uiState().str[0].note > fromNote + 0.5f || std::fabs(inst.uiState().str[0].target - fromNote) > 0.5f, "a slide moves the string's pitch");
        wait(700);
        c.call(B(R"({"cmd":"panic"})"));

        // every command: a request that works, then one that must be refused
        struct Case { const char *req; bool ok; int wait; };
        const Case cases[] = {
            {R"({"cmd":"note","string":1,"degree":0,"dur_ms":50,"velocity":0.5})", true, 0},
            {R"({"cmd":"note","string":3,"degree":0})", false, 0},
            {R"({"cmd":"note","string":"low","degree":0})", false, 0},
            {R"({"cmd":"note","string":0})", false, 0},
            {R"({"cmd":"note","string":0,"degree":1,"midi":60})", false, 0},
            {R"({"cmd":"note","string":0,"midi":300})", false, 0},
            {R"({"cmd":"note","string":0,"degree":0,"dur_ms":99999999})", false, 0},
            {R"({"cmd":"chord","degrees":[0,2,4],"dur_ms":100})", true, 0},
            {R"({"cmd":"chord","degrees":[0,2,4,6]})", false, 0},
            {R"({"cmd":"notes","notes":[{"string":0,"degree":0},{"string":2,"midi":72}],"dur_ms":100})", true, 0},
            {R"({"cmd":"notes","notes":[{"string":9,"degree":0}]})", false, 0},
            {R"({"cmd":"notes","notes":[]})", false, 0},
            {R"({"cmd":"slide","string":0,"from":0,"to":2,"ms":200})", true, 300},
            {R"({"cmd":"slide","string":0,"from":0})", false, 0},
            {R"({"cmd":"slide","string":0,"from":"a","to":2})", false, 0},
            {R"({"cmd":"release","string":0})", true, 0},
            {R"({"cmd":"release","string":7})", false, 0},
            {R"({"cmd":"panic"})", true, 100},
            {R"({"cmd":"get_params"})", true, 0},
            {R"({"cmd":"set_param","name":"cutoff","value":99999})", true, 0},
            {R"({"cmd":"set_param","name":"nonsense","value":1})", false, 0},
            {R"({"cmd":"set_param","name":"cutoff","value":"high"})", false, 0},
            {R"({"cmd":"set_param","name":"cutoff"})", false, 0},
            {R"({"cmd":"sound","slot":3})", true, 80},
            {R"({"cmd":"sound","slot":11})", false, 0},
            {R"({"cmd":"sound","name":"mutate"})", true, 0},
            {R"({"cmd":"sound","name":"undo"})", true, 0},
            {R"({"cmd":"sound","name":"random"})", true, 0},
            {R"({"cmd":"sound","name":"Haze"})", true, 80},
            {R"({"cmd":"sound","name":"no such sound"})", false, 0},
            {R"({"cmd":"sound"})", false, 0},
            {R"({"cmd":"save_favourite","name":"Copilot test"})", true, 0},
            {R"({"cmd":"save_favourite"})", false, 0},
            {R"({"cmd":"scale","name":"major","root":"C"})", true, 80},
            {R"({"cmd":"scale","name":"minor pentatonic","root":"A"})", true, 80},
            {R"({"cmd":"scale","name":"polka"})", false, 0},
            {R"({"cmd":"scale","root":"H"})", false, 0},
            {R"({"cmd":"tempo","bpm":120})", true, 60},
            {R"({"cmd":"tempo","bpm":10})", false, 0},
            {R"({"cmd":"tempo"})", false, 0},
            {R"({"cmd":"octave","delta":1})", true, 80},
            {R"({"cmd":"octave","value":0})", true, 80},
            {R"({"cmd":"octave","value":9})", false, 0},
            {R"({"cmd":"bend","value":0.5,"ms":100})", true, 0},
            {R"({"cmd":"bend","value":3})", false, 0},
            {R"({"cmd":"sustain","on":true})", true, 0},
            {R"({"cmd":"sustain","on":false})", true, 0},
            {R"({"cmd":"sustain","on":"yes"})", false, 0},
            {R"({"cmd":"loop","action":"clear"})", true, 120},
            {R"({"cmd":"loop","action":"stop"})", false, 0},
            {R"({"cmd":"loop","action":"play"})", false, 0},
            {R"({"cmd":"loop","action":"clear"})", true, 0},
            {R"({"cmd":"loop","action":"undo"})", true, 0},
            {R"({"cmd":"loop","action":"record"})", true, 100},
            {R"({"cmd":"loop","action":"dance"})", false, 0},
            {R"({"cmd":"loop","action":"clear"})", true, 100},
            {R"({"cmd":"backing","chords":["I","vi",4,5],"style":"pulse"})", true, 150},
            {R"({"cmd":"backing","chords":[9]})", false, 0},
            {R"({"cmd":"backing","chords":"I"})", false, 0},
            {R"({"cmd":"backing","action":"stop"})", true, 100},
            {R"({"cmd":"backing","action":"play"})", true, 100},
            {R"({"cmd":"backing","action":"remove_last"})", true, 0},
            {R"({"cmd":"backing"})", false, 0},
            {R"({"cmd":"beat","kit":"crunchy"})", true, 0},
            {R"({"cmd":"beat","kit":"disco"})", false, 0},
            {R"({"cmd":"beat","set_step":{"drum":"kick","step":0,"on":true}})", true, 0},
            {R"({"cmd":"beat","set_step":[{"drum":"snare","step":4},{"drum":2,"step":2}]})", true, 100},
            {R"({"cmd":"beat","set_step":{"drum":"kick","step":16}})", false, 0},
            {R"({"cmd":"beat","set_step":{"drum":"banjo","step":1}})", false, 0},
            {R"({"cmd":"beat","hit":{"drum":"clap"}})", true, 0},
            {R"({"cmd":"beat","hit":"nope"})", false, 0},
            {R"({"cmd":"beat","action":"play"})", true, 100},
            {R"({"cmd":"beat","action":"stop"})", true, 100},
            {R"({"cmd":"beat"})", false, 0},
            {R"({"cmd":"track","action":"clear"})", true, 60},
            {R"({"cmd":"track","action":"play"})", false, 0},
            {R"({"cmd":"track","action":"export"})", false, 0},
            {R"({"cmd":"track","action":"place","clip":0,"bar":0})", false, 0},
            {R"({"cmd":"track","action":"keep"})", true, 100},
            {R"({"cmd":"track","action":"place","clip":0,"bar":0})", true, 60},
            {R"({"cmd":"track","action":"place","clip":0,"bar":0})", false, 0},
            {R"({"cmd":"track","action":"place","clip":0,"bar":99})", false, 0},
            {R"({"cmd":"track","action":"play"})", true, 150},
            {R"({"cmd":"track","action":"stop"})", true, 100},
            {R"({"cmd":"track","action":"export"})", true, 0},
            {R"({"cmd":"track","action":"rewind"})", false, 0},
            {R"({"cmd":"page","name":"beat"})", true, 0},
            {R"({"cmd":"page","name":"chat"})", true, 0},
            {R"({"cmd":"page","name":"synth"})", true, 0},
            {R"({"cmd":"page","name":"nowhere"})", false, 0},
            {R"({"cmd":"session","action":"save","name":"copilot-test"})", true, 0},
            {R"({"cmd":"session","action":"load","name":"copilot-test"})", true, 100},
            {R"({"cmd":"session","action":"load","name":"missing"})", false, 0},
            {R"({"cmd":"session","action":"save","name":"../evil"})", false, 0},
            {R"({"cmd":"screenshot"})", true, 0},
            {R"({"cmd":"chat_say","text":"hello from the test"})", true, 0},
            {R"({"cmd":"chat_say","text":""})", false, 0},
            {R"({"cmd":"chat_say","text":"x","kind":"shout"})", false, 0},
            {R"({"cmd":"chat_say"})", false, 0},
            {R"({"cmd":"chat_poll","since":0})", true, 0},
            {R"({"cmd":"chat_poll","since":"zero"})", false, 0},
            {R"({"cmd":"chat_wait","since":0,"timeout_ms":99999})", false, 0},
            {R"({"cmd":"frobnicate"})", false, 0},
            {R"({"cmd":42})", false, 0},
            {R"({"id":7})", false, 0},
        };
        int bad = 0;
        QString firstBad;
        QString shotPath, exportPath;
        for (const Case &cs : cases) {
            const QJsonObject rr = c.call(B(cs.req));
            const bool got = rr.contains("ok") && rr["ok"].toBool();
            const bool answered = rr.contains("ok") && (got || !rr["error"].toString().isEmpty());
            if (got != cs.ok || !answered) { ++bad; if (firstBad.isEmpty()) firstBad = QString("%1 -> %2").arg(cs.req, QString::fromUtf8(QJsonDocument(rr).toJson(QJsonDocument::Compact))); }
            if (QByteArray(cs.req).contains("screenshot")) shotPath = rr["result"].toObject()["path"].toString();
            if (QByteArray(cs.req).contains("\"export\"") && got) exportPath = rr["result"].toObject()["path"].toString();
            if (cs.wait) wait(cs.wait);
        }
        check(bad == 0, QString("%1 requests: every command works with good arguments and refuses bad ones%2").arg((int)(sizeof cases / sizeof cases[0])).arg(bad ? " (first: " + firstBad + ")" : QString()));
        c.call(B(R"({"cmd":"panic"})"));
        c.call(B(R"({"cmd":"loop","action":"clear"})"));
        {
            const QImage png(shotPath);
            check(!shotPath.isEmpty() && png.width() == 720 && png.height() == 720, "screenshot writes a 720x720 PNG");
            QFile::remove(shotPath);
        }
        {
            for (int i = 0; i < 100 && w.exportStatus() == 1; ++i) wait(100);
            wait(150);
            QFile ef(exportPath);
            check(!exportPath.isEmpty() && ef.exists() && ef.size() > 44 + 48000, "track export (async) writes a WAV and gives its path");
        }
        r = c.call(B(R"({"cmd":"state"})"));
        {
            const QJsonObject st = r["result"].toObject();
            check(st["track"].toObject()["clips"].toArray().size() >= 2 && st["backing"].toObject()["chords"].toArray().size() >= 3 &&
                  st["beat"].toObject()["steps"].toObject()["kick"].toString().startsWith('x') && st["tempo"].toDouble() == 120 && st["page"].toString() == "synth",
                  "state reflects what the commands did (track clips, backing chords, beat steps, tempo, page)");
        }
        inst.set(S_BPM, 100);
        inst.beatDo(BT_ERASE, -1);
        inst.cmd(EV_BACK, BC_CLEAR);

        // ---- chat ----
        w.setPage(PG_PLAY); wait(100);
        const int before = w.chat.seq;
        r = c.call(B(R"({"cmd":"chat_say","from":"rook","text":"Try a minor third up on the high string.","kind":"tip"})"));
        wait(100);
        check(okOf(r) && w.chat.seq == before + 1 && w.chat.unread >= 1, "chat_say adds an agent message and counts it as unread");
        check(w.chat.tickerShowing(), "on another page the message shows in the ticker");
        const QImage withTicker = w.shot();
        w.setPage(PG_CHAT); wait(100);
        const QImage chatShot = w.shot();
        check(w.chat.unread == 0 && w.chat.log.back().text.startsWith("Try a minor third") && chatShot != withTicker, "the Chat page shows it and clears the unread count");
        r = c.call(B(R"({"cmd":"state"})"));
        check(r["result"].toObject()["chat_tail"].toArray().last().toObject()["text"].toString().startsWith("Try a minor third"), "and state's chat tail holds it");
        r = c2.call(B(R"({"cmd":"chat_poll","since":0})"));
        r = c.call(B(R"({"cmd":"state"})"));
        check(r["result"].toObject()["chat"].toObject()["listening"].toBool(), "state says the copilot is listening after a poll");

        // a waiting copilot is woken by the player's message, typed on the keyboard
        const int seq0 = w.chat.seq;
        c2.send(QByteArray(R"({"id":"w1","cmd":"chat_wait","since":)") + QByteArray::number(seq0) + R"(,"timeout_ms":8000})");
        wait(100);
        check(c2.line(50).isEmpty(), "chat_wait holds its reply while nobody has typed");
        r = c.call(B(R"({"cmd":"state"})"));
        check(okOf(r), "and other clients are answered meanwhile");
        const int voices0 = inst.uiState().voices;
        for (int k : {Qt::Key_H, Qt::Key_I, Qt::Key_Space, Qt::Key_O, Qt::Key_K, Qt::Key_Z}) press(k);
        check(w.chat.draft == "hi okz" && inst.uiState().voices == voices0 && inst.uiState().voices == 0, "on the Chat page letters type and no string plays");
        press(Qt::Key_Backspace);
        check(w.chat.draft == "hi ok" && inst.uiState().droppedEvents == 0, "Backspace edits the line instead of silencing");
        QElapsedTimer wt; wt.start();
        press(Qt::Key_Return);
        const QByteArray got = c2.line(2000);
        const QJsonObject wr = QJsonDocument::fromJson(got).object();
        const QJsonArray wm = wr["result"].toObject()["messages"].toArray();
        check(wr["id"].toString() == "w1" && wr["ok"].toBool() && !wm.isEmpty() && wm.last().toObject()["text"].toString() == "hi ok" &&
              wm.last().toObject()["user"].toBool() && wm.last().toObject()["seq"].toInt() == seq0 + 1 && !wr["result"].toObject()["timed_out"].toBool() && wt.elapsed() < 1500,
              QString("Enter sends it: the waiting chat_wait is answered at once (%1 ms) with the message").arg(wt.elapsed()));
        check(w.chat.draft.isEmpty(), "the input line empties after sending");
        r = c.call(B((QByteArray(R"({"cmd":"chat_poll","since":)") + QByteArray::number(seq0 - 1) + "}").constData()));
        check(r["result"].toObject()["messages"].toArray().size() == 2, "chat_poll returns messages from both directions");
        // touch: type, then tap Send
        for (int k : {Qt::Key_Y, Qt::Key_E, Qt::Key_S}) press(k);
        tap(w, "chat_send"); wait(60);
        check(w.chat.log.back().text == "yes" && w.chat.log.back().user, "the Send button sends too");
        // timeout answer
        c2.send(QByteArray(R"({"id":"w2","cmd":"chat_wait","since":)") + QByteArray::number(w.chat.seq) + R"(,"timeout_ms":300})");
        const QJsonObject to = QJsonDocument::fromJson(c2.line(2000)).object();
        check(to["ok"].toBool() && to["result"].toObject()["messages"].toArray().isEmpty() && to["result"].toObject()["timed_out"].toBool(), "chat_wait times out with an empty list");
        // an already-waiting message answers immediately
        r = c2.call(B((QByteArray(R"({"cmd":"chat_wait","since":)") + QByteArray::number(seq0 - 1) + R"(,"timeout_ms":2000})").constData()), 500);
        check(okOf(r) && !r["result"].toObject()["messages"].toArray().isEmpty(), "chat_wait answers at once when a player message is already waiting");
        // the hotkey and Esc
        key(w, Qt::Key_Escape, true); key(w, Qt::Key_Escape, false);
        check(w.page() != PG_CHAT, "Esc leaves the Chat page");
        w.setPage(PG_BEAT);
        key(w, Qt::Key_Control, true); key(w, Qt::Key_Shift, true); press(Qt::Key_Y); key(w, Qt::Key_Shift, false); key(w, Qt::Key_Control, false);
        check(w.page() == PG_CHAT, "Ctrl+Shift+Y jumps to the Chat page");
        key(w, Qt::Key_Control, true); press(Qt::Key_Y); key(w, Qt::Key_Control, false);
        check(w.page() == PG_BEAT, "and Ctrl+Y again goes back to where you were");
        // history survives a restart
        w.saveState();
        QJsonObject sj; store::readJson(store::baseDir() + "/state.json", sj);
        bool kept = false;
        for (const QJsonValue &v : sj["chat"].toArray()) kept = kept || v.toObject()["text"].toString() == "hi ok";
        check(kept && sj["chatSeq"].toInt() == w.chat.seq, "the chat history is in the state file");
        // more than 200 messages: only the newest are kept
        for (int i = 0; i < 230; ++i) w.chat.add("rook", QString("filler %1").arg(i), "say", false);
        check((int)w.chat.log.size() == 200 && w.chat.log.back().text == "filler 229", "only the last 200 messages are kept");
        w.setPage(PG_CHAT);
        QImage full; for (int i = 0; i < 3; ++i) full = w.shot();
        check(full.width() == 720, "a full history draws");
        w.setPage(PG_PLAY);

        // ---- hostile input ----
        c.send(B("this is not json"));
        QJsonObject h = QJsonDocument::fromJson(c.line(1000)).object();
        check(!h["ok"].toBool(true) && h["error"].toString().startsWith("bad JSON"), "garbage is answered with an error");
        c.send(B("[1,2,3]"));
        h = QJsonDocument::fromJson(c.line(1000)).object();
        check(!h["ok"].toBool(true), "a JSON array is refused");
        c.send(B("{\"cmd\":\"note\",\"string\":0,\"degree\":0,\"dur_ms\":" + QByteArray(40000, '9') + "}"));
        h = QJsonDocument::fromJson(c.line(2000)).object();
        check(h.contains("ok") && !h["ok"].toBool(), "a number thousands of digits long is refused");
        c.send(QByteArray(R"({"cmd":"chat_say","text":")") + QByteArray(2 * 1024 * 1024, 'a') + "\"}");
        h = QJsonDocument::fromJson(c.line(3000)).object();
        check(!h["ok"].toBool(true), "a request line over a megabyte is refused");
        r = c.call(B(R"({"cmd":"state"})"));
        check(okOf(r), "and the connection still works after it");
        c.send(B("{\"cmd\":\"chat_say\",\"text\":" + QByteArray(2000, '[') + "}"));
        h = QJsonDocument::fromJson(c.line(2000)).object();
        check(h.contains("ok") && !h["ok"].toBool(), "deeply nested JSON is refused");
        c.send(B("{\"cmd\":\"note\",\"string\":0,\"degree\":0}\n{\"cmd\":\"state\"}\n{bad\n{\"cmd\":\"state\"}"));
        int answers = 0;
        for (int i = 0; i < 4; ++i) if (!c.line(500).isEmpty()) ++answers;
        check(answers == 4, "several requests in one write are answered one by one");
        c.s.write(B("{\"cmd\":\"state\"")); c.s.flush(); // half a line, then the client goes away
        c.s.disconnectFromServer();
        wait(100);
        check(c.open(sock) && okOf(c.call(B(R"({"cmd":"state"})"))), "a client that vanishes mid-line does not disturb the server");
        // every command with every argument set to the wrong kind of value
        static const char *cmds[] = {"note", "notes", "chord", "slide", "release", "set_param", "sound", "save_favourite", "scale", "tempo", "octave", "bend", "sustain",
                                     "loop", "backing", "beat", "track", "page", "session", "chat_say", "chat_poll", "highlight", "unhighlight", "zones"};
        static const char *argk[] = {"string", "degree", "midi", "dur_ms", "velocity", "from", "to", "ms", "name", "value", "slot", "action", "chords", "kit", "set_step", "hit",
                                     "clip", "bar", "lane", "text", "since", "notes", "degrees", "midis", "bpm", "root", "on", "delta", "style", "seventh", "beats", "kind", "limit", "zones", "label", "sub", "page"};
        static const char *argv[] = {"null", "\"x\"", "1e99", "-1e99", "[]", "{}", "true", "[[1]]", "{\"a\":1}", "0.5", "-1"};
        int refused = 0, total = 0, malformed = 0;
        for (const char *cm : cmds)
            for (const char *k : argk)
                for (const char *v : argv) {
                    const QByteArray req = QByteArray("{\"cmd\":\"") + cm + "\",\"" + k + "\":" + v + "}";
                    const QJsonObject rr = ctl.handle(QJsonDocument::fromJson(req).object());
                    ++total;
                    if (!rr.contains("ok")) ++malformed;
                    else if (!rr["ok"].toBool()) ++refused;
                }
        c.call(B(R"({"cmd":"panic"})"));
        c.call(B(R"({"cmd":"loop","action":"clear"})"));
        wait(150);
        check(malformed == 0 && refused > total / 2, QString("%1 requests with one wrongly typed or out of range argument: every one answered, none crashed (%2 refused)").arg(total).arg(refused));
        r = c.call(B(R"({"cmd":"state"})"));
        check(okOf(r) && inst.uiState().droppedEvents == 0, "the app is still fine, no events dropped");
        { // the output never exceeds full scale even with the abuse above
            static float x[8192];
            inst.readScope(x, 8192);
            float pk = 0; for (float v : x) pk = std::max(pk, std::fabs(v));
            check(pk <= 1.0f, "output stays inside full scale");
        }
        c.call(B(R"({"cmd":"panic"})"));
        ctl.stop();
        check(!QFile::exists(sock), "the socket file is removed when the server stops");
        w.setPage(PG_PLAY);
        inst.set(S_VOLUME, 0.06f);
        wait(300);
    }

    std::printf("- guided help: highlights, zones and the chat dock\n");
    {
        QTemporaryDir gdir;
        const QString gsock = gdir.path() + "/vrequency.sock";
        ControlServer gctl(w, inst);
        check(gctl.start(gsock), "a control socket for the guided-help checks");
        Client c, c2;
        check(c.open(gsock) && c2.open(gsock), "two clients connect");
        auto B = [](const char *s) { return QByteArray(s); };
        auto okOf = [](const QJsonObject &o) { return o["ok"].toBool(); };
        auto res = [](const QJsonObject &o) { return o["result"].toObject(); };
        auto strs = [](const QJsonArray &a) { QStringList l; for (const QJsonValue &v : a) l << v.toString(); return l; };
        auto press = [&](int k) { key(w, k, true); key(w, k, false); };
        auto ctrl = [&](int k) { key(w, Qt::Key_Control, true); press(k); key(w, Qt::Key_Control, false); };
        auto state = [&] { return res(c.call(B(R"({"cmd":"state"})"))); };
        auto hf = [&] { return state()["help_focus"].toObject(); };
        inst.cmd(EV_PANIC);
        w.guide.dockClose();
        w.guide.clearHighlight();
        w.setPage(PG_PLAY); wait(150); w.shot();

        { // zones: every page, and the listing agrees with what is drawn
            bool allSame = true; int total = 0; QStringList bad;
            static const char *pn[] = {"track", "synth", "sound", "beat", "backing", "tune", "system", "help", "chat"};
            for (int p = 0; p < PG_COUNT; ++p) {
                w.setPage(p); w.shot();
                QSet<QString> drawn; for (const QString &n : w.zoneNames()) if (n != "ticker") drawn.insert(n);
                const QJsonObject r = c.call(QByteArray(R"({"cmd":"zones","page":")") + pn[p] + "\"}");
                QSet<QString> listed; bool shaped = okOf(r);
                for (const QJsonValue &v : res(r)["zones"].toArray()) {
                    const QJsonObject z = v.toObject();
                    if (z["name"].toString() != "ticker") listed.insert(z["name"].toString());
                    shaped = shaped && z["w"].toInt() > 0 && z["h"].toInt() > 0 && !z["label"].toString().isEmpty() && z["label"].toString().size() <= 60;
                }
                total += listed.size();
                if (!shaped || listed != drawn) { allSame = false; bad << pn[p]; }
            }
            check(allSame && total > 150, QString("zones lists every named zone of every page, with a rectangle and a label (%1 in all%2)").arg(total).arg(bad.isEmpty() ? QString() : " - differs on " + bad.join(",")));
            w.setPage(PG_PLAY); w.shot();
            // another page is listed without leaving this one
            const QJsonObject r = c.call(B(R"({"cmd":"zones","page":"beat"})"));
            check(okOf(r) && w.page() == PG_PLAY && w.zoneNames().contains("octave_up"), "listing another page does not move the player off the page they are on");
            QStringList names; QHash<QString, QString> labels;
            for (const QJsonValue &v : res(r)["zones"].toArray()) { names << v.toObject()["name"].toString(); labels[v.toObject()["name"].toString()] = v.toObject()["label"].toString(); }
            check(names.contains("pad0") && names.contains("beatrec") && labels["beatrec"].contains("Rec"), "labels come from the text drawn on the control (Rec)");
            QStringList synth = strs(QJsonArray::fromStringList(w.zoneNames()));
            check(synth.contains("octave_down") && synth.contains("octave_up") && synth.contains("loop_pedal") && synth.contains("xy") && synth.contains("octave_readout") && synth.contains("hdr_tune"),
                  "the Synth page names its octave buttons, loop pedal, pad and tabs readably");
            check(!c.call(B(R"({"cmd":"zones","page":"nowhere"})"))["ok"].toBool(true) && !c.call(B(R"({"cmd":"zones","page":"help","sub":"x"})"))["ok"].toBool(true), "zones refuses an unknown page or help section");
            // Help: every row is a zone, all four sections stay reachable
            int rowsPer[4]; bool unique = true; QSet<QString> seen;
            for (int sub = 0; sub < 4; ++sub) {
                const QJsonObject hr = c.call(QByteArray(R"({"cmd":"zones","page":"help","sub":)") + QByteArray::number(sub) + "}");
                int n = 0;
                for (const QJsonValue &v : res(hr)["zones"].toArray()) {
                    const QString nm = v.toObject()["name"].toString();
                    if (nm.startsWith("help_row_")) { ++n; unique = unique && !seen.contains(nm + QString::number(sub)); seen.insert(nm + QString::number(sub)); }
                }
                rowsPer[sub] = n;
            }
            check(rowsPer[0] == 16 && rowsPer[1] == 16 && rowsPer[2] == 14 && rowsPer[3] == 16 && unique, QString("every help row on all four sections is a named zone (%1/%2/%3/%4)").arg(rowsPer[0]).arg(rowsPer[1]).arg(rowsPer[2]).arg(rowsPer[3]));
            w.setPage(PG_HELP); w.shot();
            QStringList hz = w.zoneNames();
            check(hz.contains("help_ask") && hz.contains("help_next") && hz.contains("help_back") && hz.contains("help_section"), "the Help page has Ask Rook, next section, back and its section title as zones");
            w.setPage(PG_PLAY); w.shot();
        }

        { // highlight: lifecycle
            QJsonObject r = c.call(B(R"({"cmd":"highlight","zones":["octave"],"label":"OCTAVE","ms":700})"));
            check(okOf(r) && strs(res(r)["highlighted"].toArray()) == QStringList({"octave_down", "octave_up"}) && res(r)["unknown"].toArray().isEmpty() && res(r)["caption"].toBool(),
                  "highlight accepts an alias (octave) and reports the zones it ringed, with a caption");
            const QJsonObject st = hf();
            check(st["page"].toString() == "synth" && strs(st["highlighted"].toArray()).size() == 2, "state.help_focus lists the highlighted zones and the page");
            const QRectF up = w.zoneRect("octave_up"), cap = w.guide.highlightCaptionRect();
            check(!cap.isNull() && !cap.intersects(up) && !cap.intersects(w.zoneRect("octave_down")) && QRectF(0, 0, 720, 720).contains(cap), "the caption tag stays on screen and does not cover the highlighted zones");
            QImage a = w.shot();
            bool ring = false;
            for (int dx = 3; dx <= 3; ++dx) ring = ring || qGray(a.pixel((int)up.center().x(), (int)up.top() - dx)) > 170;
            check(ring, "a bright rule is drawn around the highlighted zone");
            wait(900);
            check(hf()["highlighted"].toArray().isEmpty() && w.guide.highlightCaptionRect().isNull(), "a highlight clears itself when its time is up");
            r = c.call(B(R"({"cmd":"highlight","zones":["octave_up","no_such_zone","  "],"ms":5000})"));
            check(okOf(r) && strs(res(r)["highlighted"].toArray()) == QStringList({"octave_up"}) && strs(res(r)["unknown"].toArray()).contains("no_such_zone"),
                  "unknown names are reported in the reply and the known ones still highlight");
            r = c.call(B(R"({"cmd":"highlight","zones":["nope"]})"));
            check(okOf(r) && res(r)["highlighted"].toArray().isEmpty() && strs(res(r)["unknown"].toArray()) == QStringList({"nope"}), "all-unknown names highlight nothing and say so");
            r = c.call(B(R"({"cmd":"highlight","zones":["chord*"]})"));
            check(okOf(r) && res(r)["highlighted"].toArray().size() == 7, "a name ending in * stands for every zone with that prefix (chord*)");
            w.setPage(PG_SOUND); wait(60); w.shot();
            check(hf()["highlighted"].toArray().isEmpty(), "changing page clears the highlight");
            w.setPage(PG_PLAY); w.shot();
            check(hf()["highlighted"].toArray().isEmpty(), "and it does not come back when the page does");
            r = c.call(B(R"({"cmd":"highlight","zones":["help_ask"],"page":"help","label":"ask me","ms":3000})"));
            check(okOf(r) && w.page() == PG_HELP && strs(res(r)["highlighted"].toArray()) == QStringList({"help_ask"}) && strs(hf()["highlighted"].toArray()) == QStringList({"help_ask"}),
                  "highlight with page switches page and highlights in the same step");
            r = c.call(B(R"({"cmd":"highlight","zones":["help_row_loop_pedal_count_in_record_play_over"],"page":"help","sub":"playing"})"));
            const QStringList got = strs(res(r)["highlighted"].toArray());
            check(okOf(r) && (!got.isEmpty() || !res(r)["unknown"].toArray().isEmpty()), "highlight takes a help section (sub) with the page");
            r = c.call(B(R"({"cmd":"unhighlight"})"));
            check(okOf(r) && hf()["highlighted"].toArray().isEmpty() && res(r)["cleared"].toInt() >= 0, "unhighlight clears at once");
            check(okOf(c.call(B(R"({"cmd":"unhighlight"})"))), "and is harmless when there is nothing to clear");
            r = c.call(B(R"({"cmd":"highlight","zones":["octave_up"],"page":"synth","ms":9000})"));
            check(w.page() == PG_PLAY && !hf()["highlighted"].toArray().isEmpty(), "a highlight can be re-issued after a page change");
            c.call(B(R"({"cmd":"unhighlight"})"));
        }

        { // hostile arguments
            int refused = 0, total = 0, malformed = 0;
            static const char *zv[] = {"null", "\"x\"", "[]", "{}", "[1]", "[null]", "[[\"a\"]]", "true", "1e99", "[\"a\",\"b\"]", "[\"\"]"};
            static const char *lv[] = {"null", "1", "[]", "{}", "true", "\"ok\"", "\"line\\nbreak\\u0000x\""};
            static const char *mv[] = {"null", "-5", "0", "99", "1e99", "\"x\"", "250", "9000", "30000", "30001", "0.5"};
            static const char *pv[] = {"null", "\"x\"", "1", "\"help\"", "\"\"", "[]", "\"CHAT\"", "\"play\""};
            for (const char *z : zv) for (const char *l : lv) for (const char *m : mv) for (const char *p : pv) {
                const QByteArray req = QByteArray("{\"cmd\":\"highlight\",\"zones\":") + z + ",\"label\":" + l + ",\"ms\":" + m + ",\"page\":" + p + "}";
                const QJsonObject rr = gctl.handle(QJsonDocument::fromJson(req).object());
                ++total;
                if (!rr.contains("ok")) ++malformed; else if (!rr["ok"].toBool()) ++refused;
            }
            w.setPage(PG_PLAY); w.guide.clearHighlight();
            for (const char *p : pv) for (const char *s : {"null", "\"beat\"", "7", "[]", "\"x\""}) {
                const QByteArray req = QByteArray("{\"cmd\":\"zones\",\"page\":") + p + ",\"sub\":" + s + "}";
                const QJsonObject rr = gctl.handle(QJsonDocument::fromJson(req).object());
                ++total;
                if (!rr.contains("ok")) ++malformed; else if (!rr["ok"].toBool()) ++refused;
            }
            w.setPage(PG_PLAY); w.guide.clearHighlight(); w.shot();
            check(malformed == 0 && refused > total / 3, QString("%1 highlight and zones requests with wrongly typed arguments: all answered, none crashed (%2 refused)").arg(total).arg(refused));
            QJsonArray many; for (int i = 0; i < 40; ++i) many.append(QString("z%1").arg(i));
            QJsonObject big; big["cmd"] = "highlight"; big["zones"] = many;
            check(!gctl.handle(big)["ok"].toBool(true), "more than 16 names is refused");
            QJsonObject longName; longName["cmd"] = "highlight"; longName["zones"] = QJsonArray{QString(500, 'a')};
            check(!gctl.handle(longName)["ok"].toBool(true), "a name longer than 48 characters is refused");
            QJsonObject longLabel; longLabel["cmd"] = "highlight"; longLabel["zones"] = QJsonArray{"octave_up"}; longLabel["label"] = QString(41, 'a');
            check(!gctl.handle(longLabel)["ok"].toBool(true), "a label longer than 40 characters is refused");
            QJsonObject ctl; ctl["cmd"] = "highlight"; ctl["zones"] = QJsonArray{"octave_up"}; ctl["label"] = "a\nb\x01 c"; ctl["ms"] = 200;
            check(gctl.handle(ctl)["ok"].toBool() && !w.guide.highlightLabel().contains('\n') && !w.guide.highlightLabel().contains(QChar(1)), "control characters in a label are turned into spaces");
            wait(300);
            w.guide.clearHighlight();
        }

        { // the dock: opening, focus, Esc, close
            w.setPage(PG_PLAY); w.shot();
            check(!w.guide.dockIsOpen() && !w.guide.dockHasFocus(), "the dock starts closed");
            ctrl(Qt::Key_Y); w.shot();
            check(w.guide.dockIsOpen() && w.guide.dockHasFocus() && w.page() == PG_PLAY, "Ctrl+Y opens the dock with the keyboard, without leaving the page");
            const QRectF d = w.guide.dockRect();
            check(d.height() >= 120 && d.height() <= 150 && d.bottom() > 700 && w.zoneNames().contains("dock_input") && w.zoneNames().contains("dock_send"), QString("the dock is %1 px tall at the bottom, with an input and Send").arg(d.height()));
            const QJsonObject st = hf();
            check(st["dock_open"].toBool() && st["dock_focus"].toBool(), "state.help_focus reports the dock open and focused");
            // letters type, they do not play
            press(Qt::Key_Z); press(Qt::Key_Q); press(Qt::Key_A); wait(120);
            const UiState us = inst.uiState();
            check(w.chat.draft == "zqa" && !us.str[0].gate && !us.str[1].gate && !us.str[2].gate, "letters type into the dock and do not play the strings");
            // Enter sends with the same numbering, and wakes a waiting agent
            const int seq0 = w.chat.seq;
            c2.send(QByteArray(R"({"id":"w","cmd":"chat_wait","since":)") + QByteArray::number(seq0) + R"(,"timeout_ms":3000})");
            wait(50);
            w.guide.dockHasFocus();
            press(Qt::Key_Return); wait(50);
            QJsonObject wr = QJsonDocument::fromJson(c2.line(1500)).object();
            const QJsonArray wm = res(wr)["messages"].toArray();
            check(okOf(wr) && !res(wr)["timed_out"].toBool(true) && wm.size() == 1 && wm[0].toObject()["text"].toString() == "zqa" && wm[0].toObject()["seq"].toInt() == seq0 + 1,
                  "Enter in the dock sends through the same path: the next seq, and a waiting chat_wait wakes");
            check(wm[0].toObject()["page"].toString() == "synth" && wm[0].toObject().contains("focus") && wm[0].toObject()["via"].toString() == "dock", "the message carries page and focus metadata");
            check(w.chat.draft.isEmpty() && w.guide.dockHasFocus(), "the input clears and keeps the keyboard");
            // Esc: the keys play again, the dock stays
            press(Qt::Key_Escape);
            check(w.guide.dockIsOpen() && !w.guide.dockHasFocus() && w.page() == PG_PLAY, "Esc moves the keyboard back to the page and keeps the dock");
            key(w, Qt::Key_Z, true); wait(120);
            check(inst.uiState().str[0].gate && w.chat.draft.isEmpty(), "and the keys play again");
            key(w, Qt::Key_Z, false); wait(100);
            press(Qt::Key_Escape); w.shot();
            check(!w.guide.dockIsOpen() && w.page() == PG_PLAY, "Esc again closes the dock");
            press(Qt::Key_Escape);
            check(w.page() == PG_PLAY && !w.guide.dockIsOpen(), "and Esc after that is the old Esc (back to Synth)");
            // Ctrl+Y toggles focus while the dock is open
            ctrl(Qt::Key_Y); ctrl(Qt::Key_Y);
            check(w.guide.dockIsOpen() && !w.guide.dockHasFocus(), "Ctrl+Y on the dock gives the keyboard back to the page");
            ctrl(Qt::Key_Y);
            check(w.guide.dockIsOpen() && w.guide.dockHasFocus(), "and again takes it");
            // touch
            w.shot(); tap(w, "dock_close");
            check(!w.guide.dockIsOpen(), "Hide closes the dock");
            ctrl(Qt::Key_Y); press(Qt::Key_Escape); w.shot();
            tap(w, "dock_input");
            check(w.guide.dockHasFocus(), "touching the input line gives it the keyboard");
            press(Qt::Key_H); press(Qt::Key_I); w.shot();
            tap(w, "dock_send"); wait(40);
            check(w.chat.log.back().text == "hi" && w.chat.log.back().user, "touching Send sends");
            // the page behind stays usable above the dock; the dock swallows touches on itself
            const int oct0 = inst.uiState().set.tuning.octave;
            tap(w, "dock_close"); w.shot();
            tap(w, "octave_up"); wait(80);
            check(inst.uiState().set.tuning.octave == oct0 + 1, "with the dock closed the page's own buttons work");
            tap(w, "octave_down"); wait(60);
            // Ctrl+Shift+Y is the full page, Ctrl+Y on it goes back
            key(w, Qt::Key_Control, true); key(w, Qt::Key_Shift, true); press(Qt::Key_Y); key(w, Qt::Key_Shift, false); key(w, Qt::Key_Control, false);
            check(w.page() == PG_CHAT && w.guide.dockRect().isNull(), "Ctrl+Shift+Y opens the full Chat page, where there is no dock");
            press(Qt::Key_A);
            check(w.chat.draft == "a", "and letters still type on the Chat page");
            press(Qt::Key_Backspace);
            ctrl(Qt::Key_Y);
            check(w.page() == PG_PLAY, "Ctrl+Y on the Chat page goes back");
            // the Full button
            ctrl(Qt::Key_Y); w.shot(); tap(w, "dock_full");
            check(w.page() == PG_CHAT, "the dock's Full button opens the Chat page");
            press(Qt::Key_Escape);
            w.guide.dockClose();
        }

        { // the ticker opens the dock
            w.setPage(PG_PLAY); w.guide.dockClose(); w.shot();
            w.chat.add("rook", "A new message while you play.", "say", false);
            w.shot();
            check(w.chat.tickerShowing() && w.zoneNames().contains("ticker"), "a new agent message shows the ticker with a touch target");
            tap(w, "ticker"); w.shot();
            check(w.guide.dockIsOpen() && w.guide.dockHasFocus(), "touching the ticker opens the dock with the keyboard");
            const int unread = w.chat.unread;
            w.chat.add("rook", "And another one.", "say", false);
            check(w.chat.unread == unread && !w.chat.tickerShowing(), "with the dock open there is no ticker and nothing counts as unread");
            w.guide.dockClose();
            { // the ticker does not cover a highlight: it moves to the top
                QStringList unk; w.refreshZones(); w.guide.setHighlight({"loop_pedal"}, QString(), 20000, unk);
                w.chat.add("rook", "Look at the loop pedal.", "say", false); w.shot();
                check(w.zoneRect("ticker").top() < 100 && !w.zoneRect("ticker").intersects(w.zoneRect("loop_pedal")), "when a highlight is at the bottom the ticker moves to the top");
                w.guide.clearHighlight(); w.shot();
                check(w.zoneRect("ticker").top() > 600, "and is back at the bottom without one");
            }
            w.chat.add("rook", "Ticker again.", "say", false); w.shot();
            ctrl(Qt::Key_Y);
            check(w.guide.dockIsOpen() && w.guide.dockHasFocus(), "Ctrl+Y while the ticker shows also opens the dock");
            w.guide.dockClose();
        }

        { // the dock never covers what is highlighted; what is discussed is tagged; focus travels with the message
            w.setPage(PG_PLAY); w.guide.dockClose(); w.shot();
            ctrl(Qt::Key_Y); press(Qt::Key_Escape);
            QJsonObject r = c.call(B(R"({"cmd":"highlight","zones":["octave"],"label":"OCTAVE","ms":20000})"));
            const QRectF d = w.guide.dockRect(), o1 = w.zoneRect("octave_down"), o2 = w.zoneRect("octave_up");
            check(okOf(r) && w.guide.dockIsOpen() && !d.intersects(o1) && !d.intersects(o2) && d.top() < 100, "octave is at the bottom of Synth: the dock moves to the top rather than cover it");
            check(w.guide.dockTopic() == "OCTAVE" && res(r)["dock"].toObject()["position"].toString() == "top", "the dock is tagged with what is being discussed (OCTAVE)");
            QImage im = w.shot();
            if (!shotsDir.isEmpty()) im.save(shotsDir + "/16-dock-synth.png");
            r = c.call(B(R"({"cmd":"highlight","zones":["hdr_sound"],"label":"the sound name"})"));
            check(okOf(r) && !w.guide.dockRect().intersects(w.zoneRect("hdr_sound")) && w.guide.dockRect().top() > 500, "a highlight at the top leaves the dock at the bottom");
            r = c.call(B(R"({"cmd":"highlight","zones":["octave"],"label":"OCTAVE","ms":20000})"));
            ctrl(Qt::Key_Y); // focus the dock
            press(Qt::Key_W); press(Qt::Key_H); press(Qt::Key_Y); press(Qt::Key_Return); wait(40);
            const ChatMsg &m = w.chat.log.back();
            check(m.user && m.page == "synth" && m.focus == QStringList({"octave_down", "octave_up"}) && m.via == "dock", "a question sent from the dock says it was asked on Synth with OCTAVE highlighted");
            check(hf()["last_question_zone"].toString() == "octave_down", "state.help_focus.last_question_zone is the first focused zone");
            QJsonObject pr = c.call(QByteArray(R"({"cmd":"chat_poll","since":)") + QByteArray::number(m.seq - 1) + "}");
            const QJsonObject pm = res(pr)["messages"].toArray().last().toObject();
            check(pm["page"].toString() == "synth" && strs(pm["focus"].toArray()) == QStringList({"octave_down", "octave_up"}), "chat_poll returns the page and focus with the player's message");
            // the topic carries to a message that comes with the highlight, and goes after one that does not
            w.chat.add("rook", "Octave shifts every string up or down twelve notes.", "say", false);
            check(w.guide.dockTopic() == "OCTAVE", "an agent message just after a highlight keeps the topic tag");
            wait(5300);
            w.chat.add("rook", "Anything else?", "say", false);
            check(w.guide.dockTopic().isEmpty(), "a later message with no highlight clears it");
            // page change closes the topic; the dock stays
            c.call(B(R"({"cmd":"highlight","zones":["octave"],"label":"OCTAVE"})"));
            w.setPage(PG_TUNE); w.shot();
            check(w.guide.dockIsOpen() && w.guide.dockTopic().isEmpty() && hf()["highlighted"].toArray().isEmpty(), "changing page keeps the dock but drops the highlight and the tag");
            w.guide.dockClose();
        }

        { // the dock never covers a highlighted zone, on any page
            int tested = 0, clashes = 0; QStringList bad;
            for (int p = 0; p < PG_COUNT; ++p) {
                if (p == PG_CHAT) continue;
                w.setPage(p); w.guide.dockClose(); w.guide.clearHighlight(); w.shot();
                const QStringList names = w.zoneNames();
                for (const QString &n : names) {
                    if (n.startsWith("dock") || n == "ticker" || n.startsWith("tab_")) continue;
                    const QRectF zr = w.zoneRect(n);
                    if (zr.height() > 330) continue; // taller than any dock could leave room beside
                    w.guide.dockOpenFocus();
                    QStringList unk; w.guide.setHighlight({n}, "x", 20000, unk); w.shot();
                    ++tested;
                    if (w.guide.dockRect().intersects(zr) || w.guide.dockRect().isNull()) { ++clashes; if (bad.size() < 4) bad << QString("%1/%2").arg(pageName(p), n); }
                    const QRectF cp = w.guide.highlightCaptionRect();
                    if (!cp.isNull() && (cp.intersects(zr) || cp.intersects(w.guide.dockRect()) || !QRectF(0, 0, 720, 720).contains(cp))) { ++clashes; if (bad.size() < 4) bad << QString("caption %1/%2").arg(pageName(p), n); }
                    w.guide.clearHighlight();
                }
                w.guide.dockClose();
            }
            check(clashes == 0 && tested > 150, QString("across every page the dock and the caption stay clear of what is highlighted (%1 zones tried%2)").arg(tested).arg(bad.isEmpty() ? QString() : ", clashes: " + bad.join(" ")));
            // two zones far apart: still no overlap, when there is room
            w.setPage(PG_PLAY); w.guide.dockOpenFocus();
            QStringList unk; w.guide.setHighlight({"octave_up", "hdr_sound"}, "both ends", 20000, unk); w.shot();
            check(w.guide.dockRect().isNull() || !(w.guide.dockRect().intersects(w.zoneRect("octave_up")) && w.guide.dockRect().intersects(w.zoneRect("hdr_sound"))), "with a highlight at both ends the dock gives way to at least one of them");
            w.guide.clearHighlight(); w.guide.dockClose();
        }

        { // the Help page: ask from the guide without going to the Chat page
            w.setPage(PG_HELP); w.guide.dockClose(); w.setHelpSubPage(0); w.shot();
            tap(w, "help_ask"); w.shot();
            check(w.guide.dockIsOpen() && w.guide.dockHasFocus() && w.page() == PG_HELP && !w.chat.draft.isEmpty(), "Ask Rook on the Help page opens the dock with the keyboard and a starting question");
            QStringList hrows; for (const QString &n : w.zoneNames()) if (n.startsWith("help_row_")) hrows << n;
            bool visible = true;
            for (const QString &n : hrows) visible = visible && !w.guide.dockRect().intersects(w.zoneRect(n));
            check(hrows.size() == 16 && visible, "with the dock open every Help row is still visible above it");
            for (int i = 0; i < 80; ++i) press(Qt::Key_Backspace);
            w.guide.dockClose(); w.shot();
            tap(w, hrows.value(8)); w.shot();
            check(w.guide.dockHasFocus() && w.chat.draft.startsWith("What does"), QString("tapping a Help row asks about it in the dock (draft: %1)").arg(w.chat.draft));
            const int before = w.chat.seq;
            press(Qt::Key_Return); wait(40);
            const ChatMsg &hm = w.chat.log.back();
            check(w.chat.seq == before + 1 && hm.page == "help" && hm.focus == QStringList({hrows.value(8)}) && w.page() == PG_HELP,
                  QString("and the question goes out with the row as its focus, still on the Help page (%1 %2)").arg(hm.page, hm.focus.join(",")));
            QJsonObject r = c.call(QByteArray(R"({"cmd":"highlight","zones":[")") + hrows.value(3).toUtf8() + R"("],"label":"this one"})");
            check(okOf(r) && strs(res(r)["highlighted"].toArray()) == QStringList({hrows.value(3)}) && !w.guide.dockRect().intersects(w.zoneRect(hrows.value(3))), "Rook can highlight a single Help row");
            QImage im = w.shot();
            if (!shotsDir.isEmpty()) im.save(shotsDir + "/17-dock-help.png");
            const int sub0 = w.helpSubPage();
            tap(w, "help_next");
            check(w.helpSubPage() == (sub0 + 1) % 4, "the Help sections are still reachable with the next button");
            w.guide.clearHighlight(); w.guide.dockClose(); w.setPage(PG_PLAY);
        }
        gctl.stop();
        w.guide.dockClose(); w.guide.clearHighlight(); w.setPage(PG_PLAY);
        inst.cmd(EV_PANIC);
        wait(200);
    }

    std::printf("- bug fixes (0.12.1)\n");
    {
        QTemporaryDir bdir;
        const QString bsock = bdir.path() + "/vrequency.sock";
        ControlServer bctl(w, inst);
        check(bctl.start(bsock), "a control socket for the bug-fix checks");
        Client c;
        check(c.open(bsock), "a client connects");
        auto B = [](const QString &s) { return s.toUtf8(); };
        auto press = [&](int k) { key(w, k, true); key(w, k, false); };
        auto ctrl = [&](int k) { key(w, Qt::Key_Control, true); press(k); key(w, Qt::Key_Control, false); };
        w.guide.dockClose(); w.guide.clearHighlight(); w.setPage(PG_PLAY); w.shot();

        { // every armed mode is dropped whenever the page changes, by any route
            struct Route { const char *name; std::function<void()> go; };
            const Route routes[] = {
                {"a tab tap", [&] { tap(w, "tab_beat"); }},
                {"the Tab key", [&] { press(Qt::Key_Tab); }},
                {"Esc", [&] { press(Qt::Key_Escape); }},
                {"the control socket page command", [&] { c.call(B(R"({"cmd":"page","name":"tune"})")); }},
                {"a highlight that names a page", [&] { c.call(B(R"({"cmd":"highlight","zones":["loop_pedal"],"page":"synth","ms":500})")); }},
                {"Ctrl+Shift+Y (Chat page)", [&] { key(w, Qt::Key_Shift, true); ctrl(Qt::Key_Y); key(w, Qt::Key_Shift, false); }},
                {"Ctrl+H (Help)", [&] { ctrl(Qt::Key_H); }},
            };
            for (const Route &r : routes) {
                w.setPage(PG_SOUND); w.shot();
                w.armAllModes();
                check(w.armedModes() == "delete,store,erase,remove", "all four modes can be armed");
                r.go(); w.shot();
                check(w.page() != PG_SOUND && w.armedModes().isEmpty(), QString("leaving the page by %1 clears Delete, Store, Erase and the remove confirmation").arg(r.name));
                w.guide.clearHighlight(); w.guide.dockClose();
            }
            w.setPage(PG_SOUND); w.shot();
            const int favN = QDir(store::baseDir() + "/favourites").entryList({"*.json"}).size();
            tap(w, "favdel"); w.shot();
            check(w.armedModes() == "delete", "Delete arms on the Sound page");
            tap(w, "tab_tune"); w.shot(); tap(w, "tab_sound"); w.shot();
            tap(w, "favitem0"); wait(60);
            check(QDir(store::baseDir() + "/favourites").entryList({"*.json"}).size() == favN, "the favourite tapped after coming back is recalled, not deleted");
            w.setPage(PG_PLAY); w.shot();
        }

        { // the key test panel and the ticker swallow touches
            w.setPage(PG_SYS); w.shot();
            const QPointF q = w.zoneRect("quit").center(), fs = w.zoneRect("factory").center();
            check(w.zoneAt(q) == "quit" && w.zoneAt(fs) == "factory", "(control) Quit and Factory sounds take their own touches");
            w.setKeyTest(true); w.shot();
            check(w.zoneAt(q) == "keytestblock" && w.zoneAt(fs) == "keytestblock" && w.zoneAt(QPointF(4, 4)) == "keytestblock", "with the key test open, touches over Quit, Factory sounds and the tabs go to the panel");
            check(w.zoneAt(w.zoneRect("keytestclose").center()) == "keytestclose", "but Close still works");
            w.setKeyTest(false);
            w.setPage(PG_PLAY); w.setKeyTest(true); w.shot();
            const QPointF m = w.zoneRect("menu").center();
            w.pointer(5, m, PH_PRESS); w.pointer(5, m, PH_RELEASE);
            check(w.page() == PG_PLAY, "a tap on Menu under the key test panel does nothing");
            w.setKeyTest(false);
            w.setPage(PG_SYS); w.guide.dockClose(); w.shot();
            w.chat.add("rook", "Ticker over the bottom row.", "say", false); w.shot();
            check(w.chat.tickerShowing() && w.zoneAt(q) == "ticker" && w.zoneAt(fs) == "ticker", "the ticker takes the touches over Quit and Factory sounds");
            tap(w, "ticker"); w.shot();
            check(w.guide.dockIsOpen() && w.zoneAt(w.guide.dockRect().center()).startsWith("dock"), "and the chat dock takes every touch inside it");
            w.guide.dockClose(); w.setPage(PG_PLAY);
        }

        { // favourites: overwrite by name; the paging button goes both ways
            QDir fd(store::baseDir() + "/favourites");
            c.call(B(R"({"cmd":"save_favourite","name":"Same name"})"));
            const int n1 = fd.entryList({"*.json"}).size();
            const QJsonObject r2 = c.call(B(R"({"cmd":"save_favourite","name":"Same name"})"));
            check(fd.entryList({"*.json"}).size() == n1 && r2["result"].toObject()["updated"].toBool() && !fd.exists("same-name-2.json"),
                  "saving a favourite under an existing name updates it instead of adding a copy");
            for (int i = 1; i <= 14; ++i) c.call(B(QString(R"({"cmd":"save_favourite","name":"Pg %1"})").arg(i)));
            const int total = fd.entryList({"*.json"}).size(), pages = (total + 5) / 6;
            check(pages >= 3, QString("%1 favourites make %2 pages").arg(total).arg(pages));
            w.setPage(PG_SOUND); w.shot();
            const QRectF fr = w.zoneRect("favprev");
            auto at = [&](double fx) { const QPointF p(fr.left() + fr.width() * fx, fr.center().y()); w.pointer(6, p, PH_PRESS); w.pointer(6, p, PH_RELEASE); w.shot(); };
            const int p0 = w.favouritesPage();
            at(0.8); const int p1 = w.favouritesPage();
            at(0.2); const int p2 = w.favouritesPage();
            at(0.2); const int p3 = w.favouritesPage();
            at(0.8); const int p4 = w.favouritesPage();
            check(p0 == 0 && p1 == 1 && p2 == 0 && p3 == pages - 1 && p4 == 0, "right half pages forward, left half pages back, both wrapping");
            tap(w, "favprev"); w.shot();
            check(w.favouritesPage() == 1, "a tap in the middle (the control socket) still goes forward");
            w.setPage(PG_PLAY); w.shot();
        }

        { // one tempo step everywhere
            inst.set(S_BPM, 100); wait(40);
            w.setPage(PG_BACK); w.shot(); tap(w, "bpm2+"); wait(60);
            check(std::lround(inst.uiState().set.bpm) == 101, "Backing page tempo steps by 1");
            w.setPage(PG_BEAT); w.shot(); tap(w, "bpm3-"); wait(60); w.shot(); tap(w, "bpm3-"); wait(60);
            check(std::lround(inst.uiState().set.bpm) == 99, "Beat page tempo steps by 1");
            w.setPage(PG_TUNE); w.shot(); tap(w, "bpm+"); wait(60);
            check(std::lround(inst.uiState().set.bpm) == 100, "Tune page tempo steps by 1");
            w.setPage(PG_PLAY); w.shot();
        }
    }

    runTourTests(w, inst, audio, check);

    std::printf("- panic / health\n");
    key(w, Qt::Key_Q, true); key(w, Qt::Key_A, true); wait(60);
    key(w, Qt::Key_Backspace, true); key(w, Qt::Key_Backspace, false); wait(200);
    check(inst.uiState().voices == 0, "Backspace silences everything");
    key(w, Qt::Key_Q, false); key(w, Qt::Key_A, false);
    check(inst.uiState().droppedEvents == 0, "no input events were dropped");
    std::printf("  info  engine load %.1f%% avg, %.1f%% peak, %u late blocks, backend %s\n", audio.load() * 100, audio.peakLoad() * 100, audio.overruns(), audio.backend().c_str());
    check(audio.peakLoad() < 1.0f, "audio never fell behind real time");

    std::printf("\n%d passed, %d failed\n", passes, fails);
    return fails ? 1 : 0;
}

// Pictures of every page, for the documentation: `vrequency --audio null --render DIR`.
int renderShots(MainWindow &w, Instrument &inst, const QString &dir) {
    QDir().mkpath(dir);
    static const char *files[] = {"10-track", "0-play", "1-sound", "9-beat", "2-backing", "3-tune", "4-system", "5-help", "14-chat"};
    static const char *names[] = {"track", "synth", "sound", "beat", "backing", "tune", "system", "help", "chat"};
    inst.set(S_VOLUME, 0.5f);
    w.setPage(PG_PLAY);
    key(w, Qt::Key_A, true); wait(250);
    key(w, Qt::Key_F, true); // a held slide, so the scope and the strings have something to show
    for (int p = 0; p < PG_COUNT; ++p) {
        w.setPage(p);
        if (p == PG_BEAT) { // a beat in progress, so the strip and the play line show something
            static const char *pat[kDrums] = {"x......x..x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "...............x",
                                              "............x...", "..x......x......", "..............x.", "................"};
            for (int d = 0; d < kDrums; ++d)
                for (int st = 0; st < kDrumSteps; ++st) inst.beatDo(BT_STEP, d, st, pat[d][st] == 'x');
            inst.set(S_BEATVOL, 0.7f);
            inst.beatDo(BT_PLAYSTOP);
        }
        if (p == PG_CHAT) { // a conversation, so the page has something to show
            w.chat.add("rook", "Hi. I can hear A minor pentatonic at 100 bpm. Want a backing under it?", "ask", false);
            w.chat.add("you", "yes, something slow with a soft pad", "say", true);
            w.chat.add("rook", "On it: I-vi-IV-V on sound 3, tempo down to 84. Play over it and I will listen.", "say", false);
            w.chat.add("rook", "Tip: hold Space and slide between the top row keys for long glides.", "tip", false);
            w.chat.seen();
            w.chat.toggle(); w.chat.toggle(); w.setPage(PG_CHAT); // leaves the draft empty, unread cleared
        }
        for (int i = 0; i < 30; ++i) { w.shot(); wait(40); } // let the trace and the auto-range settle
        if (p != PG_TRACK) w.shot().save(QString("%1/%2.png").arg(dir).arg(files[p]));
        if (p == PG_BEAT) { inst.beatDo(BT_PLAYSTOP); inst.set(S_BEATVOL, 0.7f); wait(60); }
    }
    (void)names;
    {   // a new message from the copilot while playing: the ticker at the bottom
        w.setPage(PG_PLAY);
        w.chat.add("rook", "Nice slide. Try ending the phrase on the fifth: hold Z, press N.", "tip", false);
        for (int i = 0; i < 8; ++i) { w.shot(); wait(40); }
        w.shot().save(QString("%1/15-ticker.png").arg(dir));
    }
    {   // guided help: the dock on Synth with the octave buttons highlighted, a caption without the dock, the dock on Help
        QStringList unk;
        w.setPage(PG_PLAY);
        wait(7300); w.refreshZones();   // the ticker has gone
        w.guide.setHighlight({"loop_pedal"}, "LOOP PEDAL", 20000, unk);
        for (int i = 0; i < 6; ++i) { w.shot(); wait(40); }
        w.shot().save(QString("%1/18-caption.png").arg(dir));
        w.guide.clearHighlight();
        w.guide.dockOpenFocus(); w.shot();
        w.chat.add("you", "what does octave do?", "say", true, "synth", {});
        w.chat.add("rook", "It moves every string up or down twelve notes. Tap + and play the same key again.", "say", false);
        w.guide.setHighlight({"octave"}, "OCTAVE", 20000, unk);
        for (int i = 0; i < 6; ++i) { w.shot(); wait(40); }
        w.shot().save(QString("%1/16-dock-synth.png").arg(dir));
        w.guide.clearHighlight(); w.guide.dockClose();
        w.setPage(PG_HELP); w.shot();
        w.guide.dockOpenFocus();
        w.chat.add("rook", "Here is the Loop pedal row: Enter starts a count-in, records, then plays and overdubs.", "say", false);
        w.shot();
        w.guide.setHighlight({"help_row_loop_pedal_count_in_record_play_ov"}, "LOOP PEDAL", 20000, unk);
        for (int i = 0; i < 6; ++i) { w.shot(); wait(40); }
        w.shot().save(QString("%1/17-dock-help.png").arg(dir));
        w.guide.clearHighlight(); w.guide.dockClose();
        w.setPage(PG_PLAY);
    }
    {   // the Glitch mapping with a finger on the pad
        w.setPage(PG_PLAY);
        inst.set(S_XYMAP, XY_GLITCH); wait(60);
        QRectF xy = w.zoneRect("xy");
        w.pointer(1, QPointF(xy.left() + xy.width() * 0.45, xy.top() + xy.height() * 0.62), PH_PRESS);
        for (int i = 0; i < 40; ++i) { w.shot(); wait(40); }
        w.shot().save(QString("%1/7-glitch.png").arg(dir));
        w.pointer(1, xy.center(), PH_RELEASE);
        inst.set(S_XYMAP, XY_FILTER); wait(60);
    }
    key(w, Qt::Key_F, false); key(w, Qt::Key_A, false);
    {   // a small arrangement for the Track page: two beats, a progression and two loops
        auto press = [&](int k) { key(w, k, true); key(w, k, false); };
        auto ctrl = [&](int k) { key(w, Qt::Key_Control, true); press(k); key(w, Qt::Key_Control, false); };
        auto melody = [](const Patch &sound, std::initializer_list<std::array<double, 3>> notes) {
            LayerData ld; ld.patches.push_back(sound);
            uint32_t seq = 0;
            for (const auto &n : notes) {
                LoopEv a; a.beat = n[0]; a.op = OP_ON; a.slot = 0; a.a = (float)n[2]; a.b = 1; a.seq = seq++;
                LoopEv b; b.beat = n[0] + n[1]; b.op = OP_OFF; b.slot = 0; b.a = b.b = 0; b.seq = seq++;
                ld.ev.push_back(a); ld.ev.push_back(b);
            }
            return ld;
        };
        SessionData sd; inst.snapshot(sd);
        sd.prog[0] = 5; sd.prog[1] = 3; sd.prog[2] = 0; sd.prog[3] = 4; sd.progLen = 4; sd.settings.chordBeats = 4;
        sd.backing = extraPatch(1);
        sd.loopBeats = 8;
        sd.layers = {melody(extraPatch(0), {{0, 1.5, 64}, {1.5, 1.5, 67}, {3.5, 0.5, 69}, {4, 2, 72}, {6, 1, 71}, {7, 0.9, 67}})};
        inst.restore(sd); wait(80);
        w.setPage(PG_TRACK); wait(60);
        tap(w, "track_keep"); wait(60);                       // Beat 1, Chords 1, Loop 1
        inst.beatDo(BT_ERASE, D_HAT); inst.beatDo(BT_STEP, D_CLAP, 4, 1); inst.beatDo(BT_STEP, D_TOM, 13, 1); wait(80);
        inst.snapshot(sd);
        sd.loopBeats = 16;
        sd.layers = {melody(extraPatch(1), {{0, 3, 52}, {4, 2, 55}, {6, 2, 57}, {8, 3.5, 60}, {12, 1, 59}, {13, 1, 57}, {14, 1.9, 55}}),
                     melody(extraPatch(1), {{2, 1, 76}, {7, 0.5, 79}, {10, 2, 81}, {15, 0.5, 79}})};
        inst.restore(sd); wait(80);
        tap(w, "track_keep"); wait(60);                       // Beat 2, Loop 2
        press(Qt::Key_Escape);
        press(Qt::Key_Return);
        press(Qt::Key_1); for (int i = 0; i < 4; ++i) ctrl(Qt::Key_V);                 // Beat 1 on bars 1-4
        press(Qt::Key_4); ctrl(Qt::Key_V);                                              // Beat 2 on bar 5
        press(Qt::Key_1); for (int i = 0; i < 3; ++i) ctrl(Qt::Key_V);                 // Beat 1 on bars 6-8
        press(Qt::Key_Return); press(Qt::Key_2); ctrl(Qt::Key_V); ctrl(Qt::Key_V);     // Chords 1 twice
        QRectF lanes = w.zoneRect("track_lanes");
        auto cell = [&](int lane, double bar) { return QPointF(lanes.left() + (bar + 0.5) * lanes.width() / 8.0, lanes.top() + 2 + lane * 70 + 33); };
        auto touch = [&](QPointF a) { w.pointer(3, a, PH_PRESS); w.pointer(3, a, PH_RELEASE); };
        press(Qt::Key_3); touch(cell(LN_LOOPA, 2)); touch(cell(LN_LOOPA, 6));          // Loop 1 on bars 3-4 and 7-8
        press(Qt::Key_5); touch(cell(LN_LOOPB, 4));                                     // Loop 2 on bars 5-8
        press(Qt::Key_Escape);
        touch(cell(LN_BEAT, 0)); ctrl(Qt::Key_J); ctrl(Qt::Key_J); ctrl(Qt::Key_J);    // the four beat bars become one clip
        touch(cell(LN_LOOPA, 2));
        press(Qt::Key_Return);
        key(w, Qt::Key_Shift, true); press(Qt::Key_Right); press(Qt::Key_Right); key(w, Qt::Key_Shift, false); // play line on bar 3
        press(Qt::Key_C);                                                               // cycle round the chosen loop
        press(Qt::Key_3);                                                               // Loop 1 in hand again
        touch(cell(LN_LOOPA, 2));
        wait(1700);                                                                     // let the notices fade
        for (int i = 0; i < 5; ++i) { w.shot(); wait(40); }
        w.shot().save(QString("%1/10-track.png").arg(dir));
        press(Qt::Key_Escape);
        press(Qt::Key_C);
        press(Qt::Key_E);
        w.shot();
        QRectF strip = w.zoneRect("chop_strip");
        for (double f : {0.25, 0.5, 0.75}) { touch(QPointF(strip.left() + strip.width() * f + 2, strip.center().y())); w.shot(); }
        touch(QPointF(strip.left() + strip.width() * 0.6, strip.center().y())); w.shot(); // choose piece C
        tap(w, "chop_move_left"); w.shot();
        wait(1700);
        for (int i = 0; i < 5; ++i) { w.shot(); wait(40); }
        w.shot().save(QString("%1/11-chop.png").arg(dir));
        tap(w, "chop_done");
        w.shot();
        press(Qt::Key_I); w.shot();                                                     // the sound picker for the chosen loop
        press(Qt::Key_Right); w.shot();
        press(Qt::Key_1); w.shot();                                                     // Guitar, Pluck
        press(Qt::Key_Space);                                                           // not playing in the picture
        wait(1900);
        for (int i = 0; i < 5; ++i) { w.shot(); wait(40); }
        w.shot().save(QString("%1/12-sound.png").arg(dir));
        press(Qt::Key_Return);
        inst.cmd(EV_PANIC); wait(60);
        press(Qt::Key_X); w.shot();                                                     // the mixer, playing
        press(Qt::Key_4); press(Qt::Key_M);                                             // Loop B muted
        press(Qt::Key_2); for (int i = 0; i < 3; ++i) press(Qt::Key_Down);              // Chords at 65
        press(Qt::Key_3); press(Qt::Key_Up); press(Qt::Key_Up);                         // Loop A at 90
        press(Qt::Key_1);
        tap(w, "mixer_play");
        wait(2600);
        for (int i = 0; i < 5; ++i) { w.shot(); wait(40); }
        w.shot().save(QString("%1/13-mixer.png").arg(dir));
        tap(w, "mixer_play");
        inst.cmd(EV_PANIC); wait(60);
    }
    if (qEnvironmentVariableIsSet("VREQUENCY_HELP_SHOTS")) { // every page of the built-in guide, for proof-reading
        w.setPage(PG_HELP);
        for (int i = 0; i < 4; ++i) { w.shot(); w.shot().save(QString("%1/help-%2.png").arg(dir).arg(i)); tap(w, "help_next"); }
    }
    w.setPage(PG_SYS);
    w.setKeyTest(true);
    key(w, Qt::Key_Z, true); key(w, Qt::Key_S, true); key(w, Qt::Key_E, true); wait(120);
    w.shot().save(QString("%1/6-keytest.png").arg(dir));
    key(w, Qt::Key_E, false); key(w, Qt::Key_S, false); key(w, Qt::Key_Z, false);
    w.setKeyTest(false);
    w.setPage(PG_PLAY);
    std::printf("wrote %s\n", dir.toUtf8().constData());
    return 0;
}
