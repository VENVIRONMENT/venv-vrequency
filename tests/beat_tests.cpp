// venv-vrequency — engine tests: the sixteen-step beat.
#include "tests/support.h"
#include "core/drums/drums.h"
#include "core/instrument/session.h"
#include <cstdint>

void testBeat() {
    SECTION("beat: sixteen steps in time with the tempo");
    {
        Rig r;
        r.inst.set(S_BPM, 120); // one step = 125 ms, one bar = 2 s
        for (int s : {0, 4, 8, 12}) r.inst.beatDo(BT_STEP, D_KICK, s, 1);
        r.inst.beatDo(BT_STEP, D_HAT, 2, 1);
        r.inst.beatDo(BT_PLAYSTOP);
        auto kicks = [&] { return (int)r.inst.uiState().drumHits[D_KICK]; };
        r.run(0.02);  CHECK(kicks() == 1 && r.inst.uiState().beatPlaying, "step 1 sounds as soon as the beat starts");
        CHECK(r.inst.uiState().beatStep == 0, "the display is on step 1");
        r.run(0.46);  CHECK(kicks() == 1, "nothing extra before beat 2 (0.48 s)");
        r.run(0.04);  CHECK(kicks() == 2, "second kick on beat 2 (0.50 s)");
        CHECK(r.inst.uiState().drumHits[D_HAT] == 1, "the hat on step 3 has played once");
        r.run(1.0);   CHECK(kicks() == 4, "four kicks in the first bar");
        r.run(0.5);   CHECK(kicks() == 5 && r.inst.uiState().beatStep == 0, "the bar repeats (2.02 s)");
        r.inst.set(S_BPM, 240);
        r.run(1.0);   CHECK(kicks() == 9, "doubling the tempo doubles the rate (%d kicks)", kicks());
        r.inst.beatDo(BT_PLAYSTOP);
        r.run(1.0);   CHECK(kicks() == 9 && r.inst.uiState().beatStep == -1, "stop stops it");
        r.inst.beatDo(BT_STEP, D_KICK, 4, 2);
        r.run(0.01);  CHECK(r.inst.uiState().beatPat[D_KICK] == 0x1101, "toggling a step off (pattern %04x)", r.inst.uiState().beatPat[D_KICK]);
    }

    SECTION("beat: recording what the pads play");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        r.inst.beatDo(BT_REC);
        r.run(0.37);
        UiState u = r.inst.uiState();
        CHECK(u.beatPlaying && u.beatRec, "Rec starts the beat");
        r.inst.hit(D_SNARE);      // 0.37 s = step 3.96 -> step 4 (index 3)... nearest sixteenth
        r.run(0.12);
        r.inst.hit(D_CLAP);       // 0.49 s: just before step 5 (0.50 s)
        r.run(0.2);
        u = r.inst.uiState();
        CHECK(u.beatPat[D_SNARE] == (1u << 3), "a hit at 0.37 s lands on step 4 (pattern %04x)", u.beatPat[D_SNARE]);
        CHECK(u.beatPat[D_CLAP] == (1u << 4), "a hit just before step 5 lands on step 5 (pattern %04x)", u.beatPat[D_CLAP]);
        CHECK(u.drumHits[D_CLAP] == 1, "and is not sounded a second time when the step arrives (%u)", u.drumHits[D_CLAP]);
        r.run(2.0);
        u = r.inst.uiState();
        CHECK(u.drumHits[D_SNARE] == 2 && u.drumHits[D_CLAP] == 2, "both play back in the next bar");
        r.inst.beatDo(BT_REC);
        r.run(0.3);
        r.inst.hit(D_TOM);
        r.run(0.1);
        u = r.inst.uiState();
        CHECK(!u.beatRec && u.beatPlaying && u.beatPat[D_TOM] == 0, "with Rec off a pad plays without being written");
        r.inst.beatDo(BT_ERASE, D_SNARE);
        r.run(0.01);
        u = r.inst.uiState();
        CHECK(u.beatPat[D_SNARE] == 0 && u.beatPat[D_CLAP] != 0, "erase clears one drum only");
        r.inst.beatDo(BT_ERASE, -1);
        r.inst.cmd(EV_PANIC);
        r.run(0.01);
        u = r.inst.uiState();
        CHECK(u.beatPat[D_CLAP] == 0 && !u.beatPlaying, "erase all, and panic stops the beat");
    }
    {
        Rig r;
        r.inst.set(S_BPM, 120); r.inst.set(S_LATENCY, 200); // Bluetooth: the player hears everything 200 ms late
        r.inst.beatDo(BT_REC);
        r.run(0.70);              // the player hits on what they hear as beat 2 (0.5 s), 200 ms later
        r.inst.hit(D_SNARE);
        r.run(0.05);
        CHECK(r.inst.uiState().beatPat[D_SNARE] == (1u << 4), "with a 200 ms output delay a late hit is written where it was heard (pattern %04x)", r.inst.uiState().beatPat[D_SNARE]);
    }

    SECTION("beat: switching to another kept beat");
    {
        Rig r;
        r.inst.set(S_BPM, 120); r.inst.set(S_KIT, KIT_SOFT);
        r.inst.beatDo(BT_STEP, D_KICK, 0, 1);
        auto send = [&](uint16_t kick, uint16_t snare, int kit) {
            for (int d = 0; d < kDrums; ++d) r.inst.beatDo(BT_NEXTROW, d, d == D_KICK ? (int16_t)kick : (d == D_SNARE ? (int16_t)snare : 0));
            r.inst.beatDo(BT_SWITCH, -1, 0, kit);
        };
        send(0, 1u << 4, KIT_CRUNCHY);
        r.run(0.01);
        UiState u = r.inst.uiState();
        CHECK(u.beatPat[D_KICK] == 0 && u.beatPat[D_SNARE] == (1u << 4) && u.set.kit == KIT_CRUNCHY && !u.beatSwitching,
              "stopped, the other beat and its kit arrive at once");
        send(1u << 0, 0, KIT_SOFT);
        r.run(0.01);
        r.inst.beatDo(BT_PLAYSTOP);
        r.run(0.6); // at 120 bpm a bar is 2 s: step 5 has played, the bar is not over
        send((1u << 0) | (1u << 8), 1u << 12, KIT_GLITCHY);
        r.run(0.5);
        u = r.inst.uiState();
        CHECK(u.beatSwitching && u.beatPat[D_KICK] == 1u && u.set.kit == KIT_SOFT, "playing, it waits: the bar finishes as it was");
        const uint32_t k0 = u.drumHits[D_KICK];
        r.run(1.2); // past the bar line at 2 s
        u = r.inst.uiState();
        CHECK(!u.beatSwitching && u.beatPat[D_KICK] == ((1u << 0) | (1u << 8)) && u.beatPat[D_SNARE] == (1u << 12) && u.set.kit == KIT_GLITCHY,
              "and switches on the bar line, kit included");
        CHECK(u.drumHits[D_KICK] == k0 + 1, "the new beat's first kick plays on the bar line (%u hits)", u.drumHits[D_KICK] - k0);
        send(0, 0, KIT_SOFT);
        r.run(0.1);
        r.inst.beatDo(BT_PLAYSTOP);
        r.run(0.01);
        u = r.inst.uiState();
        CHECK(!u.beatSwitching && u.beatPat[D_KICK] == 0 && u.set.kit == KIT_SOFT, "stopping the beat lets a waiting one arrive at once");
    }

    SECTION("beat: with backing, saved in a session, and never over full scale");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        r.inst.beatDo(BT_STEP, D_KICK, 0, 1);
        r.inst.beatDo(BT_PLAYSTOP);
        r.run(1.3);
        r.inst.cmd(EV_CHORD, 0);  // a chord tap starts the backing while the beat runs
        r.run(0.5);
        CHECK(r.inst.uiState().drumHits[D_KICK] == 1 && r.inst.uiState().backPlaying, "starting the backing does not restart the bar");
        r.run(0.3);
        CHECK(r.inst.uiState().drumHits[D_KICK] == 2, "the next bar still starts at 2.0 s");
    }
    {
        Rig r;
        r.inst.set(S_KIT, KIT_GLITCHY); r.inst.set(S_BEATVOL, 0.4f);
        r.inst.beatDo(BT_STEP, D_COWBELL, 15, 1); r.inst.beatDo(BT_STEP, D_KICK, 0, 1);
        r.run(0.01);
        SessionData sd; r.inst.snapshot(sd);
        Rig q; q.inst.restore(sd);
        UiState u = q.inst.uiState();
        CHECK(u.beatPat[D_COWBELL] == 0x8000 && u.beatPat[D_KICK] == 1 && u.set.kit == KIT_GLITCHY && near(u.set.beatVol, 0.4f, 0.01f),
              "pattern, kit and beat volume survive a session save and load");
        CHECK(!u.beatPlaying, "a loaded session does not start the beat");
    }
    for (int k = 0; k < kKits; ++k) {
        Rig r;
        r.inst.set(S_KIT, (float)k); r.inst.set(S_BEATVOL, 1); r.inst.set(S_VOLUME, 1); r.inst.set(S_BPM, 200);
        for (int d = 0; d < kDrums; ++d) for (int s = 0; s < kDrumSteps; ++s) r.inst.beatDo(BT_STEP, d, s, 1);
        r.inst.beatDo(BT_PLAYSTOP);
        r.run(2.5);
        bool finite = true;
        for (float v : r.L) if (!(v == v)) finite = false;
        CHECK(finite && peak(r.L) <= 1.0f && peak(r.R) <= 1.0f, "%s kit: every drum on every step stays inside full scale (peak %.2f)", kitName(k), peak(r.L));
    }
}
