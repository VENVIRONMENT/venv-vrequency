// venv-vrequency — engine tests: the pulse pads in the instrument: taps, LATCH, SYNC, the loop, panic, sessions.
#include "tests/support.h"
#include "core/pulse/pulse_bank.h"
#include <algorithm>
#include <vector>

namespace {
int ticksOf(const std::vector<PulseTick> &t, int pad) {
    return (int)std::count_if(t.begin(), t.end(), [pad](const PulseTick &x) { return x.pad == pad; });
}
} // namespace

void testPulsePads() {
    SECTION("pulse pads: a tap");
    {
        Rig r;
        r.run(0.05);
        CHECK(peak(r.L) == 0.0f, "silent before the tap");
        r.clear();
        r.inst.hit(6); // sub thump
        r.run(0.03);
        CHECK(peak(r.L) > 0.1f, "a tap is heard inside 30 ms (peak %.2f)", peak(r.L));
        CHECK(r.inst.uiState().pulseHits[6] == 1 && r.inst.uiState().pulsing, "it is counted");
        r.run(1.0); r.clear(); r.run(0.2);
        CHECK(peak(r.L) < 1e-4f && !r.inst.uiState().pulsing, "and it is over after its length (peak %.6f)", peak(r.L));
    }
    {   // the bank plays exactly what pulseRender draws
        PulseBank b; b.init(SR);
        for (int p = 0; p < kPulsePads; ++p) {
            const int n = PulseVoice::lengthSamples(b.pad[p], SR) + 64;
            std::vector<float> live(n, 0.0f), pic(n);
            b.trigger(p, 1.0f, 120);
            for (int i = 0; i < n; i += kBlock) b.render(live.data() + i, std::min(kBlock, n - i), 0, 0, 120, (uint64_t)i);
            pulseRender(b.pad[p], SR, b.rateOf(p, 120), pic.data(), n);
            CHECK(live == pic, "pad %d (%s): the sound played is the picture's sound, sample for sample", p + 1, pulseTypeName(b.pad[p].type));
        }
    }
    {
        Rig r;
        r.inst.pulseSet(0, PF_TYPE, PT_BLIP); r.inst.pulseSet(0, PF_TONE, 660); r.inst.pulseSet(0, PF_LENGTH, 200);
        r.run(0.01); r.clear();
        r.inst.hit(0); r.run(0.15);
        const UiState u = r.inst.uiState();
        CHECK(u.pulse[0].type == PT_BLIP && u.pulse[0].tone == 660 && u.pulse[0].lengthMs == 200, "any pad can be any kind, with its own settings");
        CHECK(near(measureHz(r.L.data() + 2400, 4800), 660, 0.005f), "and it plays them (%.1f Hz)", measureHz(r.L.data() + 2400, 4800));
        r.inst.pulseSet(0, PF_TONE, 1e9f); r.inst.pulseSet(0, PF_LEVEL, -3); r.run(0.01);
        CHECK(r.inst.uiState().pulse[0].tone == kPulseToneMax && r.inst.uiState().pulse[0].level == 0, "settings out of range are held at the limits");
    }

    SECTION("pulse pads: LATCH");
    {
        Rig r;
        r.inst.pulseSet(0, PF_RATE, 10);
        r.inst.pulseLatch(0, true);
        std::vector<PulseTick> t = pulseTicks(r, 1.02);
        bool even = t.size() >= 2;
        for (size_t i = 1; i < t.size(); ++i) if (std::llabs((long long)(t[i].frame - t[i - 1].frame) - 4800) > 1) even = false;
        CHECK(ticksOf(t, 0) == 11 && even, "a latched click repeats at RATE, to the sample (%d hits in 1.02 s at 10 Hz)", ticksOf(t, 0));
        r.inst.pulseLatch(0, false);
        t = pulseTicks(r, 0.5);
        CHECK(t.empty(), "tapped again it stops (%zu more)", t.size());
    }
    {
        Rig r;
        r.inst.pulseLatch(5, true); // pulse train, 250 ms: latched it runs on
        r.run(1.0); r.clear(); r.run(0.2);
        CHECK(rms(r.L.data(), (int)r.L.size()) > 0.01f, "a latched pulse train keeps going past its length");
        std::vector<PulseTick> t = pulseTicks(r, 0.5);
        CHECK(ticksOf(t, 5) >= 19 && ticksOf(t, 5) <= 21, "at its rate (%d clicks in 0.5 s at 40 Hz)", ticksOf(t, 5));
        r.inst.pulseSet(5, PF_RATE, 20);
        r.run(0.01);
        t = pulseTicks(r, 0.5);
        CHECK(ticksOf(t, 5) >= 9 && ticksOf(t, 5) <= 11, "and a new rate is followed at once (%d in 0.5 s at 20 Hz)", ticksOf(t, 5));
        r.inst.pulseLatch(5, false);
        r.run(0.05); r.clear(); r.run(0.2);
        CHECK(peak(r.L) < 1e-4f, "unlatched it stops (peak %.6f)", peak(r.L));
        r.inst.pulseLatch(2, true); r.inst.pulseLatch(4, true); r.run(0.3);
        r.inst.cmd(EV_PANIC);
        r.run(0.05); r.clear(); r.run(0.2);
        const UiState u = r.inst.uiState();
        CHECK(peak(r.L) < 1e-4f && !u.pulseLatched[2] && !u.pulseLatched[4] && !u.pulsing, "panic stops every pulse and drops the latches");
    }

    SECTION("pulse pads: SYNC");
    {
        float k = 0;
        CHECK(pulseSyncRate(7.5f, 120, &k) == 8.0f && k == 4, "at 120 bpm, 7.5 Hz snaps to sixteenth notes (8 Hz)");
        CHECK(pulseSyncRate(0.3f, 120, &k) == 0.25f && k == 0.125f, "and 0.3 Hz to once every two bars");
        Rig r;
        r.inst.set(S_BPM, 120); r.inst.set(S_PULSESYNC, 1);
        r.inst.pulseSet(1, PF_RATE, 7.5f);
        r.run(0.37);
        r.inst.pulseLatch(1, true);
        std::vector<PulseTick> t = pulseTicks(r, 1.0);
        int off = 0;
        for (size_t i = 1; i < t.size(); ++i) { long long m = (long long)(t[i].frame % 6000); if (m > 1 && m < 5999) ++off; }
        CHECK(t.size() >= 8 && off == 0, "a latched tick then falls on the sixteenth-note grid of the beat clock (%zu hits, %d off it)", t.size(), off);
        CHECK(r.inst.uiState().set.pulseSync, "SYNC is a setting the interface sees");
    }

    SECTION("pulse pads: the loop records taps");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        r.inst.cmd(EV_LOOP, LC_PEDAL);
        r.run(2.05);           // count-in done, recording
        CHECK(r.inst.uiState().loopState == L_REC, "recording");
        r.run(0.45); r.inst.hit(3);
        r.run(1.4);
        r.inst.cmd(EV_LOOP, LC_PEDAL);
        r.run(0.3);
        CHECK(r.inst.uiState().loopState == L_PLAY && r.inst.uiState().layers == 1, "a one-bar loop with a tap in it");
        const uint32_t h0 = r.inst.uiState().pulseHits[3];
        std::vector<PulseTick> t = pulseTicks(r, 4.0);
        CHECK(ticksOf(t, 3) == 2 && r.inst.uiState().pulseHits[3] == h0 + 2, "the loop plays the tap back once a bar (%d in two bars)", ticksOf(t, 3));
        bool onTime = ticksOf(t, 3) == 2 && std::llabs((long long)(t[1].frame - t[0].frame) - (long long)(2.0 * SR)) < 64;
        CHECK(onTime, "exactly a bar apart");
    }

    SECTION("pulse pads: loud and saved");
    {
        Rig r;
        for (int p = 0; p < kPulsePads; ++p) { r.inst.pulseSet(p, PF_LEVEL, 1); r.inst.pulseLatch(p, true); }
        r.inst.set(S_VOLUME, 1);
        r.run(1.5);
        CHECK(peak(r.L) <= 1.0f, "all eight latched at full level stay inside full scale (%.3f)", peak(r.L));
        r.inst.pulseSet(4, PF_TYPE, PT_HISS); r.inst.pulseSet(4, PF_TONE, 333); r.inst.set(S_PULSESYNC, 1);
        r.run(0.01);
        SessionData s; r.inst.snapshot(s);
        Rig b; b.inst.restore(s); b.run(0.01);
        const UiState u = b.inst.uiState();
        CHECK(u.pulse[4].type == PT_HISS && u.pulse[4].tone == 333 && u.pulse[0].level == 1 && u.set.pulseSync,
              "a session keeps every pad's settings and SYNC");
        CHECK(!u.pulseLatched[0] && !u.pulsing, "but nothing is latched after loading");
    }
}
