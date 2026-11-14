// venv-vrequency — engine tests: the track: the lane mixer.
#include "tests/support.h"
#include "core/track/track.h"

void testTrackMixer() {
    SECTION("track: the lane mixer");
    {
        TrackData m;
        CHECK(laneHeard(m, LN_BEAT) && near(laneGain(m, LN_BEAT), 1.0f, 1e-6f), "a new track plays every lane as recorded");
        m.mix[LN_CHORDS].solo = true;
        CHECK(!laneHeard(m, LN_BEAT) && laneHeard(m, LN_CHORDS) && laneGain(m, LN_LOOPA) == 0.0f, "a solo silences the lanes that are not soloed");
        m.mix[LN_CHORDS].mute = true;
        CHECK(!laneHeard(m, LN_CHORDS), "a muted lane stays quiet even when soloed");
        m = TrackData(); m.mix[LN_LOOPA].level = 100;
        CHECK(near(laneGain(m, LN_LOOPA), 1.5625f, 1e-4f), "level 100 is about 4 dB up (%.3f)", laneGain(m, LN_LOOPA));

        // two notes, one on each loop lane: 440 Hz on A, 659 Hz on B
        auto loops = [&](const LaneMix &a, const LaneMix &b, float *hzA, float *hzB, float *pkA = nullptr, float *pkB = nullptr) {
            Rig r;
            r.inst.set(S_BPM, 120);
            TrackData t; t.bars = 1;
            addClip(t, noteClip(69, 0.0, 3.9)); addClip(t, noteClip(76, 0.0, 3.9));
            put(t, LN_LOOPA, 0, 0); put(t, LN_LOOPB, 1, 0);
            t.mix[LN_LOOPA] = a; t.mix[LN_LOOPB] = b;
            r.inst.setTrack(share(t));
            r.inst.trackDo(TC_PLAY, 0);
            r.run(0.6); r.clear(); r.run(0.3);
            int n; const float *x = r.tail(0.25, n);
            *hzA = goertzel(x, n, 440.0f); *hzB = goertzel(x, n, 659.26f);
            UiState u = r.inst.uiState();
            if (pkA) *pkA = u.lanePeak[LN_LOOPA];
            if (pkB) *pkB = u.lanePeak[LN_LOOPB];
        };
        LaneMix plain, half, muted, solo;
        half.level = 40; muted.mute = true; solo.solo = true;
        float a0, b0, a1, b1, pa, pb;
        loops(plain, plain, &a0, &b0, &pa, &pb);
        CHECK(a0 > 0.01f && b0 > 0.01f && pa > 0.01f && pb > 0.01f, "both loop lanes sound and their meters move (%.3f %.3f)", pa, pb);
        loops(half, plain, &a1, &b1);
        CHECK(near(a1 / a0, 0.25f, 0.08f) && near(b1, b0, 0.05f), "Loop A at 40: a quarter of the level, 12 dB down (%.3f); Loop B untouched", a1 / a0);
        loops(muted, plain, &a1, &b1, &pa, &pb);
        CHECK(a1 < a0 * 0.001f && near(b1, b0, 0.05f) && pa < 1e-4f && pb > 0.01f, "Loop A muted: gone, and its meter still (%.5f)", a1 / a0);
        loops(plain, solo, &a1, &b1);
        CHECK(a1 < a0 * 0.001f && near(b1, b0, 0.05f), "Loop B soloed: only Loop B is heard (%.5f)", a1 / a0);

        auto chords = [&](const LaneMix &mx) {
            Rig r;
            r.inst.set(S_BPM, 120);
            Clip c; c.kind = CK_CHORDS; c.bars = 1; c.droneMask = 1; c.patch = purePatch();
            TrackData t; t.bars = 1;
            addClip(t, c); put(t, LN_CHORDS, 0, 0);
            t.mix[LN_CHORDS] = mx;
            r.inst.setTrack(share(t));
            r.inst.trackDo(TC_PLAY, 0);
            r.run(0.6); r.clear(); r.run(0.3);
            int n; const float *x = r.tail(0.25, n);
            return rms(x, n);
        };
        const float c0 = chords(plain), c1 = chords(half), c2 = chords(muted);
        CHECK(c0 > 0.01f && near(c1 / c0, 0.25f, 0.08f) && c2 < c0 * 0.001f, "the chords lane follows its level (%.3f) and its mute", c1 / c0);

        auto beat = [&](const LaneMix &mx, float *meter) {
            Rig r;
            r.inst.set(S_BPM, 120);
            TrackData t; t.bars = 1;
            addClip(t, kickSnareClip()); put(t, LN_BEAT, 0, 0);
            t.mix[LN_BEAT] = mx;
            r.inst.setTrack(share(t));
            r.inst.trackDo(TC_PLAY, 0);
            r.run(0.3);
            if (meter) *meter = r.inst.uiState().lanePeak[LN_BEAT];
            r.run(1.2);
            return peak(r.L);
        };
        float m0 = 0;
        const float d0 = beat(plain, &m0), d1 = beat(half, nullptr), d2 = beat(muted, nullptr);
        CHECK(d0 > 0.05f && m0 > 0.01f && near(d1 / d0, 0.25f, 0.08f), "the beat lane at 40 hits a quarter as hard (%.3f), and its meter moves", d1 / d0);
        CHECK(d2 < 1e-5f, "the beat lane muted: no hits at all");
    }
}
