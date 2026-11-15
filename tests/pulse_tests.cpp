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
            if (d.type != PT_TICK && last > 0 && std::fabs(b[last]) > 2e-3f) { ++tails; std::printf("    %s ends at %.4f\n", pulseTypeName(d.type), b[last]); }
        }
        CHECK(bad == 0, "all eight are finite");
        CHECK(loud == 0, "every one peaks between 0.05 and 1.0 (%d outside)", loud);
        CHECK(longer == 0, "none sounds past its LENGTH (%d do)", longer);
        CHECK(tails == 0, "each one fades to silence at its end (%d do not)", tails);
        const PulsePad d = pulseDefault(5);
        std::vector<float> a = render(d, 0.3), b = render(d, 0.3);
        CHECK(a == b, "the same settings make exactly the same sound twice (so a picture of it is the sound)");
    }

    SECTION("pulse: click and tick");
    {
        std::vector<float> c = render(kind(PT_CLICK, 3000, 3), 0.02);
        CHECK(std::fabs(c[0]) == peak(c) && c[0] > 0.7f, "a click starts at its peak (first sample %.3f)", c[0]);
        int n = (int)(0.001 * SR);
        CHECK(rms(c.data(), n) > 4.0f * rms(c.data() + n, n * 4), "most of a click is in its first millisecond");
        const float tones[] = {18000, 12000, 8000, 1000};
        const int widths[] = {1, 2, 3, 3};
        for (int i = 0; i < 4; ++i) {
            std::vector<float> t = render(kind(PT_TICK, tones[i], 500), 0.01);
            int nz = 0; for (float v : t) if (v != 0) ++nz;
            CHECK(nz == widths[i], "a tick at %.0f Hz is %d samples (got %d), whatever its LENGTH", tones[i], widths[i], nz);
        }
    }

    SECTION("pulse: noise burst and hiss");
    {
        std::vector<float> hi = render(kind(PT_NOISE, 16000, 100), 0.1), lo = render(kind(PT_NOISE, 600, 100), 0.1);
        CHECK(brightness(hi.data(), 2400) > 3.0f * brightness(lo.data(), 2400), "TONE darkens the noise burst (%.3f vs %.3f)",
              brightness(hi.data(), 2400), brightness(lo.data(), 2400));
        const int q = (int)(0.02 * SR);
        CHECK(rms(hi.data(), q) > 4.0f * rms(hi.data() + 3 * q, q), "and it decays fast");
        CHECK(rms(lo.data(), q) > 0.3f * rms(hi.data(), q), "a dark burst keeps most of its loudness");
        std::vector<float> h = render(kind(PT_HISS, 4000, 1000), 1.0);
        int win = (int)(0.05 * SR), best = 0; float bestE = 0;
        for (int s = 0; s + win <= (int)h.size(); s += win) { float e = rms(h.data() + s, win); if (e > bestE) { bestE = e; best = s; } }
        const float at = (best + win * 0.5f) / SR;
        CHECK(at > 0.25f && at < 0.45f, "the hiss swells to its loudest about a third of the way in (%.2f s)", at);
        const float g0 = goertzel(h.data() + best, win, 4000), gLo = goertzel(h.data() + best, win, 1000), gHi = goertzel(h.data() + best, win, 16000);
        CHECK(g0 > 2.0f * gLo && g0 > 2.0f * gHi, "its noise sits in a band around TONE (%.4f vs %.4f / %.4f)", g0, gLo, gHi);
    }

    SECTION("pulse: pure tones");
    {
        for (float f : {440.0f, 1000.0f, 5000.0f}) {
            std::vector<float> b = render(kind(PT_BLIP, f, 80), 0.08);
            const int a = (int)(0.005 * SR), n = (int)(0.06 * SR);
            CHECK(near(measureHz(b.data() + a, n), f, 0.003f), "a sine blip at %.0f Hz measures %.1f Hz", f, measureHz(b.data() + a, n));
            CHECK(shareAt(b.data() + a, n, f) > 0.99f, "and it is pure (%.4f of its energy at the tone)", shareAt(b.data() + a, n, f));
        }
        PulsePad g = kind(PT_GATED, 2000, 500, 10);
        std::vector<float> b = render(g, 0.5);
        const int half = (int)(SR / 20);
        CHECK(rms(b.data() + half + half / 4, half / 2) < 0.01f * rms(b.data() + half / 4, half / 2), "a gated tone is silent while its gate is shut");
        CHECK(near(measureHz(b.data() + 100, half - 200), 2000, 0.005f), "and its tone is TONE while it is open");
        int gates = pulseRender(g, SR, g.rate, b.data(), (int)b.size());
        CHECK(gates == 5, "500 ms at 10 Hz opens five gates (%d)", gates);
        std::vector<float> s = render(kind(PT_THUMP, 45, 400), 0.4);
        const float early = measureHz(s.data(), (int)(0.025 * SR)), late = measureHz(s.data() + (int)(0.1 * SR), (int)(0.25 * SR));
        CHECK(early > 90 && near(late, 45, 0.05f), "a sub thump falls from above twice its tone (%.0f Hz) to TONE (%.1f Hz)", early, late);
    }

    SECTION("pulse: a pulse train");
    {
        PulsePad t = pulseDefault(5); // 1 kHz, 250 ms, 40 Hz
        std::vector<float> b((size_t)(0.3 * SR));
        int clicks = pulseRender(t, SR, t.rate, b.data(), (int)b.size());
        CHECK(clicks == 10, "250 ms at 40 Hz is ten clicks (%d)", clicks);
        int at[12], n = 0;
        for (int i = 1; i < (int)b.size() && n < 12; ++i) if (b[i] > 0.5f && b[i - 1] < 0.3f) at[n++] = i;
        bool even = n >= 9;
        for (int i = 1; i < n; ++i) if (std::abs(at[i] - at[i - 1] - 1200) > 1) even = false;
        CHECK(even, "the clicks are 25 ms apart to the sample (%d found)", n);
        CHECK(clicks == pulseRender(t, SR, 80, b.data(), (int)b.size()) / 2, "twice the rate, twice the clicks");
    }
}
