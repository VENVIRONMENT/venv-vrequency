// venv-vrequency — engine tests: the Freq tones. Every check measures the rendered output.
#include "tests/freq_support.h"


void testFreqTones() {
    SECTION("freq: beating and tone accuracy");
    {
        FreqBank b; b.init(SR);
        b.set(-1, FF_HOLD, 1); // the defaults: 220 Hz and 221.5 Hz sines
        std::vector<float> x = renderBank(b, 4.0);
        const float beat = envelopeHz(x, 4800);
        CHECK(near(beat, 1.5f, 0.02f), "220 + 221.5 Hz beat at 1.5 Hz in the output (%.3f Hz)", beat);
        CHECK(std::fabs(freqBeat(b.s) - 1.5f) < 1e-3f, "and the page's sum says 1.5 Hz (%.3f)", freqBeat(b.s));
    }
    for (int w : {FW_SINE, FW_TRI, FW_SQUARE, FW_SAW})
        for (float hz : {55.0f, 220.0f, 1000.0f, 3520.0f}) {
            FreqBank b; b.init(SR); solo(b, w, hz);
            std::vector<float> x = renderBank(b, 0.6);
            const float m = measureHz(x.data() + 4800, (int)x.size() - 4800);
            CHECK(near(m, hz, 0.005f), "%s at %.0f Hz plays %.2f Hz (within 0.5%%)", freqWaveName(w), hz, m);
        }
    {
        FreqBank b; b.init(SR); solo(b, FW_NOISE, 1000);
        std::vector<float> x = renderBank(b, 1.0);
        const float at = goertzel(x.data() + 4800, 24000, 1000), off = goertzel(x.data() + 4800, 24000, 3000);
        CHECK(at > 6 * off && rms(x.data() + 4800, 24000) > 0.03f, "NOISE is a band at the tone's Hz (%.4f there, %.4f at 3x)", at, off);
    }

    SECTION("freq: glides, fades and GATE never click");
    {
        FreqBank b; b.init(SR); solo(b, FW_SINE, 220);
        std::vector<float> x = renderBank(b, 0.3);
        const float steady = maxStep(x, 9600, (int)x.size());
        b.set(0, FF_HZ, 440); b.set(0, FF_LEVEL, 0.3f);
        std::vector<float> y = renderBank(b, 0.3);
        const float hz = measureHz(y.data() + 4800, 9600);
        CHECK(near(hz, 440, 0.005f), "a new Hz is reached inside 100 ms (%.2f)", hz);
        CHECK(maxStep(y, 0, 1200) < steady * 1.3f, "with no jump on the way (%.4f, a steady 220 Hz moves %.4f)", maxStep(y, 0, 1200), steady);
        CHECK(std::fabs(x[0]) < 0.01f && maxStep(x, 0, 480) < steady * 1.1f, "HOLD fades in");
        b.set(-1, FF_HOLD, 0);
        std::vector<float> z = renderBank(b, 0.05);
        CHECK(maxStep(z, 0, 480) < steady * 1.1f && rms(z.data() + 1200, 1200) < 1e-4f, "and out, inside 50 ms");
        b.set(-1, FF_HOLD, 1); b.set(-1, FF_GATE, 1); b.set(-1, FF_RATE, 40);
        std::vector<float> g = renderBank(b, 0.5);
        CHECK(maxStep(g, 0, (int)g.size()) < 0.12f, "a 40 Hz gate has soft edges (largest step %.3f)", maxStep(g, 0, (int)g.size()));
    }
    {
        FreqBank b; b.init(SR); solo(b, FW_SINE, 1000);
        b.set(-1, FF_HOLD, 0); b.set(-1, FF_GATE, 1); b.set(-1, FF_RATE, 8);
        int n = onsets(renderBank(b, 2.0), 0.05f);
        CHECK(n == 16, "GATE alone pulses at RATE: 8 Hz is 16 openings in 2 s (%d)", n);
        b.set(-1, FF_RATE, 3);
        float perBeat = 0;
        const float r = b.rateOf(120, true, &perBeat);
        CHECK(r == 4 && perBeat == 2, "with SYNC, 3 Hz at 120 bpm snaps to eighth notes (%.2f Hz)", r);
        std::vector<float> x = renderBank(b, 2.0, 120, true);
        n = onsets(x, 0.05f);
        CHECK(n == 8, "and the tones pulse on that grid (%d openings in 2 s)", n);
        CHECK(rms(x.data() + 12000 - 480, 480) < 0.02f && rms(x.data() + 12000 + 60, 480) > 0.1f, "opening on the eighth-note line (sample 12000)");
    }

    SECTION("freq: 2 > 1 changes the spectrum");
    {
        auto run = [](int mode) {
            FreqBank b; b.init(SR); solo(b, FW_SINE, 1000);
            b.set(1, FF_ON, 1); b.set(1, FF_HZ, 200); b.set(1, FF_LEVEL, 0); // tone 2 only bends tone 1
            b.set(-1, FF_MOD, (float)mode); b.set(-1, FF_MODAMT, 0.6f);
            std::vector<float> x = renderBank(b, 0.5);
            return std::vector<float>(x.begin() + 9600, x.end());
        };
        std::vector<float> off = run(FM_OFF), fm = run(FM_FM), ring = run(FM_RING);
        const int n = (int)off.size();
        CHECK(shareAt(off.data(), n, 1000) > 0.95f && shareAt(off.data(), n, 1200) < 0.01f, "off: one line at 1 kHz");
        CHECK(shareAt(fm.data(), n, 1200) > 0.05f && shareAt(fm.data(), n, 800) > 0.05f && shareAt(fm.data(), n, 1000) < 0.8f,
              "FM: sidebands at 800 and 1200 Hz (%.2f, %.2f)", shareAt(fm.data(), n, 800), shareAt(fm.data(), n, 1200));
        CHECK(shareAt(ring.data(), n, 1200) > 0.1f && shareAt(ring.data(), n, 800) > 0.1f, "RING: the sum and difference (%.2f, %.2f)",
              shareAt(ring.data(), n, 800), shareAt(ring.data(), n, 1200));
    }

    SECTION("freq: safe levels, whatever the settings");
    {
        float worst = 0;
        Rng rng;
        for (int k = 0; k < 40; ++k) {
            FreqBank b; b.init(SR);
            for (int i = 0; i < kFreqTones; ++i) {
                b.set(i, FF_ON, 1); b.set(i, FF_LEVEL, 1);
                b.set(i, FF_WAVE, (float)(k < 5 ? k : rng.next() % kFreqWaves));
                b.set(i, FF_HZ, k < 5 ? 110.0f : 20.0f + rng.uni() * 4000.0f);
            }
            b.set(-1, FF_DRIVE, 1); b.set(-1, FF_FOLD, 1);
            b.set(-1, FF_CRUSH, k % 3 ? rng.uni() : 0.0f);
            b.set(-1, FF_FILTER, k % 2 ? kWarpCutMax : 40 + rng.uni() * 8000);
            b.set(-1, FF_MOD, (float)(k % 3)); b.set(-1, FF_MODAMT, 1);
            b.set(-1, FF_HOLD, 1); b.set(-1, FF_GATE, (float)(k % 2)); b.set(-1, FF_RATE, 50);
            std::vector<float> x = renderBank(b, 0.25);
            worst = std::max(worst, peak(x));
        }
        CHECK(worst <= kFreqOut + 1e-4f, "four tones at full level, drive and fold maxed: peak %.3f, never past %.2f", worst, kFreqOut);
        Warp w; w.init(SR);
        float in[kBlock], top = 0;
        for (float c : {0.0f, 0.3f, 1.0f}) {
            WarpParams p; p.drive = 1; p.fold = 1; p.crush = c; p.cutoff = kWarpCutMax; w.set(p);
            for (int blk = 0; blk < 300; ++blk) {
                for (int i = 0; i < kBlock; ++i) in[i] = ((blk * kBlock + i) / 7) % 2 ? 1.0f : -1.0f; // a full-scale square
                w.process(in, kBlock);
                for (float v : in) top = std::max(top, std::fabs(v));
            }
        }
        CHECK(top <= 1.0f, "the warp stage alone keeps a full-scale square inside +-1 (%.4f)", top);
    }
    {
        FreqBank b; b.init(SR); solo(b, FW_SINE, 220);
        std::vector<float> clean = renderBank(b, 0.3);
        b.set(-1, FF_DRIVE, 1);
        std::vector<float> hot = renderBank(b, 0.3);
        CHECK(brightness(hot.data() + 9600, 4800) > 1.5f * brightness(clean.data() + 9600, 4800), "DRIVE adds harmonics");
        b.set(-1, FF_DRIVE, 0); b.set(0, FF_WAVE, FW_SAW);
        std::vector<float> saw = renderBank(b, 0.3);
        b.set(-1, FF_FILTER, 200);
        std::vector<float> dark = renderBank(b, 0.3);
        CHECK(brightness(dark.data() + 9600, 4800) < 0.3f * brightness(saw.data() + 9600, 4800), "FILTER darkens a saw");
        b.set(-1, FF_FILTER, kWarpCutMax); b.set(-1, FF_CRUSH, 1);
        std::vector<float> c = renderBank(b, 0.3);
        std::vector<float> lv(c.begin() + 9600, c.end());
        std::sort(lv.begin(), lv.end());
        const int levels = (int)(std::unique(lv.begin(), lv.end(), [](float a, float x) { return std::fabs(a - x) < 1e-3f; }) - lv.begin());
        CHECK(levels < 40, "CRUSH at full leaves a handful of levels (%d)", levels);
    }
}
