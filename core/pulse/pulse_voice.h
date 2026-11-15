// venv-vrequency — one pulse sound being played: the generator behind every pad.
//
// The same code makes the sound on the audio thread and the pictures on the Pulse page
// (pulseRender), so what is drawn is what is heard. No allocation; noise starts from the same
// seed on every hit, so every hit of a pad is exactly the same sound.
#pragma once
#include "core/dsp.h"
#include "core/fx/warp.h"
#include "core/pulse/pulse.h"
#include <cstdint>

namespace td {

class PulseVoice {
public:
    // rateHz: the rate to use (SYNC already applied). endless: run until release() (latched gated
    // tone and pulse train). delay: samples of silence before it starts (sample-exact timing).
    void start(int pad, const PulsePad &p, float sr, float rateHz, bool endless, int delay = 0);
    void retune(const PulsePad &p, float rateHz);   // live changes on an endless voice
    void release(float ms = 2.0f);                  // fade out quickly
    bool active() const { return on; }
    bool endless() const { return runOn; }
    bool releasing() const { return fading; }
    int pad() const { return padNo; }
    int age() const { return pos; }
    // One sample. `onset` is set where a hit, a click of a train or a gate begins, and `dur`
    // to how long that part lasts in samples (0 for a click).
    float next(bool &onset, int &dur);

    static int lengthSamples(const PulsePad &p, float sr);   // how long one hit lasts

private:
    bool on = false, runOn = false, fading = false;
    int padNo = 0, type = 0, pos = 0, len = 1, wait = 0, fadeLen = 1;
    float sr = 48000, tone = 1000, rate = 4, level = 0.8f, relGain = 1, relCoef = 0.99f;
    float decay = 1, decayCoef = 1;   // the main exponential envelope, multiplied each sample
    float decay2 = 1, decay2Coef = 1; // a second one: the snap's noise, the tom's strike
    double ph = 0;                    // sine phase (0..1)
    double sq[6] = {0};               // metal: the six squares' phases
    float hpA = 0, hp[4] = {0};       // metal: two one-pole high-passes (in and out of each)
    float ic1 = 0, ic2 = 0, svA1 = 0, svA2 = 0, svA3 = 0, svK = 0.5f, svNorm = 1; // a band-pass (metal, snap, clap)
    Rng rng;
    WarpShape shape;                  // DRIVE and FOLD (the shared warp curves)
    WarpGrid grid;                    // CRUSH
    bool warpOn = false, gridOn = false;
    float tone0 = 1000;
    double bendMul = 1, bendStep = 1; // BEND: tone = tone0 * bendMul, moving each sample
    void setFilters();
    void setWarp(const PulsePad &p); // after tone and len are set
    float band(float x);              // one sample through the band-pass
};

// The sound of a pad offline, as the audio thread would play it: n samples into out (the rest is
// silence). Returns how many onsets (hits, clicks, gates) fall inside those samples.
int pulseRender(const PulsePad &p, float sr, float rateHz, float *out, int n);

} // namespace td
