// venv-vrequency — a musical session: what snapshot() copies out of the instrument and restore() puts back.
#pragma once
#include "core/instrument/limits.h"
#include "core/instrument/loop_event.h"
#include "core/instrument/settings.h"
#include "core/freq/freq.h"
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
    Patch lead, backing;
    int prog[kProgMax] = {0};
    int progLen = 0;
    int holdChord = -1;
    int droneMask = 0;
    double loopBeats = 0;
    std::vector<LayerData> layers;
    PulsePad pulse[kPulsePads];  // the pulse pads' settings
    FreqState freq;              // the Freq tones (HOLD and GATE are never restored on)
    SessionData() { for (int p = 0; p < kPulsePads; ++p) pulse[p] = pulseDefault(p); }
};

} // namespace td
