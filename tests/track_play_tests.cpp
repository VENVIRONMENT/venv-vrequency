// venv-vrequency — engine tests: the track: beat and loop clips played in time.
#include "tests/support.h"
#include "core/track/track.h"
#include <cstdint>
#include <vector>

void testTrackPlayback() {
    SECTION("track: a beat clip plays in time, a gap is silent, the track goes round");
    {
        Rig r;
        r.inst.set(S_BPM, 120); // one bar = 2 s
        TrackData t; t.bars = 2;
        addClip(t, kickSnareClip());
        put(t, LN_BEAT, 0, 0);
        r.inst.setTrack(share(t));
        r.run(0.2);
        CHECK(!r.inst.uiState().trackPlaying && r.inst.uiState().drumHits[D_KICK] == 0, "setting a track does not start it");
        r.inst.trackDo(TC_PLAY, 0);
        std::vector<double> k = hitTimes(r, D_KICK, 8.1);
        CHECK(k.size() == 3, "kick once per pass of a two-bar track: 3 in 8.1 s (got %zu)", k.size());
        if (k.size() == 3) CHECK(k[0] < 0.02 && near((float)k[1], 4.0f, 0.02f) && near((float)k[2], 8.0f, 0.02f), "at 0, 4 and 8 s (%.3f %.3f %.3f)", k[0], k[1], k[2]);
        CHECK(r.inst.uiState().drumHits[D_SNARE] == 2, "snare twice (%u)", r.inst.uiState().drumHits[D_SNARE]);
        UiState u = r.inst.uiState();
        CHECK(u.trackPlaying && near((float)u.trackPos, 0.2f, 0.15f), "the position wrapped to the start (%.3f beats)", u.trackPos);
        r.inst.trackDo(TC_STOP);
        r.run(0.1);
        uint32_t n = r.inst.uiState().drumHits[D_KICK];
        r.run(4.5);
        CHECK(!r.inst.uiState().trackPlaying && r.inst.uiState().drumHits[D_KICK] == n, "stop stops it");
    }

    SECTION("track: a clip stretched over more bars repeats");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        TrackData t; t.bars = 4;
        addClip(t, kickSnareClip());
        put(t, LN_BEAT, 0, 1, 3);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        std::vector<double> k = hitTimes(r, D_KICK, 7.9);
        CHECK(k.size() == 3, "placed on bars 2 to 4: three kicks (got %zu)", k.size());
        if (k.size() == 3) CHECK(near((float)k[0], 2.0f, 0.02f) && near((float)k[1], 4.0f, 0.02f) && near((float)k[2], 6.0f, 0.02f), "at 2, 4 and 6 s (%.3f %.3f %.3f)", k[0], k[1], k[2]);
    }

    SECTION("track: chopped pieces play in their new order; a muted piece is silent");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        TrackData t; t.bars = 1;
        addClip(t, kickSnareClip());
        put(t, LN_BEAT, 0, 0);
        Placement &p = t.lane[LN_BEAT][0];
        cutPiece(p, 2.0);
        movePiece(p, 0, 1); // second half first: snare on beat 1, kick on beat 3
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_ONCE, 0);
        std::vector<double> sn, k;
        {
            uint32_t ss = 0, kk = 0; double tm = 0;
            r.run(2.5, [&] {
                tm += 256.0 / SR;
                UiState u = r.inst.uiState();
                for (; ss < u.drumHits[D_SNARE]; ++ss) sn.push_back(tm);
                for (; kk < u.drumHits[D_KICK]; ++kk) k.push_back(tm);
            });
        }
        CHECK(sn.size() == 1 && k.size() == 1, "one snare and one kick (%zu, %zu)", sn.size(), k.size());
        if (sn.size() == 1 && k.size() == 1) CHECK(sn[0] < 0.02 && near((float)k[0], 1.0f, 0.02f), "swapped halves: snare at 0 s, kick at 1 s (%.3f, %.3f)", sn[0], k[0]);
        UiState u = r.inst.uiState();
        CHECK(!u.trackPlaying && u.trackPasses == 1, "play once stops at the end and says so");

        t.lane[LN_BEAT][0].pieces[0].mute = true; // mute the snare half
        r.inst.setTrack(share(t));
        uint32_t s0 = u.drumHits[D_SNARE], k0 = u.drumHits[D_KICK];
        r.inst.trackDo(TC_ONCE, 0);
        r.run(2.5);
        u = r.inst.uiState();
        CHECK(u.drumHits[D_SNARE] == s0 && u.drumHits[D_KICK] == k0 + 1, "muted piece: no snare, the kick still plays");

        // repeat: both halves play the kick half
        Placement &q = t.lane[LN_BEAT][0];
        resetPieces(q, *t.clips[0]); cutPiece(q, 2.0); repeatPiece(q, 0);
        r.inst.setTrack(share(t));
        s0 = u.drumHits[D_SNARE]; k0 = u.drumHits[D_KICK];
        r.inst.trackDo(TC_ONCE, 0);
        r.run(2.5);
        u = r.inst.uiState();
        CHECK(u.drumHits[D_SNARE] == s0 && u.drumHits[D_KICK] == k0 + 2, "repeated piece: the kick twice, no snare (%u, %u)", u.drumHits[D_KICK] - k0, u.drumHits[D_SNARE] - s0);
    }

    SECTION("track: a loop clip plays its notes on its own parts");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        TrackData t; t.bars = 2;
        addClip(t, noteClip(69, 0.0, 2.0));
        addClip(t, noteClip(57, 1.0, 3.0));
        put(t, LN_LOOPA, 0, 1);
        put(t, LN_LOOPB, 1, 1);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        r.run(1.5);
        CHECK(r.active(kTrackPart0) == 0 && r.active(kTrackPart0 + kLayers) == 0, "nothing before the clips start");
        r.run(0.75); // 2.25 s: bar 2, beat 1.5
        CHECK(r.gated(kTrackPart0) == 1 && r.gated(kTrackPart0 + kLayers) == 0, "lane A's note is on, lane B's has not started");
        r.clear(); r.run(0.2);
        int n; const float *x = r.tail(0.15, n);
        CHECK(near(measureHz(x, n), 440.0f, 0.01f), "and it is the A that was recorded (%.1f Hz)", measureHz(x, n));
        r.run(0.3); // 2.75 s: beat 2.5 of the bar
        CHECK(r.gated(kTrackPart0) == 1 && r.gated(kTrackPart0 + kLayers) == 1, "both lanes sound together");
        r.run(0.5); // 3.25 s
        CHECK(r.gated(kTrackPart0) == 0 && r.gated(kTrackPart0 + kLayers) == 1, "lane A's note has ended");
        r.run(1.0); // 4.25 s: wrapped, bar 1 again
        CHECK(r.gated(kTrackPart0) == 0 && r.gated(kTrackPart0 + kLayers) == 0, "after the wrap the lanes are silent again");
        CHECK(r.gated(0) == 0, "the lead part was never used");
    }

    SECTION("track: a note still sounding at the end of a clip is let go");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        TrackData t; t.bars = 2;
        Clip c = noteClip(69, 0.0, 3.9);
        c.layers[0].ev.pop_back(); // never released
        addClip(t, c);
        put(t, LN_LOOPA, 0, 0);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        r.run(1.0);
        CHECK(r.gated(kTrackPart0) == 1, "held during the clip");
        r.run(1.5);
        CHECK(r.gated(kTrackPart0) == 0, "released when the clip ends");
    }
}
