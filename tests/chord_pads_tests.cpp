// venv-vrequency — engine tests: the chord pads (Hold and Drone).
#include "tests/support.h"
#include "core/presets/presets.h"
#include <cstdint>
#include <cstring>

namespace {
uint64_t padSerials(const Rig &r) { // which voices part 1 is sounding: the same sum means no voice was restarted
    uint64_t s = 0;
    for (int i = 0; i < kVoices; ++i) if (r.inst.voice(i).part == 1 && r.inst.voice(i).gate) s += r.inst.voice(i).serial;
    return s;
}
} // namespace

void testChordPads() {
    SECTION("chord pads: a held chord under a melody keeps its sound");
    Rig r;
    r.inst.setPatch(0, factoryPatch(0));
    r.run(0.05);
    r.inst.cmd(EV_CHORD, 3); // Hold is the mode a fresh instrument starts in
    int minPads = 99; bool leadHeard = false; double t = 0;
    auto watch = [&]() {
        if (t > 0.3 && r.gated(1) < minPads) minPads = r.gated(1);
        if (r.gated(0) > 0) leadHeard = true;
        t += 256 / SR;
    };
    r.run(0.5, watch);
    UiState u = r.inst.uiState();
    CHECK(u.padsPlaying && u.holdChord == 3 && u.curChord == 3, "a tap holds chord IV");
    CHECK(std::strcmp(u.padsName, "Warm Lead") == 0, "the pads took the live sound (%s)", u.padsName);
    r.inst.setPatch(0, factoryPatch(1)); // change the lead instrument
    r.inst.key(2, 1, true);
    r.run(2.0, watch);
    u = r.inst.uiState();
    CHECK(std::strcmp(u.padsName, "Warm Lead") == 0, "the pads keep their sound when the lead changes (%s)", u.padsName);
    CHECK(std::strcmp(u.leadName, "Glass Bells") == 0, "the lead has the new sound (%s)", u.leadName);
    CHECK(minPads >= 4, "bass + triad sounded all the time under the melody (min %d voices)", minPads);
    CHECK(leadHeard, "the melody played over the chord");
    r.inst.key(2, 1, false);

    SECTION("chord pads: glide, stop and start, drones, panic");
    const uint64_t before = padSerials(r);
    r.inst.cmd(EV_CHORD, 5); r.run(0.05);
    CHECK(r.gated(1) == 4 && padSerials(r) == before && r.inst.uiState().curChord == 5, "changing the held chord glides the same four voices");
    r.inst.cmd(EV_PADS); r.run(0.3);
    CHECK(r.gated(1) == 0 && !r.inst.uiState().padsPlaying, "Stop releases the pads");
    r.inst.cmd(EV_PADS); r.run(0.3);
    CHECK(r.gated(1) == 4 && r.inst.uiState().holdChord == 5, "Play brings the held chord back");
    r.inst.cmd(EV_CHORD, 5); r.run(0.3);
    CHECK(r.gated(1) == 0 && r.inst.uiState().holdChord == -1, "tapping the held chord again lets it go");
    r.inst.set(S_PADMODE, PM_DRONE);
    r.inst.cmd(EV_CHORD, 0); r.inst.cmd(EV_CHORD, 4); r.run(0.3);
    CHECK(r.gated(1) == 2, "two latched drone notes (%d)", r.gated(1));
    r.inst.cmd(EV_CHORD, 4); r.run(0.2);
    CHECK(r.gated(1) == 1, "tapping a drone again removes it (%d)", r.gated(1));
    r.inst.set(S_PADMODE, PM_HOLD);
    r.inst.cmd(EV_CHORD, 2); r.run(0.3);
    CHECK(r.gated(1) == 5, "a held chord sounds over the drone (%d voices)", r.gated(1));
    r.inst.cmd(EV_PANIC); r.run(0.3);
    CHECK(r.gated(1) == 0 && !r.inst.uiState().padsPlaying, "panic stops the pads");
}
