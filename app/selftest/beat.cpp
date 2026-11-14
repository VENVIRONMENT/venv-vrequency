// venv-vrequency self-test — the Beat page: pads, recording into the bar, Erase, the kit.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include "app/ui/zones.h"
#include <QJsonArray>
#include <QJsonObject>
#include <cstdio>

using namespace td;

namespace selftest {
void testBeat(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    std::printf("- beat\n");
    w.setPage(PG_BEAT); wait(100);
    inst.set(S_BEATVOL, 0.3f); inst.set(S_BPM, 120); wait(60);
    uint32_t k0 = inst.uiState().drumHits[D_KICK];
    tap(w, zn::at(zn::pad, 0)); wait(80);
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
    tap(w, zn::at(zn::pad, 2)); wait(120);
    UiState bu = inst.uiState();
    check(bu.beatPat[D_SNARE] != 0 && bu.beatPat[D_HAT] != 0, "hits played while Rec is on are written into the bar");
    uint32_t s0 = bu.drumHits[D_SNARE];
    wait(2100);
    check(inst.uiState().drumHits[D_SNARE] > s0, "and the bar plays them back");
    tap(w, zn::beaterase); wait(60); tap(w, zn::at(zn::pad, 1)); wait(100);
    check(inst.uiState().beatPat[D_SNARE] == 0 && inst.uiState().beatPat[D_HAT] != 0, "Erase, then a pad, clears that drum only");
    int kit0 = inst.uiState().set.kit;
    tap(w, zn::plus(zn::kit)); wait(80);
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
}
} // namespace selftest
