// venv-vrequency — engine tests: the limiter, the XY pad and the crusher.
#include "tests/support.h"
#include "core/presets/presets.h"
#include "core/synth/scale.h"
#include <algorithm>
#include <cmath>
#include <vector>

void testLimiter() {
    SECTION("limiter: loud input cannot exceed full scale");
    Rig r;
    Patch p = factoryPatch(8);
    p.p[P_LEVEL] = 4.0f; p.p[P_RESO] = 0.95f; p.p[P_DRIVE] = 1.0f;
    r.inst.setPatch(0, p);
    r.inst.set(S_VOLUME, 1.0f);
    r.inst.cmd(EV_CHORD, 0);
    for (int s = 0; s < 3; ++s) r.inst.key(s, s + 1, true);
    r.run(2.0);
    float pk = std::max(peak(r.L), peak(r.R));
    CHECK(pk <= 1.0f, "peak %.4f <= 1.0 with a deliberately abusive patch", pk);
    CHECK(pk > 0.5f, "and it is actually loud (%.3f)", pk);
}

void testXY() {
    SECTION("XY pad modulates the sound while a note is held");
    Rig r;
    Patch p = purePatch();
    p.p[P_WAVE1] = W_SAW; p.p[P_CUTOFF] = 1200; p.p[P_RESO] = 0.2f;
    r.inst.setPatch(0, p);
    r.inst.key(1, 2, true);
    Event e; e.type = EV_XY; e.a = 1; e.f = 0.05f; e.g = 0.0f; r.inst.push(e);
    r.run(0.5);
    int n; float dark = brightness(r.tail(0.2, n), (int)(0.2 * SR));
    e.f = 0.95f; r.inst.push(e);
    r.run(0.5);
    float bright = brightness(r.tail(0.2, n), (int)(0.2 * SR));
    CHECK(bright > dark * 2.0f, "x sweeps the filter (%.4f -> %.4f)", dark, bright);
    CHECK(r.active(0) == 1, "note kept sounding throughout");
    e.a = 0; r.inst.push(e); r.run(0.3);
    float back = brightness(r.tail(0.2, n), (int)(0.2 * SR));
    CHECK(back < bright * 0.8f && back > dark * 1.2f, "releasing the pad returns to the patch setting (%.4f)", back);
    // vibrato mapping
    r.inst.set(S_XYMAP, XY_WOBBLE);
    e.a = 1; e.f = 0.5f; e.g = 1.0f; r.inst.push(e);
    float lo = 1e9f, hi = 0;
    for (int i = 0; i < 20; ++i) { r.run(0.03); float hz = measureHz(r.tail(0.03, n), (int)(0.03 * SR)); if (hz < lo) lo = hz; if (hz > hi) hi = hz; }
    CHECK(hi / lo > 1.03f, "y adds vibrato in WOBBLE mapping (%.2f..%.2f Hz)", lo, hi);
    // pitch bend
    e.a = 0; r.inst.push(e);
    Event b; b.type = EV_BEND; b.f = 1.0f; r.inst.push(b);
    r.run(0.8);
    float hz = measureHz(r.tail(0.2, n), (int)(0.2 * SR));
    float expect = (float)midiToHz(55 + 2); // middle string col 2 = G3
    CHECK(near(hz, expect, 0.01f), "bend up reaches +2 semitones (%.2f vs %.2f Hz)", hz, expect);
}

static float heldFraction(const float *x, int n) { // how much of the wave is flat steps
    int same = 0;
    for (int i = 1; i < n; ++i) if (std::fabs(x[i] - x[i - 1]) < 2e-5f) ++same; // the limiter after it may drift by a hair
    return n > 1 ? (float)same / (n - 1) : 0;
}
static int distinctLevels(const float *x, int n) {
    std::vector<long> v(n);
    for (int i = 0; i < n; ++i) v[i] = std::lround(x[i] * 2000);
    std::sort(v.begin(), v.end());
    return (int)(std::unique(v.begin(), v.end()) - v.begin());
}

