// venv-vrequency — engine tests: the looper.
#include "tests/support.h"
#include "core/presets/presets.h"
#include <cmath>
#include <cstring>

void testLooper() {
    SECTION("looper: count-in, record, loop playback");
    Rig r;
    r.inst.setPatch(0, purePatch());
    r.inst.set(S_BPM, 120); // beat 0.5 s, bar 2 s
    r.run(0.05);
    r.inst.cmd(EV_LOOP, LC_PEDAL);
    r.run(0.1);
    CHECK(r.inst.uiState().loopState == L_COUNTIN, "pedal starts a count-in");
    r.clear();
    r.run(1.85);
    // four clicks: count onsets in the rendered audio
    int clicks = 0; bool in = false;
    for (size_t i = 0; i < r.L.size(); ++i) {
        float a = std::fabs(r.L[i]);
        if (!in && a > 0.05f) { ++clicks; in = true; }
        if (in && i % 4800 == 0) in = false;
    }
    r.run(0.1);
    CHECK(clicks >= 3 && clicks <= 4, "count-in clicks heard (%d after the first)", clicks);
    CHECK(r.inst.uiState().loopState == L_REC, "recording starts after the count-in (state %d)", r.inst.uiState().loopState);
    // we are ~0.05 s into the bar. Play: A2 at beat 1, slide to D3 at beat 1.5, release at beat 2
    r.run(0.45); r.inst.key(0, 0, true);
    r.run(0.25); r.inst.key(0, 2, true);
    r.run(0.25); r.inst.key(0, 2, false); r.inst.key(0, 0, false);
    r.run(0.85);
    r.inst.cmd(EV_LOOP, LC_PEDAL); // beat ~3.7: closes at the bar line
    r.run(0.05);
    UiState u = r.inst.uiState();
    CHECK(u.loopState == L_PLAY && u.loopClosing, "pedal closes the loop, snapped to the bar (closing=%d)", u.loopClosing);
    r.run(0.3);
    u = r.inst.uiState();
    CHECK(u.layers == 1 && u.loopBars == 1, "one layer, one bar (layers %d bars %d)", u.layers, u.loopBars);
    // Now nothing is held. Listen to two full cycles: the phrase must come back each bar.
    int cyclesHeard = 0; bool glideSeen = false; bool wasOn = false; float fLo = 1e9f, fHi = 0;
    r.clear();
    r.run(4.0, [&]() {
        bool on = r.gated(2) > 0;
        if (on && !wasOn) ++cyclesHeard;
        wasOn = on;
        for (int i = 0; i < kVoices; ++i) {
            const Voice &v = r.inst.voice(i);
            if (v.active() && v.part == 2 && v.gate) {
                if (v.note > 45.5f && v.note < 49.5f) glideSeen = true;
                if (v.note < fLo) fLo = v.note;
                if (v.note > fHi) fHi = v.note;
            }
        }
    });
    CHECK(cyclesHeard == 2, "recorded phrase replays once per bar (%d in two bars)", cyclesHeard);
    CHECK(glideSeen && fLo < 45.2f && fHi > 49.8f, "the recorded slide is reproduced as a continuous glide (%.2f..%.2f)", fLo, fHi);
    CHECK(rms(r.L.data(), (int)r.L.size()) > 0.01f, "loop is audible with no keys held");
    CHECK(r.gated(0) == 0, "live strings are free while the loop plays");

    SECTION("looper: overdub, undo, stop/start, clear");
    r.inst.cmd(EV_LOOP, LC_PEDAL); r.run(0.1);
    CHECK(r.inst.uiState().loopState == L_OVERDUB, "pedal while playing starts an overdub");
    r.inst.key(2, 3, true); r.run(0.4); r.inst.key(2, 3, false); r.run(0.3);
    r.inst.cmd(EV_LOOP, LC_PEDAL); r.run(0.1);
    u = r.inst.uiState();
    CHECK(u.loopState == L_PLAY && u.layers == 2, "overdub committed as a second layer (%d)", u.layers);
    int l1 = 0, l2 = 0; bool w1 = false, w2 = false;
    r.run(4.0, [&]() {
        bool a = r.gated(2) > 0, b = r.gated(3) > 0;
        if (a && !w1) ++l1;
        if (b && !w2) ++l2;
        w1 = a; w2 = b;
    });
    CHECK(l1 == 2 && l2 == 2, "both layers replay every bar (%d, %d)", l1, l2);
    // live playing during loop playback is not blocked
    r.inst.key(1, 4, true); r.run(0.2);
    CHECK(r.gated(0) == 1, "live note sounds over the loop");
    r.inst.key(1, 4, false);
    r.inst.cmd(EV_LOOP, LC_UNDO); r.run(0.1);
    CHECK(r.inst.uiState().layers == 1, "undo removes the last overdub");
    l2 = 0; w2 = false;
    r.run(2.2, [&]() { bool b = r.gated(3) > 0; if (b && !w2) ++l2; w2 = b; });
    CHECK(l2 == 0, "undone layer no longer plays");
    r.inst.cmd(EV_LOOP, LC_PLAYSTOP); r.run(0.3);
    CHECK(r.inst.uiState().loopState == L_STOPPED && r.gated(2) == 0, "stop halts playback");
    r.inst.cmd(EV_LOOP, LC_PLAYSTOP);
    l1 = 0; w1 = false;
    r.run(2.1, [&]() { bool a = r.gated(2) > 0; if (a && !w1) ++l1; w1 = a; });
    CHECK(r.inst.uiState().loopState == L_PLAY && l1 == 1, "start resumes it (%d phrase)", l1);
    r.inst.cmd(EV_LOOP, LC_CLEAR); r.run(0.2);
    u = r.inst.uiState();
    CHECK(u.loopState == L_EMPTY && u.layers == 0, "clear empties the looper");

    SECTION("looper: long loops and layers keep their own sound");
    r.inst.cmd(EV_LOOP, LC_PEDAL);
    r.run(2.05);
    for (int i = 0; i < 16; ++i) { r.inst.key(i % 3, i % 7, true); r.run(1.0); r.inst.key(i % 3, i % 7, false); r.run(0.125); }
    r.run(0.1); // 18.1 s recorded = 9.05 bars -> snaps to 9 bars
    r.inst.cmd(EV_LOOP, LC_PEDAL); r.run(0.2);
    u = r.inst.uiState();
    CHECK(u.loopBars == 9 && u.layers == 1, "18-second loop recorded (%d bars)", u.loopBars);
    r.inst.setPatch(0, factoryPatch(4)); r.run(0.1);
    CHECK(std::strcmp(r.inst.part(2).patch.name, "Pure") == 0, "loop layer keeps the sound it was recorded with (%s)", r.inst.part(2).patch.name);
}
