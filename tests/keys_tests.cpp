// venv-vrequency — engine tests: playing the strings: pitch, slides, polyphony, sustain, a change of sound.
#include "tests/support.h"
#include "core/instrument/events.h"
#include "core/instrument/settings.h"
#include "core/synth/voice.h"
#include <algorithm>
#include <cstdint>
#include <vector>

void testFrequenciesAndGlide() {
    SECTION("a key press sounds the correct frequency");
    Rig r;
    r.inst.setPatch(0, purePatch());
    r.inst.key(0, 0, true);
    r.run(0.5);
    int n; const float *x = r.tail(0.3, n);
    float hz = measureHz(x, n);
    CHECK(near(hz, 110.0f, 0.002f), "A2 expected 110 Hz, measured %.3f", hz);
    CHECK(rms(x, n) > 0.05f, "note is audible (rms %.4f)", rms(x, n));
    CHECK(r.gated(0) == 1, "one voice sounding");
    uint64_t serial0 = 0; int vi = -1;
    for (int i = 0; i < kVoices; ++i) if (r.inst.voice(i).gate) { serial0 = r.inst.voice(i).serial; vi = i; }

    SECTION("second key on the same string slides (no re-attack)");
    r.inst.key(0, 2, true); // D3 = 146.83 Hz, glide 200 ms
    std::vector<float> track; bool reAttack = false; float minEnv = 1;
    for (int step = 0; step < 15; ++step) {
        r.run(0.02);
        const float *w = r.tail(0.02, n);
        track.push_back(measureHz(w, n));
        const Voice &v = r.inst.voice(vi);
        if (v.amp.st == Env::ATTACK || v.serial != serial0) reAttack = true;
        if (v.amp.v < minEnv) minEnv = v.amp.v;
    }
    CHECK(!reAttack, "same voice keeps sounding, envelope never re-enters attack");
    CHECK(minEnv > 0.95f, "amplitude envelope stays up through the slide (min %.3f)", minEnv);
    CHECK(r.active(0) == 1, "still exactly one voice (no second note was triggered)");
    int between = 0; bool monotonic = true;
    for (size_t i = 0; i < track.size(); ++i) {
        if (track[i] > 113 && track[i] < 143) ++between;
        if (i > 0 && track[i] < track[i - 1] - 0.8f) monotonic = false;
    }
    CHECK(between >= 5, "pitch passes continuously through intermediate frequencies (%d windows between)", between);
    CHECK(monotonic, "pitch rises monotonically during the slide");
    r.run(0.2);
    x = r.tail(0.15, n);
    hz = measureHz(x, n);
    CHECK(near(hz, 146.832f, 0.002f), "slide lands on D3 146.83 Hz, measured %.3f", hz);

    SECTION("releasing the newer key slides back to the held key");
    r.inst.key(0, 2, false);
    r.run(0.1);
    x = r.tail(0.02, n);
    float mid = measureHz(x, n);
    CHECK(mid > 111 && mid < 146, "mid-return pitch is in between (%.2f Hz)", mid);
    r.run(0.4);
    x = r.tail(0.2, n);
    hz = measureHz(x, n);
    CHECK(near(hz, 110.0f, 0.002f), "returns to 110 Hz, measured %.3f", hz);
    CHECK(r.inst.voice(vi).serial == serial0 && r.inst.voice(vi).gate, "still the original voice");

    SECTION("releasing the last key releases the voice");
    r.inst.key(0, 0, false);
    r.run(0.5);
    CHECK(r.active(0) == 0, "voice freed after release (%d active)", r.active(0));
    x = r.tail(0.1, n);
    CHECK(rms(x, n) < 1e-4f, "silence after release (rms %.6f)", rms(x, n));

    SECTION("glide time is configurable");
    r.inst.set(S_GLIDESCALE, 0.0f);
    r.inst.key(0, 0, true); r.run(0.2);
    r.inst.key(0, 2, true); r.run(0.06);
    x = r.tail(0.04, n);
    CHECK(near(measureHz(x, n), 146.832f, 0.004f), "glide scale 0 jumps at once (%.2f Hz)", measureHz(x, n));
    r.inst.cmd(EV_PANIC); r.run(0.1);
}

