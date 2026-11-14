// venv-vrequency — master effect: reverb. The buffers are allocated in init(); process() never allocates.
#pragma once
#include "core/dsp.h"
#include <vector>

namespace td {

// Schroeder/Moorer "Freeverb" topology: 8 damped combs + 4 allpasses a side.
struct Reverb {
    struct Comb {
        std::vector<float> b; int i = 0; float store = 0;
        inline float tick(float x, float fb, float damp) {
            float y = b[i];
            store = y * (1.0f - damp) + store * damp;
            b[i] = x + store * fb;
            if (++i >= (int)b.size()) i = 0;
            return y;
        }
    };
    struct Allpass {
        std::vector<float> b; int i = 0;
        inline float tick(float x) {
            float y = b[i];
            b[i] = x + y * 0.5f;
            if (++i >= (int)b.size()) i = 0;
            return y - x;
        }
    };
    Comb cl[8], cr[8];
    Allpass al[4], ar[4];
    void init(float sr) {
        static const int C[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        static const int A[4] = {556, 441, 341, 225};
        float k = sr / 44100.0f;
        for (int i = 0; i < 8; ++i) {
            cl[i].b.assign((int)(C[i] * k), 0.0f);
            cr[i].b.assign((int)((C[i] + 23) * k), 0.0f);
        }
        for (int i = 0; i < 4; ++i) {
            al[i].b.assign((int)(A[i] * k), 0.0f);
            ar[i].b.assign((int)((A[i] + 23) * k), 0.0f);
        }
    }
    void process(const float *in, float *outL, float *outR, int n, float size) {
        float fb = 0.70f + 0.28f * clampf(size, 0.0f, 1.0f);
        const float damp = 0.28f;
        for (int s = 0; s < n; ++s) {
            float x = in[s] * 0.03f;
            float l = 0, r = 0;
            for (int i = 0; i < 8; ++i) { l += cl[i].tick(x, fb, damp); r += cr[i].tick(x, fb, damp); }
            for (int i = 0; i < 4; ++i) { l = al[i].tick(l); r = ar[i].tick(r); }
            outL[s] += l;
            outR[s] += r;
        }
    }
};

} // namespace td
