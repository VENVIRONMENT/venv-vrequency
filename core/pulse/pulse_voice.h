// venv-vrequency — one pulse sound being played: the generator behind every pad.
//
// The same code makes the sound on the audio thread and the pictures on the Pulse page
// (pulseRender), so what is drawn is what is heard. No allocation; noise starts from the same
// seed on every hit, so every hit of a pad is exactly the same sound.
#pragma once
#include "core/dsp.h"
#include "core/pulse/pulse.h"
#include <cstdint>

namespace td {

class PulseVoice {
public:
    // rateHz: the rate to use (SYNC already applied). endless: run until release() (a pad going:
    // every kind but the click and the tick, which have nothing to hold and are hit again instead).
    // A held one-shot keeps its sound at the level it settles to. delay: samples of silence
    // before it starts (sample-exact timing). owner: the part whose bend it follows (0 live,
    // 2.. a loop layer).
    void start(int pad, const PulsePad &p, float sr, float rateHz, bool endless, int delay = 0, int owner = 0);
    void setBend(float semitones);                  // the tone follows the owner's bend
    void retune(const PulsePad &p, float rateHz);   // live changes on an endless voice
    void release(float ms = 2.0f);                  // fade out quickly
    bool active() const { return on; }
    bool endless() const { return runOn; }
    bool releasing() const { return fading; }
    int pad() const { return padNo; }
    int owner() const { return ownerPart; }
    int age() const { return pos; }
    // One sample. `onset` is set where a hit, a click of a train or a gate begins, and `dur`
    // to how long that part lasts in samples (0 for a click).
    float next(bool &onset, int &dur);

    static int lengthSamples(const PulsePad &p, float sr);   // how long one hit lasts
    static int tickWidth(float tone, float sr);              // the tick's 1..3 samples

private:
    bool on = false, runOn = false, fading = false;
    int padNo = 0, type = 0, pos = 0, len = 1, wait = 0, fadeLen = 1, ownerPart = 0;
    float baseTone = 1000, bendSemis = 0; // the pad's tone, and the bend on it (tone = the two together)
    float sr = 48000, tone = 1000, rate = 4, level = 0.8f, relGain = 1, relCoef = 0.99f;
    float decay = 1, decayCoef = 1;   // exponential envelope, multiplied each sample
    double ph = 0, gatePh = 0;        // sine phase, gate / click phase (0..1)
    float gate = 0, gateK = 0.05f;    // the smoothed gate of the gated tone
    float clickTau = 0.001f;
    int clickAt = 0;                  // the sample the current click of a train began
    float lp = 0, lpA = 1, lpNorm = 1;            // noise burst: one-pole low-pass
    float ic1 = 0, ic2 = 0, svA1 = 0, svA2 = 0, svA3 = 0, svK = 0.5f, svNorm = 1; // hiss: band-pass
    Rng rng;
    void setFilters();
};

// The sound of a pad offline, as the audio thread would play it: n samples into out (the rest is
// silence). Returns how many onsets (hits, clicks, gates) fall inside those samples.
int pulseRender(const PulsePad &p, float sr, float rateHz, float *out, int n);

} // namespace td
