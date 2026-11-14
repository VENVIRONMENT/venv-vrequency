// venv-vrequency — engine tests: the track: chords clips, the cycle and the live transport.
#include "tests/support.h"
#include "core/presets/presets.h"
#include "core/instrument/session.h"
#include "core/track/track.h"
#include <algorithm>
#include <cstdint>

void testTrackChordsAndTransport() {
    SECTION("track: a chords clip drives the backing and gives it back");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        r.inst.set(S_BACKSTYLE, BS_PULSE); // the live backing: pulse, one chord, not playing
        r.inst.set(S_CHORDBEATS, 2);
        r.inst.cmd(EV_CHORD, 5);
        r.run(0.1);
        r.inst.cmd(EV_BACK, BC_PLAYSTOP);
        r.run(0.5);
        CHECK(r.gated(1) == 0 && r.inst.uiState().progLen == 1, "live backing: one chord, stopped");

        Clip c; c.kind = CK_CHORDS; c.bars = 2;
        c.prog[0] = 0; c.prog[1] = 3; c.progLen = 2; c.chordBeats = 4; c.backStyle = BS_SUSTAIN; c.patch = purePatch();
        TrackData t; t.bars = 3;
        addClip(t, c);
        put(t, LN_CHORDS, 0, 0);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        r.run(1.0);
        UiState u = r.inst.uiState();
        CHECK(u.backPlaying && u.curChord == 0 && r.gated(1) >= 3, "bar 1: chord I sounds (%d voices)", r.gated(1));
        SessionData s; r.inst.snapshot(s);
        CHECK(s.progLen == 1 && s.prog[0] == 5 && s.settings.backStyle == BS_PULSE && s.settings.chordBeats == 2, "a snapshot taken meanwhile still holds the live backing");
        r.inst.cmd(EV_CHORD, 2);
        r.run(1.5); // 2.5 s
        u = r.inst.uiState();
        CHECK(u.curChord == 3 && u.progLen == 2, "bar 2: chord IV; a chord tap during the track is ignored");
        r.run(2.0); // 4.5 s: bar 3 is empty
        CHECK(r.gated(1) == 0, "bar 3: no clip, the chord is let go");
        r.run(2.0); // 6.5 s: wrapped into bar 1
        CHECK(r.inst.uiState().curChord == 0 && r.gated(1) >= 3, "round again: chord I");
        r.inst.trackDo(TC_STOP);
        r.run(0.2);
        u = r.inst.uiState();
        CHECK(!u.backPlaying && r.gated(1) == 0, "stop lets the chord go");
        CHECK(u.progLen == 1 && u.prog[0] == 5 && u.set.backStyle == BS_PULSE && u.set.chordBeats == 2, "and the live backing is back as it was");
    }

    SECTION("track: a chords clip cut in two and swapped starts on its second chord");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        Clip c; c.kind = CK_CHORDS; c.bars = 2;
        c.prog[0] = 0; c.prog[1] = 4; c.progLen = 2; c.chordBeats = 4; c.patch = purePatch();
        TrackData t; t.bars = 2;
        addClip(t, c);
        put(t, LN_CHORDS, 0, 0);
        cutPiece(t.lane[LN_CHORDS][0], 4.0);
        movePiece(t.lane[LN_CHORDS][0], 0, 1);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        int first = -2;
        r.run(0.05, [&] { if (first == -2 && r.inst.uiState().curChord >= 0) first = r.inst.uiState().curChord; });
        CHECK(first == 4, "the very first chord heard is V, not I (%d)", first);
        r.run(2.45);
        CHECK(r.inst.uiState().curChord == 0, "then I in the second bar");
    }

    SECTION("track: cycle, play from a bar, and the live transport");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        r.inst.beatDo(BT_STEP, D_COWBELL, 0, 1);
        r.inst.beatDo(BT_PLAYSTOP);
        r.run(0.5);
        CHECK(r.inst.uiState().beatPlaying, "the live beat is running");
        TrackData t; t.bars = 4; t.cycleFrom = 2; t.cycleTo = 3;
        addClip(t, kickSnareClip());
        put(t, LN_BEAT, 0, 0, 4);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 2);
        r.run(0.1);
        UiState u = r.inst.uiState();
        CHECK(u.trackPlaying && !u.beatPlaying, "starting the track stops the live beat");
        CHECK(u.trackPos >= 8.0 && u.trackPos < 8.5, "it starts from bar 3 (%.2f beats)", u.trackPos);
        double lo = 1e9, hi = -1;
        r.run(5.0, [&] { double p = r.inst.uiState().trackPos; lo = std::min(lo, p); hi = std::max(hi, p); });
        CHECK(lo >= 8.0 && hi < 12.0, "with a cycle on bar 3 the position stays inside it (%.2f to %.2f)", lo, hi);
        uint32_t cb = r.inst.uiState().drumHits[D_COWBELL];
        r.inst.beatDo(BT_PLAYSTOP); // the live beat joins in
        double before = r.inst.uiState().trackPos;
        r.run(0.1);
        double after = r.inst.uiState().trackPos;
        CHECK(after > before && after - before < 0.3, "starting the live beat does not restart the track (%.2f to %.2f)", before, after);
        r.run(4.0);
        CHECK(r.inst.uiState().drumHits[D_COWBELL] >= cb + 2, "and the live beat plays over it");
        t.cycleFrom = t.cycleTo = 0;
        t.lane[LN_BEAT].clear();
        r.inst.setTrack(share(t));
        r.run(0.5);
        CHECK(r.inst.uiState().trackPlaying, "changing the track while it plays keeps it playing");
        r.inst.cmd(EV_PANIC);
        r.run(0.1);
        CHECK(!r.inst.uiState().trackPlaying && !r.inst.uiState().beatPlaying, "panic stops everything");
        r.inst.setTrack(nullptr);
        r.inst.trackDo(TC_PLAY, 0);
        r.run(0.1);
        CHECK(!r.inst.uiState().trackPlaying, "with no track there is nothing to play");
    }

    SECTION("track: recording a loop over a playing track stays on its bar lines");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        TrackData t; t.bars = 4;
        addClip(t, kickSnareClip());
        put(t, LN_BEAT, 0, 0, 4);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        r.run(1.0);
        r.inst.cmd(EV_LOOP, LC_PEDAL);
        r.run(0.2);
        UiState u = r.inst.uiState();
        CHECK(u.trackPlaying && u.loopState == L_COUNTIN && u.trackPos > 2.0, "the count-in does not restart the track (%.2f beats)", u.trackPos);
        r.run(3.0); // 4.2 s: recording began at the bar line at 4 s
        CHECK(r.inst.uiState().loopState == L_REC, "recording starts on the next bar line but one");
    }

    SECTION("track: everything at once stays inside full scale");
    {
        Rig r;
        r.inst.set(S_BPM, 150); r.inst.set(S_VOLUME, 1.0f); r.inst.set(S_BEATVOL, 1.0f); r.inst.set(S_LOOPVOL, 1.0f); r.inst.set(S_BACKVOL, 1.0f);
        TrackData t; t.bars = 2;
        Clip b; b.kind = CK_BEAT; b.kit = KIT_CRUNCHY;
        for (int d = 0; d < kDrums; ++d) b.beat[d] = 0xffff;
        Clip ch; ch.kind = CK_CHORDS; ch.bars = 1; ch.prog[0] = 0; ch.progLen = 1; ch.seventh = true; ch.patch = factoryPatch(0);
        addClip(t, b); addClip(t, ch);
        addClip(t, noteClip(45, 0.0, 3.9)); addClip(t, noteClip(64, 0.0, 3.9));
        put(t, LN_BEAT, 0, 0, 2); put(t, LN_CHORDS, 1, 0, 2); put(t, LN_LOOPA, 2, 0, 2); put(t, LN_LOOPB, 3, 0, 2);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        r.run(4.0);
        bool finite = true;
        for (float v : r.L) if (!(v == v)) finite = false;
        CHECK(finite && peak(r.L) <= 1.0f && peak(r.R) <= 1.0f && peak(r.L) > 0.3f, "peak %.3f / %.3f", peak(r.L), peak(r.R));
    }
}
