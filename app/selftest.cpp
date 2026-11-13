// venv vrequency — built-in self-test. Drives the real window with real Qt key and
// pointer events while the real audio thread runs, then checks what the
// engine did. (It cannot press the physical keys or listen to the speaker:
// those two checks need a human.)
#include "audio.h"
#include "instrument.h"
#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QScreen>
#include <QThread>
#include <cmath>
#include <cstdio>

using namespace td;

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
    w.pointer(7, r.center(), MainWindow::PH_PRESS);
    w.pointer(7, r.center(), MainWindow::PH_RELEASE);
}
} // namespace

int runSelfTest(MainWindow &w, Instrument &inst, AudioOut &audio, const QString &shotsDir) {
    std::printf("venv vrequency self-test\n");
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
    static const char *files[] = {"0-play", "1-sound", "2-backing", "9-beat", "3-tune", "4-system", "5-help"};
    static const char *names[] = {"play", "sound", "backing", "beat", "tune", "system", "help"};
    if (!shotsDir.isEmpty()) QDir().mkpath(shotsDir);
    for (int p = 0; p < MainWindow::PG_COUNT; ++p) {
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
    w.setPage(MainWindow::PG_PLAY);
    wait(100);
    {
        QElapsedTimer pt; pt.start();
        for (int i = 0; i < 40; ++i) w.shot();
        std::printf("  info  play page redraw: %.1f ms per frame\n", pt.elapsed() / 40.0);
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
    QString before = w.leadPatch().name;
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
    w.setPage(MainWindow::PG_SOUND); wait(100);
    tap(w, "favitem0"); wait(80);
    check(QString(inst.uiState().leadName) == rnd, "favourite recalled from the sound page");
    tap(w, "favitem1"); wait(80);
    check(QString(inst.uiState().leadName) == "Haze", "the dream sounds follow in order: Haze is next");
    tap(w, "favitem5"); wait(80);
    check(QString(inst.uiState().leadName) == "Grain", "and Grain is last on the first page");
    check(w.addExtraSounds() == 0 && favDir.entryList({"*.json"}).size() == favBefore + 1, "they are not added twice");
    w.setPage(MainWindow::PG_PLAY); wait(100);
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

    std::printf("- touch\n");
    QRectF xy = w.zoneRect("xy");
    w.pointer(1, xy.center(), MainWindow::PH_PRESS); wait(60);
    w.pointer(1, QPointF(xy.left() + xy.width() * 0.9, xy.top() + xy.height() * 0.2), MainWindow::PH_MOVE); wait(80);
    u = inst.uiState();
    check(u.xyTouch && u.xyX > 0.85f && u.xyY > 0.75f, "dragging the pad moves the XY control");
    check(inst.part(0).modCut > 2.0f, "which modulates the filter live");
    // second finger plays a string while the first stays on the pad
    QRectF s1 = w.zoneRect("string1");
    w.pointer(2, QPointF(s1.left() + s1.width() * 0.3, s1.center().y()), MainWindow::PH_PRESS); wait(80);
    check(inst.uiState().str[1].gate, "a second touch plays a string at the same time");
    float n0 = inst.uiState().str[1].target;
    w.pointer(2, QPointF(s1.left() + s1.width() * 0.6, s1.center().y()), MainWindow::PH_MOVE); wait(80);
    check(inst.uiState().str[1].target > n0 && inst.uiState().str[1].gate, "dragging along the string slides the pitch");
    w.pointer(2, s1.center(), MainWindow::PH_RELEASE);
    w.pointer(1, xy.center(), MainWindow::PH_RELEASE); wait(80);
    check(!inst.uiState().xyTouch && std::fabs(inst.part(0).modCut) < 0.01f, "lifting the finger returns the sound to normal");
    inst.set(S_XYMAP, XY_GLITCH); wait(60);
    w.pointer(1, xy.center(), MainWindow::PH_PRESS); wait(80);
    check(inst.uiState().glitch, "the Glitch mapping crushes the sound while the pad is touched");
    w.pointer(1, xy.center(), MainWindow::PH_RELEASE); wait(80);
    check(!inst.uiState().glitch, "and stops when the finger lifts");
    inst.set(S_XYMAP, XY_FILTER); wait(60);
    tap(w, "chord4"); wait(60);
    check(inst.uiState().progLen == 5, "touch chord pads work");

    std::printf("- saving\n");
    QString wavPath = w.startRecording();
    key(w, Qt::Key_D, true); wait(700); key(w, Qt::Key_D, false); wait(200);
    w.stopRecording();
    QFile wf(wavPath);
    bool wavOk = wf.open(QIODevice::ReadOnly);
    QByteArray wd = wavOk ? wf.readAll() : QByteArray();
    int nz = 0;
    for (int i = 44; i + 1 < wd.size(); i += 2) if (wd[i] != 0 || wd[i + 1] != 0) ++nz;
    check(wavOk && wd.size() > 44 + 48000 * 2 && wd.startsWith("RIFF") && nz > 1000, QString("WAV export written with audio in it (%1 kB)").arg(wd.size() / 1024));
    QString sp = store::baseDir() + "/sessions/selftest.json";
    SessionData a; inst.snapshot(a);
    check(w.saveSession(sp), "session saved");
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
    w.setPage(MainWindow::PG_BEAT); wait(100);
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
    check(inst.uiState().beatRec && inst.uiState().beatPlaying && inst.uiState().loopState == L_EMPTY, "Enter switches Rec on and starts the beat (not the loop pedal)");
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
    w.setPage(MainWindow::PG_PLAY); wait(80);
    key(w, Qt::Key_Control, true); key(w, Qt::Key_J, true); key(w, Qt::Key_J, false); key(w, Qt::Key_Control, false); wait(100);
    check(inst.uiState().beatPlaying, "Ctrl+J starts the beat from the Play page");
    key(w, Qt::Key_Control, true); key(w, Qt::Key_J, true); key(w, Qt::Key_J, false); key(w, Qt::Key_Control, false); wait(100);
    w.saveState();
    QJsonObject sj; store::readJson(store::baseDir() + "/state.json", sj);
    check(!inst.uiState().beatPlaying && sj["beat"].toArray().size() == kDrums && sj["beat"].toArray()[D_HAT].toString().contains('x'),
          "the beat is kept in the saved state");

    std::printf("- listening back\n");
    check(w.togglePlayback() && w.playingBack(), "Play last starts playing the newest recording");
    wait(300);
    check(w.togglePlayback(), "and the same button stops it");
    wait(300);
    check(!w.playingBack(), "playback has stopped");

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
    static const char *files[] = {"0-play", "1-sound", "2-backing", "9-beat", "3-tune", "4-system", "5-help"};
    static const char *names[] = {"play", "sound", "backing", "beat", "tune", "system", "help"};
    inst.set(S_VOLUME, 0.5f);
    w.setPage(MainWindow::PG_PLAY);
    key(w, Qt::Key_A, true); wait(250);
    key(w, Qt::Key_F, true); // a held slide, so the scope and the strings have something to show
    for (int p = 0; p < MainWindow::PG_COUNT; ++p) {
        w.setPage(p);
        if (p == MainWindow::PG_BEAT) { // a beat in progress, so the strip and the play line show something
            static const char *pat[kDrums] = {"x......x..x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "...............x",
                                              "............x...", "..x......x......", "..............x.", "................"};
            for (int d = 0; d < kDrums; ++d)
                for (int st = 0; st < kDrumSteps; ++st) inst.beatDo(BT_STEP, d, st, pat[d][st] == 'x');
            inst.set(S_BEATVOL, 0.0f);
            inst.beatDo(BT_PLAYSTOP);
        }
        for (int i = 0; i < 30; ++i) { w.shot(); wait(40); } // let the trace and the auto-range settle
        w.shot().save(QString("%1/%2.png").arg(dir).arg(files[p]));
        if (p == MainWindow::PG_BEAT) { inst.beatDo(BT_PLAYSTOP); inst.set(S_BEATVOL, 0.7f); wait(60); }
    }
    (void)names;
    {   // the Glitch mapping with a finger on the pad
        w.setPage(MainWindow::PG_PLAY);
        inst.set(S_XYMAP, XY_GLITCH); wait(60);
        QRectF xy = w.zoneRect("xy");
        w.pointer(1, QPointF(xy.left() + xy.width() * 0.45, xy.top() + xy.height() * 0.62), MainWindow::PH_PRESS);
        for (int i = 0; i < 40; ++i) { w.shot(); wait(40); }
        w.shot().save(QString("%1/7-glitch.png").arg(dir));
        w.pointer(1, xy.center(), MainWindow::PH_RELEASE);
        inst.set(S_XYMAP, XY_FILTER); wait(60);
    }
    key(w, Qt::Key_F, false); key(w, Qt::Key_A, false);
    w.setPage(MainWindow::PG_SYS);
    w.setKeyTest(true);
    key(w, Qt::Key_Z, true); key(w, Qt::Key_S, true); key(w, Qt::Key_E, true); wait(120);
    w.shot().save(QString("%1/6-keytest.png").arg(dir));
    key(w, Qt::Key_E, false); key(w, Qt::Key_S, false); key(w, Qt::Key_Z, false);
    w.setKeyTest(false);
    w.setPage(MainWindow::PG_PLAY);
    std::printf("wrote %s\n", dir.toUtf8().constData());
    return 0;
}
