// venv vrequency — built-in self-test. Drives the real window with real Qt key and
// pointer events while the real audio thread runs, then checks what the
// engine did. (It cannot press the physical keys or listen to the speaker:
// those two checks need a human.)
#include "audio.h"
#include "instrument.h"
#include "mainwindow.h"
#include "presets.h"
#include "sounds.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QScreen>
#include <QThread>
#include <array>
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
    static const char *files[] = {"10-track", "0-play", "1-sound", "9-beat", "2-backing", "3-tune", "4-system", "5-help"};
    static const char *names[] = {"track", "synth", "sound", "beat", "backing", "tune", "system", "help"};
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
    key(w, Qt::Key_Control, false);

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
    w.setPage(MainWindow::PG_PLAY); wait(80);
    key(w, Qt::Key_Control, true); key(w, Qt::Key_J, true); key(w, Qt::Key_J, false); key(w, Qt::Key_Control, false); wait(100);
    check(inst.uiState().beatPlaying, "Ctrl+J starts the beat from the Synth page");
    key(w, Qt::Key_Control, true); key(w, Qt::Key_J, true); key(w, Qt::Key_J, false); key(w, Qt::Key_Control, false); wait(100);
    w.saveState();
    QJsonObject sj; store::readJson(store::baseDir() + "/state.json", sj);
    check(!inst.uiState().beatPlaying && sj["beat"].toArray().size() == kDrums && sj["beat"].toArray()[D_HAT].toString().contains('x'),
          "the beat is kept in the saved state");

    std::printf("- track\n");
    {
        auto press = [&](int k) { key(w, k, true); key(w, k, false); };
        auto ctrl = [&](int k) { key(w, Qt::Key_Control, true); press(k); key(w, Qt::Key_Control, false); };
        auto T = [&]() -> const TrackData & { return w.trackData(); };
        w.setPage(MainWindow::PG_TRACK); wait(100);
        check(w.page() == MainWindow::PG_TRACK && MainWindow::PG_TRACK == 0 && MainWindow::PG_PLAY == 1, "Track is the first tab, Synth the second");
        check(T().clips.empty(), "the track starts empty");
        press(Qt::Key_Space); wait(80);
        check(!inst.uiState().trackPlaying, "Space on an empty track plays nothing");
        tap(w, "keep"); wait(80);
        check(T().clips.size() == 3 && T().clips[0]->kind == CK_BEAT && T().clips[1]->kind == CK_CHORDS && T().clips[2]->kind == CK_LOOP,
              "+ Keep copies the beat, the chords and the loop onto the shelf");
        check(w.clipInHand(), "and the newest clip is in hand, ready to place");
        tap(w, "keep"); wait(60);
        check(T().clips.size() == 3, "keeping the same things again adds nothing");
        QRectF lanes = w.zoneRect("lanes");
        auto cell = [&](int lane, double bar) { return QPointF(lanes.left() + (bar + 0.5) * lanes.width() / 8.0, lanes.top() + 2 + lane * 70 + 33); };
        auto touch = [&](QPointF a) { w.pointer(3, a, MainWindow::PH_PRESS); w.pointer(3, a, MainWindow::PH_RELEASE); };
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
        w.pointer(3, cell(LN_BEAT, 2), MainWindow::PH_PRESS);
        w.pointer(3, cell(LN_BEAT, 3.6), MainWindow::PH_MOVE);
        w.pointer(3, cell(LN_BEAT, 5), MainWindow::PH_MOVE);
        w.pointer(3, cell(LN_BEAT, 5), MainWindow::PH_RELEASE);
        check(T().lane[LN_BEAT].size() == 2 && T().lane[LN_BEAT][1].bar == 5 && w.trackUndoDepth() == depth + 1, "dragging a clip moves it by whole bars, as one undo step");
        press(Qt::Key_M);
        check(T().lane[LN_BEAT][1].mute, "M mutes the chosen clip");
        press(Qt::Key_M);
        key(w, Qt::Key_Alt, true); press(Qt::Key_Right); key(w, Qt::Key_Alt, false);
        check(!T().lane[LN_BEAT][1].mute && T().lane[LN_BEAT][1].bar == 6, "Alt+Right moves it a bar");
        press(Qt::Key_2);
        tap(w, "tplace"); wait(60);
        check(T().lane[LN_CHORDS].size() == 1 && T().lane[LN_CHORDS][0].bar == 1 && T().lane[LN_CHORDS][0].bars == T().clips[1]->bars, "Place puts the chords clip on the Chords lane at the play line");
        press(Qt::Key_Escape);
        check(w.page() == MainWindow::PG_TRACK && !w.clipInHand(), "Esc puts the clip in hand down (and stays on the page)");

        touch(cell(LN_LOOPA, 0));
        press(Qt::Key_E); wait(60);
        check(w.chopping() && w.chosenLane() == LN_LOOPA, "E opens Chop for the chosen clip");
        const double Lc = T().clips[2]->beats();
        tap(w, "chalves"); wait(40);
        check(T().lane[LN_LOOPA][0].pieces.size() == 2, "Halves cuts it in two");
        tap(w, "cright"); wait(40);
        check(T().lane[LN_LOOPA][0].pieces[0].src == Lc / 2 && T().lane[LN_LOOPA][0].pieces[1].src == 0, "Move swaps the halves");
        tap(w, "cmute"); wait(40);
        check(T().lane[LN_LOOPA][0].pieces[1].mute, "Mute piece silences the chosen piece");
        tap(w, "creset"); wait(40);
        check(T().lane[LN_LOOPA][0].pieces.size() == 1 && !T().lane[LN_LOOPA][0].pieces[0].mute, "Reset undoes every cut");
        QRectF strip = w.zoneRect("chopstrip");
        const QPointF quarter(strip.left() + strip.width() / 4 + 3, strip.center().y());
        touch(quarter); wait(40);
        check(T().lane[LN_LOOPA][0].pieces.size() == 2 && T().lane[LN_LOOPA][0].pieces[0].len == Lc / 4, "tapping a dotted line cuts there");
        touch(quarter); wait(40);
        check(T().lane[LN_LOOPA][0].pieces.size() == 1, "tapping the cut mends it");
        tap(w, "cdone"); wait(40);
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
        w.setPage(MainWindow::PG_PLAY); wait(60);
        key(w, Qt::Key_Q, true); wait(80);
        check(inst.uiState().str[2].gate && inst.uiState().trackPlaying, "on the Synth page you play over the track");
        key(w, Qt::Key_Q, false);
        ctrl(Qt::Key_Space); wait(120);
        check(!inst.uiState().trackPlaying, "Ctrl+Space stops the track from any page");
        w.setPage(MainWindow::PG_TRACK); wait(60);

        tap(w, "texport");
        check(w.exportStatus() == 1, "Export starts writing in the background");
        QElapsedTimer et; et.start();
        while (w.exportStatus() != 0 && et.elapsed() < 60000) { wait(100); w.shot(); }
        QFile ef(w.lastRecordingPath());
        const bool eok = ef.open(QIODevice::ReadOnly);
        const QByteArray ed = eok ? ef.readAll() : QByteArray();
        int enz = 0;
        for (int i = 44; i + 1 < ed.size(); i += 2) if (ed[i] != 0 || ed[i + 1] != 0) ++enz;
        const double esec = (ed.size() - 44) / 4.0 / audio.sampleRate(), want = T().bars * 4 * 60.0 / inst.uiState().set.bpm;
        std::printf("  info  export: %d bars, %.1f s of audio, written in %.1f s\n", T().bars, esec, et.elapsed() / 1000.0);
        check(eok && w.lastRecordingPath().contains("/track-") && ed.startsWith("RIFF") && enz > 1000, "the track is exported as a WAV file with audio in it");
        check(esec >= want && esec < want + 9, QString("it lasts the track plus the dying echoes (%1 s for a %2 s track)").arg(esec, 0, 'f', 1).arg(want, 0, 'f', 1));

        touch(cell(LN_BEAT, 0));
        press(Qt::Key_Backspace); wait(60);
        check(T().lane[LN_BEAT].size() == 1, "Backspace deletes the chosen clip");
        press(Qt::Key_1);
        tap(w, "tremove"); wait(40);
        check(T().clips.size() == 3, "Remove on a shelf clip asks twice");
        tap(w, "tremove"); wait(40);
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
        tap(w, "soct-"); tap(w, "soct-"); tap(w, "soct-"); w.shot();
        check(lc().pick.octave == -2, "the Octave stepper stops two octaves down");
        tap(w, "soct+"); tap(w, "soct+"); w.shot();
        tap(w, "sscope"); w.shot();
        check(lp().pick.own && QString(lp().pick.patch.name) == "Sub" && QString(lc().pick.patch.name) == "Sub", "Only this one: the placement takes the sound as its own");
        tap(w, "skind0"); w.shot(); tap(w, "scell2"); w.shot();
        check(QString(lp().pick.patch.name) == "Organ" && QString(lc().pick.patch.name) == "Sub", "now a pick changes this placement and leaves the clip's other copies alone");
        press(Qt::Key_Backspace); w.shot();
        check(lp().pick.own && !lp().pick.any() && QString(soundName(lc(), pickFor(lc(), lp()))) == QString(recordedSoundName(lc())),
              "Backspace: this one plays as recorded again, the others keep Sub");
        tap(w, "skind2"); w.shot(); tap(w, "scell0"); w.shot();
        check(w.hearingAlone() && inst.uiState().trackPlaying, "all the while the clip keeps going round alone");
        press(Qt::Key_Space); wait(200); w.shot();
        check(!w.hearingAlone() && !inst.uiState().trackPlaying, "Space stops it");
        press(Qt::Key_Return); w.shot();
        check(!w.pickingSound(), "Enter closes the picker");

        touch(cell(LN_BEAT, T().lane[LN_BEAT][0].bar)); w.shot();
        const int bci = T().lane[LN_BEAT][0].clip;
        tap(w, "tsound"); w.shot();
        check(w.pickingSound() && w.soundKindShown() == MainWindow::PK_DRUMS, "the Sound button on a beat clip offers the kits");
        const int otherKit = (T().clips[bci]->kit + 1) % kKits;
        press(Qt::Key_1 + otherKit); w.shot();
        check(T().clips[bci]->pick.kit == otherKit && !T().clips[bci]->pick.on, "a number picks another kit for the beat");
        press(Qt::Key_Up); press(Qt::Key_Right); w.shot();
        check(T().clips[bci]->pick.octave == 0 && w.soundKindShown() == MainWindow::PK_DRUMS, "a beat has no octave and no other kinds");
        tap(w, "srec"); w.shot();
        check(!T().clips[bci]->pick.any(), "As recorded gives the beat its own kit back");
        press(Qt::Key_1 + otherKit); w.shot();
        tap(w, "sdone"); wait(250); w.shot();
        check(!w.pickingSound() && !w.hearingAlone() && !inst.uiState().trackPlaying, "Done closes the picker and stops the clip");
        ctrl(Qt::Key_Z);
        check(!T().clips[bci]->pick.any(), "undo takes the kit back");
        key(w, Qt::Key_Shift, true); ctrl(Qt::Key_Z); key(w, Qt::Key_Shift, false);
        check(T().clips[bci]->pick.kit == otherKit, "and redo picks it again");

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
        check(rsame, "the track read back from the saved state is the same track, picked sounds included");
        check(rt.lane[LN_LOOPA][0].pick.own && QString(rt.lane[LN_LOOPA][0].pick.patch.name) == "Pluck" && QString(rt.clips[lci]->pick.patch.name) == "Sub"
              && rt.clips[bci]->pick.kit == otherKit, "a placement's own sound, a clip's sound and a beat's kit all come back");
        QString sp2 = store::baseDir() + "/sessions/selftest-track.json";
        check(w.saveSession(sp2), "a session saves the track with it");
        const int bars0 = T().bars;
        tap(w, "tlen+"); wait(40);
        check(T().bars == bars0 + 1, "the Length stepper makes the track longer");
        check(w.loadSession(sp2) && T().bars == bars0 && T().clips.size() == 3, "loading the session brings the track back");
        inst.set(S_VOLUME, 0.06f);
        w.setPage(MainWindow::PG_PLAY); wait(60);
    }

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
    static const char *files[] = {"10-track", "0-play", "1-sound", "9-beat", "2-backing", "3-tune", "4-system", "5-help"};
    static const char *names[] = {"track", "synth", "sound", "beat", "backing", "tune", "system", "help"};
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
        if (p != MainWindow::PG_TRACK) w.shot().save(QString("%1/%2.png").arg(dir).arg(files[p]));
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
        w.setPage(MainWindow::PG_TRACK); wait(60);
        tap(w, "keep"); wait(60);                       // Beat 1, Chords 1, Loop 1
        inst.beatDo(BT_ERASE, D_HAT); inst.beatDo(BT_STEP, D_CLAP, 4, 1); inst.beatDo(BT_STEP, D_TOM, 13, 1); wait(80);
        inst.snapshot(sd);
        sd.loopBeats = 16;
        sd.layers = {melody(extraPatch(1), {{0, 3, 52}, {4, 2, 55}, {6, 2, 57}, {8, 3.5, 60}, {12, 1, 59}, {13, 1, 57}, {14, 1.9, 55}}),
                     melody(extraPatch(1), {{2, 1, 76}, {7, 0.5, 79}, {10, 2, 81}, {15, 0.5, 79}})};
        inst.restore(sd); wait(80);
        tap(w, "keep"); wait(60);                       // Beat 2, Loop 2
        press(Qt::Key_Escape);
        press(Qt::Key_Return);
        press(Qt::Key_1); for (int i = 0; i < 4; ++i) ctrl(Qt::Key_V);                 // Beat 1 on bars 1-4
        press(Qt::Key_4); ctrl(Qt::Key_V);                                              // Beat 2 on bar 5
        press(Qt::Key_1); for (int i = 0; i < 3; ++i) ctrl(Qt::Key_V);                 // Beat 1 on bars 6-8
        press(Qt::Key_Return); press(Qt::Key_2); ctrl(Qt::Key_V); ctrl(Qt::Key_V);     // Chords 1 twice
        QRectF lanes = w.zoneRect("lanes");
        auto cell = [&](int lane, double bar) { return QPointF(lanes.left() + (bar + 0.5) * lanes.width() / 8.0, lanes.top() + 2 + lane * 70 + 33); };
        auto touch = [&](QPointF a) { w.pointer(3, a, MainWindow::PH_PRESS); w.pointer(3, a, MainWindow::PH_RELEASE); };
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
        QRectF strip = w.zoneRect("chopstrip");
        for (double f : {0.25, 0.5, 0.75}) { touch(QPointF(strip.left() + strip.width() * f + 2, strip.center().y())); w.shot(); }
        touch(QPointF(strip.left() + strip.width() * 0.6, strip.center().y())); w.shot(); // choose piece C
        tap(w, "cleft"); w.shot();
        wait(1700);
        for (int i = 0; i < 5; ++i) { w.shot(); wait(40); }
        w.shot().save(QString("%1/11-chop.png").arg(dir));
        tap(w, "cdone");
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
    }
    if (qEnvironmentVariableIsSet("VREQUENCY_HELP_SHOTS")) { // every page of the built-in guide, for proof-reading
        w.setPage(MainWindow::PG_HELP);
        for (int i = 0; i < 4; ++i) { w.shot(); w.shot().save(QString("%1/help-%2.png").arg(dir).arg(i)); tap(w, "helppage"); }
    }
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
