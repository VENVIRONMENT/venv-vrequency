// venv-vrequency — engine benchmark: CPU cost at maximum polyphony.
#include "tests/support.h"
#include "core/presets/presets.h"
#include <chrono>
#include <cstdio>

void bench() {
    Instrument inst(SR);
    enableFlushToZero();
    Patch heavy = factoryPatch(2); // unison + osc B + chorus + reverb
    heavy.p[P_SUB] = 0.4f; heavy.p[P_NOISE] = 0.05f; heavy.p[P_DRIVE] = 0.4f; heavy.p[P_FM] = 0.5f;
    heavy.p[P_ATK] = 0.01f; heavy.p[P_REL] = 6.0f;
    inst.setPatch(0, heavy);
    inst.set(S_SEVENTH, 1);
    float l[256], r[256];
    inst.process(l, r, 256);
    inst.cmd(EV_CHORD, 0);
    // retrigger strings so release tails pile up to the full voice pool
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
    std::printf("bench: 20 s of audio, up to %d voices (pool %d), heavy patch + all effects\n", maxV, kVoices);
    std::printf("  render time %.3f s  ->  %.1f%% of one core, %.1fx realtime\n", el, el / 20.0 * 100.0, 20.0 / el);
    std::printf("  worst 256-frame block %.3f ms of %.3f ms budget (%.0f%%)\n", worst * 1e3, period * 1e3, worst / period * 100);
}
