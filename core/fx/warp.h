// venv-vrequency — the warp stage: drive, fold, crush and a low-pass, one mono unit any sound can use.
//
// In order: DRIVE (a soft saturation blended in), FOLD (a sine wave-folder blended in), CRUSH (off,
// then fewer bits and a lower sample rate from one amount), and a 12 dB/octave low-pass made of two
// one-pole filters. Every stage keeps the peak: a signal inside +-1 comes out inside +-1, whatever
// the settings (drive and fold are blends of curves bounded by 1; crush rounds to a grid inside
// +-1; the filter's impulse response is positive and sums to one). Settings glide over ~20 ms.
// No allocation, no locks: process() runs on the audio thread.
#pragma once

namespace td {

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
    float lp1 = 0, lp2 = 0, held = 0, ph = 1;
};

} // namespace td
