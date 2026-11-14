// venv-vrequency self-test — Track: the sound picker, for a loop clip and for a beat clip.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/ui/zones.h"

using namespace td;

namespace selftest {
// the sound picker on a loop clip
void trackPickLoopSound(Context &ctx, TrackState &tr) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    const TrackData &T = w.trackData();
    press(w, Qt::Key_Return); w.shot();
    touch(w, tr.lanes.cell(LN_LOOPA, 0)); w.shot();
    tr.loopClip = T.lane[LN_LOOPA][0].clip;
    auto lp = [&]() -> const Placement & { return T.lane[LN_LOOPA][0]; };
    auto lc = [&]() -> const Clip & { return *T.clips[tr.loopClip]; };
    press(w, Qt::Key_I); w.shot();
    check(w.pickingSound() && w.soundKindShown() == MainWindow::PK_SYNTH, "I opens the sound picker on the kind the clip plays now");
    press(w, Qt::Key_Right); w.shot();
    check(w.soundKindShown() == MainWindow::PK_GUITAR, "right arrow moves to Guitar");
    const int undo0 = w.trackUndoDepth();
    press(w, Qt::Key_1); w.shot();
    check(lc().pick.on && QString(lc().pick.patch.name) == "Pluck" && !lp().pick.any() && lc().layers[0].patches[0].name != QString("Pluck"),
          "1 picks Pluck for every placement of the clip; the recorded sound stays inside it");
    check(w.trackUndoDepth() == undo0 + 1, "and that is one undo step");
    check(w.hearingAlone(), "picking a sound while nothing plays starts the clip alone");
    bool hv = false;
    for (int i = 0; i < 60 && !hv; ++i) { wait(50); w.shot(); hv = inst.uiState().trackPlaying && inst.uiState().voices > 0; }
    const uint32_t hh = inst.uiState().drumHits[D_HAT];
    wait(700);
    check(hv && w.hearingAlone() && inst.uiState().drumHits[D_HAT] == hh, "it is heard, and the beat lane is silent meanwhile");
    press(w, Qt::Key_Right); press(w, Qt::Key_Right); w.shot();
    check(w.soundKindShown() == MainWindow::PK_BASS, "right again is Bass, and it stops there");
    press(w, Qt::Key_1); w.shot();
    check(QString(lc().pick.patch.name) == "Sub" && lc().pick.octave == -1, "a bass sound puts the clip an octave down");
    press(w, Qt::Key_Up); w.shot();
    check(lc().pick.octave == 0, "up arrow lifts it an octave");
    tap(w, zn::minus(zn::sound_octave)); tap(w, zn::minus(zn::sound_octave)); tap(w, zn::minus(zn::sound_octave)); w.shot();
    check(lc().pick.octave == -2, "the Octave stepper stops two octaves down");
    tap(w, zn::plus(zn::sound_octave)); tap(w, zn::plus(zn::sound_octave)); w.shot();
    tap(w, zn::sound_scope); w.shot();
    check(lp().pick.own && QString(lp().pick.patch.name) == "Sub" && QString(lc().pick.patch.name) == "Sub", "Only this one: the placement takes the sound as its own");
    tap(w, zn::at(zn::sound_kind, 0)); w.shot(); tap(w, zn::at(zn::sound_cell, 2)); w.shot();
    check(QString(lp().pick.patch.name) == "Organ" && QString(lc().pick.patch.name) == "Sub", "now a pick changes this placement and leaves the clip's other copies alone");
    press(w, Qt::Key_Backspace); w.shot();
    check(lp().pick.own && !lp().pick.any() && QString(soundName(lc(), pickFor(lc(), lp()))) == QString(recordedSoundName(lc())),
          "Backspace: this one plays as recorded again, the others keep Sub");
    tap(w, zn::at(zn::sound_kind, 2)); w.shot(); tap(w, zn::at(zn::sound_cell, 0)); w.shot();
    check(w.hearingAlone() && inst.uiState().trackPlaying, "all the while the clip keeps going round alone");
    press(w, Qt::Key_Space); wait(200); w.shot();
    check(!w.hearingAlone() && !inst.uiState().trackPlaying, "Space stops it");
    press(w, Qt::Key_Return); w.shot();
    check(!w.pickingSound(), "Enter closes the picker");
}

// the sound picker on a beat clip: the kits
void trackPickBeatKit(Context &ctx, TrackState &tr) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    const TrackData &T = w.trackData();
    touch(w, tr.lanes.cell(LN_BEAT, T.lane[LN_BEAT][0].bar)); w.shot();
    tr.beatClip = T.lane[LN_BEAT][0].clip;
    tap(w, zn::track_sound); w.shot();
    check(w.pickingSound() && w.soundKindShown() == MainWindow::PK_DRUMS, "the Sound button on a beat clip offers the kits");
    tr.otherKit = (T.clips[tr.beatClip]->kit + 1) % kKits;
    press(w, Qt::Key_1 + tr.otherKit); w.shot();
    check(T.clips[tr.beatClip]->pick.kit == tr.otherKit && !T.clips[tr.beatClip]->pick.on, "a number picks another kit for the beat");
    press(w, Qt::Key_Up); press(w, Qt::Key_Right); w.shot();
    check(T.clips[tr.beatClip]->pick.octave == 0 && w.soundKindShown() == MainWindow::PK_DRUMS, "a beat has no octave and no other kinds");
    tap(w, zn::sound_as_recorded); w.shot();
    check(!T.clips[tr.beatClip]->pick.any(), "As recorded gives the beat its own kit back");
    press(w, Qt::Key_1 + tr.otherKit); w.shot();
    tap(w, zn::sound_done); wait(250); w.shot();
    check(!w.pickingSound() && !w.hearingAlone() && !inst.uiState().trackPlaying, "Done closes the picker and stops the clip");
    ctrl(w, Qt::Key_Z);
    check(!T.clips[tr.beatClip]->pick.any(), "undo takes the kit back");
    key(w, Qt::Key_Shift, true); ctrl(w, Qt::Key_Z); key(w, Qt::Key_Shift, false);
    check(T.clips[tr.beatClip]->pick.kit == tr.otherKit, "and redo picks it again");
}
} // namespace selftest
