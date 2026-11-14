// venv-vrequency — master effect: chorus. The buffer is allocated in init(); process() never allocates.
#pragma once
#include "core/dsp.h"
#include "core/fx/ring.h"
#include <cmath>
#include <vector>

namespace td {

struct Chorus {
    std::vector<float> buf;
    int w = 0, mask = 0;
    float ph = 0, sr = 48000;
    void init(float sr_) {
        sr = sr_;
        int n = 1; while (n < (int)(sr * 0.05f)) n <<= 1;
        buf.assign(n, 0.0f); mask = n - 1; w = 0;
    }
    void process(const float *in, float *outL, float *outR, int n) {
        float inc = 0.55f / sr;
        for (int i = 0; i < n; ++i) {
            buf[w & mask] = in[i];
            ph += inc; if (ph >= 1.0f) ph -= 1.0f;
            float m1 = fastSin(ph), ph2 = ph + 0.25f;
            if (ph2 >= 1.0f) ph2 -= 1.0f;
            float m2 = fastSin(ph2);
            float base = 0.014f * sr, depth = 0.004f * sr;
            outL[i] += ringTap(buf, mask, w, base + depth * m1);
            outR[i] += ringTap(buf, mask, w, base + depth * m2);
            ++w;
        }
    }
};

} // namespace td
