// venv-vrequency — chopping: cutting a placed clip into pieces, reordering them, and splitting it at a bar line.
#include "core/track/track.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace td {

static const double kEdge = 1e-6;

double piecesLength(const Placement &p) {
    double s = 0;
    for (const Piece &x : p.pieces) s += x.len;
    return s;
}

bool cutPiece(Placement &p, double beat) {
    double st = 0;
    int i = pieceAt(p, beat, &st);
    if (i < 0) return false;
    Piece a = p.pieces[i];
    double off = beat - st;
    if (off < kEdge || off > a.len - kEdge) return false;
    Piece b = a;
    a.len = off; b.src = a.src + off; b.len -= off;
    p.pieces[i] = a;
    p.pieces.insert(p.pieces.begin() + i + 1, b);
    return true;
}

bool joinPieces(Placement &p, int first) {
    if (first < 0 || first + 1 >= (int)p.pieces.size()) return false;
    Piece &a = p.pieces[first];
    const Piece &b = p.pieces[first + 1];
    if (std::fabs(a.src + a.len - b.src) > kEdge || a.mute != b.mute) return false;
    a.len += b.len;
    p.pieces.erase(p.pieces.begin() + first + 1);
    return true;
}

void cutEvery(Placement &p, double every) {
    if (every < 0.25) return;
    double total = piecesLength(p);
    for (double x = every; x < total - kEdge; x += every) cutPiece(p, x);
}

void resetPieces(Placement &p, const Clip &c) {
    p.pieces.clear();
    Piece one; one.src = 0; one.len = c.beats(); one.mute = false;
    p.pieces.push_back(one);
}

bool movePiece(Placement &p, int i, int dir) {
    int j = i + (dir < 0 ? -1 : 1);
    if (i < 0 || j < 0 || i >= (int)p.pieces.size() || j >= (int)p.pieces.size()) return false;
    std::swap(p.pieces[i], p.pieces[j]);
    return true;
}

bool repeatPiece(Placement &p, int i) {
    if (i < 0 || i + 1 >= (int)p.pieces.size()) return false;
    p.pieces[i + 1].src = p.pieces[i].src;
    p.pieces[i + 1].mute = p.pieces[i].mute;
    return true;
}

double sourceAt(const Placement &p, double beat) {
    double st = 0;
    int i = pieceAt(p, beat, &st);
    if (i < 0 || p.pieces[i].mute) return -1;
    return p.pieces[i].src + (beat - st);
}

std::vector<Piece> rotatedPieces(const Placement &p, double offset) {
    Placement q = p;
    double total = piecesLength(q);
    if (total <= 0) return q.pieces;
    offset = std::fmod(offset, total);
    if (offset < kEdge || offset > total - kEdge) return q.pieces;
    cutPiece(q, offset); // no effect when a cut is already there
    double acc = 0; size_t k = 0;
    while (k < q.pieces.size() && acc < offset - kEdge) acc += q.pieces[k++].len;
    std::rotate(q.pieces.begin(), q.pieces.begin() + k, q.pieces.end());
    return q.pieces;
}

bool splitPlacement(TrackData &t, int lane, int idx, int atBar) {
    if (lane < 0 || lane >= kLanes || idx < 0 || idx >= (int)t.lane[lane].size()) return false;
    Placement a = t.lane[lane][idx];
    if (atBar <= a.bar || atBar >= a.bar + a.bars) return false;
    Placement b = a;
    b.bar = atBar; b.bars = a.bar + a.bars - atBar;
    b.pieces = rotatedPieces(a, (atBar - a.bar) * 4.0);
    a.bars = atBar - a.bar;
    t.lane[lane][idx] = a;
    t.lane[lane].insert(t.lane[lane].begin() + idx + 1, b);
    return true;
}

bool joinPlacements(TrackData &t, int lane, int idx) {
    if (lane < 0 || lane >= kLanes || idx < 0 || idx + 1 >= (int)t.lane[lane].size()) return false;
    Placement &a = t.lane[lane][idx];
    const Placement &b = t.lane[lane][idx + 1];
    if (a.clip != b.clip || a.bar + a.bars != b.bar || a.mute != b.mute || !samePick(a.pick, b.pick)) return false;
    Placement want = a;
    want.pieces = rotatedPieces(a, a.bars * 4.0);
    const double total = piecesLength(a);
    if (std::fabs(total - piecesLength(b)) > kEdge) return false;
    for (double x = 0.0625; x < total; x += 0.125) { // the second must carry on exactly where the first stops
        double p = sourceAt(want, x), q = sourceAt(b, x);
        if (std::fabs(p - q) > 1e-4) return false;
    }
    a.bars += b.bars;
    t.lane[lane].erase(t.lane[lane].begin() + idx + 1);
    return true;
}

} // namespace td