void testGlitch() {
    SECTION("GLITCH pad mapping crushes the whole output and lets go cleanly");
    Rig r;
    r.inst.setPatch(0, purePatch());
    r.inst.key(1, 2, true); // G3, 196 Hz
    r.run(0.5);
    int n; const float *t = r.tail(0.2, n);
    const float hz = (float)midiToHz(55);
    float cleanHeld = heldFraction(t, n), cleanRms = rms(t, n), cleanTone = goertzel(t, n, hz);
    int cleanLevels = distinctLevels(t, n);
    CHECK(cleanHeld < 0.05f && cleanLevels > 150, "clean sine is smooth (%.3f flat, %d levels)", cleanHeld, cleanLevels);

    r.inst.set(S_XYMAP, XY_GLITCH);
    Event e; e.type = EV_XY; e.a = 1; e.f = 0.5f; e.g = 0.8f; r.inst.push(e);
    r.run(0.5);
    t = r.tail(0.2, n);
    float held = heldFraction(t, n), tone = goertzel(t, n, hz);
    int levels = distinctLevels(t, n);
    CHECK(r.inst.uiState().glitch, "pad down switches the crusher on");
    CHECK(held > 0.8f, "wave becomes flat steps (%.3f flat; expect about %.3f)", held, 1.0f - 1.0f / r.inst.crusher().holdSamples());
    CHECK(levels >= 4 && levels <= 64, "only a few levels are left (%d)", levels);
    CHECK(tone > 0.5f * cleanTone, "the note is still there underneath (%.4f vs %.4f)", tone, cleanTone);
    CHECK(rms(t, n) > 0.5f * cleanRms && rms(t, n) < 1.7f * cleanRms, "loudness stays in the same range (%.4f vs %.4f)", rms(t, n), cleanRms);
    CHECK(peak(r.L) <= 1.0f, "output stays inside full scale (%.3f)", peak(r.L));

    // moving on the pad changes the character: right = coarser, down = more levels
    e.f = 0.95f; e.g = 0.1f; r.inst.push(e); r.run(0.3);
    t = r.tail(0.2, n);
    CHECK(heldFraction(t, n) > held && distinctLevels(t, n) > levels, "x coarsens, low y restores levels (%.3f flat, %d levels)", heldFraction(t, n), distinctLevels(t, n));

    // quieter playing gets the same grit, not silence
    r.inst.set(S_VOLUME, 0.15f); e.f = 0.5f; e.g = 0.8f; r.inst.push(e); r.run(0.8);
    t = r.tail(0.2, n);
    int quietLevels = distinctLevels(t, n);
    CHECK(quietLevels >= 4 && quietLevels <= 64 && rms(t, n) > 0.001f, "same grit at low volume (%d levels, rms %.4f)", quietLevels, rms(t, n));
    r.inst.set(S_VOLUME, 0.7f);

    e.a = 0; r.inst.push(e); r.run(0.4);
    t = r.tail(0.2, n);
    CHECK(!r.inst.uiState().glitch && heldFraction(t, n) < 0.05f, "lifting the finger gives the clean sound back (%.3f flat)", heldFraction(t, n));

    // Hold latches it; changing the mapping or panic always releases it
    r.inst.set(S_XYHOLD, 1); e.a = 1; r.inst.push(e); r.run(0.1); e.a = 0; r.inst.push(e); r.run(0.2);
    CHECK(r.inst.uiState().glitch, "Hold keeps it on after the finger lifts");
    r.inst.set(S_XYMAP, XY_FILTER); r.run(0.2);
    CHECK(!r.inst.uiState().glitch, "choosing another mapping switches it off");
    r.inst.set(S_XYMAP, XY_GLITCH); e.a = 1; r.inst.push(e); r.run(0.1);
    r.inst.cmd(EV_PANIC); r.run(0.1);
    CHECK(!r.inst.uiState().glitch, "panic switches it off");
}
