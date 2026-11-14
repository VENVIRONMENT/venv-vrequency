// venv-vrequency — engine tests: the track: chopping, arranging and keeping clips.
#include "tests/support.h"
#include "core/instrument/session.h"
#include "core/track/track.h"
#include <cmath>

void testTrackClips() {
    SECTION("track: chopping keeps a clip's length");
    {
        Clip c = kickSnareClip();
        Placement p = placeClip(c, 0);
        CHECK(p.pieces.size() == 1 && p.pieces[0].len == 4.0 && p.bars == 1, "a placed clip is one piece of four beats");
        CHECK(cutPiece(p, 2.0) && p.pieces.size() == 2, "cut at beat 3 makes two pieces");
        CHECK(!cutPiece(p, 2.0) && !cutPiece(p, 0.0) && !cutPiece(p, 4.0), "cutting on an existing edge does nothing");
        CHECK(p.pieces[1].src == 2.0 && p.pieces[1].len == 2.0, "the second piece starts where the cut was");
        CHECK(movePiece(p, 0, 1) && p.pieces[0].src == 2.0 && p.pieces[1].src == 0.0, "pieces swap places");
        CHECK(!joinPieces(p, 0), "pieces out of order cannot be mended");
        movePiece(p, 0, 1);
        CHECK(joinPieces(p, 0) && p.pieces.size() == 1 && p.pieces[0].len == 4.0, "pieces in order mend back into one");
        cutEvery(p, 1.0);
        CHECK(p.pieces.size() == 4 && near((float)piecesLength(p), 4.0f, 1e-6f), "cut on every beat: four pieces, still four beats");
        CHECK(repeatPiece(p, 0) && p.pieces[1].src == 0.0 && near((float)piecesLength(p), 4.0f, 1e-6f), "repeat: the next piece plays the same material");
        double st = -1;
        CHECK(pieceAt(p, 2.5, &st) == 2 && st == 2.0, "beat 2.5 falls in the third piece");
        resetPieces(p, c);
        CHECK(p.pieces.size() == 1 && p.pieces[0].src == 0.0, "reset undoes every cut");
        TrackData t; addClip(t, c);
        put(t, LN_BEAT, 0, 2, 2);
        CHECK(!laneFree(t, LN_BEAT, 3, 1) && !laneFree(t, LN_BEAT, 1, 2) && laneFree(t, LN_BEAT, 4, 1) && laneFree(t, LN_BEAT, 0, 2), "placements may not overlap");
        CHECK(laneFree(t, LN_BEAT, 3, 1, 0) && placementAt(t, LN_BEAT, 3) == 0 && placementAt(t, LN_BEAT, 4) == -1, "a placement can be moved over itself");
        // split at a bar line and join again
        Clip two = noteClip(60, 0.0, 7.0, 2);
        TrackData s; int ci = addClip(s, two);
        put(s, LN_LOOPA, ci, 0, 4); // two passes
        cutPiece(s.lane[LN_LOOPA][0], 4.0);
        movePiece(s.lane[LN_LOOPA][0], 0, 1); // heard: bar 2 of the clip, then bar 1
        const Placement whole = s.lane[LN_LOOPA][0];
        CHECK(splitPlacement(s, LN_LOOPA, 0, 1) && s.lane[LN_LOOPA].size() == 2, "a placed clip splits at a bar line");
        CHECK(!splitPlacement(s, LN_LOOPA, 0, 0) && !splitPlacement(s, LN_LOOPA, 1, 4), "but not at its own edges");
        const Placement &lft = s.lane[LN_LOOPA][0], &rgt = s.lane[LN_LOOPA][1];
        CHECK(lft.bar == 0 && lft.bars == 1 && rgt.bar == 1 && rgt.bars == 3, "into bars 1 and 2 to 4");
        bool sameSound = true;
        for (double x = 0.1; x < 12.0; x += 0.25) // bars 2 to 4 as heard before and after
            if (std::fabs(sourceAt(whole, std::fmod(4.0 + x, 8.0)) - sourceAt(rgt, std::fmod(x, 8.0))) > 1e-6) sameSound = false;
        CHECK(sameSound && near((float)piecesLength(rgt), 8.0f, 1e-6f), "the right half carries on from where the left stops");
        CHECK(joinPlacements(s, LN_LOOPA, 0) && s.lane[LN_LOOPA].size() == 1 && s.lane[LN_LOOPA][0].bars == 4, "and the two join back into one");
        put(s, LN_LOOPA, ci, 4, 2);
        CHECK(!joinPlacements(s, LN_LOOPA, 0), "a fresh copy does not continue a reordered clip, so they do not join");
        TrackData s2; ci = addClip(s2, two);
        put(s2, LN_LOOPA, ci, 0); put(s2, LN_LOOPA, ci, 2);
        CHECK(joinPlacements(s2, LN_LOOPA, 0) && s2.lane[LN_LOOPA][0].bars == 4, "a clip and its repeat join into one long one");
        CHECK(laneTakes(LN_LOOPB, CK_LOOP) && laneTakes(LN_LOOPA, CK_LOOP) && !laneTakes(LN_BEAT, CK_LOOP) && !laneTakes(LN_LOOPA, CK_CHORDS), "each kind of clip has its lanes");
    }

    SECTION("track: keeping what is live as clips");
    {
        Rig r;
        SessionData s; r.inst.snapshot(s);
        Clip c;
        CHECK(!beatClip(s, c) && !chordsClip(s, c) && !loopClip(s, c), "nothing to keep from an empty instrument");
        r.inst.set(S_KIT, KIT_GLITCHY);
        r.inst.beatDo(BT_STEP, D_KICK, 0, 1);
        r.inst.set(S_BACKMODE, BM_PROG); r.inst.set(S_CHORDBEATS, 4);
        r.inst.cmd(EV_CHORD, 0); r.inst.cmd(EV_CHORD, 3); r.inst.cmd(EV_CHORD, 4);
        r.run(0.1);
        r.inst.cmd(EV_BACK, BC_PLAYSTOP);
        r.run(0.1);
        r.inst.snapshot(s);
        Clip b, ch;
        CHECK(beatClip(s, b) && b.kind == CK_BEAT && b.kit == KIT_GLITCHY && b.beat[D_KICK] == 1 && b.bars == 1, "the beat becomes a one-bar clip with its kit");
        CHECK(chordsClip(s, ch) && ch.kind == CK_CHORDS && ch.progLen == 3 && ch.bars == 3, "three chords of a bar each become a three-bar clip");
        Clip b2 = b;
        CHECK(sameClip(b, b2), "a clip equals its copy");
        b2.beat[D_HAT] = 5;
        CHECK(!sameClip(b, b2) && !sameClip(b, ch), "and differs from a changed one");
    }
}
