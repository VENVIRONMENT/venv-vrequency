// venv-vrequency — the small structs the engine keeps its state in: a string's held keys, a looper
// layer, a lane's play position, the backing saved while a track plays.
#pragma once
#include "core/instrument/limits.h"
#include "core/instrument/loop_event.h"
#include "core/synth/params.h"
#include <vector>

namespace td {

struct StringState {
    int col[kMaxHeld];
    float pitch[kMaxHeld];
    int n = 0;
    bool pendingOff = false;
};
struct Layer {
    std::vector<LoopEv> ev;
    int n = 0;
    Patch patches[kLayerPatches];
    int nPatches = 0;
    int cursor = 0;
};
struct LaneState { int pl = -1, piece = -1; double lastLp = -1; };
struct LiveBacking { int prog[kProgMax]; int progLen, holdChord, droneMask, backStyle, chordBeats, arpRate; bool seventh; Patch patch; };

} // namespace td
