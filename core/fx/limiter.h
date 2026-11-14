// venv-vrequency — master effect: the output limiter.
#pragma once
#include "core/dsp.h"
#include <cmath>

namespace td {

// Stereo-linked peak limiter followed by a soft clip: output can never exceed
// +-1.0 whatever the patch or polyphony.
struct Limiter {
    float env = 0, gain = 1, relCoef = 0.0002f;
    float reduction = 1; // for metering
    void init(float sr) { relCoef = 1.0f - std::exp(-1.0f / (0.12f * sr)); }
    static inline float softClip(float x) {
        const float t = 0.8f;
        float a = std::fabs(x);
        if (a <= t) return x;
        float y = t + (1.0f - t) * fastTanh((a - t) / (1.0f - t)) * 0.95f;
        return x < 0 ? -y : y;
    }
    void process(float *l, float *r, int n) {
        const float thr = 0.75f;
        float minG = 1;
        for (int i = 0; i < n; ++i) {
            float p = std::fabs(l[i]); float q = std::fabs(r[i]);
            if (q > p) p = q;
            if (p > env) env = p; else env -= relCoef * env;
            float g = env > thr ? thr / env : 1.0f;
            gain += (g < gain ? 0.25f : 0.002f) * (g - gain);
            if (gain < minG) minG = gain;
            l[i] = softClip(l[i] * gain);
            r[i] = softClip(r[i] * gain);
        }
        reduction = minG;
    }
};

} // namespace td
