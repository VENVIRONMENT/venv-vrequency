// venv-vrequency — the numbers behind the Sound page's settings pictures. Plain C++, no Qt: each
// function takes a patch's parameter array and fills a curve using the engine's own oscillator,
// envelope and filter maths (core/dsp.h), so a picture can only show what the sound really does.
#pragma once
#include "core/synth/params.h"

namespace td {

// One cycle of oscillator A (with FM, its mix and the sub added) and of B (scaled by its mix), before the filter.
struct OscCurve {
    static const int N = 256;
    float a[N + 1], b[N + 1];
    float scale = 1;                // the larger swing of A and B: the picture's full height
    bool bOn = false;
    float bSemis = 0;               // B's pitch against A, in semitones (semi2 + detune)
};
void oscCurve(const float *p, OscCurve &c);

// The filter's magnitude in dB on a log axis from kFreqLo to kFreqHi; env: the cutoff at the top of the filter envelope.
constexpr float kFreqLo = 30.0f, kFreqHi = 16000.0f, kFilterSr = 48000.0f;
struct FilterCurve {
    static const int N = 160;
    float db[N + 1], dbEnv[N + 1];
    float cutoff = 0, cutDb = 0, envCutoff = 0;
    bool envOn = false;
};
float filterDb(int mode, float cutoff, float reso, float hz);
void filterCurve(const float *p, FilterCurve &c);
float freqAt(int i, int n);                    // the i-th of n+1 log-spaced points

// The amp envelope on a square-root time axis: x = sqrt(t / total). The sustain is drawn as a
// fixed half second hold, then the key is let go.
struct EnvCurve {
    static const int N = 200;
    float y[N + 1];
    float tA = 0, tD = 0, tH = 0, tR = 0, total = 1;   // attack, decay, the drawn hold, release (seconds)
    float xA() const { return xOf(tA); }               // x at the end of attack, decay, hold
    float xD() const { return xOf(tA + tD); }
    float xH() const { return xOf(tA + tD + tH); }
    float xOf(float t) const;
    float yOf(float x) const;
};
void envCurve(const float *p, EnvCurve &c);

} // namespace td
