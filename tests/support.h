// venv-vrequency — engine tests: the check macros, the rig that renders the instrument offline,
// and the measurements and small clips the tests share.
#pragma once
#include "core/drums/drums.h"
#include "core/dsp.h"
#include "core/instrument/instrument.h"
#include "core/instrument/session.h"
#include "core/synth/params.h"
#include "core/track/track.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <vector>

using namespace td;

extern int g_fail, g_pass;
#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        if (cond) { ++g_pass; }                                               \
        else { ++g_fail; std::printf("  FAIL %s:%d  ", __FILE__, __LINE__);   \
               std::printf(__VA_ARGS__); std::printf("\n"); }                 \
    } while (0)
#define SECTION(name) std::printf("- %s\n", name)

static const float SR = 48000.0f;

struct Rig {
    Instrument inst{SR};
    std::vector<float> L, R;
    Rig() { enableFlushToZero(); }
    void run(double seconds, const std::function<void()> &watch = nullptr) {
        int total = (int)(seconds * SR);
        float l[256], r[256];
        while (total > 0) {
            int n = total < 256 ? total : 256;
            inst.process(l, r, n);
            L.insert(L.end(), l, l + n);
            R.insert(R.end(), r, r + n);
            total -= n;
            if (watch) watch();
        }
    }
    void clear() { L.clear(); R.clear(); }
    // last `seconds` of rendered audio
    const float *tail(double seconds, int &n) const {
        n = (int)(seconds * SR);
        if (n > (int)L.size()) n = (int)L.size();
        return L.data() + L.size() - n;
    }
    int gated(int part) const {
        int c = 0;
        for (int i = 0; i < kVoices; ++i)
            if (inst.voice(i).active() && inst.voice(i).part == part && inst.voice(i).gate) ++c;
        return c;
    }
    int active(int part) const {
        int c = 0;
        for (int i = 0; i < kVoices; ++i)
            if (inst.voice(i).active() && inst.voice(i).part == part) ++c;
        return c;
    }
};

inline Patch purePatch() {
    Patch p;
    p.setName("Pure");
    p.p[P_WAVE1] = W_SINE; p.p[P_MIX2] = 0; p.p[P_UNISON] = 0; p.p[P_SUB] = 0; p.p[P_NOISE] = 0;
    p.p[P_ATK] = 0.002f; p.p[P_DEC] = 0.1f; p.p[P_SUS] = 1.0f; p.p[P_REL] = 0.05f;
    p.p[P_FMODE] = F_LP; p.p[P_CUTOFF] = 16000; p.p[P_RESO] = 0; p.p[P_FENV] = 0; p.p[P_KTRK] = 0;
    p.p[P_VIBDEPTH] = 0; p.p[P_LFOCUT] = 0; p.p[P_LFOAMP] = 0; p.p[P_GLIDE] = 200;
    p.p[P_DRIVE] = 0; p.p[P_CHORUS] = 0; p.p[P_DLYMIX] = 0; p.p[P_REVMIX] = 0; p.p[P_LEVEL] = 0.5f;
    return p;
}

inline float rms(const float *x, int n) {
    double e = 0;
    for (int i = 0; i < n; ++i) e += (double)x[i] * x[i];
    return n > 0 ? (float)std::sqrt(e / n) : 0;
}
inline float peak(const std::vector<float> &v) {
    float p = 0;
    for (float x : v) if (std::fabs(x) > p) p = std::fabs(x);
    return p;
}
// frequency from interpolated positive-going zero crossings
inline float measureHz(const float *x, int n) {
    double first = -1, last = -1; int count = 0;
    for (int i = 1; i < n; ++i)
        if (x[i - 1] < 0 && x[i] >= 0) {
            double t = (i - 1) + (-x[i - 1]) / (double)(x[i] - x[i - 1]);
            if (first < 0) first = t;
            last = t; ++count;
        }
    if (count < 2) return 0;
    return (float)((count - 1) * SR / (last - first));
}
inline float goertzel(const float *x, int n, float f) {
    float c = 2.0f * std::cos(2.0f * kPi * f / SR), s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { // Hann window keeps neighbouring notes from leaking in
        float w = 0.5f - 0.5f * std::cos(2.0f * kPi * i / (n - 1));
        float s0 = x[i] * w + c * s1 - s2; s2 = s1; s1 = s0;
    }
    return std::sqrt(std::fabs(s1 * s1 + s2 * s2 - c * s1 * s2)) / n * 4;
}
// brightness proxy: energy of the first difference relative to the signal
inline float brightness(const float *x, int n) {
    double d = 0, e = 0;
    for (int i = 1; i < n; ++i) { d += (double)(x[i] - x[i - 1]) * (x[i] - x[i - 1]); e += (double)x[i] * x[i]; }
    return e > 0 ? (float)std::sqrt(d / e) : 0;
}
inline bool near(float a, float b, float relTol) { return std::fabs(a - b) <= relTol * std::fabs(b); }

// Share of a signal's energy that sits at one frequency (projection onto a sine/cosine pair).
inline float shareAt(const float *x, int n, double hz) {
    double c = 0, q = 0, e = 0;
    for (int i = 0; i < n; ++i) {
        double ph = 2.0 * M_PI * hz * i / SR;
        c += x[i] * std::cos(ph); q += x[i] * std::sin(ph); e += (double)x[i] * x[i];
    }
    return e > 0 ? (float)(2.0 * (c * c + q * q) / (n * e)) : 0;
}

// ---- small tracks and clips for the track tests ----

inline std::shared_ptr<const TrackData> share(const TrackData &t) { return std::make_shared<const TrackData>(t); }

inline Clip kickSnareClip() { // kick on step 1, snare on step 9
    Clip c; c.kind = CK_BEAT; c.bars = 1; c.kit = KIT_SOFT;
    c.beat[D_KICK] = 1u << 0; c.beat[D_SNARE] = 1u << 8;
    return c;
}

inline Clip noteClip(float note, double on, double off, int bars = 1) {
    Clip c; c.kind = CK_LOOP; c.bars = bars;
    LayerData ld; ld.patches.push_back(purePatch());
    LoopEv a; a.beat = on; a.op = OP_ON; a.slot = 0; a.a = note; a.b = 1; a.seq = 0;
    LoopEv b; b.beat = off; b.op = OP_OFF; b.slot = 0; b.a = 0; b.b = 0; b.seq = 1;
    ld.ev.push_back(a); ld.ev.push_back(b);
    c.layers.push_back(ld);
    return c;
}

inline void put(TrackData &t, int lane, int clip, int bar, int bars = 0) {
    Placement p = placeClip(*t.clips[clip], bar);
    p.clip = clip;
    if (bars > 0) p.bars = bars;
    t.lane[lane].push_back(p);
    sortLane(t, lane);
}

// seconds at which each new hit of `drum` was counted, to the nearest block of 256 frames
inline std::vector<double> hitTimes(Rig &r, int drum, double seconds) {
    std::vector<double> out;
    uint32_t seen = r.inst.uiState().drumHits[drum];
    double t = 0;
    r.run(seconds, [&] {
        t += 256.0 / SR;
        uint32_t now = r.inst.uiState().drumHits[drum];
        for (; seen < now; ++seen) out.push_back(t);
    });
    return out;
}
