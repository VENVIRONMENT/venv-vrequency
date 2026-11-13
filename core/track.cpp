// venv vrequency — the track: clips arranged on four lanes and played against the beat clock.
//
// A clip is a copy of something made elsewhere in the instrument: the one-bar beat, the
// backing's chords, or the looper's layers. Placing it on a lane does not copy it again; a
// placement only says where it plays and how its pieces are ordered. The engine reads the
// track through a shared pointer that the app replaces after every edit, so nothing here
// allocates or frees on the audio thread.
#include "instrument.h"
#include <algorithm>
#include <cstring>

namespace td {

// ------------------------------------------------------------ making clips

static void setName(Clip &c, const char *s) { std::snprintf(c.name, sizeof c.name, "%s", s); }

bool beatClip(const SessionData &s, Clip &out) {
    bool any = false;
    for (int d = 0; d < kDrums; ++d) any = any || s.beat[d] != 0;
    if (!any) return false;
    out = Clip();
    out.kind = CK_BEAT; out.bars = 1;
    for (int d = 0; d < kDrums; ++d) out.beat[d] = s.beat[d];
    out.kit = std::clamp(s.settings.kit, 0, kKits - 1);
    setName(out, "BEAT");
    return true;
}

bool chordsClip(const SessionData &s, Clip &out) {
    if (s.progLen <= 0 && s.holdChord < 0 && s.droneMask == 0) return false;
    out = Clip();
    out.kind = CK_CHORDS;
    out.progLen = std::clamp(s.progLen, 0, kProgMax);
    for (int i = 0; i < kProgMax; ++i) out.prog[i] = s.prog[i];
    out.holdChord = s.holdChord; out.droneMask = s.droneMask;
    out.backStyle = s.settings.backStyle; out.chordBeats = std::max(1, s.settings.chordBeats);
    out.arpRate = s.settings.arpRate; out.seventh = s.settings.seventh;
    out.patch = s.backing;
    // a progression lasts its chords, rounded up to whole bars; a held chord or drone is one bar
    int beats = (out.progLen > 0 && out.holdChord < 0) ? out.progLen * out.chordBeats : 4;
    out.bars = std::clamp((beats + 3) / 4, 1, kTrackMaxBars);
    setName(out, "CHORDS");
    return true;
}

bool loopClip(const SessionData &s, Clip &out) {
    if (s.loopBeats < 4 || s.layers.empty()) return false;
    out = Clip();
    out.kind = CK_LOOP;
    out.bars = std::clamp((int)(s.loopBeats / 4.0 + 0.5), 1, kTrackMaxBars);
    out.layers = s.layers;
    if ((int)out.layers.size() > kLayers) out.layers.resize(kLayers);
    for (auto &ld : out.layers) {
        if (ld.patches.empty()) ld.patches.push_back(s.lead);
        std::stable_sort(ld.ev.begin(), ld.ev.end(), [](const LoopEv &a, const LoopEv &b) { return a.beat < b.beat; });
    }
    setName(out, "LOOP");
    return true;
}

static bool samePatch(const Patch &a, const Patch &b) {
    for (int i = 0; i < P_COUNT; ++i) if (a.p[i] != b.p[i]) return false;
    return true;
}

bool sameClip(const Clip &a, const Clip &b) {
    if (a.kind != b.kind || a.bars != b.bars) return false;
    switch (a.kind) {
    case CK_BEAT:
        if (a.kit != b.kit) return false;
        for (int d = 0; d < kDrums; ++d) if (a.beat[d] != b.beat[d]) return false;
        return true;
    case CK_CHORDS:
        if (a.progLen != b.progLen || a.holdChord != b.holdChord || a.droneMask != b.droneMask) return false;
        if (a.backStyle != b.backStyle || a.chordBeats != b.chordBeats || a.arpRate != b.arpRate || a.seventh != b.seventh) return false;
        for (int i = 0; i < a.progLen; ++i) if (a.prog[i] != b.prog[i]) return false;
        return samePatch(a.patch, b.patch);
    default:
        if (a.layers.size() != b.layers.size()) return false;
        for (size_t i = 0; i < a.layers.size(); ++i) {
            const LayerData &x = a.layers[i], &y = b.layers[i];
            if (x.ev.size() != y.ev.size() || x.patches.size() != y.patches.size()) return false;
            for (size_t k = 0; k < x.ev.size(); ++k)
                if (x.ev[k].beat != y.ev[k].beat || x.ev[k].op != y.ev[k].op || x.ev[k].slot != y.ev[k].slot || x.ev[k].a != y.ev[k].a) return false;
            for (size_t k = 0; k < x.patches.size(); ++k) if (!samePatch(x.patches[k], y.patches[k])) return false;
        }
        return true;
    }
}

Placement placeClip(const Clip &c, int bar) {
    Placement p;
    p.bar = bar; p.bars = c.bars;
    resetPieces(p, c);
    return p;
}

// -------------------------------------------------------------- arranging

bool laneFree(const TrackData &t, int lane, int bar, int bars, int ignore) {
    if (lane < 0 || lane >= kLanes || bar < 0 || bars < 1 || bar + bars > kTrackMaxBars) return false;
    const auto &v = t.lane[lane];
    for (int i = 0; i < (int)v.size(); ++i) {
        if (i == ignore) continue;
        if (bar < v[i].bar + v[i].bars && v[i].bar < bar + bars) return false;
    }
    return true;
}

int placementAt(const TrackData &t, int lane, int bar) {
    if (lane < 0 || lane >= kLanes) return -1;
    const auto &v = t.lane[lane];
    for (int i = 0; i < (int)v.size(); ++i)
        if (bar >= v[i].bar && bar < v[i].bar + v[i].bars) return i;
    return -1;
}

void sortLane(TrackData &t, int lane) {
    std::stable_sort(t.lane[lane].begin(), t.lane[lane].end(), [](const Placement &a, const Placement &b) { return a.bar < b.bar; });
}

// ---------------------------------------------------------------- chopping

static const double kEdge = 1e-6;

double piecesLength(const Placement &p) {
    double s = 0;
    for (const Piece &x : p.pieces) s += x.len;
    return s;
}

int pieceAt(const Placement &p, double beat, double *start) {
    double acc = 0;
    int n = (int)p.pieces.size();
    for (int i = 0; i < n; ++i) {
        if (beat + 1e-9 < acc + p.pieces[i].len || i == n - 1) { if (start) *start = acc; return i; }
        acc += p.pieces[i].len;
    }
    if (start) *start = 0;
    return -1;
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
    if (a.clip != b.clip || a.bar + a.bars != b.bar || a.mute != b.mute) return false;
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

// ---------------------------------------------------------------- playback

void Instrument::setTrack(std::shared_ptr<const TrackData> t) {
    {
        std::lock_guard<std::mutex> lk(mx);
        track.swap(t);
        trackDirty = true;
    }
    // the previous track, now in `t`, is released here on the caller's thread
}

static int lanePart0(int lane) { return kTrackPart0 + (lane == LN_LOOPB ? kLayers : 0); }

void Instrument::laneOff(int lane) {
    if (lane == LN_CHORDS) backPlaying = false;       // tickBacking lets the chord go
    else if (lane == LN_LOOPA || lane == LN_LOOPB) {
        for (int li = 0; li < kLayers; ++li) {
            int p = lanePart0(lane) + li;
            if (glitchOwner == p) { crush.set(false, 0, 0); glitchOwner = -1; }
            partOff(p, false);
            parts[p].clearMod();
            bendTarget[p] = 0;
        }
    }
    laneSt[lane] = LaneState();
}

void Instrument::trackStart(int fromBar, bool once) {
    if (!track) return;
    // the track takes over the transport: stop what was playing live
    beatPlaying = beatRec = false;
    if (loopState == L_COUNTIN || loopState == L_REC || closing) loopClear();
    else if (loopState == L_PLAY || loopState == L_OVERDUB) loopPlayStop();
    if (!liveSaved) { // the chords lane borrows the backing; remember what was there
        for (int i = 0; i < kProgMax; ++i) live.prog[i] = prog[i];
        live.progLen = progLen; live.holdChord = holdChord; live.droneMask = droneMask;
        live.backStyle = st.backStyle; live.chordBeats = st.chordBeats; live.arpRate = st.arpRate; live.seventh = st.seventh;
        live.patch = parts[1].patch;
        liveSaved = true;
    }
    backPlaying = false; selSlot = -1;
    for (int ln = 0; ln < kLanes; ++ln) laneOff(ln);
    int bars = std::clamp(track->bars, 1, kTrackMaxBars);
    trackPos = std::clamp(fromBar, 0, bars - 1) * 4.0;
    beat = trackPos; progOrigin = 0;
    lastDrumStep = (long)std::floor(beat * 4.0) - 1;
    lastPulseStep = lastArpStep = -1;
    trackPlaying = true; trackOnce = once; trackDirty = false;
}

void Instrument::trackStop() {
    for (int ln = 0; ln < kLanes; ++ln) laneOff(ln);
    trackPlaying = trackOnce = false;
    trackKit = -1;
    if (liveSaved) {
        for (int i = 0; i < kProgMax; ++i) prog[i] = live.prog[i];
        progLen = live.progLen; holdChord = live.holdChord; droneMask = live.droneMask;
        st.backStyle = live.backStyle; st.chordBeats = live.chordBeats; st.arpRate = live.arpRate; st.seventh = live.seventh;
        parts[1].patch = live.patch;
        liveSaved = false;
    }
    backPlaying = false;
}

void Instrument::applyTrackOp(int part, const LayerData &ld, const LoopEv &e) {
    switch (e.op) {
    case OP_ON: opOn(part, e.slot, e.a, e.b); break;
    case OP_GLIDE: opGlide(part, e.slot, e.a); break;
    case OP_OFF: opOff(part, e.slot); break;
    case OP_BEND: bendTarget[part] = e.a; break;
    case OP_XY: applyXY(part, e.slot & 0x0f, e.a, e.b, (e.slot & 0x80) != 0, (e.slot & 0x40) != 0); break;
    case OP_PATCH: if (e.slot < ld.patches.size()) parts[part].patch = ld.patches[e.slot]; break;
    case OP_PARAM: if (e.slot < P_COUNT) parts[part].patch.p[e.slot] = e.a; break;
    }
}

// Plays lane `lane` for track time [a, b). `g` is the beat clock's value at `a`.
void Instrument::runLane(const TrackData &t, int lane, double a, double b, double g) {
    const auto &pls = t.lane[lane];
    LaneState &ls = laneSt[lane];
    double x = a;
    int guard = 0;
    while (x < b && ++guard < 64) {
        // the placement under x, or the next one to start before b
        int pi = -1; double next = 1e18;
        for (int i = 0; i < (int)pls.size(); ++i) {
            double s = pls[i].bar * 4.0, e = s + pls[i].bars * 4.0;
            if (x >= s && x < e) { pi = i; break; }
            if (s > x && s < next) next = s;
        }
        if (pi >= 0 && (pls[pi].clip < 0 || pls[pi].clip >= (int)t.clips.size() || !t.clips[pls[pi].clip] || pls[pi].pieces.empty())) {
            x = std::min(b, pls[pi].bar * 4.0 + pls[pi].bars * 4.0); // damaged data: treat as a gap
            if (ls.pl != -1) laneOff(lane);
            continue;
        }
        if (pi < 0) {
            if (ls.pl != -1) laneOff(lane);
            if (next >= b) break;
            x = next;
            continue;
        }
        const Placement &pl = pls[pi];
        const Clip &c = *t.clips[pl.clip];
        const double ps = pl.bar * 4.0, pe = ps + pl.bars * 4.0, L = std::max(4.0, c.beats());
        double local = x - ps;
        long pass = (long)std::floor((local + 1e-9) / L);
        double lp = local - pass * L;
        if (lp < 0) lp = 0;
        double pstart = 0;
        int pc = pieceAt(pl, lp, &pstart);
        const Piece &pcs = pl.pieces[pc];
        const double pieceEndT = ps + pass * L + pstart + pcs.len;   // track time this piece ends
        double segEnd = std::min(b, std::min(pe, pieceEndT));
        if (segEnd <= x) segEnd = std::min(b, x + 1e-9);
        const bool endsHere = segEnd >= pieceEndT - 1e-12;
        const bool silent = pl.mute || pcs.mute;
        const int ident = (int)(pass * 4096 + pc);

        bool fresh = ls.pl != pi || ls.piece != ident;
        double srcA;
        if (fresh) {
            laneOff(lane);
            ls.pl = pi; ls.piece = ident;
            double into = lp - pstart;
            srcA = pcs.src + (into < 1e-6 ? 0.0 : into);
            const double gx = g + (x - a);
            if (c.kind == CK_BEAT) {
                trackKit = c.kit;
            } else if (c.kind == CK_CHORDS && !silent) {
                progLen = std::clamp(c.progLen, 0, kProgMax);
                for (int i = 0; i < kProgMax; ++i) prog[i] = ((c.prog[i] % 7) + 7) % 7;
                holdChord = c.holdChord; droneMask = c.droneMask & 0x7f;
                st.backStyle = std::clamp(c.backStyle, 0, BS_COUNT - 1);
                st.chordBeats = std::clamp(c.chordBeats, 1, 8);
                st.arpRate = std::clamp(c.arpRate, 1, 4);
                st.seventh = c.seventh;
                parts[1].patch = c.patch;
                progOrigin = gx - srcA;
                lastPulseStep = lastArpStep = -1;
                backPlaying = true;
            } else if (c.kind == CK_LOOP) {
                for (int li = 0; li < (int)c.layers.size() && li < kLayers; ++li) {
                    const LayerData &ld = c.layers[li];
                    int p = lanePart0(lane) + li;
                    if (!ld.patches.empty()) parts[p].patch = ld.patches[0];
                    parts[p].gain = st.loopVol;
                    // catch up on the sound changes recorded before this point
                    for (const LoopEv &e : ld.ev) {
                        if (e.beat >= srcA) break;
                        if (e.op == OP_PATCH || e.op == OP_PARAM) applyTrackOp(p, ld, e);
                    }
                }
            }
        } else {
            srcA = ls.lastLp;
        }
        double srcB = endsHere ? pcs.src + pcs.len : srcA + (segEnd - x);
        if (srcB > pcs.src + pcs.len) srcB = pcs.src + pcs.len;

        if (!silent) {
            if (c.kind == CK_BEAT) {
                const double e = 1e-7;
                for (long s = (long)std::ceil((srcA - e) * 4.0); s * 0.25 < srcB - e; ++s) {
                    int step = (int)(((s % kDrumSteps) + kDrumSteps) % kDrumSteps);
                    for (int d = 0; d < kDrums; ++d) {
                        if (!(c.beat[d] & (1u << step))) continue;
                        drums.trigger(c.kit, d, (d == D_HAT && (step & 3) != 0) ? 0.7f : 1.0f);
                        ++drumHits[d];
                    }
                }
            } else if (c.kind == CK_LOOP) {
                for (int li = 0; li < (int)c.layers.size() && li < kLayers; ++li) {
                    const LayerData &ld = c.layers[li];
                    int p = lanePart0(lane) + li;
                    parts[p].gain = st.loopVol;
                    auto it = std::lower_bound(ld.ev.begin(), ld.ev.end(), srcA, [](const LoopEv &ev, double v) { return ev.beat < v; });
                    for (; it != ld.ev.end() && it->beat < srcB; ++it) applyTrackOp(p, ld, *it);
                }
            }
        }
        ls.lastLp = srcB;
        x = segEnd;
    }
}

void Instrument::runLanes(const TrackData &t, double a, double b, double g) {
    if (b <= a) return;
    for (int ln = 0; ln < kLanes; ++ln) runLane(t, ln, a, b, g);
}

void Instrument::tickTrack(double b0, double b1) {
    if (!trackPlaying) return;
    const TrackData *t = track.get();
    if (!t) { trackStop(); return; }
    const double total = std::clamp(t->bars, 1, kTrackMaxBars) * 4.0;
    if (trackDirty) { // the arrangement changed under us: let go and pick up where we are
        for (int ln = 0; ln < kLanes; ++ln) laneOff(ln);
        trackDirty = false;
        if (trackPos >= total) trackPos = 0;
    }
    const bool cycle = !trackOnce && t->cycleTo > t->cycleFrom && t->cycleFrom >= 0 && t->cycleTo * 4.0 <= total;
    double end = total, start = 0;
    if (cycle) {
        start = t->cycleFrom * 4.0;
        if (trackPos < t->cycleTo * 4.0) end = t->cycleTo * 4.0;
    }
    const double a = trackPos, b = a + (b1 - b0);
    if (b < end) {
        runLanes(*t, a, b, b0);
        trackPos = b;
        return;
    }
    runLanes(*t, a, end, b0);
    if (trackOnce) { ++trackPasses; trackStop(); return; }
    for (int ln = 0; ln < kLanes; ++ln) laneOff(ln);
    const double rest = b - end;
    runLanes(*t, start, start + rest, b0 + (end - a));
    trackPos = start + rest;
}

} // namespace td
