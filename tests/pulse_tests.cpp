// venv-vrequency — engine tests: the eight pulse sounds, rendered by the generator itself.
#include "tests/support.h"
#include "core/pulse/pulse_voice.h"
#include <algorithm>
#include <vector>

namespace {
std::vector<float> render(const PulsePad &p, double seconds, float rate = -1) {
    std::vector<float> b((size_t)(seconds * SR));
    pulseRender(p, SR, rate > 0 ? rate : p.rate, b.data(), (int)b.size());
    return b;
}
int lastNonZero(const std::vector<float> &b) {
    for (int i = (int)b.size() - 1; i >= 0; --i) if (b[i] != 0.0f) return i;
    return -1;
}
PulsePad kind(int type, float tone, float ms, float rate = 4, float level = 0.8f) {
    PulsePad p; p.type = type; p.tone = tone; p.lengthMs = ms; p.rate = rate; p.level = level; return p;
}
} // namespace

void testPulseSounds() {
    SECTION("pulse: every default pad is a finite sound that ends in silence");
    {
        int bad = 0, loud = 0, tails = 0, longer = 0;
        for (int p = 0; p < kPulsePads; ++p) {
            const PulsePad d = pulseDefault(p);
            CHECK(d.type == p, "pad %d is the %d-th kind by default (%s)", p + 1, p + 1, pulseTypeName(d.type));
            std::vector<float> b = render(d, d.lengthMs * 0.001 + 0.05);
            for (float v : b) if (!std::isfinite(v)) { ++bad; break; }
            float pk = peak(b);
            if (pk < 0.05f || pk > 1.0f) { ++loud; std::printf("    %s peak %.3f\n", pulseTypeName(d.type), pk); }
            const int len = PulseVoice::lengthSamples(d, SR), last = lastNonZero(b);
            if (last >= len) ++longer;
            if (last > 0 && std::fabs(b[last]) > 2e-3f) { ++tails; std::printf("    %s ends at %.4f\n", pulseTypeName(d.type), b[last]); }
        }
        CHECK(bad == 0, "all eight are finite");
        CHECK(loud == 0, "every one peaks between 0.05 and 1.0 (%d outside)", loud);
        CHECK(longer == 0, "none sounds past its LENGTH (%d do)", longer);
        CHECK(tails == 0, "each one fades to silence at its end (%d do not)", tails);
        const PulsePad d = pulseDefault(2); // the snap: it has noise in it
        std::vector<float> a = render(d, 0.3), b = render(d, 0.3);
        CHECK(a == b, "the same settings make exactly the same sound twice (so a picture of it is the sound)");
    }

    SECTION("pulse: click");
    {
        std::vector<float> c = render(kind(PT_CLICK, 3000, 3), 0.02);
        CHECK(std::fabs(c[0]) == peak(c) && c[0] > 0.7f, "a click starts at its peak (first sample %.3f)", c[0]);
        int n = (int)(0.001 * SR);
        CHECK(rms(c.data(), n) > 4.0f * rms(c.data() + n, n * 4), "most of a click is in its first millisecond");
    }

    SECTION("pulse: metal, snap and clap");
    {
        const int w = (int)(0.02 * SR);
        std::vector<float> m = render(kind(PT_METAL, 400, 80), 0.08);
        CHECK(goertzel(m.data(), w, 8000) > 4.0f * goertzel(m.data(), w, 500), "metal sits high, in a band around twenty times TONE (%.4f at 8 kHz vs %.4f at 500 Hz)",
              goertzel(m.data(), w, 8000), goertzel(m.data(), w, 500));
        CHECK(rms(m.data(), w / 2) > 4.0f * rms(m.data() + 3 * w, w / 2), "and it dies inside its length");
        std::vector<float> sn = render(kind(PT_SNAP, 190, 180), 0.18);
        const int a = (int)(0.03 * SR);
        CHECK(goertzel(sn.data() + a, w * 2, 190) > 2.0f * goertzel(sn.data() + a, w * 2, 400), "a snap has a body at TONE (%.4f vs %.4f an octave up)",
              goertzel(sn.data() + a, w * 2, 190), goertzel(sn.data() + a, w * 2, 400));
        CHECK(brightness(sn.data(), w) > 0.2f, "under bright noise (brightness %.2f)", brightness(sn.data(), w));
        std::vector<float> cl = render(kind(PT_CLAP, 1200, 260), 0.26);
        auto at = [&](float ms) { return rms(cl.data() + (int)(ms * 0.001 * SR), (int)(0.002 * SR)); };
        CHECK(at(0) > 3 * at(8) && at(11) > 3 * at(8) && at(22) > 3 * at(19), "a clap is three bursts 11 ms apart (%.3f %.3f %.3f, gaps %.3f %.3f)",
              at(0), at(11), at(22), at(8), at(19));
        CHECK(at(40) > 0.2f * at(0) && at(240) < 0.2f * at(40), "then a tail that fades out (%.3f, %.3f)", at(40), at(240));
        CHECK(goertzel(cl.data() + (int)(0.04 * SR), w, 1200) > 2.0f * goertzel(cl.data() + (int)(0.04 * SR), w, 6000), "with its noise in a band around TONE");
    }

    SECTION("pulse: pitched strikes");
    {
        std::vector<float> pg = render(kind(PT_PING, 880, 700), 0.7);
        const int w = (int)(0.03 * SR);
        CHECK(shareAt(pg.data() + (int)(0.003 * SR), w, 880) < 0.8f, "an FM ping starts rich (%.2f of its energy at TONE)", shareAt(pg.data() + (int)(0.003 * SR), w, 880));
        CHECK(near(measureHz(pg.data() + (int)(0.35 * SR), w * 3), 880, 0.005f) && shareAt(pg.data() + (int)(0.35 * SR), w * 3, 880) > 0.95f,
              "and rings out as a pure TONE (%.1f Hz)", measureHz(pg.data() + (int)(0.35 * SR), w * 3));
        std::vector<float> z = render(kind(PT_ZAP, 110, 300), 0.3);
        const float early = measureHz(z.data(), (int)(0.004 * SR)), late = measureHz(z.data() + (int)(0.15 * SR), (int)(0.12 * SR));
        CHECK(early > 1000 && near(late, 110, 0.05f), "a zap sweeps from high (%.0f Hz) down to TONE (%.1f Hz)", early, late);
        std::vector<float> s = render(kind(PT_THUMP, 45, 400), 0.4);
        const float e2 = measureHz(s.data(), (int)(0.025 * SR)), l2 = measureHz(s.data() + (int)(0.1 * SR), (int)(0.25 * SR));
        CHECK(e2 > 90 && near(l2, 45, 0.05f), "a sub thump falls from above twice its tone (%.0f Hz) to TONE (%.1f Hz)", e2, l2);
        std::vector<float> k = render(kind(PT_KNOCK, 800, 400), 0.4); // long enough to measure its modes
        const int a = (int)(0.012 * SR), n = (int)(0.08 * SR);
        const float modes = shareAt(k.data() + a, n, 800) + shareAt(k.data() + a, n, 2160);
        CHECK(modes > 0.9f && shareAt(k.data() + a, n, 2160) > 0.01f, "a knock is two modes, TONE and 2.7 times TONE (%.2f of its energy)", modes);
    }
}
