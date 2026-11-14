// venv-vrequency — engine tests: the track: sounds, kits and octaves picked for a clip.
#include "tests/support.h"
#include "core/track/track.h"
#include <algorithm>
#include <cstring>
#include <vector>

void testTrackPicks() {
    SECTION("track: a picked sound stands in for the recorded one");
    {
        Patch saw = purePatch(); saw.setName("Sawish"); saw.p[P_WAVE1] = W_SAW;
        auto second = [&](const SoundPick &clipPick, const SoundPick &placePick, float *hz) { // share of the 2nd harmonic, and the pitch
            Rig r;
            r.inst.set(S_BPM, 120);
            TrackData t; t.bars = 1;
            Clip c = noteClip(69, 0.0, 3.9);
            c.pick = clipPick;
            addClip(t, c);
            put(t, LN_LOOPA, 0, 0);
            t.lane[LN_LOOPA][0].pick = placePick;
            r.inst.setTrack(share(t));
            r.inst.trackDo(TC_PLAY, 0);
            r.run(0.6); r.clear(); r.run(0.3);
            int n; const float *x = r.tail(0.25, n);
            if (hz) *hz = measureHz(x, n);
            return shareAt(x, n, 880.0 * (hz && *hz < 300 ? 0.5 : 1.0));
        };
        SoundPick none, sawPick, down;
        sawPick.on = true; sawPick.patch = saw;
        down.octave = -1;
        float hz = 0;
        const float plain = second(none, none, &hz);
        CHECK(plain < 0.02f && near(hz, 440.0f, 0.01f), "as recorded: a sine at 440 Hz (2nd harmonic %.3f)", plain);
        const float onPlace = second(none, sawPick, &hz);
        CHECK(onPlace > 0.1f && near(hz, 440.0f, 0.01f), "picked for the placement: the same note, now a saw (2nd harmonic %.3f)", onPlace);
        const float onClip = second(sawPick, none, &hz);
        CHECK(onClip > 0.1f, "picked for the clip: every placement without its own pick plays it (%.3f)", onClip);
        second(sawPick, down, &hz);
        CHECK(near(hz, 220.0f, 0.01f), "a placement's own pick wins over the clip's: one octave down, %.1f Hz", hz);
        SoundPick up2; up2.octave = 2;
        second(none, up2, &hz);
        CHECK(near(hz, 1760.0f, 0.01f), "two octaves up: %.1f Hz", hz);

        Clip c = noteClip(69, 0.0, 3.9);
        Placement pl = placeClip(c, 0);
        CHECK(!pickFor(c, pl).any() && std::strcmp(soundName(c, pickFor(c, pl)), "Pure") == 0, "nothing picked: the clip names its recorded sound");
        c.pick = sawPick;
        CHECK(std::strcmp(soundName(c, pickFor(c, pl)), "Sawish") == 0 && std::strcmp(recordedSoundName(c), "Pure") == 0, "picked: the pick's name, and the recorded one is still known");
        CHECK(samePick(sawPick, sawPick) && !samePick(sawPick, none) && !samePick(down, none), "picks compare by sound, kit and octave");
    }

    SECTION("track: sound changes recorded in a loop are passed over once a sound is picked");
    {
        auto second = [&](bool picked) {
            Rig r;
            r.inst.set(S_BPM, 120);
            TrackData t; t.bars = 1;
            Clip c = noteClip(69, 0.0, 3.9);
            Patch saw = purePatch(); saw.p[P_WAVE1] = W_SAW;
            c.layers[0].patches.push_back(saw);
            LoopEv e; e.beat = 0.5; e.op = OP_PATCH; e.slot = 1; e.a = 0; e.b = 0; e.seq = 2; // the player changed to the saw half a beat in
            c.layers[0].ev.insert(c.layers[0].ev.begin() + 1, e);
            if (picked) { c.pick.on = true; c.pick.patch = purePatch(); }
            addClip(t, c);
            put(t, LN_LOOPA, 0, 0);
            r.inst.setTrack(share(t));
            r.inst.trackDo(TC_PLAY, 0);
            r.run(0.8); r.clear(); r.run(0.3);
            int n; const float *x = r.tail(0.25, n);
            return shareAt(x, n, 880.0);
        };
        const float rec = second(false), pk = second(true);
        CHECK(rec > 0.1f, "as recorded the change to a saw is heard (2nd harmonic %.3f)", rec);
        CHECK(pk < 0.02f, "with a sine picked it stays a sine (%.3f)", pk);
    }

    SECTION("track: a picked kit and a picked backing sound");
    {
        auto beatAudio = [&](int clipKit, int pickKit) {
            Rig r;
            r.inst.set(S_BPM, 120);
            TrackData t; t.bars = 1;
            Clip c = kickSnareClip(); c.kit = clipKit;
            addClip(t, c);
            put(t, LN_BEAT, 0, 0);
            t.lane[LN_BEAT][0].pick.kit = pickKit;
            r.inst.setTrack(share(t));
            r.inst.trackDo(TC_PLAY, 0);
            r.run(1.5);
            return r.L;
        };
        auto diff = [](const std::vector<float> &a, const std::vector<float> &b) {
            float d = 0;
            for (size_t i = 0; i < a.size() && i < b.size(); ++i) d = std::max(d, std::fabs(a[i] - b[i]));
            return d;
        };
        const std::vector<float> soft = beatAudio(KIT_SOFT, -1), crunchy = beatAudio(KIT_CRUNCHY, -1), picked = beatAudio(KIT_SOFT, KIT_CRUNCHY);
        CHECK(diff(soft, crunchy) > 0.01f, "the two kits do sound different (%.3f)", diff(soft, crunchy));
        CHECK(diff(picked, crunchy) < 1e-6f, "a soft beat with the crunchy kit picked is the crunchy beat (difference %.6f)", diff(picked, crunchy));

        auto droneHz = [&](int octave, bool saw, float *second) {
            Rig r;
            r.inst.set(S_BPM, 120);
            Clip c; c.kind = CK_CHORDS; c.bars = 1; c.droneMask = 1; c.patch = purePatch();
            if (saw) { c.pick.on = true; c.pick.patch = purePatch(); c.pick.patch.p[P_WAVE1] = W_SAW; }
            c.pick.octave = octave;
            TrackData t; t.bars = 1;
            addClip(t, c);
            put(t, LN_CHORDS, 0, 0);
            r.inst.setTrack(share(t));
            r.inst.trackDo(TC_PLAY, 0);
            r.run(0.6); r.clear(); r.run(0.3);
            int n; const float *x = r.tail(0.25, n);
            const float hz = measureHz(x, n);
            if (second) *second = shareAt(x, n, hz * 2.0);
            return hz;
        };
        float h0 = 0, h1 = 0;
        const float base = droneHz(0, false, &h0), low = droneHz(-1, false, nullptr), sawHz = droneHz(0, true, &h1);
        CHECK(base > 60 && near(low, base / 2, 0.01f), "a chords clip an octave down: %.1f Hz against %.1f", low, base);
        CHECK(near(sawHz, base, 0.01f) && h0 < 0.02f && h1 > 0.1f, "a chords clip with a saw picked: same note, new sound (2nd harmonic %.3f, was %.3f)", h1, h0);

        Rig r; // the octave does not leak into the live backing once the track stops
        r.inst.set(S_BPM, 120);
        r.inst.setPatch(0, purePatch());
        Clip c; c.kind = CK_CHORDS; c.bars = 1; c.droneMask = 1; c.patch = purePatch(); c.pick.octave = -2;
        TrackData t; t.bars = 1;
        addClip(t, c); put(t, LN_CHORDS, 0, 0);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        r.run(0.5);
        r.inst.trackDo(TC_STOP);
        r.run(0.5);
        r.inst.set(S_BACKMODE, BM_DRONE);
        r.inst.cmd(EV_CHORD, 0);
        r.run(0.4); r.clear(); r.run(0.3);
        int n; const float *x = r.tail(0.25, n);
        CHECK(near(measureHz(x, n), base, 0.01f), "after the track the live drone is at its own pitch again (%.1f Hz)", measureHz(x, n));
    }
}

void testTrackPickJoin() {
    SECTION("track: clips with different picks do not join");
    {
        TrackData t; t.bars = 4;
        addClip(t, kickSnareClip());
        put(t, LN_BEAT, 0, 0, 4);
        CHECK(splitPlacement(t, LN_BEAT, 0, 2), "split a four-bar placement");
        t.lane[LN_BEAT][1].pick.kit = KIT_GLITCHY;
        CHECK(!joinPlacements(t, LN_BEAT, 0), "the halves no longer join once one has another kit");
        t.lane[LN_BEAT][0].pick.kit = KIT_GLITCHY;
        CHECK(joinPlacements(t, LN_BEAT, 0) && t.lane[LN_BEAT][0].pick.kit == KIT_GLITCHY, "with the same kit they do, and keep it");
    }
}
