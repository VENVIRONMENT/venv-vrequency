// venv-vrequency — engine tests: poly mode (every key its own note) against the three strings.
#include "tests/support.h"
#include "core/presets/presets.h"

void testPoly() {
    SECTION("poly: three keys on one row are three notes; strings mode slides them into one");
    Rig r;
    r.inst.setPatch(0, factoryPatch(0));
    r.run(0.05);
    for (int c = 0; c < 3; ++c) r.inst.key(1, c, true);
    r.run(0.1);
    CHECK(r.gated(0) == 1, "strings: three keys on one row sound one note (%d)", r.gated(0));
    for (int c = 0; c < 3; ++c) r.inst.key(1, c, false);
    r.run(0.6);

    r.inst.set(S_POLY, 1);
    r.run(0.05);
    for (int c = 0; c < 3; ++c) r.inst.key(1, c, true);
    r.inst.key(2, 4, true);
    r.run(0.1);
    CHECK(r.gated(0) == 4, "poly: four keys, four notes (%d)", r.gated(0));
    r.inst.key(1, 1, false);
    r.run(0.05);
    CHECK(r.gated(0) == 3, "poly: letting one key go ends only that note (%d)", r.gated(0));

    r.inst.cmd(EV_SUSTAIN, 1);
    r.inst.key(1, 0, false);
    r.run(0.05);
    CHECK(r.gated(0) == 3, "poly: sustain holds a released key (%d)", r.gated(0));
    UiState u = r.inst.uiState();
    CHECK(!(u.str[1].heldMask & 1) && (u.str[1].soundMask & 1), "poly: the screen shows the released key still sounding");
    CHECK((u.str[1].heldMask & 4) && (u.str[1].soundMask & 4), "poly: and the held key as held and sounding");
    r.inst.cmd(EV_SUSTAIN, 0);
    r.run(0.05);
    CHECK(r.gated(0) == 2, "poly: lifting sustain lets it go (%d)", r.gated(0));

    for (int k = 0; k < 14; ++k) r.inst.key(0, k % 7, true), r.inst.key(2, k % 10, true);
    r.run(0.05);
    CHECK(r.gated(0) <= kSlots, "poly: never more notes than voice slots (%d)", r.gated(0));

    r.inst.set(S_POLY, 0);
    r.run(0.05);
    CHECK(r.gated(0) == 0, "switching back to strings lets every poly note go (%d)", r.gated(0));
}
