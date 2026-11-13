// Tendril — built-in self-test. Drives the real window with real Qt key and
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

int renderShots(MainWindow &w, Instrument &inst, const QString &dir);

int runSelfTest(MainWindow &w, Instrument &inst, AudioOut &audio, const QString &shotsDir) {
    std::printf("Tendril self-test\n");
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
    if (!shotsDir.isEmpty()) renderShots(w, inst, shotsDir);
    {
        bool inside = true; int count = 0;
        auto sweep = [&] {
            w.shot();
            for (const QString &z : w.zoneNames()) {
                QRectF r = w.zoneRect(z);
                ++count;
                if (r.left() < 0 || r.top() < 0 || r.right() > 720 || r.bottom() > 720) inside = false;
            }
        };
        for (int m = 0; m < MainWindow::M_COUNT; ++m) { w.setMenu(m); sweep(); }
        for (int v = 1; v < MainWindow::V_COUNT; ++v) { w.setView(v); sweep(); }
        w.setMenu(MainWindow::M_SOUND);
        w.shot();
        check(inside && count > 400, QString("every control of every menu and view lies inside 720x720 (%1 zones checked)").arg(count));
        bool need = true;
        for (const char *z : {"timediv", "glide", "entry", "cutoff", "reso", "fenv", "drive", "delay", "reverb", "volume", "soft0", "soft5", "softlbl0", "xy",
                              "menu0", "menu6", "autoscale", "fmode0", "fmode2", "loop", "loopstop", "loopundo", "backplay", "xymap", "xyhold", "power", "drive",
                              "slot0", "slot9", "string0", "string1", "string2", "sus", "chordrow", "octdn", "octup"})
            if (w.zoneRect(z).isNull()) { need = false; std::printf("        missing control: %s\n", z); }
        check(need, "all ten knobs, the soft keys, menu keys, run controls and key deck are present");
    }
    w.shot();
    {
        QElapsedTimer pt; pt.start();
        for (int i = 0; i < 40; ++i) w.shot();
        std::printf("  info  full panel redraw: %.1f ms per frame\n", pt.elapsed() / 40.0);
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
    key(w, Qt::Key_F, true); key(w, Qt::Key_F, false); wait(60);
    check(!QDir(store::baseDir() + "/favourites").entryList({"*.json"}).isEmpty(), "Ctrl+F saves it as a favourite file");
    key(w, Qt::Key_Z, true); key(w, Qt::Key_Z, false); wait(80);
    check(QString(inst.uiState().leadName) == before, "Ctrl+Z undoes back to the previous sound");
    key(w, Qt::Key_Control, false);
    w.setMenu(MainWindow::M_SOUND); wait(60);
    tap(w, "softlbl4"); wait(40);   // Recall: first tap hands it the entry knob (and lists the favourites)
    tap(w, "soft4"); wait(80);      // second tap, on the physical soft key, recalls
    check(QString(inst.uiState().leadName) == rnd, "favourite recalled with the Recall soft key");
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
    tap(w, "chordrow"); wait(60);
    w.shot();
    tap(w, "chord4"); wait(60);
    check(w.chordRowOn() && inst.uiState().progLen == 5, "the deck's chords key turns the bottom row into chord keys");
    tap(w, "chordrow"); wait(40);
    w.shot();

    std::printf("- front panel\n");
    key(w, Qt::Key_Control, false);
    key(w, Qt::Key_1, true); key(w, Qt::Key_1, false); wait(80);
    w.shot();
    auto drag = [&w](const QString &zone, qreal dx, qreal dy) {
        QPointF c = w.zoneRect(zone).center();
        w.pointer(9, c, MainWindow::PH_PRESS);
        for (int i = 1; i <= 6; ++i) w.pointer(9, c + QPointF(dx * i / 6.0, dy * i / 6.0), MainWindow::PH_MOVE);
        w.pointer(9, c + QPointF(dx, dy), MainWindow::PH_RELEASE);
        w.shot();
    };
    float c0 = w.leadPatch().p[P_CUTOFF];
    drag("cutoff", 0, -50); wait(80);
    float c1 = w.leadPatch().p[P_CUTOFF];
    check(c1 > c0 * 1.5f && std::fabs(inst.part(0).patch.p[P_CUTOFF] - c1) < 1.0f, QString("dragging the Cutoff knob up opens the filter in the engine (%1 -> %2 Hz)").arg((int)c0).arg((int)c1));
    drag("cutoff", 50, 0); wait(60);
    float c2 = w.leadPatch().p[P_CUTOFF];
    check(c2 > c1 && c2 / c1 < 1.0f + (c1 / c0 - 1.0f) * 0.5f, QString("dragging sideways is the fine adjustment (%1 -> %2 Hz)").arg((int)c1).arg((int)c2));
    drag("reso", 0, -40); drag("delay", 0, -40); drag("reverb", 0, 30); drag("fenv", 0, -20); drag("drive", 0, -30); wait(80);
    const Patch &ep = inst.part(0).patch, &lp = w.leadPatch();
    check(std::fabs(ep.p[P_RESO] - lp.p[P_RESO]) < 0.01f && std::fabs(ep.p[P_DLYMIX] - lp.p[P_DLYMIX]) < 0.01f && std::fabs(ep.p[P_REVMIX] - lp.p[P_REVMIX]) < 0.01f &&
          std::fabs(ep.p[P_FENV] - lp.p[P_FENV]) < 0.01f && std::fabs(ep.p[P_DRIVE] - lp.p[P_DRIVE]) < 0.01f && lp.p[P_DRIVE] > 0.1f,
          "Resonance, Envelope, Drive, Delay and Reverb knobs reach the engine");
    tap(w, "fmode2"); wait(60);
    check((int)inst.part(0).patch.p[P_FMODE] == F_BP, "the BP key switches the filter to band-pass");
    tap(w, "fmode0"); wait(40);
    float vol0 = inst.uiState().set.volume;
    drag("volume", 0, -30); wait(60);
    check(inst.uiState().set.volume > vol0 + 0.05f, "Volume knob raises the master volume");
    inst.set(S_VOLUME, 0.06f);
    drag("glide", 0, 60); wait(60);
    check(inst.uiState().set.glideScale < 0.8f, "Glide knob changes the glide speed");
    inst.set(S_GLIDESCALE, 1.0f);
    key(w, Qt::Key_A, true); wait(150);
    float nA = inst.uiState().str[1].note;
    int tb0 = w.timebaseIndex();
    drag("timediv", 0, -60); wait(80);
    check(w.timebaseIndex() > tb0 && std::fabs(inst.uiState().str[1].note - nA) < 0.001f, "Time/Div changes the picture and leaves the pitch alone");
    tap(w, "autoscale"); wait(60);
    w.shot();
    check(w.lastToast().startsWith("Auto-scale"), QString("Auto-scale fits the trace to the sounding note (%1)").arg(w.lastToast()));
    key(w, Qt::Key_A, false); wait(60);
    tap(w, "menu1"); wait(40); // Edit
    QString wt = w.softValue(0);
    tap(w, "softlbl0"); wait(20);
    w.turnEntry(4); wait(60);
    check(w.menu() == MainWindow::M_EDIT && w.softValue(0) != wt && (int)inst.part(0).patch.p[P_WAVE1] == (int)w.leadPatch().p[P_WAVE1],
          QString("Edit menu: soft key 1 + entry knob changes oscillator A's wave (%1 -> %2)").arg(wt, w.softValue(0)));
    float a0 = w.leadPatch().p[P_UNISON];
    tap(w, "soft2"); drag("entry", 0, -40); wait(60);
    check(w.leadPatch().p[P_UNISON] > a0 + 0.05f, "dragging the Entry knob turns the selected soft key's value");
    tap(w, "menu4"); wait(40); // Time
    float b0 = inst.uiState().set.bpm;
    tap(w, "softlbl0"); w.turnEntry(-10); wait(60);
    check(std::fabs(inst.uiState().set.bpm - (b0 - 10)) < 0.6f, "Time menu: the entry knob sets the tempo");
    inst.set(S_BPM, b0);
    key(w, Qt::Key_Control, true); key(w, Qt::Key_3, true); key(w, Qt::Key_3, false); key(w, Qt::Key_Control, false); wait(60);
    check(inst.uiState().set.metro, "Ctrl+3 presses soft key 3 (Click on)");
    w.pressSoft(2); wait(40);
    bool bp0 = inst.uiState().backPlaying;
    tap(w, "backplay"); wait(80);
    check(inst.uiState().backPlaying != bp0, "the Backing key starts and stops the backing");
    tap(w, "backplay"); wait(60);
    w.setMenu(MainWindow::M_SOUND); wait(40);
    {
        // The app repaints the key deck and the control panel on their own when only they change.
        // Such a partial repaint over an old picture must equal a full repaint.
        key(w, Qt::Key_Backspace, true); key(w, Qt::Key_Backspace, false); wait(250);
        QImage before = w.shot();
        key(w, Qt::Key_S, true); key(w, Qt::Key_Y, true); key(w, Qt::Key_4, true); wait(200);
        tap(w, "fmode1"); tap(w, "xyhold"); wait(60);
        const QRect deck(8, 472, 704, 242), panel(476, 30, 236, 436);
        QImage full = w.shot();
        QImage part = w.shotPartial(w.shotPartial(before, deck), panel);
        int diff = 0, changed = 0;
        QRect bad;
        for (const QRect &r : {deck, panel})
            for (int y = r.top(); y <= r.bottom(); ++y)
                for (int x = r.left(); x <= r.right(); ++x) {
                    // (clipped antialiasing differs by a shade on a few edge pixels; anything more is a real error)
                    QRgb a = part.pixel(x, y), b = full.pixel(x, y);
                    if (std::abs(qRed(a) - qRed(b)) > 40 || std::abs(qGreen(a) - qGreen(b)) > 40 || std::abs(qBlue(a) - qBlue(b)) > 40) { ++diff; bad |= QRect(x, y, 1, 1); }
                    if (before.pixel(x, y) != full.pixel(x, y)) ++changed;
                }
        if (diff && qEnvironmentVariableIsSet("TENDRIL_DEBUG_DIR")) {
            QString d = qEnvironmentVariable("TENDRIL_DEBUG_DIR");
            full.copy(bad.adjusted(-12, -12, 12, 12)).scaled(bad.width() * 6 + 144, bad.height() * 6 + 144).save(d + "/full.png");
            part.copy(bad.adjusted(-12, -12, 12, 12)).scaled(bad.width() * 6 + 144, bad.height() * 6 + 144).save(d + "/part.png");
            before.copy(bad.adjusted(-12, -12, 12, 12)).scaled(bad.width() * 6 + 144, bad.height() * 6 + 144).save(d + "/before.png");
        }
        if (diff) std::printf("        differing area: x %d..%d  y %d..%d\n", bad.left(), bad.right(), bad.top(), bad.bottom());
        check(diff == 0 && changed > 500, QString("partial repaints of the deck and panel match a full repaint (%1 pixels changed, %2 wrong)").arg(changed).arg(diff));
        key(w, Qt::Key_S, false); key(w, Qt::Key_Y, false); key(w, Qt::Key_4, false);
        tap(w, "fmode0"); tap(w, "xyhold"); wait(60);
        key(w, Qt::Key_1, true); key(w, Qt::Key_1, false); wait(60);
    }

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

// Pictures of every menu and view, with a chord held so the trace is alive.
int renderShots(MainWindow &w, Instrument &inst, const QString &dir) {
    QDir().mkpath(dir);
    static const char *menus[MainWindow::M_COUNT] = {"patch", "edit", "key", "chords", "time", "file", "utility"};
    static const char *views[MainWindow::V_COUNT] = {"scope", "help", "status", "keytest", "listen"};
    float vol = inst.uiState().set.volume;
    inst.set(S_VOLUME, 0.5f);
    key(w, Qt::Key_A, true); wait(200); key(w, Qt::Key_F, true);
    wait(400);
    for (int i = 0; i < 6; ++i) { wait(45); w.shot(); }
    tap(w, "autoscale");
    int n = 0;
    for (int m = 0; m < MainWindow::M_COUNT; ++m) {
        w.setMenu(m);
        for (int i = 0; i < 4; ++i) { wait(45); w.shot(); }
        w.shot().save(QString("%1/%2-%3.png").arg(dir).arg(n++).arg(menus[m]));
    }
    key(w, Qt::Key_F, false); key(w, Qt::Key_A, false);
    for (int v = 1; v < MainWindow::V_COUNT; ++v) {
        w.setView(v);
        wait(60);
        w.shot().save(QString("%1/%2-%3.png").arg(dir).arg(n++).arg(views[v]));
    }
    w.setMenu(MainWindow::M_SOUND);
    inst.set(S_VOLUME, vol);
    wait(200);
    return 0;
}
