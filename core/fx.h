// venv vrequency — master effects: chorus, tempo delay, reverb, crush, limiter.
// All buffers are allocated in init(); process paths never allocate.
#pragma once
#include "dsp.h"
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
    inline float tap(float delaySamples) const {
        float rp = (float)w - delaySamples;
        int i0 = (int)std::floor(rp);
        float fr = rp - i0;
        float a = buf[i0 & mask], b = buf[(i0 + 1) & mask];
        return a + (b - a) * fr;
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
            outL[i] += tap(base + depth * m1);
            outR[i] += tap(base + depth * m2);
            ++w;
        }
    }
};

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
    inline float tap(const std::vector<float> &b, float d) const {
        float rp = (float)w - d;
        int i0 = (int)std::floor(rp);
        float fr = rp - i0;
        float a = b[i0 & mask], c = b[(i0 + 1) & mask];
        return a + (c - a) * fr;
    }
    // in: mono send. Ping-pong feedback with gentle damping.
    void process(const float *in, float *outL, float *outR, int n, float seconds, float fb) {
        float targ = clampf(seconds, 0.02f, 2.0f) * sr;
        for (int i = 0; i < n; ++i) {
            cur += 0.0004f * (targ - cur);
            float dl = tap(bl, cur), dr = tap(br, cur);
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
