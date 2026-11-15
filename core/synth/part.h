// venv-vrequency — a Part: one sound and the live modulation applied to it.
#pragma once
#include "core/synth/params.h"

namespace td {

// A Part is one independent instrument layer: the live lead, the chord pads, or a
// loop layer. Each owns its own patch so the pads and loops keep their sound when
// the lead changes.
struct Part {
    Patch patch;
    float bend = 0;     // semitones
    float modCut = 0;   // octaves added to cutoff
    float modRes = 0;   // added to resonance
    float modVib = 0;   // cents added to vibrato depth
    float modMix = 0;   // added to osc B mix (timbre morph)
    float modDrive = 0; // added to drive
    float modDly = 0;   // added to delay send
    float modRev = 0;   // added to reverb send
    float gain = 1.0f;
    void clearMod() { modCut = modRes = modVib = modMix = modDrive = modDly = modRev = 0; }
};

} // namespace td