void testPolyphony() {
    SECTION("strings sound simultaneously; held chord shapes glide together");
    Rig r;
    r.inst.setPatch(0, purePatch());
    r.inst.key(0, 0, true); r.inst.key(1, 0, true); r.inst.key(2, 0, true); // A2 D3 G3
    r.run(0.6);
    int n; const float *x = r.tail(0.4, n);
    CHECK(r.gated(0) == 3, "three voices gated (%d)", r.gated(0));
    float a = goertzel(x, n, 110.0f), d = goertzel(x, n, 146.832f), g = goertzel(x, n, 195.998f), off = goertzel(x, n, 170.0f);
    CHECK(a > 0.03f && d > 0.03f && g > 0.03f, "A2, D3, G3 all present (%.3f %.3f %.3f)", a, d, g);
    CHECK(off < a * 0.05f, "no energy between the notes");
    // re-finger the whole shape one key to the right while the old keys are held
    r.inst.key(0, 1, true); r.inst.key(1, 1, true); r.inst.key(2, 1, true);
    r.run(0.1);
    CHECK(r.active(0) == 3, "chord slide reuses the three voices (%d active)", r.active(0));
    r.run(0.6);
    x = r.tail(0.4, n);
    float c3 = goertzel(x, n, 130.813f), e3 = goertzel(x, n, 164.814f), a3 = goertzel(x, n, 220.0f);
    CHECK(c3 > 0.03f && e3 > 0.03f && a3 > 0.03f, "shape arrived on C3, E3, A3 (%.3f %.3f %.3f)", c3, e3, a3);
    CHECK(goertzel(x, n, 110.0f) < 0.005f, "old pitches are gone (%.4f)", goertzel(x, n, 110.0f));

    SECTION("sustain holds released notes; panic silences everything");
    for (int s = 0; s < 3; ++s) { r.inst.key(s, 0, false); }
    r.inst.cmd(EV_SUSTAIN, 1);
    for (int s = 0; s < 3; ++s) { r.inst.key(s, 1, false); }
    r.run(0.3);
    CHECK(r.gated(0) == 3, "sustain keeps strings ringing after key release (%d)", r.gated(0));
    r.inst.key(0, 4, true); r.run(0.05);
    CHECK(r.active(0) == 3, "a key on a sustained string slides it instead of adding a voice");
    r.inst.key(0, 4, false);
    r.inst.cmd(EV_SUSTAIN, 0);
    r.run(0.4);
    CHECK(r.active(0) == 0, "releasing sustain releases the strings (%d active)", r.active(0));
    r.inst.key(2, 2, true); r.inst.key(2, 2, false); // press+release inside one audio block
    r.run(0.02);
    int nn; CHECK(rms(r.tail(0.015, nn), (int)(0.015 * SR)) > 0.01f, "a very short tap still makes a sound");
    r.run(0.3);
    r.inst.key(0, 0, true); r.inst.key(1, 3, true); r.run(0.1);
    r.inst.cmd(EV_PANIC); r.run(0.2);
    CHECK(r.inst.uiState().voices == 0, "panic frees all voices");

    SECTION("octave shift sweeps held notes");
    r.inst.key(0, 0, true); r.run(0.3);
    r.inst.cmd(EV_OCTAVE, 1); r.run(0.6);
    x = r.tail(0.2, n);
    CHECK(near(measureHz(x, n), 220.0f, 0.003f), "held A2 swept up to A3 (%.2f Hz)", measureHz(x, n));
    CHECK(r.active(0) == 1, "without retriggering");
}

void testPatchSwitchClicks() {
    SECTION("no discontinuity when switching sound under a held note");
    Rig r;
    Patch a = purePatch(); a.p[P_WAVE1] = W_TRI;
    Patch b = a; b.p[P_CUTOFF] = 600; b.p[P_LEVEL] = 0.25f; b.setName("B");
    r.inst.setPatch(0, a);
    r.inst.key(1, 0, true);
    r.run(0.5);
    float steady = 0;
    for (size_t i = r.L.size() - 4800; i < r.L.size(); ++i) steady = std::max(steady, std::fabs(r.L[i] - r.L[i - 1]));
    size_t mark = r.L.size();
    r.inst.setPatch(0, b);
    r.run(0.2);
    float sw = 0;
    for (size_t i = mark; i < r.L.size(); ++i) sw = std::max(sw, std::fabs(r.L[i] - r.L[i - 1]));
    CHECK(sw <= steady * 1.5f + 0.002f, "max sample step at switch %.5f vs steady %.5f", sw, steady);
}
