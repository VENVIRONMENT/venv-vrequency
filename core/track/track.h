// venv-vrequency — the track: clips arranged on four lanes and played against the beat clock.
//
// A clip is a copy of something made elsewhere in the instrument: the one-bar beat, the
// backing's chords, or the looper's layers. Placing it on a lane does not copy it again; a
// placement only says where it plays and how its pieces are ordered. The engine reads the
// track through a shared pointer that the app replaces after every edit, so nothing here
// allocates or frees on the audio thread.
#pragma once
#include "core/track/clip.h"
#include "core/track/lanes.h"
#include <memory>
#include <vector>

namespace td {

// A placed clip is played as pieces laid end to end. Uncut, it is one piece covering the clip;
// chopping makes more. Together the pieces always last exactly one pass of the clip.
struct Piece { double src = 0, len = 4; bool mute = false; };

struct Placement {
    int clip = 0;
    int bar = 0, bars = 1;   // where it sits; longer than the clip = the clip repeats
    bool mute = false;
    std::vector<Piece> pieces;
    SoundPick pick;          // for this placement only; wins over the clip's
};

// What a placement really plays: its own pick, else the clip's, else what was recorded.
inline const SoundPick &pickFor(const Clip &c, const Placement &pl) {
    return (pl.pick.own || pl.pick.any()) ? pl.pick : c.pick;
}

// One lane's place in the mix. Level 80 plays a lane as recorded; 100 is about 4 dB louder.
struct LaneMix {
    int level = kLaneLevelNormal; // 0..100
    bool mute = false, solo = false;
};

struct TrackData {
    std::vector<std::shared_ptr<const Clip>> clips; // shared, never changed: copying a track is cheap
    std::vector<Placement> lane[kLanes];
    int bars = 8;
    int cycleFrom = 0, cycleTo = 0; // in bars; cycleTo <= cycleFrom means the whole track
    LaneMix mix[kLanes];
};

// What the mix makes of a lane: 0 when muted or another lane is soloed, 1 at level 80.
float laneGain(const TrackData &t, int lane);
bool laneHeard(const TrackData &t, int lane);
inline bool sameMix(const LaneMix &a, const LaneMix &b) { return a.level == b.level && a.mute == b.mute && a.solo == b.solo; }

inline int laneForKind(int kind) { return kind == CK_BEAT ? LN_BEAT : (kind == CK_CHORDS ? LN_CHORDS : LN_LOOPA); }
inline bool laneTakes(int lane, int kind) { return kind == CK_LOOP ? (lane == LN_LOOPA || lane == LN_LOOPB) : lane == laneForKind(kind); }

Placement placeClip(const Clip &c, int bar);
inline int addClip(TrackData &t, const Clip &c) { t.clips.push_back(std::make_shared<const Clip>(c)); return (int)t.clips.size() - 1; }
// Arranging. Bars are whole numbers; nothing may overlap in a lane.
bool laneFree(const TrackData &t, int lane, int bar, int bars, int ignore = -1);
int placementAt(const TrackData &t, int lane, int bar);
void sortLane(TrackData &t, int lane);
// Chopping. Positions are beats inside one pass of the clip.
double piecesLength(const Placement &p);
inline int pieceAt(const Placement &p, double beat, double *start = nullptr) {
    double acc = 0;
    int n = (int)p.pieces.size();
    for (int i = 0; i < n; ++i) {
        if (beat + 1e-9 < acc + p.pieces[i].len || i == n - 1) { if (start) *start = acc; return i; }
        acc += p.pieces[i].len;
    }
    if (start) *start = 0;
    return -1;
}
bool cutPiece(Placement &p, double beat);            // false when that is already an edge
bool joinPieces(Placement &p, int first);            // mend the cut after piece `first`; false if they do not belong together
void cutEvery(Placement &p, double every);
void resetPieces(Placement &p, const Clip &c);
bool movePiece(Placement &p, int i, int dir);        // swap with its neighbour
bool repeatPiece(Placement &p, int i);               // the next piece plays this one's material again
double sourceAt(const Placement &p, double beat);    // where in the clip a moment of the pass reads from; -1 = muted
std::vector<Piece> rotatedPieces(const Placement &p, double offset); // the pass as heard from `offset` beats in
// Splitting a placed clip at a bar line makes two that sound the same as before; joining undoes it.
bool splitPlacement(TrackData &t, int lane, int idx, int atBar);
bool joinPlacements(TrackData &t, int lane, int idx); // with the one after it; false if they do not belong together

} // namespace td
