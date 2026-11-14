// venv-vrequency — the documentation pictures of the Track page: a small arrangement, then Chop, the
// sound picker and the mixer on it (see render_shots.cpp).
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "core/presets/presets.h"
#include "app/ui/zones.h"
#include <array>
#include <initializer_list>

using namespace td;

namespace selftest {
namespace {
// one loop layer: each note is {beat, length in beats, MIDI note}
LayerData melody(const Patch &sound, std::initializer_list<std::array<double, 3>> notes) {
    LayerData ld; ld.patches.push_back(sound);
    uint32_t seq = 0;
    for (const auto &n : notes) {
        LoopEv a; a.beat = n[0]; a.op = OP_ON; a.slot = 0; a.a = (float)n[2]; a.b = 1; a.seq = seq++;
        LoopEv b; b.beat = n[0] + n[1]; b.op = OP_OFF; b.slot = 0; b.a = b.b = 0; b.seq = seq++;
        ld.ev.push_back(a); ld.ev.push_back(b);
    }
    return ld;
}

// a small arrangement for the Track page: two beats, a progression and two loops
void arrange(MainWindow &w, Instrument &inst) {
    SessionData sd; inst.snapshot(sd);
    sd.prog[0] = 5; sd.prog[1] = 3; sd.prog[2] = 0; sd.prog[3] = 4; sd.progLen = 4; sd.settings.chordBeats = 4;
    sd.backing = extraPatch(1);
    sd.loopBeats = 8;
    sd.layers = {melody(extraPatch(0), {{0, 1.5, 64}, {1.5, 1.5, 67}, {3.5, 0.5, 69}, {4, 2, 72}, {6, 1, 71}, {7, 0.9, 67}})};
    inst.restore(sd); wait(80);
    w.setPage(PG_TRACK); wait(60);
    tap(w, zn::track_keep); wait(60);                       // Beat 1, Chords 1, Loop 1
    inst.beatDo(BT_ERASE, D_HAT); inst.beatDo(BT_STEP, D_CLAP, 4, 1); inst.beatDo(BT_STEP, D_TOM, 13, 1); wait(80);
    inst.snapshot(sd);
    sd.loopBeats = 16;
    sd.layers = {melody(extraPatch(1), {{0, 3, 52}, {4, 2, 55}, {6, 2, 57}, {8, 3.5, 60}, {12, 1, 59}, {13, 1, 57}, {14, 1.9, 55}}),
                 melody(extraPatch(1), {{2, 1, 76}, {7, 0.5, 79}, {10, 2, 81}, {15, 0.5, 79}})};
    inst.restore(sd); wait(80);
    tap(w, zn::track_keep); wait(60);                       // Beat 2, Loop 2
    press(w, Qt::Key_Escape);
    press(w, Qt::Key_Return);
    press(w, Qt::Key_1); for (int i = 0; i < 4; ++i) ctrl(w, Qt::Key_V);                 // Beat 1 on bars 1-4
    press(w, Qt::Key_4); ctrl(w, Qt::Key_V);                                              // Beat 2 on bar 5
    press(w, Qt::Key_1); for (int i = 0; i < 3; ++i) ctrl(w, Qt::Key_V);                 // Beat 1 on bars 6-8
    press(w, Qt::Key_Return); press(w, Qt::Key_2); ctrl(w, Qt::Key_V); ctrl(w, Qt::Key_V);     // Chords 1 twice
    const LaneGrid lanes{w.zoneRect(zn::track_lanes)};
    press(w, Qt::Key_3); touch(w, lanes.cell(LN_LOOPA, 2)); touch(w, lanes.cell(LN_LOOPA, 6)); // Loop 1 on bars 3-4 and 7-8
    press(w, Qt::Key_5); touch(w, lanes.cell(LN_LOOPB, 4));                               // Loop 2 on bars 5-8
    press(w, Qt::Key_Escape);
    touch(w, lanes.cell(LN_BEAT, 0)); ctrl(w, Qt::Key_J); ctrl(w, Qt::Key_J); ctrl(w, Qt::Key_J); // the four beat bars become one clip
    touch(w, lanes.cell(LN_LOOPA, 2));
    press(w, Qt::Key_Return);
    key(w, Qt::Key_Shift, true); press(w, Qt::Key_Right); press(w, Qt::Key_Right); key(w, Qt::Key_Shift, false); // play line on bar 3
    press(w, Qt::Key_C);                                                                  // cycle round the chosen loop
    press(w, Qt::Key_3);                                                                  // Loop 1 in hand again
    touch(w, lanes.cell(LN_LOOPA, 2));
}

// Chop for the chosen loop, cut in four with a piece moved
void shootChop(MainWindow &w, const QString &dir) {
    press(w, Qt::Key_Escape);
    press(w, Qt::Key_C);
    press(w, Qt::Key_E);
    w.shot();
    QRectF strip = w.zoneRect(zn::chop_strip);
    for (double f : {0.25, 0.5, 0.75}) { touch(w, QPointF(strip.left() + strip.width() * f + 2, strip.center().y())); w.shot(); }
    touch(w, QPointF(strip.left() + strip.width() * 0.6, strip.center().y())); w.shot(); // choose piece C
    tap(w, zn::chop_move_left); w.shot();
    wait(1700);
    for (int i = 0; i < 5; ++i) { w.shot(); wait(40); }
    w.shot().save(QString("%1/11-chop.png").arg(dir));
    tap(w, zn::chop_done);
    w.shot();
}

// the sound picker for the chosen loop
void shootSoundPicker(MainWindow &w, Instrument &inst, const QString &dir) {
    press(w, Qt::Key_I); w.shot();
    press(w, Qt::Key_Right); w.shot();
    press(w, Qt::Key_1); w.shot();                          // Guitar, Pluck
    press(w, Qt::Key_Space);                                // not playing in the picture
    wait(1900);
    for (int i = 0; i < 5; ++i) { w.shot(); wait(40); }
    w.shot().save(QString("%1/12-sound.png").arg(dir));
    press(w, Qt::Key_Return);
    inst.cmd(EV_PANIC); wait(60);
}

// the mixer, playing
void shootMixer(MainWindow &w, Instrument &inst, const QString &dir) {
    press(w, Qt::Key_X); w.shot();
    press(w, Qt::Key_4); press(w, Qt::Key_M);                                // Loop B muted
    press(w, Qt::Key_2); for (int i = 0; i < 3; ++i) press(w, Qt::Key_Down); // Chords at 65
    press(w, Qt::Key_3); press(w, Qt::Key_Up); press(w, Qt::Key_Up);         // Loop A at 90
    press(w, Qt::Key_1);
    tap(w, zn::mixer_play);
    wait(2600);
    for (int i = 0; i < 5; ++i) { w.shot(); wait(40); }
    w.shot().save(QString("%1/13-mixer.png").arg(dir));
    tap(w, zn::mixer_play);
    inst.cmd(EV_PANIC); wait(60);
}
} // namespace

void renderTrackShots(MainWindow &w, Instrument &inst, const QString &dir) {
    arrange(w, inst);
    wait(1700);                                             // let the notices fade
    for (int i = 0; i < 5; ++i) { w.shot(); wait(40); }
    w.shot().save(QString("%1/10-track.png").arg(dir));
    shootChop(w, dir);
    shootSoundPicker(w, inst, dir);
    shootMixer(w, inst, dir);
}
} // namespace selftest
