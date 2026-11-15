// venv-vrequency — engine tests: the pulse pads' warp (DRIVE, FOLD, CRUSH, BEND), measured in the
// generator, and the pictures still the sound.
#include "tests/support.h"
#include "core/pulse/pulse_bank.h"
#include "core/pulse/pulse_voice.h"
#include <algorithm>
#include <set>
#include <vector>

namespace {
std::vector<float> hit(const PulsePad &p) {
    std::vector<float> b((size_t)PulseVoice::lengthSamples(p, SR));
    pulseRender(p, SR, p.rate, b.data(), (int)b.size());
    return b;
}
PulsePad pad(int type, float tone, float ms) { PulsePad p = pulseDefault(type); p.tone = tone; p.lengthMs = ms; return p; }
float hzAt(const std::vector<float> &b, double t0, double t1) { return measureHz(b.data() + (int)(t0 * SR), (int)((t1 - t0) * SR)); }
} // namespace

void testPulseWarp() {
    SECTION("pulse warp: BEND sweeps the pitch across the hit");
    {
        PulsePad t = pad(PT_TOM, 200, 400);
        const std::vector<float> flat = hit(t);
        for (float st : {12.0f, -12.0f}) {
            t.bend = st;
            const std::vector<float> b = hit(t);
            for (double at : {0.125, 0.325}) {
                const float ratio = hzAt(b, at - 0.025, at + 0.025) / hzAt(flat, at - 0.025, at + 0.025);
                const float want = std::exp2(st / 12.0f * (float)(at / 0.4));
                CHECK(near(ratio, want, 0.02f), "%+.0f st, at %.0f ms: %.3f times the unbent pitch (expected %.3f)", st, at * 1000, ratio, want);
            }
        }
    }
    {   // a noise kind: its band moves instead
        PulsePad c = pad(PT_CLAP, 1200, 300);
        const std::vector<float> flat = hit(c);
        c.bend = 24;
        const std::vector<float> b = hit(c);
        const int at = (int)(0.24 * SR), n = (int)(0.04 * SR);
        const float hiFlat = goertzel(flat.data() + at, n, 4000) / goertzel(flat.data() + at, n, 1200);
        const float hiBent = goertzel(b.data() + at, n, 4000) / goertzel(b.data() + at, n, 1200);
        CHECK(hiBent > 4 * hiFlat, "clap +24 st: late in the hit its band has moved up (4 kHz / 1.2 kHz %.2f, unbent %.2f)", hiBent, hiFlat);
    }

    SECTION("pulse warp: DRIVE, FOLD and CRUSH change the sound and keep it inside full scale");
    {
        int loud = 0, same = 0;
        for (int k = 0; k < kPulseTypes; ++k) {
            PulsePad p = pulseDefault(k); p.level = 1;
            const std::vector<float> dry = hit(p);
            p.drive = 1; p.fold = 1; p.crush = 1; p.bend = 24;
            const std::vector<float> w = hit(p);
            if (peak(w) > 1.0f) ++loud;
            if (w == dry) ++same;
        }
        CHECK(loud == 0 && same == 0, "all eight kinds at full warp: changed, and none above 1.0 (%d loud, %d unchanged)", loud, same);
        PulsePad s = pad(PT_THUMP, 60, 400);
        s.crush = 1;
        const std::vector<float> c = hit(s);
        std::set<float> levels(c.begin(), c.begin() + c.size() / 2);
        CHECK(levels.size() <= 40, "CRUSH at the top leaves a coarse grid (%zu levels in the first half)", levels.size());
        s.crush = 0; s.fold = 1;
        const std::vector<float> f = hit(s), d = hit(pad(PT_THUMP, 60, 400));
        CHECK(brightness(f.data(), (int)f.size()) > 2 * brightness(d.data(), (int)d.size()), "FOLD brightens a sub thump (%.4f vs %.4f)",
              brightness(f.data(), (int)f.size()), brightness(d.data(), (int)d.size()));
    }

    SECTION("pulse warp: the bank plays exactly what pulseRender draws, warp on");
    {
        PulseBank b; b.init(SR);
        for (int p = 0; p < kPulsePads; ++p) {
            b.set(p, PF_DRIVE, 0.6f, 120); b.set(p, PF_FOLD, 0.4f, 120); b.set(p, PF_CRUSH, 0.3f, 120); b.set(p, PF_BEND, p % 2 ? -7.0f : 12.0f, 120);
            const int n = PulseVoice::lengthSamples(b.pad[p], SR) + 64;
            std::vector<float> live(n, 0.0f), pic(n);
            b.trigger(p, 1.0f, 120);
            for (int i = 0; i < n; i += kBlock) b.render(live.data() + i, std::min(kBlock, n - i), 0, 0, 120, (uint64_t)i);
            pulseRender(b.pad[p], SR, b.rateOf(p, 120), pic.data(), n);
            CHECK(live == pic, "pad %d (%s) warped: the sound played is the picture's sound, sample for sample", p + 1, pulseTypeName(b.pad[p].type));
        }
        b.set(0, PF_BEND, 99, 120); b.set(0, PF_FOLD, -1, 120);
        CHECK(b.pad[0].bend == kPulseBendMax && b.pad[0].fold == 0, "warp settings out of range are held at the limits");
    }
}
