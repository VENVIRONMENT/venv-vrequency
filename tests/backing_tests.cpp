// venv-vrequency — engine tests: the backing.
#include "tests/support.h"
#include "core/presets/presets.h"
#include <cstdint>
#include <cstring>
#include <vector>

void testBacking() {
    SECTION("backing: tapped progression loops at tempo and keeps its sound");
    Rig r;
    Patch lead = factoryPatch(0);
    r.inst.setPatch(0, lead);
    r.inst.set(S_BPM, 120); // 4-beat chords = 2 s each
    r.run(0.05);
    for (int d : {0, 3, 4, 3}) r.inst.cmd(EV_CHORD, d);
    std::vector<int> seq; int minBack = 99; bool leadHeard = false;
    double t = 0;
    auto watch = [&]() {
        UiState u = r.inst.uiState();
        if (seq.empty() || seq.back() != u.curChord) seq.push_back(u.curChord);
        if (t > 0.3 && r.gated(1) < minBack) minBack = r.gated(1);
        if (r.gated(0) > 0) leadHeard = true;
        t += 256 / SR;
    };
    r.run(1.0, watch);
    CHECK(std::strcmp(r.inst.uiState().backName, "Warm Lead") == 0, "backing captured the live sound");
    r.inst.setPatch(0, factoryPatch(1)); // change the lead instrument
    r.inst.key(2, 1, true);
    r.run(4.0, watch);
    r.inst.key(2, 3, true);
    r.run(4.5, watch);
    UiState u = r.inst.uiState();
    CHECK(std::strcmp(u.backName, "Warm Lead") == 0, "backing still uses its original sound (%s)", u.backName);
    CHECK(std::strcmp(u.leadName, "Glass Bells") == 0, "lead uses the new sound (%s)", u.leadName);
    int expect[5] = {0, 3, 4, 3, 0};
    bool ok = seq.size() >= 5;
    for (int i = 0; ok && i < 5; ++i) if (seq[i] != expect[i]) ok = false;
    CHECK(ok, "chords stepped I-IV-V-IV-I (got %zu steps: %d %d %d %d %d)", seq.size(), seq.size() > 0 ? seq[0] : -9,
          seq.size() > 1 ? seq[1] : -9, seq.size() > 2 ? seq[2] : -9, seq.size() > 3 ? seq[3] : -9, seq.size() > 4 ? seq[4] : -9);
    CHECK(minBack >= 4, "bass + triad sounded continuously under the melody (min %d voices)", minBack);
    CHECK(leadHeard, "melody played over the backing");
    CHECK(u.progLen == 4 && u.backPlaying, "progression has 4 chords and is playing");

    SECTION("backing: remove, replace, stop, drone, arp");
    r.inst.cmd(EV_BACK, BC_REMOVE_LAST); r.run(0.05);
    CHECK(r.inst.uiState().progLen == 3, "remove last chord");
    r.inst.cmd(EV_BACK, BC_SELECT_SLOT, 1); r.inst.cmd(EV_CHORD, 5); r.run(0.05);
    u = r.inst.uiState();
    CHECK(u.progLen == 3 && u.prog[1] == 5, "selected slot replaced (slot1=%d)", u.prog[1]);
    r.inst.cmd(EV_BACK, BC_PLAYSTOP); r.run(0.3);
    CHECK(r.gated(1) == 0, "stop releases the backing");
    r.inst.cmd(EV_BACK, BC_PLAYSTOP); r.run(0.3);
    CHECK(r.gated(1) >= 4, "start resumes it");
    r.inst.set(S_BACKSTYLE, BS_ARP_UP); r.inst.set(S_ARPRATE, 4);
    int trig = 0; uint64_t lastSerial = 0;
    r.run(2.0, [&]() {
        for (int i = 0; i < kVoices; ++i) {
            const Voice &v = r.inst.voice(i);
            if (v.active() && v.part == 1 && v.slot == 1 && v.serial > lastSerial) { lastSerial = v.serial; ++trig; }
        }
    });
    CHECK(trig >= 12 && trig <= 18, "arpeggio retriggers 4 steps per beat (%d in 4 beats)", trig);
    r.inst.cmd(EV_BACK, BC_CLEAR); r.run(0.2);
    u = r.inst.uiState();
    CHECK(u.progLen == 0 && !u.backPlaying && r.gated(1) == 0, "clear empties the backing");
    r.inst.set(S_BACKSTYLE, BS_SUSTAIN);
    r.inst.set(S_BACKMODE, BM_DRONE);
    r.inst.cmd(EV_CHORD, 0); r.inst.cmd(EV_CHORD, 4); r.run(0.3);
    CHECK(r.gated(1) == 2, "two latched drone notes (%d)", r.gated(1));
    r.inst.cmd(EV_CHORD, 4); r.run(0.2);
    CHECK(r.gated(1) == 1, "tapping a drone again removes it (%d)", r.gated(1));
    r.inst.set(S_BACKMODE, BM_HOLD);
    r.inst.cmd(EV_BACK, BC_CLEAR); r.run(0.1);
    r.inst.cmd(EV_CHORD, 2); r.run(0.3);
    CHECK(r.gated(1) == 4 && r.inst.uiState().curChord == 2, "hold mode latches one chord");
    uint64_t before = 0, after = 0;
    for (int i = 0; i < kVoices; ++i) if (r.inst.voice(i).part == 1 && r.inst.voice(i).gate) before += r.inst.voice(i).serial;
    r.inst.cmd(EV_CHORD, 5); r.run(0.05);
    for (int i = 0; i < kVoices; ++i) if (r.inst.voice(i).part == 1 && r.inst.voice(i).gate) after += r.inst.voice(i).serial;
    CHECK(r.gated(1) == 4 && before == after, "changing a held chord glides the same four voices");
}
