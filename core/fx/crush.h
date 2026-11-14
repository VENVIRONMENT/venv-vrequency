// venv-vrequency — master effect: the crusher.
#pragma once
#include "core/dsp.h"
#include <cmath>

namespace td {

// Bit crusher and sample-rate reducer: the GLITCH touch-pad mapping. `coarse` holds each
// sample for longer (lower sample rate), `grit` leaves fewer levels (lower bit depth). The
// levels are measured against the signal's own peak, so it sounds the same at any volume.
struct Crush {
    float target = 0, mix = 0, coarse = 0, grit = 0;
    float ph = 1, hl = 0, hr = 0, env = 0.05f;
    float mixCoef = 0.005f, relCoef = 0.00005f, rateScale = 1;
    void init(float sr) {
        mixCoef = 1.0f - std::exp(-1.0f / (0.004f * sr));
        relCoef = 1.0f - std::exp(-1.0f / (0.4f * sr));
        rateScale = sr / 48000.0f;
    }
    void set(bool on, float x, float y) {
        target = on ? 1.0f : 0.0f;
        if (on) { coarse = clampf(x, 0, 1); grit = clampf(y, 0, 1); }
    }
    bool on() const { return target > 0.5f; }
    bool active() const { return target > 0.5f || mix > 1e-4f; }
    float holdSamples() const { return (2.0f + coarse * coarse * 46.0f) * rateScale; }
    float bits() const { return 12.0f - 9.0f * grit; }
    void process(float *l, float *r, int n) {
        if (!active()) return;
        const float inc = 1.0f / holdSamples();
        const float q = std::pow(2.0f, bits() - 1.0f);
        for (int i = 0; i < n; ++i) {
            float a = std::fabs(l[i]), b = std::fabs(r[i]);
            if (b > a) a = b;
            if (a > env) env = a; else env -= relCoef * env;
            mix += mixCoef * (target - mix);
            ph += inc;
            if (ph >= 1.0f) {
                ph -= 1.0f;
                const float k = q * 0.7f / (env > 0.02f ? env : 0.02f);
                hl = std::floor(l[i] * k + 0.5f) / k;
                hr = std::floor(r[i] * k + 0.5f) / k;
            }
            l[i] += mix * (hl - l[i]);
            r[i] += mix * (hr - r[i]);
        }
    }
};

} // namespace td
