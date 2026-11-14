// venv-vrequency — playing the track's lanes on the audio thread: each placement, piece by piece.
#include "core/instrument/instrument.h"
#include "core/drums/drums.h"
#include "core/instrument/session.h"
#include "core/synth/params.h"
#include "core/track/track.h"
#include <algorithm>
#include <cmath>

namespace td {

static int lanePart0(int lane) { return kTrackPart0 + (lane == LN_LOOPB ? kLayers : 0); }

void Instrument::laneOff(int lane) {
    if (lane == LN_CHORDS) { backPlaying = false; partShift[1] = 0; } // tickBacking lets the chord go
    else if (lane == LN_LOOPA || lane == LN_LOOPB) {
        for (int li = 0; li < kLayers; ++li) {
            int p = lanePart0(lane) + li;
            if (glitchOwner == p) { crush.set(false, 0, 0); glitchOwner = -1; }
            partOff(p, false);
            parts[p].clearMod();
            bendTarget[p] = 0;
            partShift[p] = 0;
        }
    }
    laneSt[lane] = LaneState();
}

bool laneHeard(const TrackData &t, int lane) {
    if (lane < 0 || lane >= kLanes || t.mix[lane].mute) return false;
    for (int l = 0; l < kLanes; ++l)
        if (t.mix[l].solo) return t.mix[lane].solo;
    return true;
}

float laneGain(const TrackData &t, int lane) {
    if (!laneHeard(t, lane)) return 0.0f;
    const float x = std::clamp(t.mix[lane].level, 0, 100) / (float)kLaneLevelNormal;
    return x * x;
}

// `keepSound`: a sound was picked for the clip, so the sound changes recorded in it are passed over.
void Instrument::applyTrackOp(int part, const LayerData &ld, const LoopEv &e, bool keepSound) {
    switch (e.op) {
    case OP_ON: opOn(part, e.slot, e.a, e.b); break;
    case OP_GLIDE: opGlide(part, e.slot, e.a); break;
    case OP_OFF: opOff(part, e.slot); break;
    case OP_BEND: bendTarget[part] = e.a; break;
    case OP_XY: applyXY(part, e.slot & 0x0f, e.a, e.b, (e.slot & 0x80) != 0, (e.slot & 0x40) != 0); break;
    case OP_PATCH: if (!keepSound && e.slot < ld.patches.size()) parts[part].patch = ld.patches[e.slot]; break;
    case OP_PARAM: if (!keepSound && e.slot < P_COUNT) parts[part].patch.p[e.slot] = e.a; break;
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
        const SoundPick &pick = pickFor(c, pl);
        const int kit = pick.kit >= 0 ? std::clamp(pick.kit, 0, kKits - 1) : std::clamp(c.kit, 0, kKits - 1);
        const float shift = 12.0f * std::clamp(pick.octave, -2, 2);

        bool fresh = ls.pl != pi || ls.piece != ident;
        double srcA;
        if (fresh) {
            laneOff(lane);
            ls.pl = pi; ls.piece = ident;
            double into = lp - pstart;
            srcA = pcs.src + (into < 1e-6 ? 0.0 : into);
            const double gx = g + (x - a);
            if (c.kind == CK_BEAT) {
                trackKit = kit;
            } else if (c.kind == CK_CHORDS && !silent) {
                progLen = std::clamp(c.progLen, 0, kProgMax);
                for (int i = 0; i < kProgMax; ++i) prog[i] = ((c.prog[i] % 7) + 7) % 7;
                holdChord = c.holdChord; droneMask = c.droneMask & 0x7f;
                st.backStyle = std::clamp(c.backStyle, 0, BS_COUNT - 1);
                st.chordBeats = std::clamp(c.chordBeats, 1, 8);
                st.arpRate = std::clamp(c.arpRate, 1, 4);
                st.seventh = c.seventh;
                parts[1].patch = pick.on ? pick.patch : c.patch;
                if (partShift[1] != shift) { partOff(1, false); partShift[1] = shift; } // sounding chords start again at the new pitch
                progOrigin = gx - srcA;
                lastPulseStep = lastArpStep = -1;
                backPlaying = true;
            } else if (c.kind == CK_LOOP) {
                for (int li = 0; li < (int)c.layers.size() && li < kLayers; ++li) {
                    const LayerData &ld = c.layers[li];
                    int p = lanePart0(lane) + li;
                    if (pick.on) parts[p].patch = pick.patch;
                    else if (!ld.patches.empty()) parts[p].patch = ld.patches[0];
                    partShift[p] = shift;
                    // catch up on the sound changes recorded before this point
                    for (const LoopEv &e : ld.ev) {
                        if (e.beat >= srcA) break;
                        if (e.op == OP_PATCH || e.op == OP_PARAM) applyTrackOp(p, ld, e, pick.on);
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
                const float beatGain = laneGain(t, LN_BEAT); // the beat lane's level rides on each hit
                const double e = 1e-7;
                for (long s = (long)std::ceil((srcA - e) * 4.0); s * 0.25 < srcB - e; ++s) {
                    int step = (int)(((s % kDrumSteps) + kDrumSteps) % kDrumSteps);
                    for (int d = 0; d < kDrums; ++d) {
                        if (!(c.beat[d] & (1u << step)) || beatGain <= 0) continue;
                        drums.trigger(kit, d, ((d == D_HAT && (step & 3) != 0) ? 0.7f : 1.0f) * beatGain);
                        ++drumHits[d];
                    }
                }
            } else if (c.kind == CK_LOOP) {
                for (int li = 0; li < (int)c.layers.size() && li < kLayers; ++li) {
                    const LayerData &ld = c.layers[li];
                    int p = lanePart0(lane) + li;
                    auto it = std::lower_bound(ld.ev.begin(), ld.ev.end(), srcA, [](const LoopEv &ev, double v) { return ev.beat < v; });
                    for (; it != ld.ev.end() && it->beat < srcB; ++it) applyTrackOp(p, ld, *it, pick.on);
                }
            }
        }
        ls.lastLp = srcB;
        x = segEnd;
    }
}

} // namespace td
