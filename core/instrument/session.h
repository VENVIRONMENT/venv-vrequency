// venv-vrequency — a musical session: what snapshot() copies out of the instrument and restore() puts back.
#pragma once
#include "core/drums/drums.h"
#include "core/instrument/limits.h"
#include "core/instrument/loop_event.h"
#include "core/instrument/settings.h"
#include "core/synth/params.h"
#include <cstdint>
#include <vector>

namespace td {

struct LayerData {
    std::vector<LoopEv> ev;
    std::vector<Patch> patches;
};

// Everything needed to save and restore a musical session.
struct SessionData {
    Settings settings;
    Patch lead, backing;
    int prog[kProgMax] = {0};
    int progLen = 0;
    int holdChord = -1;
    int droneMask = 0;
    double loopBeats = 0;
    std::vector<LayerData> layers;
    uint16_t beat[kDrums] = {0}; // one bit per sixteenth-note step, bit 0 = step 1
};

} // namespace td
