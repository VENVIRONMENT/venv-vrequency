// venv-vrequency — engine tests: holding a pulse pad like a synth key: down sounds, up stops,
// sustain keeps it, and the loop pedal records the hold.
#include "tests/support.h"
#include "core/pulse/pulse_bank.h"
#include <algorithm>
#include <vector>

namespace {
int hitsOf(const std::vector<PulseTick> &t, int pad) {
    return (int)std::count_if(t.begin(), t.end(), [pad](const PulseTick &x) { return x.pad == pad; });
}
} // namespace

void testPulseHold() {
    SECTION("pulse pads: holding a pad's key");
    {
        Rig r;
        r.inst.pulseSet(0, PF_RATE, 10); // a click, ten times a second while held
        r.run(0.01);
        r.inst.pulseHold(0, true);
        std::vector<PulseTick> t = pulseTicks(r, 0.52);
        CHECK(hitsOf(t, 0) == 6 && r.inst.uiState().pulseHeld[0] && !r.inst.uiState().pulseLatched[0],
              "a held click repeats at RATE while its key is down (%d hits in 0.52 s)", hitsOf(t, 0));
        r.inst.pulseHold(0, false);
        t = pulseTicks(r, 0.5);
        CHECK(hitsOf(t, 0) == 0 && !r.inst.uiState().pulseHeld[0], "and stops when the key comes up (%d after)", hitsOf(t, 0));
        r.inst.pulseHold(0, true); r.run(0.02); r.inst.pulseHold(0, false);
        t = pulseTicks(r, 0.5);
        CHECK(r.inst.uiState().pulseHits[0] == 7, "a quick press is one hit (%u in all)", r.inst.uiState().pulseHits[0]);
    }
    {
        Rig r;
        r.inst.pulseHold(5, true); // the pulse train: a sound without end while held
        r.run(1.0); r.clear(); r.run(0.2);
        CHECK(peak(r.L) > 0.05f, "a held pulse train keeps sounding past its length (peak %.2f)", peak(r.L));
        r.inst.pulseHold(5, false);
        r.run(0.5); r.clear(); r.run(0.2);
        CHECK(peak(r.L) < 1e-4f && !r.inst.uiState().pulsing, "let go, it fades out (peak %.6f)", peak(r.L));
    }
    SECTION("pulse pads: sustain keeps a let-go pad going");
    {
        Rig r;
        r.inst.pulseSet(0, PF_RATE, 10);
        r.run(0.01);
        r.inst.cmd(EV_SUSTAIN, 1);
        r.inst.pulseHold(0, true); r.run(0.1); r.inst.pulseHold(0, false);
        std::vector<PulseTick> t = pulseTicks(r, 0.5);
        CHECK(hitsOf(t, 0) >= 4 && r.inst.uiState().pulseHeld[0], "with sustain down the pad goes on after its key (%d hits)", hitsOf(t, 0));
        r.inst.cmd(EV_SUSTAIN, 0);
        t = pulseTicks(r, 0.5);
        CHECK(hitsOf(t, 0) == 0 && !r.inst.uiState().pulseHeld[0], "and stops when sustain lifts (%d after)", hitsOf(t, 0));
        r.inst.pulseLatch(0, true); r.run(0.05);
        r.inst.pulseHold(0, true); r.run(0.05); r.inst.pulseHold(0, false);
        t = pulseTicks(r, 0.3);
        CHECK(hitsOf(t, 0) >= 2 && r.inst.uiState().pulseLatched[0], "a latched pad stays latched when a key on it comes up");
    }
    SECTION("pulse pads: the loop records a hold");
    {
        Rig r;
        r.inst.set(S_BPM, 120); // a bar is 2 s; the click repeats 4 times a second
        r.run(0.05);
        r.inst.cmd(EV_LOOP, LC_PEDAL);
        r.run(2.0);                                        // count-in; recording from 2.0 s
        r.run(0.2); r.inst.pulseHold(0, true);             // held 0.6 s: hits at 0.25, 0.5, 0.75 s into the bar
        r.run(0.6); r.inst.pulseHold(0, false);
        r.run(0.95); r.inst.cmd(EV_LOOP, LC_PEDAL);        // closes at the bar line
        r.run(0.3);
        std::vector<PulseTick> t = pulseTicks(r, 4.0);     // two whole cycles
        CHECK(hitsOf(t, 0) == 6, "the layer plays the held pad back for as long as it was held (%d hits in two bars)", hitsOf(t, 0));
        r.run(1.2);                                        // the quiet part of the bar
        CHECK(!r.inst.uiState().pulseHeld[0], "and lets it go where the key came up");
        r.inst.cmd(EV_LOOP, LC_CLEAR); r.run(0.05);
        CHECK(!r.inst.uiState().pulseHeld[0], "clearing the loop never leaves a pad going");
    }
}
