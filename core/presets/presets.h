// venv-vrequency — factory sounds, the sound randomiser and loudness normalisation.
#pragma once
#include "core/dsp.h"
#include "core/synth/params.h"

namespace td {

constexpr int kFactoryCount = 10;
Patch factoryPatch(int index);

// Extra sounds outside the ten number-key slots; the app offers them as favourites.
// All five are built around a sine wave: soft, slow, washed in chorus and reverb.
constexpr int kExtraCount = 5;
Patch extraPatch(int index);

// Generate a brand-new playable sound. Every result is loudness-normalised.
Patch randomPatch(Rng &rng);
// Nudge an existing sound; amount 0..1.
Patch mutatePatch(const Patch &src, Rng &rng, float amount);

// Measure the patch by rendering it offline and set P_LEVEL so every sound
// lands at the same perceived loudness. Returns the measured peak after scaling.
float normalisePatch(Patch &p, float sr = 48000.0f);

} // namespace td
