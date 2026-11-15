// venv-vrequency — engine benchmark: CPU cost at maximum polyphony.
#include "tests/support.h"
#include "core/presets/presets.h"
#include <chrono>
#include <cstdio>

namespace {
// 20 s of strings retriggered so release tails pile up to the full voice pool.
void poolRun(const Patch &heavy, const char *what) {
    Instrument inst(SR);
    inst.setPatch(0, heavy);
    inst.set(S_SEVENTH, 1);
    float l[256], r[256];
    inst.process(l, r, 256);
    inst.cmd(EV_CHORD, 0);
    int maxV = 0;
    auto t0 = std::chrono::steady_clock::now();
    const int blocks = (int)(20.0 * SR / 256);
    double worst = 0;
    for (int b = 0; b < blocks; ++b) {
        int s = (b / 12) % 3, c = (b / 36) % 7;
        if (b % 12 == 0) inst.key(s, c, true);
        if (b % 12 == 6) inst.key(s, c, false);
        auto a = std::chrono::steady_clock::now();
        inst.process(l, r, 256);
        double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count();
        if (dt > worst) worst = dt;
        int v = inst.uiState().voices;
        if (v > maxV) maxV = v;
    }
    double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    double period = 256 / SR;
    std::printf("bench: 20 s of audio, up to %d voices (pool %d), %s\n", maxV, kVoices, what);
    std::printf("  render time %.3f s  ->  %.1f%% of one core, %.1fx realtime\n", el, el / 20.0 * 100.0, 20.0 / el);
    std::printf("  worst 256-frame block %.3f ms of %.3f ms budget (%.0f%%)\n", worst * 1e3, period * 1e3, worst / period * 100);
}

// 20 s of every pulse pad latched fast, so the 24 pulse voices stay busy.
void pulseRun(bool warp) {
    Instrument inst(SR);
    float l[256], r[256];
    for (int p = 0; p < kPulsePads; ++p) {
        inst.pulseSet(p, PF_RATE, 40);
        if (warp) { inst.pulseSet(p, PF_DRIVE, 0.7f); inst.pulseSet(p, PF_FOLD, 0.5f); inst.pulseSet(p, PF_CRUSH, 0.4f); inst.pulseSet(p, PF_BEND, -12); }
        inst.pulseLatch(p, true);
    }
    const int blocks = (int)(20.0 * SR / 256);
    auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b) inst.process(l, r, 256);
    double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("bench: 20 s of all eight pulse pads latched at 40 Hz, %s: %.1f%% of one core\n", warp ? "full warp" : "no warp", el / 20.0 * 100.0);
}
} // namespace

void bench() {
    enableFlushToZero();
    Patch heavy = factoryPatch(2); // unison + osc B + chorus + reverb
    heavy.p[P_SUB] = 0.4f; heavy.p[P_NOISE] = 0.05f; heavy.p[P_DRIVE] = 0.4f; heavy.p[P_FM] = 0.5f;
    heavy.p[P_ATK] = 0.01f; heavy.p[P_REL] = 6.0f;
    poolRun(heavy, "heavy patch + all effects");
    Patch warped = heavy; // every warp stage on, in every voice
    warped.p[P_FOLD] = 0.6f; warped.p[P_CRUSH] = 5; warped.p[P_DSAMP] = 0.4f; warped.p[P_RING] = 0.5f; warped.p[P_BENDIN] = -7;
    poolRun(warped, "the same with every warp stage on");
    pulseRun(false);
    pulseRun(true);
    // the Freq tones alone, at their busiest: four tones (one noise), FM, every warp stage
    Instrument f(SR);
    float l[256], r[256];
    for (int i = 0; i < kFreqTones; ++i) f.freqSet(i, FF_ON, 1);
    f.freqSet(3, FF_WAVE, FW_NOISE); f.freqSet(1, FF_WAVE, FW_SAW); f.freqSet(-1, FF_MOD, FM_FM);
    f.freqSet(-1, FF_DRIVE, 0.5f); f.freqSet(-1, FF_FOLD, 0.5f); f.freqSet(-1, FF_CRUSH, 0.3f); f.freqSet(-1, FF_FILTER, 3000);
    f.freqSet(-1, FF_GATE, 1); f.freqSet(-1, FF_HOLD, 1);
    const int blocks = (int)(20.0 * SR / 256);
    auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b) f.process(l, r, 256);
    double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("bench: 20 s of the four Freq tones at their busiest: %.1f%% of one core\n", el / 20.0 * 100.0);
}
