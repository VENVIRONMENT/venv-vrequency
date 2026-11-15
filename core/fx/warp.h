// venv-vrequency — the warp stage: drive, fold, crush and a low-pass, one mono unit any sound can use.
//
// In order: DRIVE (a soft saturation blended in), FOLD (a sine wave-folder blended in), CRUSH (off,
// then fewer bits and a lower sample rate from one amount), and a 12 dB/octave low-pass made of two
// one-pole filters. Every stage keeps the peak: a signal inside +-1 comes out inside +-1, whatever
// the settings (drive and fold are blends of curves bounded by 1; crush rounds to a grid inside
// +-1; the filter's impulse response is positive and sums to one). Settings glide over ~20 ms.
// No allocation, no locks: process() runs on the audio thread.
//
// The per-sample stages (WarpShape: drive and fold; WarpGrid: bits and a held sample rate) are
// public so the synth voices and the pulse pads warp with exactly the same curves, sample by sample.
#pragma once
#include "core/dsp.h"
#include <cmath>

namespace td {

// DRIVE and FOLD for one sample, with the amounts fixed for a stretch of samples.
struct WarpShape {
    float d = 0, gain = 1, norm = 1, f = 0, fgain = 1;
    void set(float drive, float fold) {
        d = drive; gain = 1.0f + 24.0f * d; norm = 1.0f / fastTanh(gain);
        f = fold; fgain = 1.0f + 3.0f * f;
    }
    bool on() const { return d > 1e-4f || f > 1e-4f; }
    float operator()(float s) const {
        if (d > 1e-4f) s += d * (fastTanh(s * gain) * norm - s);
        if (f > 1e-4f) {
            float p = s * fgain * 0.25f;                     // sin(pi/2 * s * fgain), in cycles
            p -= std::floor(p);
            s += f * (clampf(fastSin(p), -1, 1) - s);
        }
        return s;
    }
};

// Fewer bits (a grid of 2^(bits-1) steps each side, 0: none) and a lower sample rate (`keep` of the
// rate is kept, 1: all): the sample is rounded to the grid and held until the next slow tick.
struct WarpGrid {
    float q = 0, inc = 1, held = 0, ph = 1;
    void set(float bits, float keep) { q = bits > 0 ? std::pow(2.0f, bits - 1.0f) : 0; inc = keep; }
    void reset() { held = 0; ph = 1; }
    bool on() const { return q > 0 || inc < 1; }
    float operator()(float s) {
        ph += inc;
        if (ph >= 1.0f) { ph -= 1.0f; held = q > 0 ? clampf(std::floor(s * q + 0.5f) / q, -1, 1) : s; }
        return held;
    }
};

constexpr float kWarpCutMin = 40, kWarpCutMax = 18000;   // Hz, the low-pass

struct WarpParams {
    float drive = 0;          // 0..1
    float fold = 0;           // 0..1
    float crush = 0;          // 0 = off .. 1
    float cutoff = kWarpCutMax;
};

void warpClamp(WarpParams &p);
float warpCrushBits(float crush);       // the bit depth CRUSH leaves (0: off)
float warpCrushHz(float crush, float sr); // the sample rate CRUSH leaves

class Warp {
public:
    void init(float sampleRate);
    void reset();                        // clears the filter and the held sample
    void set(const WarpParams &p) { target = p; warpClamp(target); }
    const WarpParams &settings() const { return target; }
    void process(float *x, int n);       // in place

private:
    float sr = 48000, glide = 0.03f;
    WarpParams target, cur;
    WarpGrid grid;
    float lp1 = 0, lp2 = 0;
};

} // namespace td
