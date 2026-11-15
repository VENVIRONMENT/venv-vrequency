// venv-vrequency — the warp stage (see warp.h).
#include "core/fx/warp.h"
#include "core/dsp.h"
#include <cmath>

namespace td {

void warpClamp(WarpParams &p) {
    auto fin = [](float v, float lo, float hi, float d) { return std::isfinite(v) ? clampf(v, lo, hi) : d; };
    p.drive = fin(p.drive, 0, 1, 0);
    p.fold = fin(p.fold, 0, 1, 0);
    p.crush = fin(p.crush, 0, 1, 0);
    p.cutoff = fin(p.cutoff, kWarpCutMin, kWarpCutMax, kWarpCutMax);
}

float warpCrushBits(float c) { return c <= 0 ? 0 : 10.0f - 8.0f * c; }
float warpCrushHz(float c, float sr) { return c <= 0 ? sr : sr / (1.0f + 23.0f * c * c); }

void Warp::init(float sampleRate) {
    sr = sampleRate;
    glide = 1.0f - std::exp(-(float)kBlock / (0.007f * sr)); // per block of kBlock: ~20 ms to settle
    cur = target;
    reset();
}

void Warp::reset() { lp1 = lp2 = held = 0; ph = 1; }

void Warp::process(float *x, int n) {
    // the settings move once a block (kBlock samples), towards their targets
    const float k = n >= kBlock ? glide : glide * n / kBlock;
    cur.drive += k * (target.drive - cur.drive);
    cur.fold += k * (target.fold - cur.fold);
    cur.cutoff *= std::pow(target.cutoff / cur.cutoff, k);   // in octaves
    cur.crush = target.crush;                                // a grid does not glide
    const float d = cur.drive, gain = 1.0f + 24.0f * d, norm = 1.0f / fastTanh(gain);
    const float f = cur.fold, fgain = 1.0f + 3.0f * f;
    const bool crush = cur.crush > 0;
    const float q = crush ? std::pow(2.0f, warpCrushBits(cur.crush) - 1.0f) : 1;
    const float inc = crush ? warpCrushHz(cur.crush, sr) / sr : 1;
    const float a = std::exp(-2.0f * kPi * cur.cutoff / sr), b = 1.0f - a;
    for (int i = 0; i < n; ++i) {
        float s = x[i];
        if (d > 1e-4f) s += d * (fastTanh(s * gain) * norm - s);
        if (f > 1e-4f) {
            float p = s * fgain * 0.25f;                     // sin(pi/2 * s * fgain), in cycles
            p -= std::floor(p);
            s += f * (clampf(fastSin(p), -1, 1) - s);
        }
        if (crush) {
            ph += inc;
            if (ph >= 1.0f) { ph -= 1.0f; held = clampf(std::floor(s * q + 0.5f) / q, -1, 1); }
            s = held;
        }
        lp1 += b * (s - lp1);
        lp2 += b * (lp1 - lp2);
        x[i] = lp2;
    }
}

} // namespace td
