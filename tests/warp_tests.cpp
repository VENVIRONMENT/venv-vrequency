// venv-vrequency — engine tests: the synth's per-voice warp (FOLD, CRUSH, DOWNSAMPLE, RING MOD,
// BEND IN), measured in what the instrument plays and in the voice's own warp stage.
#include "tests/support.h"
#include "core/synth/voice_warp.h"
#include <set>
#include <vector>

namespace {
// One held note of the pure sine patch with some warp settings; returns the left channel.
std::vector<float> held(const std::vector<std::pair<int, float>> &set, double seconds = 0.5) {
    Patch p = purePatch();
    for (auto &s : set) p.p[s.first] = s.second;
    Rig r;
    r.inst.setPatch(0, p);
    r.inst.key(1, 0, true);
    r.run(seconds);
    return r.L;
}
// the share of energy at f across a window late in the note
float late(const std::vector<float> &x, double hz) { const int n = 9600; return shareAt(x.data() + x.size() - n, n, hz); }
std::vector<float> sineThrough(const float *P, int n, float hz) {
    VoiceWarp w; w.block(P, hz / SR, true);
    std::vector<float> y(n);
    for (int i = 0; i < n; ++i) y[i] = w(0.9f * std::sin(2 * kPi * hz * i / SR));
    return y;
}
} // namespace

void testWarp() {
    const std::vector<float> dry = held({});
    const float f0 = measureHz(dry.data() + dry.size() - 9600, 9600);
    std::printf("    the note: %.1f Hz\n", f0);

    SECTION("warp: FOLD adds harmonics");
    {
        const std::vector<float> f = held({{P_FOLD, 1}});
        const float d1 = late(dry, f0), w1 = late(f, f0);
        float dh = 0, wh = 0;
        for (int k = 2; k <= 9; ++k) { dh += late(dry, f0 * k); wh += late(f, f0 * k); }
        CHECK(d1 > 0.95f && dh < 0.01f, "dry, the sine is all fundamental (%.3f, harmonics %.4f)", d1, dh);
        CHECK(wh > 0.3f && w1 < 0.7f, "folded, harmonics 2-9 carry %.2f of the energy (fundamental %.2f)", wh, w1);
        CHECK(peak(f) <= 1.0f, "and it stays inside full scale (%.3f)", peak(f));
    }

    SECTION("warp: CRUSH and DOWNSAMPLE reduce resolution");
    {
        float P[P_COUNT] = {0};
        for (int bitsStep : {1, 6, 11}) {
            P[P_CRUSH] = (float)bitsStep;
            std::vector<float> y = sineThrough(P, 4800, 220);
            std::set<float> levels(y.begin(), y.end());
            const int bits = (int)crushBits(P[P_CRUSH]), most = (1 << bits) + 1;
            CHECK((int)levels.size() <= most, "CRUSH %d: %d bits leave %zu levels (at most %d)", bitsStep, bits, levels.size(), most);
        }
        P[P_CRUSH] = 0; P[P_DSAMP] = 1;
        std::vector<float> y = sineThrough(P, 4800, 220);
        int changes = 0;
        for (size_t i = 1; i < y.size(); ++i) if (y[i] != y[i - 1]) ++changes;
        CHECK(changes <= 4800 / 32 + 1 && changes > 4800 / 40, "DOWNSAMPLE at the top: a new sample every 32 (%d changes in 4800)", changes);
        CHECK(dsampKeep(1e-6f) > 0.49f && dsampKeep(1) == 1.0f / 32, "the range is 1/2 to 1/32 of the rate");
        const std::vector<float> ds = held({{P_DSAMP, 1}}), cr = held({{P_CRUSH, 11}});
        const float img = late(ds, 1500 - f0) + late(ds, 1500 + f0);   // 48000 / 32 = 1500 Hz
        CHECK(img > 20 * (late(dry, 1500 - f0) + late(dry, 1500 + f0)) && img > 0.01f, "in the instrument, images appear around 1.5 kHz (%.3f)", img);
        float dh = 0, ch = 0;
        for (int k = 3; k <= 9; k += 2) { dh += late(dry, f0 * k); ch += late(cr, f0 * k); }
        CHECK(ch > 0.05f && ch > 50 * dh, "two bits turn the sine into steps: odd harmonics %.3f (dry %.5f)", ch, dh);
    }

    SECTION("warp: RING MOD puts sidebands at the note times 1.41, plus and minus one");
    {
        const std::vector<float> rm = held({{P_RING, 1}});
        const float lo = late(rm, f0 * (kRingRatio - 1)), hi = late(rm, f0 * (kRingRatio + 1)), fund = late(rm, f0);
        CHECK(lo + hi > 0.85f && lo > 0.3f && hi > 0.3f, "%.0f Hz and %.0f Hz carry %.2f and %.2f", f0 * (kRingRatio - 1), f0 * (kRingRatio + 1), lo, hi);
        CHECK(fund < 0.02f, "and the note itself is gone (%.4f)", fund);
        const std::vector<float> half = held({{P_RING, 0.5f}});
        CHECK(late(half, f0) > 0.15f && late(half, f0 * (kRingRatio + 1)) > 0.05f, "at half, the two are blended");
    }

    SECTION("warp: BEND IN starts off pitch and lands");
    for (float st : {12.0f, -12.0f}) {
        const std::vector<float> b = held({{P_BENDIN, st}}, 0.4);
        const float early = measureHz(b.data() + 96, (int)(0.03 * SR)), land = measureHz(b.data() + (int)(0.15 * SR), (int)(0.2 * SR));
        const float want = st > 0 ? 1.4f : 0.72f;          // the average over the first 30 ms
        CHECK(st > 0 ? early > f0 * want : early < f0 * want, "%+.0f st: over the first 30 ms it averages %.1f Hz", st, early);
        CHECK(near(land, f0, 0.005f), "and from 150 ms it is on the note (%.2f Hz, %.3f%% off)", land, 100 * std::fabs(land / f0 - 1));
    }

    SECTION("warp: everything at maximum stays inside full scale");
    {
        Patch p = purePatch();
        p.p[P_WAVE1] = W_SAW; p.p[P_RESO] = 0.95f; p.p[P_LEVEL] = 4; p.p[P_DRIVE] = 1; p.p[P_FOLD] = 1;
        p.p[P_CRUSH] = 11; p.p[P_DSAMP] = 1; p.p[P_RING] = 1; p.p[P_BENDIN] = 24; p.p[P_REVMIX] = 0.8f; p.p[P_DLYMIX] = 0.7f;
        Rig r;
        r.inst.setPatch(0, p);
        for (int s = 0; s < 3; ++s) for (int c = 0; c < 5; c += 2) r.inst.key(s, c, true);
        r.run(1.0);
        bool finite = true;
        for (float v : r.L) if (!std::isfinite(v)) finite = false;
        CHECK(finite && peak(r.L) <= 1.0f && peak(r.R) <= 1.0f, "nine notes, all warp at the top: peak %.3f", std::max(peak(r.L), peak(r.R)));
        CHECK(rms(r.L.data(), (int)r.L.size()) > 0.02f, "and it is not silent");
        float curve[401];
        warpCurve(p.p, curve, 401);
        float worst = 0;
        for (float v : curve) worst = std::max(worst, std::fabs(v));
        CHECK(worst <= 1.0f, "the voice's warp of a full-scale ramp stays inside +-1 (%.3f)", worst);
    }
}
