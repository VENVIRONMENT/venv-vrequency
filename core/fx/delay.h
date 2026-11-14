// venv-vrequency — master effect: tempo delay. The buffers are allocated in init(); process() never allocates.
#pragma once
#include "core/dsp.h"
#include "core/fx/ring.h"
#include <cmath>
#include <vector>

namespace td {

struct Delay {
    std::vector<float> bl, br;
    int w = 0, mask = 0;
    float sr = 48000, cur = 12000, lpL = 0, lpR = 0;
    void init(float sr_) {
        sr = sr_;
        int n = 1; while (n < (int)(sr * 2.1f)) n <<= 1;
        bl.assign(n, 0.0f); br.assign(n, 0.0f); mask = n - 1; w = 0;
        cur = sr * 0.3f;
    }
    // in: mono send. Ping-pong feedback with gentle damping.
    void process(const float *in, float *outL, float *outR, int n, float seconds, float fb) {
        float targ = clampf(seconds, 0.02f, 2.0f) * sr;
        for (int i = 0; i < n; ++i) {
            cur += 0.0004f * (targ - cur);
            float dl = ringTap(bl, mask, w, cur), dr = ringTap(br, mask, w, cur);
            lpL += 0.45f * (dl - lpL);
            lpR += 0.45f * (dr - lpR);
            bl[w & mask] = in[i] + lpR * fb;
            br[w & mask] = lpL * fb;
            outL[i] += dl;
            outR[i] += dr;
            ++w;
        }
    }
};

} // namespace td
