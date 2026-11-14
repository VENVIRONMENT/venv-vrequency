// venv-vrequency — engine tests: the drum kits and the pads.
#include "tests/support.h"
#include "core/drums/drum_kit.h"
#include <algorithm>
#include <vector>

void testDrums() {
    SECTION("drums: three kits of eight sounds");
    {
        Rig r;
        const DrumKit &dk = r.inst.drumKit();
        int bad = 0, loud = 0, tails = 0, dull = 0;
        for (int k = 0; k < kKits; ++k) {
            for (int d = 0; d < kDrums; ++d) {
                const std::vector<float> &s = dk.sample(k, d);
                float pk = peak(s);
                bool finite = !s.empty();
                for (float v : s) if (!(v == v)) finite = false;
                if (!finite) ++bad;
                if (pk < 0.08f || pk > 1.3f) { ++loud; std::printf("    %s %s peak %.2f\n", kitName(k), drumName(d), pk); }
                if (!s.empty() && std::fabs(s.back()) > 1e-3f) ++tails;
            }
            float bk = brightness(dk.sample(k, D_KICK).data(), (int)dk.sample(k, D_KICK).size());
            float bh = brightness(dk.sample(k, D_HAT).data(), (int)dk.sample(k, D_HAT).size());
            if (!(bh > 4.0f * bk)) { ++dull; std::printf("    %s kick %.3f hat %.3f\n", kitName(k), bk, bh); }
        }
        CHECK(bad == 0, "all %d sounds exist and are finite", kKits * kDrums);
        CHECK(loud == 0, "every sound peaks between 0.08 and 1.3 (%d outside)", loud);
        CHECK(tails == 0, "every sound ends in silence (%d do not)", tails);
        CHECK(dull == 0, "in every kit the hat is much brighter than the kick");
        int same = 0;
        for (int d = 0; d < kDrums; ++d)
            for (int a = 0; a < kKits; ++a)
                for (int b = a + 1; b < kKits; ++b) {
                    const std::vector<float> &x = dk.sample(a, d), &y = dk.sample(b, d);
                    double diff = 0, e = 0; size_t n = std::min(x.size(), y.size());
                    for (size_t i = 0; i < n; ++i) { diff += (double)(x[i] - y[i]) * (x[i] - y[i]); e += (double)x[i] * x[i]; }
                    if (diff < 0.05 * e) ++same;
                }
        CHECK(same == 0, "the same drum differs between kits (%d alike)", same);
    }

    SECTION("drums: a pad hit sounds at once");
    {
        Rig r;
        r.inst.set(S_KIT, KIT_CRUNCHY);
        r.run(0.05);
        CHECK(peak(r.L) == 0.0f, "silent before the hit");
        r.clear();
        r.inst.hit(D_KICK);
        r.run(0.03);
        CHECK(peak(r.L) > 0.1f, "kick is audible inside 30 ms (peak %.2f)", peak(r.L));
        CHECK(r.inst.uiState().drumHits[D_KICK] == 1 && !r.inst.uiState().beatPlaying, "counted, and the beat did not start by itself");
        r.run(2.0); r.clear(); r.run(0.2);
        CHECK(peak(r.L) < 1e-3f, "and it dies away (peak %.5f two seconds later)", peak(r.L));
        float pk[kKits];
        for (int k = 0; k < kKits; ++k) {
            Rig q; q.inst.set(S_KIT, (float)k); q.inst.hit(D_SNARE); q.run(0.15);
            pk[k] = brightness(q.L.data(), (int)q.L.size());
        }
        CHECK(!near(pk[0], pk[1], 0.1f) && !near(pk[1], pk[2], 0.1f), "changing kit changes the snare (%.3f %.3f %.3f)", pk[0], pk[1], pk[2]);
    }
}
