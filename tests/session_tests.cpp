// venv-vrequency — engine tests: saving a session and loading it back.
#include "tests/support.h"
#include "core/presets/presets.h"
#include "core/instrument/session.h"
#include <cstring>

void testSessionRoundTrip() {
    SECTION("session snapshot / restore");
    Rig a;
    a.inst.setPatch(0, factoryPatch(6));
    a.inst.set(S_ROOT, 2); a.inst.set(S_SCALE, SC_DORIAN); a.inst.set(S_BPM, 96);
    a.run(0.05);
    for (int d : {0, 5, 3, 4}) a.inst.cmd(EV_CHORD, d);
    a.inst.cmd(EV_LOOP, LC_PEDAL);
    a.run(60.0 / 96 * 4 * 2 + 0.2); // count-in adapts to the sounding chord pads (<= 6 beats)
    a.inst.key(1, 1, true); a.run(0.5); a.inst.key(1, 1, false); a.run(1.0);
    a.inst.cmd(EV_LOOP, LC_PEDAL); a.run(3.0);
    SessionData s1; a.inst.snapshot(s1);
    CHECK(s1.layers.size() == 1 && s1.layers[0].ev.size() >= 2, "snapshot contains the loop (%zu layers)", s1.layers.size());
    Rig b;
    b.inst.restore(s1);
    SessionData s2; b.inst.snapshot(s2);
    CHECK(s2.settings.tuning.root == 2 && s2.settings.tuning.scale == SC_DORIAN && s2.settings.bpm == 96, "tuning and tempo restored");
    CHECK(std::strcmp(s2.lead.name, "Dreamy Keys") == 0, "lead patch restored");
    CHECK(s2.holdChord == 4 && std::strcmp(s2.pads.name, "Dreamy Keys") == 0, "the held chord and the pads' sound restored");
    CHECK(s2.layers.size() == s1.layers.size() && s2.loopBeats == s1.loopBeats, "loop restored");
    bool same = s2.layers.size() == 1 && s2.layers[0].ev.size() == s1.layers[0].ev.size();
    for (size_t i = 0; same && i < s1.layers[0].ev.size(); ++i)
        if (s1.layers[0].ev[i].beat != s2.layers[0].ev[i].beat || s1.layers[0].ev[i].a != s2.layers[0].ev[i].a) same = false;
    CHECK(same, "loop events identical after restore");
    b.inst.cmd(EV_LOOP, LC_PLAYSTOP);
    int heard = 0; bool w = false;
    b.run(6.0, [&]() { bool on = b.gated(2) > 0; if (on && !w) ++heard; w = on; });
    CHECK(heard >= 1, "restored loop plays back (%d phrases)", heard);
}
