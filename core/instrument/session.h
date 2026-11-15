// venv-vrequency — a musical session: what snapshot() copies out of the instrument and restore() puts back.
#pragma once
#include "core/instrument/limits.h"
#include "core/instrument/loop_event.h"
#include "core/instrument/settings.h"
#include "core/pulse/pulse.h"
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
    Patch lead, pads;            // pads: the chord pads' sound
    int holdChord = -1;
    int droneMask = 0;
    double loopBeats = 0;
    std::vector<LayerData> layers;
    PulsePad pulse[kPulsePads];  // the pulse pads' settings
    SessionData() { for (int p = 0; p < kPulsePads; ++p) pulse[p] = pulseDefault(p); }
};

} // namespace td
