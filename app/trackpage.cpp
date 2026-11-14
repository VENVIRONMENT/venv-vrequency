// venv-vrequency — the Track page: kept clips on the left, four lanes of bars, tools below.
//
// The window owns the track. Every edit goes through trackApply(), which keeps an undo
// step, tidies the lanes and hands the engine a fresh immutable copy. Clips themselves are
// shared and never change, so a copy of the track costs almost nothing.
#include "mainwindow.h"
#include "palette.h"
#include "sounds.h"
#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <memory>

using namespace td;
using namespace pal;

namespace {
const qreal GX = 180, GW = 524;        // the grid of bars
const int VIEW = 8;                    // bars on screen
const qreal BW = GW / VIEW;
const qreal LY = 106, LH = 66, LG = 4; // lanes
const int SHELF = 5;                   // clips shown on the shelf at once
const int kMaxClips = 40;

const char *laneLabel(int ln) {
    static const char *N[kLanes] = {"Beat", "Chords", "Loop A", "Loop B"};
    return N[qBound(0, ln, kLanes - 1)];
}
QRectF laneRect(int ln) { return QRectF(GX, LY + ln * (LH + LG), GW, LH); }
int barAtX(qreal x, int viewBar) { return viewBar + (int)std::floor((x - GX) / BW); }

QString clipSub(const Clip &c) {
    const QString what = c.kind == CK_BEAT ? nice(soundName(c, c.pick)) : QString::fromUtf8(soundName(c, c.pick));
    return QString("%1 bar%2 %3").arg(c.bars).arg(c.bars == 1 ? "" : "s").arg(what).trimmed();
}
QString barsText(int bar, int bars) {
    return bars == 1 ? QString("bar %1").arg(bar + 1) : QString("bars %1 to %2").arg(bar + 1).arg(bar + bars);
}
// how fine a cut may be: finer for short clips, where there is room to tap
double chopGrid(const Clip &c) { return c.bars <= 1 ? 0.5 : (c.bars <= 4 ? 1.0 : (c.bars <= 8 ? 2.0 : 4.0)); }
} // namespace

// ------------------------------------------------------------ bookkeeping

Placement *MainWindow::chosen() {
    if (selLane < 0 || selLane >= kLanes || selIdx < 0 || selIdx >= (int)track.lane[selLane].size()) return nullptr;
    return &track.lane[selLane][selIdx];
}

int MainWindow::maxViewBar() const { return qMin(kTrackMaxBars - VIEW, ((track.bars + 3) / 4) * 4); }

void MainWindow::showBar(int bar) {
    if (bar < viewBar) viewBar = (bar / 4) * 4;
    else if (bar >= viewBar + VIEW) viewBar = ((bar - VIEW) / 4 + 1) * 4;
    viewBar = qBound(0, viewBar, maxViewBar());
}

void MainWindow::chooseAt(int lane, int bar) {
    selLane = lane;
    selIdx = placementAt(track, lane, bar);
    if (selIdx < 0) selLane = -1;
    chopPiece = 0;
}

// Tidy the master copy and give the engine its own.
void MainWindow::trackSync() {
    int last = 0;
    for (int ln = 0; ln < kLanes; ++ln) {
        sortLane(track, ln);
        for (const Placement &p : track.lane[ln]) last = qMax(last, p.bar + p.bars);
    }
    track.bars = qBound(qMax(1, last), track.bars, kTrackMaxBars);
    if (track.cycleTo <= track.cycleFrom || track.cycleTo > track.bars) track.cycleFrom = track.cycleTo = 0;
    if (!chosen()) { selLane = selIdx = -1; chopOpen = soundOpen = false; }
    if (handOn && (hand.clip < 0 || hand.clip >= (int)track.clips.size())) handOn = false;
    playBar = qBound(0, playBar, kTrackMaxBars - 1);
    if (hearOn && !(soundOpen && chosen())) { hearOn = false; inst.trackDo(TC_STOP); }
    inst.setTrack(std::make_shared<const TrackData>(hearOn ? hearTrack() : track));
    trackSaveTimer.start(2500);
    update();
}

bool MainWindow::trackApply(const std::function<bool(TrackData &)> &fn) {
    TrackData next = track;
    if (!fn(next)) return false;
    trackUndo.push_back(track);
    if (trackUndo.size() > 40) trackUndo.erase(trackUndo.begin());
    trackRedo.clear();
    track = std::move(next);
    trackSync();
    return true;
}

// Where Place (or paste) would put the clip in hand: at the play line if there is room,
// otherwise the first free bar after it.
bool MainWindow::placeSpot(int &lane, int &bar) const {
    if (!handOn || hand.clip < 0 || hand.clip >= (int)track.clips.size()) return false;
    const Clip &c = *track.clips[hand.clip];
    int cand[2], n = 0;
    if (c.kind == CK_LOOP) {
        cand[n++] = selLane == LN_LOOPB ? LN_LOOPB : LN_LOOPA;
        cand[n++] = cand[0] == LN_LOOPA ? LN_LOOPB : LN_LOOPA;
    } else cand[n++] = laneForKind(c.kind);
    for (int i = 0; i < n; ++i)
        if (laneFree(track, cand[i], playBar, hand.bars)) { lane = cand[i]; bar = playBar; return true; }
    for (int b = playBar + 1; b + hand.bars <= kTrackMaxBars; ++b)
        if (laneFree(track, cand[0], b, hand.bars)) { lane = cand[0]; bar = b; return true; }
    return false;
}

// ---------------------------------------------------------------- actions

void MainWindow::trackTake(int ci) {
    if (ci < 0 || ci >= (int)track.clips.size()) return;
    removeArmed = -1;
    if (handOn && handFromShelf && hand.clip == ci) { handOn = false; return; }
    hand = placeClip(*track.clips[ci], 0);
    hand.clip = ci;
    handOn = true; handFromShelf = true;
    selLane = selIdx = -1; chopOpen = false;
}

// Copies of what is live now go on the shelf: the beat, the backing's chords, the loop.
void MainWindow::trackKeep() {
    SessionData sd;
    inst.snapshot(sd);
    Clip c[3];
    const bool have[3] = {beatClip(sd, c[0]), chordsClip(sd, c[1]), loopClip(sd, c[2])};
    static const char *base[3] = {"Beat", "Chords", "Loop"};
    QStringList kept;
    bool any = false, full = false;
    int lastNew = -1;
    trackApply([&](TrackData &t) {
        for (int k = 0; k < 3; ++k) {
            if (!have[k]) continue;
            any = true;
            bool dup = false;
            for (const auto &x : t.clips) dup = dup || sameClip(*x, c[k]);
            if (dup) continue;
            if ((int)t.clips.size() >= kMaxClips) { full = true; break; }
            QString name;
            for (int n = 1;; ++n) {
                name = QString("%1 %2").arg(base[k]).arg(n);
                bool taken = false;
                for (const auto &x : t.clips) taken = taken || name == QString::fromUtf8(x->name);
                if (!taken) break;
            }
            qstrncpy(c[k].name, name.toUtf8().constData(), sizeof c[k].name);
            lastNew = addClip(t, c[k]);
            kept << name;
        }
        return !kept.isEmpty();
    });
    if (full) toast("The shelf is full: remove a clip first", 2400);
    else if (kept.isEmpty()) toast(any ? "Already kept: nothing new to add" : "Nothing to keep yet: make a beat, a loop or chords first", 2800);
    else {
        toast(QString("Kept %1").arg(kept.join(" and ")), 1800);
        shelfPage = lastNew / SHELF;
        handOn = false;
        trackTake(lastNew);
    }
}

bool MainWindow::trackPlace(int lane, int bar) {
    if (!handOn) return false;
    const Clip &c = *track.clips[hand.clip];
    if (!laneTakes(lane, c.kind)) lane = laneForKind(c.kind);
    if (bar < 0 || bar + hand.bars > kTrackMaxBars) return false;
    if (!laneFree(track, lane, bar, hand.bars)) {
        int other = lane == LN_LOOPA ? LN_LOOPB : LN_LOOPA;
        if (c.kind == CK_LOOP && laneFree(track, other, bar, hand.bars)) lane = other;
        else { toast("No room there: those bars are taken", 1600); return false; }
    }
    Placement p = hand;
    p.bar = bar;
    trackApply([&](TrackData &t) { t.lane[lane].push_back(p); return true; });
    chooseAt(lane, bar);
    return true;
}

void MainWindow::trackPlaceAtLine() {
    if (!handOn) { toast("Pick a clip on the left first", 1600); return; }
    int lane, bar;
    if (!placeSpot(lane, bar)) { toast("No room left on that lane", 1600); return; }
    if (trackPlace(lane, bar)) { // the line moves on, so placing again carries on from there
        playBar = qMin(bar + hand.bars, kTrackMaxBars - 1);
        showBar(bar);
    }
}

void MainWindow::trackRemove() {
    if (Placement *p = chosen()) {
        const int ln = selLane, i = selIdx;
        (void)p;
        trackApply([&](TrackData &t) { t.lane[ln].erase(t.lane[ln].begin() + i); return true; });
        selLane = selIdx = -1; chopOpen = false; removeArmed = -1;
        return;
    }
    if (handOn && handFromShelf) { // the clip itself, with every copy of it on the track: ask twice
        const int ci = hand.clip;
        int copies = 0;
        for (int ln = 0; ln < kLanes; ++ln)
            for (const Placement &p : track.lane[ln]) copies += p.clip == ci;
        const QString name = QString::fromUtf8(track.clips[ci]->name);
        if (removeArmed != ci || clock.elapsed() - removeArmedAt > 4000) {
            removeArmed = ci; removeArmedAt = clock.elapsed();
            toast(copies ? QString("Remove again deletes %1 and its %2 placed cop%3").arg(name).arg(copies).arg(copies == 1 ? "y" : "ies")
                         : QString("Remove again deletes %1 from the shelf").arg(name), 3000);
            return;
        }
        trackApply([&](TrackData &t) {
            for (int ln = 0; ln < kLanes; ++ln) {
                auto &v = t.lane[ln];
                for (int i = (int)v.size() - 1; i >= 0; --i) {
                    if (v[i].clip == ci) v.erase(v.begin() + i);
                    else if (v[i].clip > ci) --v[i].clip;
                }
            }
            t.clips.erase(t.clips.begin() + ci);
            return true;
        });
        handOn = false; removeArmed = -1; dashCache.clear();
        toast(QString("%1 removed").arg(name), 1400);
        return;
    }
    toast("Choose a clip first", 1400);
}

void MainWindow::trackRepeat() {
    Placement *p = chosen();
    if (!p) { toast("Choose a placed clip first", 1400); return; }
    Placement q = *p;
    q.bar = p->bar + p->bars;
    const int ln = selLane;
    if (!laneFree(track, ln, q.bar, q.bars)) { toast("No room to the right of it", 1600); return; }
    trackApply([&](TrackData &t) { t.lane[ln].push_back(q); return true; });
    chooseAt(ln, q.bar);
    showBar(q.bar + q.bars - 1);
}

void MainWindow::trackSplit() {
    if (!chosen()) { toast("Choose a placed clip first", 1400); return; }
    const int ln = selLane, i = selIdx, at = playBar;
    if (!trackApply([&](TrackData &t) { return splitPlacement(t, ln, i, at); })) {
        toast("Put the play line inside the chosen clip, on a bar line", 2200);
        return;
    }
    chooseAt(ln, at);
}

void MainWindow::trackJoin() {
    Placement *p = chosen();
    if (!p) { toast("Choose a placed clip first", 1400); return; }
    const int ln = selLane, i = selIdx, bar = p->bar;
    if (!trackApply([&](TrackData &t) { return joinPlacements(t, ln, i); })) {
        toast("It only joins with what carries on from it, right after it", 2400);
        return;
    }
    chooseAt(ln, bar);
}

void MainWindow::trackMute() {
    Placement *p = chosen();
    if (!p) { toast("Choose a placed clip first", 1400); return; }
    const int ln = selLane, i = selIdx, bar = p->bar;
    trackApply([&](TrackData &t) { t.lane[ln][i].mute = !t.lane[ln][i].mute; return true; });
    chooseAt(ln, bar);
}

void MainWindow::trackCycle() {
    const bool on = track.cycleTo > track.cycleFrom;
    int from = 0, to = 0;
    if (!on) {
        if (Placement *p = chosen()) { from = p->bar; to = p->bar + p->bars; }
        else { from = qMin(playBar, track.bars - 1); to = qMin(from + 4, track.bars); }
    }
    const int ln = selLane, bar = chosen() ? chosen()->bar : -1;
    trackApply([&](TrackData &t) { t.cycleFrom = from; t.cycleTo = to; return true; });
    if (bar >= 0) chooseAt(ln, bar);
    toast(on ? QString("Cycle off") : QString("Cycle: %1").arg(barsText(from, to - from)), 1200);
}

void MainWindow::trackCopy(bool cut) {
    Placement *p = chosen();
    if (!p) { toast("Choose a placed clip first", 1400); return; }
    hand = *p;
    handOn = true; handFromShelf = false;
    const QString name = QString::fromUtf8(track.clips[hand.clip]->name);
    if (cut) {
        const int ln = selLane, i = selIdx;
        trackApply([&](TrackData &t) { t.lane[ln].erase(t.lane[ln].begin() + i); return true; });
        selLane = selIdx = -1; chopOpen = false;
    }
    toast(QString("%1 %2: tap a bar or paste to place it").arg(name).arg(cut ? "cut" : "copied"), 1800);
}

void MainWindow::trackUndoDo() {
    if (trackUndo.empty()) { toast("Nothing to undo", 1000); return; }
    trackRedo.push_back(track);
    track = trackUndo.back();
    trackUndo.pop_back();
    selLane = selIdx = -1; chopOpen = false; dashCache.clear();
    trackSync();
}

void MainWindow::trackRedoDo() {
    if (trackRedo.empty()) { toast("Nothing to redo", 1000); return; }
    trackUndo.push_back(track);
    track = trackRedo.back();
    trackRedo.pop_back();
    selLane = selIdx = -1; chopOpen = false; dashCache.clear();
    trackSync();
}

void MainWindow::trackPlayStop() {
    if (hearOn) { hearEnd(true); return; }
    if (inst.uiState().trackPlaying) { inst.trackDo(TC_STOP); return; }
    bool any = false;
    for (int ln = 0; ln < kLanes; ++ln) any = any || !track.lane[ln].empty();
    if (!any) { toast("Nothing on the track yet", 1400); return; }
    const bool cyc = track.cycleTo > track.cycleFrom;
    int from = qMin(playBar, track.bars - 1);
    if (cyc && (from < track.cycleFrom || from >= track.cycleTo)) from = track.cycleFrom;
    inst.trackDo(TC_PLAY, from);
}

void MainWindow::trackLine(int dbar) {
    playBar = qBound(0, playBar + dbar, qMin(track.bars, kTrackMaxBars - 1));
    showBar(playBar);
    if (inst.uiState().trackPlaying && playBar < track.bars) inst.trackDo(TC_PLAY, playBar);
}

void MainWindow::trackSelect(int dx, int dy) {
    Placement *p = chosen();
    if (!p) { // nothing chosen yet: the first clip at or after the play line
        int best = -1, bl = -1, bd = 1 << 30;
        for (int ln = 0; ln < kLanes; ++ln)
            for (const Placement &q : track.lane[ln]) {
                int d = q.bar + q.bars > playBar ? qMax(0, q.bar - playBar) : (playBar - q.bar) + 1000;
                if (d < bd) { bd = d; best = q.bar; bl = ln; }
            }
        if (bl >= 0) { chooseAt(bl, best); showBar(best); }
        return;
    }
    if (dx) {
        int i = selIdx + dx;
        if (i < 0 || i >= (int)track.lane[selLane].size()) return;
        selIdx = i; chopPiece = 0;
        showBar(track.lane[selLane][i].bar);
        return;
    }
    for (int ln = selLane + dy; ln >= 0 && ln < kLanes; ln += dy) {
        if (track.lane[ln].empty()) continue;
        int best = 0, bd = 1 << 30;
        for (int i = 0; i < (int)track.lane[ln].size(); ++i) {
            const Placement &q = track.lane[ln][i];
            int d = (p->bar >= q.bar && p->bar < q.bar + q.bars) ? 0 : qAbs(q.bar - p->bar);
            if (d < bd) { bd = d; best = i; }
        }
        selLane = ln; selIdx = best; chopPiece = 0;
        showBar(track.lane[ln][best].bar);
        return;
    }
}

void MainWindow::trackMove(int dbar, int dlane) {
    Placement *p = chosen();
    if (!p) return;
    const int from = selLane, i = selIdx, to = selLane + dlane, bar = p->bar + dbar;
    if (to < 0 || to >= kLanes || !laneTakes(to, track.clips[p->clip]->kind)) return;
    if (!laneFree(track, to, bar, p->bars, to == from ? i : -1)) { toast("No room there", 1000); return; }
    trackApply([&](TrackData &t) {
        Placement q = t.lane[from][i];
        q.bar = bar;
        t.lane[from].erase(t.lane[from].begin() + i);
        t.lane[to].push_back(q);
        return true;
    });
    chooseAt(to, bar);
    showBar(bar);
}

void MainWindow::trackChop() {
    if (chopOpen) { chopOpen = false; return; }
    if (!chosen()) { toast("Choose a placed clip first", 1400); return; }
    chopOpen = true; chopPiece = 0;
    soundOpen = false; mixOpen = false;
}

void MainWindow::chopEdit(const std::function<bool(Placement &)> &fn) {
    if (!chosen()) return;
    const int ln = selLane, i = selIdx, bar = chosen()->bar, pc = chopPiece;
    trackApply([&](TrackData &t) { return fn(t.lane[ln][i]); });
    chooseAt(ln, bar);
    chopPiece = pc;
    chopOpen = chosen() != nullptr;
}

void MainWindow::chopTap(double frac, double stripWidth) {
    Placement *p = chosen();
    if (!p) return;
    const Clip &c = *track.clips[p->clip];
    const double gr = chopGrid(c), total = std::max(4.0, c.beats()), pxPerBeat = stripWidth / total;
    const double beat = qBound(0.0, frac * total, total - 1e-3), q = std::round(beat / gr) * gr;
    if (q > 1e-6 && q < total - 1e-6 && std::fabs(beat - q) * pxPerBeat <= 14) {
        double acc = 0; int edge = -1;
        for (int i = 0; i + 1 < (int)p->pieces.size(); ++i) { acc += p->pieces[i].len; if (std::fabs(acc - q) < 1e-6) edge = i; }
        if (edge >= 0) {
            Placement probe = *p;
            if (!joinPieces(probe, edge)) { toast("Those two were moved apart: Reset undoes every cut", 2400); return; }
            chopEdit([edge](Placement &x) { return joinPieces(x, edge); });
        } else {
            chopEdit([q](Placement &x) { return cutPiece(x, q); });
        }
        if (Placement *n = chosen()) chopPiece = qMax(0, pieceAt(*n, beat));
        return;
    }
    chopPiece = qMax(0, pieceAt(*p, beat));
}

// ------------------------------------------------------------ sound picker
//
// A placed clip can play in another sound than the one it was recorded with: one of the
// Keys, Guitar or Bass sounds, one of the player's own (slots 1-0, then the favourites), or
// for a beat another kit. The pick goes to every placement of the clip or to this one only.
// What was recorded stays in the clip, so "As recorded" always brings it back.

void MainWindow::trackSound() {
    if (soundOpen) { soundOpen = false; return; }
    if (!chosen()) { toast("Choose a placed clip first", 1400); return; }
    soundOpen = true; chopOpen = false; mixOpen = false;
    soundSetup();
}

int MainWindow::soundCount(int kind) const {
    if (kind == PK_DRUMS) return kKits;
    if (kind == PK_SYNTH) return 10 + (int)favs.size();
    return kLibSounds;
}

static int libKind(int pickKind) { return pickKind == 0 ? SK_KEYS : (pickKind == 2 ? SK_GUITAR : SK_BASS); }
static int cellsPerPage(int count) { return count > 6 ? 5 : 6; } // a sixth cell turns the page when there are more

Patch MainWindow::soundPatch(int kind, int i) const {
    if (kind == PK_SYNTH) return i < 10 ? bank[qBound(0, i, 9)] : favs[qBound(0, i - 10, (int)favs.size() - 1)].patch;
    return librarySound(libKind(kind), i);
}

// Open on the kind, and the page, that holds what the clip plays now.
void MainWindow::soundSetup() {
    Placement *p = chosen();
    if (!p) return;
    soundForLane = selLane; soundForIdx = selIdx;
    const Clip &c = *track.clips[p->clip];
    soundPage = 0;
    if (c.kind == CK_BEAT) { soundKind = PK_DRUMS; return; }
    const SoundPick &k = pickFor(c, *p);
    const QString now = QString::fromUtf8(soundName(c, k));
    const int lk = k.on ? libraryFind(k.patch.name) : -1;
    soundKind = lk == SK_KEYS ? PK_KEYS : (lk == SK_GUITAR ? PK_GUITAR : (lk == SK_BASS ? PK_BASS : PK_SYNTH));
    if (soundKind == PK_SYNTH) {
        const int n = soundCount(PK_SYNTH), per = cellsPerPage(n);
        for (int i = 0; i < n; ++i)
            if (now == QString::fromUtf8(soundPatch(PK_SYNTH, i).name)) { soundPage = i / per; break; }
    }
}

// Changes what the chosen placement plays. `fn` edits a copy of the pick in force; it then
// goes to the clip (every placement) or stays with this placement, whichever is set.
void MainWindow::soundApply(const std::function<void(SoundPick &)> &fn) {
    Placement *p = chosen();
    if (!p) return;
    const int ln = selLane, i = selIdx, ci = p->clip;
    const bool only = p->pick.own || p->pick.any();
    trackApply([&](TrackData &t) {
        Placement &pl = t.lane[ln][i];
        SoundPick k = pickFor(*t.clips[ci], pl);
        fn(k);
        if (only) {
            k.own = true;
            if (samePick(k, pl.pick)) return false;
            pl.pick = k;
            return true;
        }
        k.own = false;
        if (samePick(k, t.clips[ci]->pick)) return false;
        Clip c = *t.clips[ci];
        c.pick = k;
        t.clips[ci] = std::make_shared<const Clip>(std::move(c));
        return true;
    });
    dashCache.clear(); // the clip was copied: its drawing is cached by address
}

void MainWindow::soundPickAt(int cell) {
    Placement *p = chosen();
    if (!p || cell < 0 || cell > 5) return;
    const Clip &c = *track.clips[p->clip];
    const int kind = c.kind == CK_BEAT ? PK_DRUMS : (soundKind == PK_DRUMS ? PK_SYNTH : soundKind);
    const int n = soundCount(kind), per = cellsPerPage(n), pages = qMax(1, (n + per - 1) / per);
    if (n > 6 && cell == 5) { soundPage = (soundPage + 1) % pages; return; }
    const int i = qBound(0, soundPage, pages - 1) * per + cell;
    if (i >= n) return;
    if (kind == PK_DRUMS) {
        const int own = c.kit;
        soundApply([i, own](SoundPick &k) { k.kit = i == own ? -1 : i; });
    } else {
        const Patch pt = soundPatch(kind, i);
        const int oct = kind == PK_SYNTH ? 0 : libraryOctave(libKind(kind));
        soundApply([&pt, oct](SoundPick &k) { k.on = true; k.patch = pt; k.octave = oct; });
    }
    if (!hearOn && !inst.uiState().trackPlaying) hearStart(); // nothing is playing: let the pick be heard
}

void MainWindow::soundKindStep(int d) {
    Placement *p = chosen();
    if (!p || track.clips[p->clip]->kind == CK_BEAT) return;
    soundKind = qBound((int)PK_KEYS, (soundKind == PK_DRUMS ? (int)PK_SYNTH : soundKind) + d, (int)PK_BASS);
    soundPage = 0;
}

// Every placement of the clip, or only this one. Either way this placement keeps its sound.
void MainWindow::soundScope() {
    Placement *p = chosen();
    if (!p) return;
    const int ln = selLane, i = selIdx, ci = p->clip;
    const bool only = p->pick.own || p->pick.any();
    const QString name = QString::fromUtf8(track.clips[ci]->name);
    trackApply([&](TrackData &t) {
        SoundPick k = pickFor(*t.clips[ci], t.lane[ln][i]);
        if (!only) { k.own = true; t.lane[ln][i].pick = k; return true; }
        k.own = false;
        for (int l = 0; l < kLanes; ++l)
            for (Placement &q : t.lane[l]) if (q.clip == ci) q.pick = SoundPick();
        Clip c = *t.clips[ci];
        c.pick = k;
        t.clips[ci] = std::make_shared<const Clip>(std::move(c));
        return true;
    });
    dashCache.clear();
    toast(only ? QString("Every %1 now plays this sound").arg(name) : QString("From here on only this %1 changes").arg(name), 1800);
}

// ------------------------------------------------------------------ mixer
//
// Each lane has a level (80 = as recorded), a mute and a solo. They live in the track, so they
// are saved with it, undone like any edit, and heard in an export.

void MainWindow::trackMixer() {
    mixOpen = !mixOpen;
    if (mixOpen) {
        chopOpen = false;
        if (soundOpen) { soundOpen = false; hearEnd(true); }
        if (selLane >= 0) mixLane = selLane;
    }
}

void MainWindow::mixApply(int lane, const std::function<bool(LaneMix &)> &fn) {
    if (lane < 0 || lane >= kLanes) return;
    mixLane = lane;
    trackApply([&](TrackData &t) {
        LaneMix m = t.mix[lane];
        if (!fn(m) || sameMix(m, t.mix[lane])) return false;
        t.mix[lane] = m;
        return true;
    });
}

void MainWindow::mixReset() {
    if (!trackApply([](TrackData &t) {
            bool changed = false;
            for (int ln = 0; ln < kLanes; ++ln) {
                changed = changed || !sameMix(t.mix[ln], LaneMix());
                t.mix[ln] = LaneMix();
            }
            return changed;
        }))
        toast("Every lane is already at 80", 1200);
}

// A fader follows the finger; letting go makes the whole move one undo step.
void MainWindow::mixFader(int lane, qreal y, qreal top, qreal bottom, int ph) {
    if (ph == PH_RELEASE) {
        if (mixDragged) {
            trackUndo.push_back(mixBefore);
            if (trackUndo.size() > 40) trackUndo.erase(trackUndo.begin());
            trackRedo.clear();
        }
        mixDragged = false;
        return;
    }
    if (ph == PH_PRESS) { mixBefore = track; mixDragged = false; mixLane = lane; }
    int v = qBound(0, (int)std::lround((bottom - y) / (bottom - top) * 100.0), 100);
    if (std::abs(v - kLaneLevelNormal) <= 2) v = kLaneLevelNormal; // easy to find "as recorded" again
    if (v == track.mix[lane].level) return;
    track.mix[lane].level = v;
    mixDragged = true;
    trackSync();
}

// The chosen clip alone, going round: the same track with everything else taken off.
TrackData MainWindow::hearTrack() {
    TrackData t = track;
    Placement *p = chosen();
    if (!p) return t;
    Placement one = *p;
    one.mute = false;
    for (int ln = 0; ln < kLanes; ++ln) { t.lane[ln].clear(); t.mix[ln] = LaneMix(); } // heard alone, whatever the mixer says
    t.lane[selLane].push_back(one);
    t.cycleFrom = one.bar; t.cycleTo = one.bar + one.bars;
    t.bars = qMax(t.bars, t.cycleTo);
    return t;
}

void MainWindow::hearStart() {
    Placement *p = chosen();
    if (!p) return;
    hearOn = true; hearSeen = false; hearAt = clock.elapsed();
    hearLane = selLane; hearIdx = selIdx;
    inst.setTrack(std::make_shared<const TrackData>(hearTrack()));
    inst.trackDo(TC_PLAY, p->bar);
}

void MainWindow::hearEnd(bool stop) {
    if (!hearOn) return;
    hearOn = false;
    if (stop) inst.trackDo(TC_STOP);
    inst.setTrack(std::make_shared<const TrackData>(track));
}

void MainWindow::hearToggle() {
    if (hearOn) { hearEnd(true); return; }
    hearStart();
}

// Called every frame: hearing alone ends with the picker, and follows the chosen clip.
void MainWindow::hearWatch() {
    if (!hearOn) return;
    if (u.trackPlaying) hearSeen = true;
    if (pg != PG_TRACK || !soundOpen || !chosen()) { hearEnd(true); return; }
    if (hearSeen ? !u.trackPlaying : clock.elapsed() - hearAt > 1500) { hearEnd(false); return; } // stopped some other way
    if (selLane != hearLane || selIdx != hearIdx) hearStart();
}

// ----------------------------------------------------------------- export
//
// The track is rendered by a second instrument on a worker thread, faster than real time,
// straight into a WAV file. It plays the whole track once, ignoring the cycle, at full
// master volume, and keeps going until the echoes have died away.

void MainWindow::trackExport() {
    if (exportState.load() == 1) { toast("Still exporting", 1200); return; }
    bool any = false;
    for (int ln = 0; ln < kLanes; ++ln) any = any || !track.lane[ln].empty();
    if (!any) { toast("Nothing on the track to export", 1600); return; }
    if (exportThread.joinable()) exportThread.join();
    store::ensureDirs();
    exportFile = store::baseDir() + "/recordings/" + store::timestampName("track", "wav");
    SessionData sd;
    inst.snapshot(sd);
    sd.settings.volume = 1.0f; sd.settings.metro = false;
    std::shared_ptr<const TrackData> t = std::make_shared<const TrackData>(track);
    const int rate = audio.sampleRate();
    const QString path = exportFile;
    exportState = 1; exportPct = 0;
    exportThread = std::thread([this, sd, t, rate, path] {
        enableFlushToZero();
        auto off = std::make_unique<Instrument>((float)rate);
        off->restore(sd);
        off->setTrack(t);
        off->trackDo(TC_ONCE, 0);
        WavWriter w;
        if (!w.open(path, rate)) { exportState = 3; return; }
        const int N = 1024;
        const double total = t->bars * 4.0 * 60.0 / sd.settings.bpm * rate; // frames of music
        const qint64 limit = (qint64)total + 8 * (qint64)rate;
        std::vector<float> l(N), r(N), il(2 * N);
        qint64 done = 0, endedAt = -1;
        int quiet = 0;
        while (done < limit) {
            off->process(l.data(), r.data(), N);
            float pk = 0;
            for (int i = 0; i < N; ++i) {
                il[2 * i] = l[i]; il[2 * i + 1] = r[i];
                pk = std::max(pk, std::max(std::fabs(l[i]), std::fabs(r[i])));
            }
            w.write(il.data(), N);
            done += N;
            if (endedAt < 0 && !off->uiState().trackPlaying) endedAt = done;
            if (endedAt >= 0) {
                quiet = pk < 2e-4f ? quiet + 1 : 0;
                if (quiet > 24 || done - endedAt > 6 * (qint64)rate) break;
            }
            exportPct = (int)std::min(99.0, done * 100.0 / total);
        }
        w.close();
        exportSecs = (int)(done / rate);
        exportState = 2;
    });
    toast("Exporting the track…", 1400);
}

void MainWindow::pollExport() {
    const int es = exportState.load();
    if (es < 2) return;
    if (exportThread.joinable()) exportThread.join();
    exportState = 0;
    if (es == 2) {
        lastRecording = exportFile;
        toast(QString("Saved %1 (%2 s): Play last plays it").arg(QFileInfo(exportFile).fileName()).arg(exportSecs.load()), 3200);
    } else toast("Could not write the track's audio file", 2600);
}

// ------------------------------------------------------------------- keys
//
// On this page letters are commands, in the spirit of Logic (the command key stands in for
// Cmd): T splits, J joins, R repeats, X C V cut, copy and paste, Z undoes, B exports.

bool MainWindow::trackKey(int key, int plainAct) {
    if (soundOpen && chosen() && !modCommand) { // the picker has the arrows, the numbers, Space and Enter
        switch (plainAct) {
        case A_SUSTAIN: hearToggle(); return true;
        case A_LOOP: case A_HOME: soundOpen = false; return true;
        case A_PANIC: soundApply([](SoundPick &k) { k = SoundPick(); }); return true;
        case A_BEND_DOWN: soundKindStep(-1); return true;
        case A_BEND_UP: soundKindStep(1); return true;
        case A_OCT_UP: case A_OCT_DOWN: {
            const int d = plainAct == A_OCT_UP ? 1 : -1;
            if (track.clips[chosen()->clip]->kind != CK_BEAT) soundApply([d](SoundPick &k) { k.octave = qBound(-2, k.octave + d, 2); });
            return true;
        }
        default: break;
        }
        if (plainAct >= A_SOUND_1 && plainAct <= A_SOUND_10) {
            if (plainAct - A_SOUND_1 < 6) soundPickAt(plainAct - A_SOUND_1);
            return true;
        }
    }
    if (mixOpen && !modCommand) { // the mixer has the arrows, 1-4, 0, M, S and Enter
        switch (key) {
        case Qt::Key_M: mixApply(mixLane, [](LaneMix &m) { m.mute = !m.mute; return true; }); return true;
        case Qt::Key_S: mixApply(mixLane, [](LaneMix &m) { m.solo = !m.solo; return true; }); return true;
        case Qt::Key_X: mixOpen = false; return true;
        }
        switch (plainAct) {
        case A_LOOP: case A_HOME: mixOpen = false; return true;
        case A_BEND_DOWN: mixLane = qMax(0, mixLane - 1); return true;
        case A_BEND_UP: mixLane = qMin(kLanes - 1, mixLane + 1); return true;
        case A_OCT_UP: case A_OCT_DOWN: {
            const bool up = plainAct == A_OCT_UP; // to the next step of 5
            mixApply(mixLane, [up](LaneMix &m) { m.level = up ? qMin(100, (m.level / 5 + 1) * 5) : qMax(0, ((m.level + 4) / 5 - 1) * 5); return true; });
            return true;
        }
        default: break;
        }
        if (plainAct >= A_SOUND_1 && plainAct <= A_SOUND_10) {
            const int k = plainAct - A_SOUND_1;
            if (k < kLanes) mixLane = k;
            else if (plainAct == A_SOUND_10) mixApply(mixLane, [](LaneMix &m) { m.level = kLaneLevelNormal; return true; });
            return true;
        }
    }
    if (modCommand) {
        switch (key) {
        case Qt::Key_T:
            if (!chopOpen) trackSplit();
            else chopEdit([this](Placement &x) { // the chosen piece, cut in the middle
                if (chopPiece >= (int)x.pieces.size()) return false;
                double st = 0;
                for (int i = 0; i < chopPiece; ++i) st += x.pieces[i].len;
                const double gr = chopGrid(*track.clips[x.clip]);
                const double mid = std::round((st + x.pieces[chopPiece].len / 2) / gr) * gr;
                if (!cutPiece(x, mid)) { toast("That piece is too short to cut again", 1600); return false; }
                return true;
            });
            return true;
        case Qt::Key_J:
            if (!chopOpen) trackJoin();
            else chopEdit([this](Placement &x) {
                if (!joinPieces(x, chopPiece)) { toast("It only mends with the piece that follows it in the clip", 2200); return false; }
                return true;
            });
            return true;
        case Qt::Key_R:
            if (!chopOpen) trackRepeat();
            else { chopEdit([this](Placement &x) { return repeatPiece(x, chopPiece); }); if (Placement *p = chosen()) chopPiece = qMin(chopPiece + 1, (int)p->pieces.size() - 1); }
            return true;
        case Qt::Key_X: trackCopy(true); return true;
        case Qt::Key_C: trackCopy(false); return true;
        case Qt::Key_V: trackPlaceAtLine(); return true;
        case Qt::Key_Z: if (modChromatic) trackRedoDo(); else trackUndoDo(); return true;
        case Qt::Key_B: trackExport(); return true;
        }
        return false; // every other command key keeps its usual meaning
    }
    switch (plainAct) {
    case A_SUSTAIN: trackPlayStop(); return true;
    case A_LOOP:
        playBar = 0; viewBar = 0;
        if (inst.uiState().trackPlaying) inst.trackDo(TC_PLAY, track.cycleTo > track.cycleFrom ? track.cycleFrom : 0);
        return true;
    case A_PANIC:
        if (chosen() && !chopOpen) { trackRemove(); return true; }
        return false;
    case A_HOME:
        if (chopOpen) { chopOpen = false; return true; }
        if (handOn) { handOn = false; return true; }
        return false;
    case A_BEND_DOWN: case A_BEND_UP: {
        const int d = plainAct == A_BEND_UP ? 1 : -1;
        if (chopOpen) {
            Placement *p = chosen();
            const int to = chopPiece + d;
            if (!p || to < 0 || to >= (int)p->pieces.size()) return true;
            if (modChord) { const int pc = chopPiece; chopEdit([pc, d](Placement &x) { return movePiece(x, pc, d); }); }
            chopPiece = to;
        } else if (modChromatic) trackLine(d);
        else if (modChord) trackMove(d, 0);
        else trackSelect(d, 0);
        return true;
    }
    case A_OCT_UP: case A_OCT_DOWN: {
        const int d = plainAct == A_OCT_DOWN ? 1 : -1;
        if (!chopOpen) { if (modChord) trackMove(0, d); else trackSelect(0, d); }
        return true;
    }
    default: break;
    }
    if (plainAct >= A_SOUND_1 && plainAct <= A_SOUND_10) {
        const int k = plainAct - A_SOUND_1;
        if (k < SHELF) trackTake(shelfPage * SHELF + k);
        return true;
    }
    switch (key) {
    case Qt::Key_Comma: trackLine(-1); return true;
    case Qt::Key_Period: trackLine(1); return true;
    case Qt::Key_M:
        if (chopOpen) chopEdit([this](Placement &x) { if (chopPiece >= (int)x.pieces.size()) return false; x.pieces[chopPiece].mute = !x.pieces[chopPiece].mute; return true; });
        else trackMute();
        return true;
    case Qt::Key_C: trackCycle(); return true;
    case Qt::Key_E: trackChop(); return true;
    case Qt::Key_I: trackSound(); return true;
    case Qt::Key_K: trackKeep(); return true;
    case Qt::Key_X: trackMixer(); return true;
    }
    int row, col;
    return km.stringKey(key, row, col); // the other letters do nothing here: no stray notes
}

// ---------------------------------------------------------------- drawing

void MainWindow::hatch(QRectF r, QColor c) {
    g->save();
    g->setClipRect(r, Qt::IntersectClip);
    g->setPen(QPen(c, 1));
    for (qreal x = r.x() - r.height(); x < r.right(); x += 7) g->drawLine(QPointF(x, r.bottom()), QPointF(x + r.height(), r.y()));
    g->restore();
}

const MainWindow::ClipDash &MainWindow::dashes(const Clip &c) {
    auto it = dashCache.constFind(&c);
    if (it != dashCache.constEnd()) return it.value();
    ClipDash cd;
    for (const LayerData &ld : c.layers) {
        float on[kSlots] = {0}, pitch[kSlots] = {0};
        bool open[kSlots] = {false};
        for (const LoopEv &e : ld.ev) {
            if (e.op != OP_ON && e.op != OP_GLIDE && e.op != OP_OFF) continue;
            if (e.slot >= kSlots) continue;
            if (open[e.slot]) { cd.d.push_back({on[e.slot], (float)e.beat, pitch[e.slot]}); open[e.slot] = false; }
            if (e.op != OP_OFF) { open[e.slot] = true; on[e.slot] = (float)e.beat; pitch[e.slot] = e.a; }
        }
        for (int s = 0; s < kSlots; ++s)
            if (open[s]) cd.d.push_back({on[s], (float)c.beats(), pitch[s]});
    }
    if (!cd.d.empty()) {
        cd.lo = 1e9f; cd.hi = -1e9f;
        for (const Dash &d : cd.d) { cd.lo = std::min(cd.lo, d.pitch); cd.hi = std::max(cd.hi, d.pitch); }
    }
    return *dashCache.insert(&c, cd);
}

// Draws the part of a clip between two points of its own time into a rectangle.
void MainWindow::drawClipBody(const Clip &c, double srcA, double srcB, QRectF r, QColor ink) {
    const double span = srcB - srcA;
    if (span <= 1e-9 || r.width() < 2) return;
    auto X = [&](double b) { return r.x() + (b - srcA) / span * r.width(); };
    if (c.kind == CK_BEAT) {
        const qreal sw = 0.25 / span * r.width(), rh = (r.height() - 2) / kDrums;
        for (long s = (long)std::ceil(srcA * 4.0 - 1e-6); s * 0.25 < srcB - 1e-6; ++s) {
            const int step = (int)(((s % kDrumSteps) + kDrumSteps) % kDrumSteps);
            const qreal x = std::floor(X(s * 0.25));
            if (step % 4 == 0) g->fillRect(QRectF(x, r.bottom() - 2, 1, 2), LOW);
            for (int d = 0; d < kDrums; ++d)
                if (c.beat[d] & (1u << step))
                    g->fillRect(QRectF(x + 1, std::floor(r.y() + 1 + d * rh), qMax(2.0, std::floor(sw) - 1), qMax(2.0, std::floor(rh) - 1)), ink);
        }
    } else if (c.kind == CK_CHORDS) {
        const bool held = c.holdChord >= 0 || c.progLen <= 0;
        const double cb = held ? c.beats() : qMax(1, c.chordBeats);
        for (long k = (long)std::floor(srcA / cb + 1e-9); k * cb < srcB - 1e-6; ++k) {
            const qreal x0 = qMax(r.x(), std::floor(X(k * cb))), x1 = qMin(r.right(), std::floor(X((k + 1) * cb)));
            if (k * cb > srcA + 1e-6) g->fillRect(QRectF(x0, r.y() + 2, 1, r.height() - 4), GRAT);
            QString name;
            if (c.holdChord >= 0) name = buildChord(u.set.tuning, c.holdChord, c.seventh).name;
            else if (c.progLen > 0) name = buildChord(u.set.tuning, c.prog[((k % c.progLen) + c.progLen) % c.progLen], c.seventh).name;
            else name = "drone";
            const int px = x1 - x0 >= 44 ? 13 : 11;
            if (x1 - x0 >= name.size() * chW(px) + 2) txt(QRectF(x0, r.y(), x1 - x0, r.height()), name, px, ink, Qt::AlignCenter, px == 13);
            else g->fillRect(QRectF(x0 + 3, r.center().y(), qMax(1.0, x1 - x0 - 6), 2), ink == INK ? MID : ink);
        }
    } else {
        const ClipDash &cd = dashes(c);
        const float range = cd.hi - cd.lo + 1.0f;
        for (const Dash &d : cd.d) {
            if (d.b <= srcA || d.a >= srcB) continue;
            const qreal x0 = std::floor(X(std::max((double)d.a, srcA))), x1 = std::floor(X(std::min((double)d.b, srcB)));
            const qreal y = std::floor(r.y() + 2 + (cd.hi - d.pitch) / range * (r.height() - 7));
            g->fillRect(QRectF(x0, y, qMax(2.0, x1 - x0 - 1), 3), ink);
        }
    }
}

void MainWindow::drawPlacement(int ln, int idx) {
    const Placement &pl = track.lane[ln][idx];
    if (pl.bar + pl.bars <= viewBar || pl.bar >= viewBar + VIEW) return;
    if (pl.clip < 0 || pl.clip >= (int)track.clips.size() || pl.pieces.empty()) return;
    const Clip &c = *track.clips[pl.clip];
    const QRectF lr = laneRect(ln);
    auto X = [&](double bars) { return std::floor(GX + (bars - viewBar) * BW); };
    const qreal x0 = X(pl.bar), x1 = X(pl.bar + pl.bars);
    const bool sel = ln == selLane && idx == selIdx;
    const QColor ink = pl.mute ? LOW : INK;
    const QRectF r(x0, lr.y(), x1 - x0 + 1, lr.height());
    g->fillRect(r, BG);
    const QRectF hd(r.x() + 1, r.y() + 1, r.width() - 2, 14);
    g->fillRect(hd, sel ? INK : GRAT);
    const SoundPick &pk = pickFor(c, pl);
    QString title = QString::fromUtf8(c.name) + (pl.mute ? " muted" : "");
    if (pk.any()) { // a picked sound is named beside the clip
        QString snd = c.kind == CK_BEAT ? nice(soundName(c, pk)) : QString::fromUtf8(soundName(c, pk));
        if (pk.octave) snd += QString(" %1%2").arg(pk.octave > 0 ? "+" : "").arg(pk.octave);
        title += "  " + snd;
    }
    txt(hd.adjusted(4, 0, -2, 0), fit(title, 11, hd.width() - 6), 11,
        sel ? BG : (pl.mute ? MID : INK), Qt::AlignLeft | Qt::AlignVCenter, true);
    const QRectF body(r.x() + 2, r.y() + 16, r.width() - 4, r.height() - 19);
    const double Lc = std::max(4.0, c.beats()), total = pl.bars * 4.0;
    double t = 0;
    for (int guard = 0; t < total - 1e-9 && guard < 4096; ++guard) {
        const double lp = std::fmod(t, Lc);
        double ps = 0;
        const int pc = pieceAt(pl, lp, &ps);
        const Piece &pcs = pl.pieces[pc];
        const double into = lp - ps, len = std::min(pcs.len - into, total - t);
        if (len < 1e-6) break;
        const qreal a = X(pl.bar + t / 4.0), b = X(pl.bar + (t + len) / 4.0);
        const QRectF pr(qMax(a, body.x()), body.y(), qMin(b, body.right()) - qMax(a, body.x()), body.height());
        if (pr.right() > GX && pr.x() < GX + GW) {
            if (pcs.mute) hatch(pr, LOW);
            else drawClipBody(c, pcs.src + into, pcs.src + into + len, pr, ink);
        }
        t += len;
        if (t < total - 1e-9) g->fillRect(QRectF(b, body.y() - 1, 1, body.height() + 2), std::fmod(t, Lc) < 1e-6 ? MID : LOW);
    }
    frame(r, sel ? INK : MID);
    if (sel) frame(r.adjusted(1, 1, -1, -1), INK);
}

void MainWindow::laneTouch(QPointF p, int ph) {
    int ln = -1;
    for (int i = 0; i < kLanes; ++i)
        if (p.y() >= laneRect(i).y() - LG / 2 && p.y() < laneRect(i).bottom() + LG / 2) ln = i;
    const int bar = barAtX(p.x(), viewBar);
    if (ph == PH_PRESS) {
        dragOn = false; removeArmed = -1;
        if (ln < 0 || bar < 0 || bar >= kTrackMaxBars) return;
        const int i = placementAt(track, ln, bar);
        if (i >= 0) { // choose it, and be ready to drag it
            selLane = ln; selIdx = i; chopPiece = 0;
            dragOn = true; dragMoved = false; dragGrab = bar - track.lane[ln][i].bar;
            dragBefore = track;
        } else if (handOn) {
            trackPlace(ln, bar);
        } else {
            selLane = selIdx = -1; chopOpen = false;
        }
        return;
    }
    if (ph == PH_RELEASE) {
        if (dragOn && dragMoved) { // the whole drag is one undo step
            trackUndo.push_back(dragBefore);
            if (trackUndo.size() > 40) trackUndo.erase(trackUndo.begin());
            trackRedo.clear();
        }
        dragOn = false;
        return;
    }
    Placement *pl = chosen();
    if (!dragOn || !pl) return;
    const int kind = track.clips[pl->clip]->kind;
    int to = (ln >= 0 && laneTakes(ln, kind)) ? ln : selLane;
    const int nb = qBound(0, bar - dragGrab, kTrackMaxBars - pl->bars);
    if (to == selLane && nb == pl->bar) return;
    if (!laneFree(track, to, nb, pl->bars, to == selLane ? selIdx : -1)) {
        to = selLane; // cannot change lane here: try the same lane
        if (nb == pl->bar || !laneFree(track, to, nb, pl->bars, selIdx)) return;
    }
    Placement q = *pl;
    q.bar = nb;
    track.lane[selLane].erase(track.lane[selLane].begin() + selIdx);
    track.lane[to].push_back(q);
    dragMoved = true;
    trackSync();
    chooseAt(to, nb);
}

void MainWindow::paintTrack() {
    const int L = Qt::AlignLeft | Qt::AlignVCenter;
    const bool cyc = track.cycleTo > track.cycleFrom;
    const int nClips = (int)track.clips.size();
    int placed = 0;
    for (int ln = 0; ln < kLanes; ++ln) placed += (int)track.lane[ln].size();
    if (u.trackPlaying && !dragOn) { // follow the play line a screenful at a time
        const int pb = (int)(u.trackPos / 4.0);
        if (pb < viewBar || pb >= viewBar + VIEW) viewBar = (pb / VIEW) * VIEW;
    }
    viewBar = qBound(0, viewBar, maxViewBar());

    // headings
    txt(QRectF(18, 60, 110, 20), "Clips", 13, MID, L);
    QString head = QString("Track  %1 bpm  %2 bar%3").arg((int)std::round(u.set.bpm)).arg(track.bars).arg(track.bars == 1 ? "" : "s");
    if (exportState.load() == 1) head += QString("  exporting %1%").arg(exportPct.load());
    else if (u.trackPlaying) head += QString("  playing bar %1").arg((int)(u.trackPos / 4.0) + 1);
    txt(QRectF(136, 60, 568, 20), head, 13, MID, L);

    // the shelf of clips
    const int pages = qMax(1, (nClips + SHELF - 1) / SHELF);
    shelfPage = qBound(0, shelfPage, pages - 1);
    for (int k = 0; k < SHELF; ++k) {
        const QRectF r(16, 84 + k * 54, 112, 48);
        const int ci = shelfPage * SHELF + k;
        if (ci >= nClips) { frame(r, GRAT); continue; }
        const Clip &c = *track.clips[ci];
        const QString name = QString("shelf_clip%1").arg(k);
        const bool on = (handOn && hand.clip == ci) || downNames.contains(name);
        frame(r, on ? INK : LOW, on ? INK : BG);
        if (removeArmed == ci && !downNames.contains(name)) frame(r.adjusted(3, 3, -3, -3), on ? BG : INK);
        txt(QRectF(r.x() + 4, r.y() + 3, 12, 14), QString::number((k + 1) % 10), 11, on ? BG : LOW, L);
        txt(QRectF(r.x(), r.y() + 5, r.width(), 22), fit(QString::fromUtf8(c.name), 14, 92), 14, on ? BG : INK, Qt::AlignCenter, true);
        txt(QRectF(r.x() + 2, r.y() + 28, r.width() - 4, 16), fit(clipSub(c), 11, 106), 11, on ? BG : MID);
        zone(r, name, [this, ci](QPointF, int ph) { if (ph == PH_PRESS) trackTake(ci); });
    }
    if (pages > 1) {
        button(QRectF(16, 354, 62, 48), "track_keep", "+ Keep", false, [this] { trackKeep(); }, "what plays");
        button(QRectF(82, 354, 46, 48), "track_shelf_page", QString("%1/%2").arg(shelfPage + 1).arg(pages), false, [this, pages] { shelfPage = (shelfPage + 1) % pages; }, "more");
    } else {
        button(QRectF(16, 354, 112, 48), "track_keep", "+ Keep", false, [this] { trackKeep(); }, "beat chord loop");
    }

    // the ruler of bars
    for (int i = 0; i < VIEW; ++i) {
        const int bar = viewBar + i;
        const qreal x = std::floor(GX + i * BW), x2 = std::floor(GX + (i + 1) * BW);
        const bool in = cyc && bar >= track.cycleFrom && bar < track.cycleTo;
        const QRectF nr(x, 86, x2 - x, 16);
        if (in) g->fillRect(nr.adjusted(0, 0, 1, 0), INK);
        txt(nr, QString::number(bar + 1), 12, in ? BG : (bar >= track.bars ? LOW : (bar % 4 == 0 ? INK : MID)), Qt::AlignCenter, bar % 4 == 0 || in);
    }
    zone(QRectF(GX, 78, GW, 26), "track_ruler", [this](QPointF p, int ph) {
        if (ph == PH_RELEASE) return;
        const int b = qBound(0, barAtX(p.x(), viewBar), qMin(track.bars, kTrackMaxBars - 1));
        if (b == playBar && ph == PH_MOVE) return;
        playBar = b;
        if (inst.uiState().trackPlaying && b < track.bars) inst.trackDo(TC_PLAY, b);
    });

    // lanes
    const qreal lanesBottom = laneRect(kLanes - 1).bottom();
    const qreal endX = std::floor(GX + (track.bars - viewBar) * BW);
    for (int ln = 0; ln < kLanes; ++ln) {
        const QRectF lr = laneRect(ln);
        const bool target = handOn && laneTakes(ln, track.clips[hand.clip]->kind);
        const bool strong = target || selLane == ln;
        const bool heard = laneHeard(track, ln);
        const QString state = track.mix[ln].mute ? "muted" : (!heard ? "off" : (track.mix[ln].solo ? "solo" : ""));
        if (state.isEmpty()) txt(QRectF(134, lr.y(), 44, lr.height()), laneLabel(ln), 11, strong ? INK : MID, L, strong);
        else {
            txt(QRectF(134, lr.y() + lr.height() / 2 - 15, 44, 15), laneLabel(ln), 11, heard ? (strong ? INK : MID) : LOW, L, strong);
            txt(QRectF(134, lr.y() + lr.height() / 2 + 1, 44, 15), state, 11, heard ? INK : LOW, L);
        }
        frame(QRectF(lr.x(), lr.y(), lr.width() + 1, lr.height()), GRAT);
        for (int i = 1; i < VIEW; ++i)
            if (viewBar + i < track.bars) g->fillRect(QRectF(std::floor(GX + i * BW), lr.y() + 1, 1, lr.height() - 2), GRAT);
        if (endX > GX && endX < GX + GW) hatch(QRectF(endX + 1, lr.y() + 1, GX + GW - endX - 1, lr.height() - 2), QColor(0x1c, 0x1c, 0x1c));
    }
    g->save();
    g->setClipRect(QRectF(GX, LY - 1, GW + 1, lanesBottom - LY + 2), Qt::IntersectClip);
    for (int ln = 0; ln < kLanes; ++ln)
        for (int i = 0; i < (int)track.lane[ln].size(); ++i) drawPlacement(ln, i);
    for (int ln = 0; ln < kLanes; ++ln) // a lane the mixer silences is drawn faint
        if (!laneHeard(track, ln)) g->fillRect(laneRect(ln).adjusted(1, 1, -1, -1), QColor(4, 4, 4, 160));
    int spotLane = -1, spotBar = -1;
    if (handOn && placeSpot(spotLane, spotBar)) { // where Place would put the clip in hand
        const QRectF lr = laneRect(spotLane);
        const qreal x0 = std::floor(GX + (spotBar - viewBar) * BW), x1 = std::floor(GX + (spotBar + hand.bars - viewBar) * BW);
        g->setPen(QPen(MID, 1, Qt::DashLine));
        g->setBrush(Qt::NoBrush);
        g->drawRect(QRectF(x0 + 2.5, lr.y() + 2.5, x1 - x0 - 4, lr.height() - 5));
        txt(QRectF(x0, lr.y(), x1 - x0, lr.height()), x1 - x0 > 60 ? "place" : "+", 11, MID);
    }
    if (endX > GX && endX <= GX + GW) g->fillRect(QRectF(endX, LY, 2, lanesBottom - LY), MID); // the end of the track
    g->restore();
    zone(QRectF(GX, LY - 2, GW, lanesBottom - LY + 4), "track_lanes", [this](QPointF p, int ph) { laneTouch(p, ph); });

    if (nClips == 0 && placed == 0) {
        const QRectF m(GX + 40, LY + 76, GW - 80, 124);
        frame(m, LOW, BG);
        tag(QRectF(m.x() + 1, m.y() + 1, 118, 18), "An empty track", 12, L);
        txt(QRectF(m.x() + 16, m.y() + 30, m.width() - 32, 20), "1  Make a beat, a loop or some chords", 13, INK, L);
        txt(QRectF(m.x() + 16, m.y() + 56, m.width() - 32, 20), "2  Tap + Keep: they become clips", 13, INK, L);
        txt(QRectF(m.x() + 16, m.y() + 82, m.width() - 32, 20), "3  Tap a clip, then tap a bar", 13, INK, L);
    }

    // the play line
    {
        const double pos = u.trackPlaying ? u.trackPos / 4.0 : (double)playBar;
        const qreal x = std::floor(GX + (pos - viewBar) * BW);
        if (x >= GX - 1 && x <= GX + GW + 1) {
            g->fillRect(QRectF(x, 84, u.trackPlaying ? 2 : 1, lanesBottom - 84), INK);
            QPainterPath m;
            m.moveTo(x - 5, 76); m.lineTo(x + 6, 76); m.lineTo(x + 0.5, 85); m.closeSubpath();
            g->setRenderHint(QPainter::Antialiasing, true);
            g->setPen(Qt::NoPen); g->setBrush(INK);
            g->drawPath(m);
            g->setRenderHint(QPainter::Antialiasing, false);
        }
    }

    if (mixOpen) paintMixer();
    else if (soundOpen && chosen()) paintClipSound();
    else if (chopOpen && chosen()) paintChop();
    else { chopOpen = soundOpen = false; paintTrackTools(); }
}

void MainWindow::paintTrackTools() {
    const int L = Qt::AlignLeft | Qt::AlignVCenter;
    const bool cyc = track.cycleTo > track.cycleFrom;
    const bool exporting = exportState.load() == 1;
    Placement *sel = chosen();
    QString s;
    if (handOn) s = QString("%1 in hand  tap a bar to place it").arg(QString::fromUtf8(track.clips[hand.clip]->name));
    else if (sel) s = QString("%1 chosen, %2  drag it to move").arg(QString::fromUtf8(track.clips[sel->clip]->name)).arg(barsText(sel->bar, sel->bars));
    else if (track.clips.empty()) s = "No clips yet  + Keep copies what you have made";
    else s = "Tap a clip on the left, then a bar";
    section(414, s);

    const qreal pw = (688 - 3 * 8) / 4.0;
    auto c4 = [pw](int i, qreal y) { return QRectF(std::floor(16 + i * (pw + 8)), y, std::floor(pw), 60); };
    int spotLane = -1, spotBar = -1;
    const bool spot = handOn && placeSpot(spotLane, spotBar);
    const qreal fw = (688 - 4 * 8) / 5.0;
    auto r5 = [fw](int i) { return QRectF(std::floor(16 + i * (fw + 8)), 438, std::floor(fw), 60); };
    button(r5(0), "track_place", "Place", handOn, [this] { trackPlaceAtLine(); },
           handOn ? (spot ? QString("at bar %1").arg(spotBar + 1) : QString("no room")) : QString("pick a clip first"));
    button(r5(1), "track_chop", "Chop", false, [this] { trackChop(); }, "cut and reorder");
    QString plays = "keys guitar bass";
    if (sel) {
        const Clip &sc = *track.clips[sel->clip];
        const char *sn = soundName(sc, pickFor(sc, *sel));
        plays = sc.kind == CK_BEAT ? nice(sn) + " kit" : QString::fromUtf8(sn);
    }
    button(r5(2), "track_sound", "Sound", false, [this] { trackSound(); }, plays);
    button(r5(3), "track_repeat", "Repeat", false, [this] { trackRepeat(); }, "copy to the right");
    const bool armed = removeArmed >= 0 && clock.elapsed() - removeArmedAt <= 4000;
    button(r5(4), "track_remove", "Remove", false, [this] { trackRemove(); },
           armed ? QString("again to delete") : (!sel && handOn && handFromShelf ? QString("clip and copies") : QString("from the track")),
           armed ? LOOK_ARMED : LOOK_PLAIN);

    const int from = cyc && (playBar < track.cycleFrom || playBar >= track.cycleTo) ? track.cycleFrom : qMin(playBar, track.bars - 1);
    button(c4(0, 506), "track_play", u.trackPlaying ? QString::fromUtf8("■ Stop") : QString::fromUtf8("▶ Play"), u.trackPlaying, [this] { trackPlayStop(); },
           u.trackPlaying ? QString("bar %1").arg((int)(u.trackPos / 4.0) + 1) : QString("from bar %1").arg(from + 1));
    button(c4(1, 506), "track_cycle", "Cycle", cyc, [this] { trackCycle(); },
           cyc ? barsText(track.cycleFrom, track.cycleTo - track.cycleFrom) : (sel ? QString("round this clip") : QString("round 4 bars")));
    stepper(c4(2, 506), "track_length", "Length", QString("%1 bars").arg(track.bars),
            [this] {
                int last = 1;
                for (int ln = 0; ln < kLanes; ++ln) for (const Placement &p : track.lane[ln]) last = qMax(last, p.bar + p.bars);
                if (track.bars <= last) { toast("A clip is in the way: move or remove it first", 1800); return; }
                --track.bars; trackSync();
            },
            [this] { if (track.bars < kTrackMaxBars) { ++track.bars; trackSync(); } });
    button(c4(3, 506), "track_export", exporting ? QString("%1%").arg(exportPct.load()) : QString("Export"), exporting, [this] { trackExport(); },
           exporting ? "writing audio" : "save as audio");

    auto c5 = [](int i) { return QRectF(16 + i * 106, 574, 98, 60); };
    button(c5(0), "track_split", "Split", false, [this] { trackSplit(); }, "at the line");
    button(c5(1), "track_join", "Join", false, [this] { trackJoin(); }, "with the next");
    button(c5(2), "track_mute", "Mute", sel && sel->mute, [this] { trackMute(); }, "this clip");
    button(c5(3), "track_undo", "Undo", false, [this] { trackUndoDo(); }, trackUndo.empty() ? QString("nothing yet") : QString("%1 step%2").arg(trackUndo.size()).arg(trackUndo.size() == 1 ? "" : "s"));
    QString mixSub = "lane levels";
    for (int ln = 0; ln < kLanes; ++ln)
        if (!laneHeard(track, ln) || track.mix[ln].level != kLaneLevelNormal) { mixSub = "lanes changed"; break; }
    button(c5(4), "track_mixer", "Mix", false, [this] { trackMixer(); }, mixSub);
    stepper(QRectF(546, 574, 158, 60), "track_view", "Shown", QString("%1-%2").arg(viewBar + 1).arg(viewBar + VIEW),
            [this] { viewBar = qMax(0, viewBar - 4); }, [this] { viewBar = qMin(maxViewBar(), viewBar + 4); });

    const QString cmd = km.keyFor(A_COMMAND, false);
    txt(QRectF(18, 644, 684, 20), QString("Keys: %1 play, %2 to bar 1, arrows choose a clip, Alt+arrows move it,")
            .arg(km.keyFor(A_SUSTAIN, false)).arg(km.keyFor(A_LOOP, false)), 12, MID, L);
    txt(QRectF(18, 664, 684, 20), "Shift+arrows move the line. 1-5 pick a clip. K keep, E chop, I sound, M mute, X mix, C cycle.", 12, MID, L);
    txt(QRectF(18, 684, 684, 20), QString("%1 + T split, J join, R repeat, X C V cut copy paste, Z undo, B export.").arg(cmd), 12, MID, L);
}

void MainWindow::paintChop() {
    const int L = Qt::AlignLeft | Qt::AlignVCenter;
    Placement &pl = *chosen();
    const Clip &c = *track.clips[pl.clip];
    const double Lc = std::max(4.0, c.beats()), gr = chopGrid(c);
    const int n = (int)pl.pieces.size();
    chopPiece = qBound(0, chopPiece, n - 1);
    section(414, QString("Chop  %1 on %2, %3").arg(QString::fromUtf8(c.name)).arg(laneLabel(selLane)).arg(barsText(pl.bar, pl.bars)));

    const QRectF sr(16, 438, 688, 124);
    frame(sr, LOW, BG);
    auto X = [&](double b) { return std::floor(sr.x() + b / Lc * sr.width()); };
    const qreal ppb = sr.width() / Lc;
    double acc = 0;
    for (int i = 0; i < n; ++i) {
        const Piece &pc = pl.pieces[i];
        const qreal x0 = X(acc), x1 = X(acc + pc.len);
        const QRectF pr(x0, sr.y(), x1 - x0 + (i == n - 1 ? 0 : 1), sr.height());
        const bool on = i == chopPiece;
        const QRectF hd(pr.x() + 1, pr.y() + 1, pr.width() - 2, 16);
        g->fillRect(hd, on ? INK : GRAT);
        const QString letter = i < 26 ? QString(QChar('A' + i)) : QString::number(i + 1);
        txt(hd.adjusted(5, 0, -2, 0), fit(pc.mute ? letter + " muted" : letter, 11, hd.width() - 6), 11, on ? BG : INK, L, true);
        const QRectF body(pr.x() + 2, pr.y() + 19, pr.width() - 4, pr.height() - 38);
        for (double b = std::ceil((acc + 1e-6) / gr) * gr; b < acc + pc.len - 1e-6; b += gr) // where a cut may fall
            for (qreal y = body.y(); y < body.bottom(); y += 4) g->fillRect(QRectF(X(b), y, 1, 2), GRAT);
        if (pc.mute) hatch(body, LOW);
        else drawClipBody(c, pc.src, pc.src + pc.len, body, INK);
        frame(pr, on ? INK : MID);
        if (on) frame(pr.adjusted(1, 1, -1, -1), INK);
        acc += pc.len;
    }
    // where each moment falls on the track
    const double step = ppb >= 44 ? 1.0 : (ppb * 2 >= 44 ? 2.0 : 4.0);
    for (double b = 0; b < Lc - 1e-6; b += step)
        txt(QRectF(X(b) + 5, sr.bottom() - 18, 60, 15), QString("%1.%2").arg(pl.bar + 1 + (int)(b / 4)).arg((int)std::fmod(b, 4.0) + 1), 11, LOW, L);
    if (pl.bars * 4.0 < Lc - 1e-6) { // split off: the rest of the clip is not heard here
        const QRectF un(X(pl.bars * 4.0) + 1, sr.y() + 18, sr.right() - X(pl.bars * 4.0) - 2, sr.height() - 19);
        g->fillRect(un, QColor(4, 4, 4, 190));
        txt(un, "not played here", 11, LOW);
    }
    if (u.trackPlaying) {
        const double rel = u.trackPos - pl.bar * 4.0;
        if (rel >= 0 && rel < pl.bars * 4.0) g->fillRect(QRectF(X(std::fmod(rel, Lc)), sr.y() + 1, 2, sr.height() - 2), INK);
    }
    zone(sr, "chop_strip", [this, sr](QPointF p, int ph) {
        if (ph == PH_PRESS) chopTap((p.x() - sr.x()) / sr.width(), sr.width());
    });

    const qreal pw = (688 - 3 * 8) / 4.0;
    auto c4 = [pw](int i, qreal y) { return QRectF(std::floor(16 + i * (pw + 8)), y, std::floor(pw), 54); };
    const double every = std::max(1.0, gr);
    button(c4(0, 570), "chop_halves", "Halves", false, [this] { chopEdit([](Placement &x) { return cutPiece(x, piecesLength(x) / 2); }); }, "cut in two");
    button(c4(1, 570), "chop_beats", every >= 4 ? "Bars" : "Beats", false, [this] {
        chopEdit([this](Placement &x) { size_t b = x.pieces.size(); cutEvery(x, std::max(1.0, chopGrid(*track.clips[x.clip]))); return x.pieces.size() != b; });
    },
           every == 1.0 ? "cut every beat" : (every == 2.0 ? "cut every 2 beats" : "cut every bar"));
    button(c4(2, 570), "chop_reset", "Reset", false, [this] {
        chopEdit([this](Placement &x) { if (x.pieces.size() == 1 && !x.pieces[0].mute) return false; resetPieces(x, *track.clips[x.clip]); return true; });
        chopPiece = 0;
    }, "undo every cut");
    button(c4(3, 570), "chop_done", "Done", false, [this] { chopOpen = false; }, "back to the track");
    const QString letter = chopPiece < 26 ? QString(QChar('A' + chopPiece)) : QString::number(chopPiece + 1);
    auto move = [this](int d) { // reads the pieces when tapped, not when drawn
        Placement *p = chosen();
        const int pc = chopPiece, to = pc + d;
        if (!p || to < 0 || to >= (int)p->pieces.size()) return;
        chopEdit([pc, d](Placement &x) { return movePiece(x, pc, d); });
        chopPiece = to;
    };
    button(c4(0, 630), "chop_move_left", QString::fromUtf8("‹ Move"), false, [move] { move(-1); }, QString("piece %1 earlier").arg(letter));
    button(c4(1, 630), "chop_move_right", QString::fromUtf8("Move ›"), false, [move] { move(1); }, QString("piece %1 later").arg(letter));
    button(c4(2, 630), "chop_mute_piece", "Mute piece", pl.pieces[chopPiece].mute, [this] {
        chopEdit([this](Placement &x) { if (chopPiece >= (int)x.pieces.size()) return false; x.pieces[chopPiece].mute = !x.pieces[chopPiece].mute; return true; });
    }, "leave a gap");
    button(c4(3, 630), "chop_repeat_piece", "Repeat piece", false, [this] {
        const int pc = chopPiece;
        chopEdit([pc](Placement &x) { return repeatPiece(x, pc); });
        if (Placement *p = chosen()) chopPiece = qMin(pc + 1, (int)p->pieces.size() - 1);
    }, "the next plays it too");
    txt(QRectF(18, 690, 684, 20), "Tap a dotted line to cut there, a cut to mend it, a piece to choose it. Pieces stay in time.", 12, MID, L);
}

void MainWindow::paintClipSound() {
    const int L = Qt::AlignLeft | Qt::AlignVCenter;
    if (selLane != soundForLane || selIdx != soundForIdx) soundSetup(); // another clip was chosen
    Placement &pl = *chosen();
    const Clip &c = *track.clips[pl.clip];
    const bool beat = c.kind == CK_BEAT;
    if (beat) soundKind = PK_DRUMS;
    else if (soundKind == PK_DRUMS) soundKind = PK_SYNTH;
    const SoundPick &pk = pickFor(c, pl);
    const bool only = pl.pick.own || pl.pick.any();
    auto shown = [beat](const char *s) { return beat ? nice(s) : QString::fromUtf8(s); };
    const QString name = QString::fromUtf8(c.name), now = shown(soundName(c, pk)), rec = shown(recordedSoundName(c));
    section(414, pk.on || pk.kit >= 0 ? QString("Sound for %1, %2   plays %3, recorded as %4").arg(name).arg(barsText(pl.bar, pl.bars)).arg(now).arg(rec)
                                      : QString("Sound for %1, %2   plays %3, as recorded").arg(name).arg(barsText(pl.bar, pl.bars)).arg(rec));

    // kinds of sound
    static const char *KN[PK_COUNT] = {"Keys", "Synth", "Guitar", "Bass", "Drums"};
    const QString KS[PK_COUNT] = {"pianos organs", QString("your %1 sounds").arg(soundCount(PK_SYNTH)), "plucked driven", "low end", "three kits"};
    const qreal kw = (688 - 4 * 6) / 5.0;
    for (int k = 0; k < PK_COUNT; ++k) {
        const QRectF r(std::floor(16 + k * (kw + 6)), 438, std::floor(kw), 52);
        const bool fits = beat ? k == PK_DRUMS : k != PK_DRUMS;
        if (fits) {
            button(r, QString("sound_kind%1").arg(k), KN[k], soundKind == k, [this, k] { if (soundKind != k) { soundKind = k; soundPage = 0; } }, KS[k]);
        } else { // not for this kind of clip
            frame(r, GRAT, BG);
            txt(QRectF(r.x(), r.y() + r.height() * 0.13, r.width(), r.height() * 0.46), KN[k], 16, LOW, Qt::AlignCenter, true);
            txt(QRectF(r.x() + 2, r.y() + r.height() * 0.58, r.width() - 4, r.height() * 0.32), beat ? "notes only" : "beat clips only", 11, LOW);
        }
    }

    // six cells: the sounds of that kind, or five and a way to the next page
    const int n = soundCount(soundKind), per = cellsPerPage(n), pages = qMax(1, (n + per - 1) / per);
    soundPage = qBound(0, soundPage, pages - 1);
    const qreal cw = (688 - 2 * 8) / 3.0;
    for (int cell = 0; cell < 6; ++cell) {
        const QRectF r(std::floor(16 + (cell % 3) * (cw + 8)), 498 + (cell / 3) * 60, std::floor(cw), 54);
        const QString zn = QString("sound_cell%1").arg(cell);
        const int i = soundPage * per + cell;
        QString label, sub;
        bool on = false;
        if (n > 6 && cell == 5) { label = "More"; sub = QString("page %1 of %2").arg(soundPage + 1).arg(pages); }
        else if (i >= n) { frame(r, GRAT, BG); continue; }
        else if (soundKind == PK_DRUMS) {
            label = nice(kitName(i)); sub = i == c.kit ? "as recorded" : "kit";
            on = i == (pk.kit >= 0 ? pk.kit : c.kit);
        } else if (soundKind == PK_SYNTH) {
            label = QString::fromUtf8(soundPatch(PK_SYNTH, i).name);
            sub = i < 10 ? QString("slot %1").arg((i + 1) % 10) : QString("favourite");
            on = label == now;
        } else {
            label = QString::fromUtf8(librarySound(libKind(soundKind), i).name);
            sub = libraryHint(libKind(soundKind), i);
            on = pk.on && label == now;
        }
        button(r, zn, label, on, [this, cell] { soundPickAt(cell); }, sub);
        txt(QRectF(r.x() + 5, r.y() + 3, 12, 14), QString::number(cell + 1), 11, on || downNames.contains(zn) ? BG : LOW, L);
    }

    const qreal y = 622, h = 56;
    button(QRectF(16, y, 130, h), "sound_hear", hearOn ? QString::fromUtf8("■ Stop") : QString::fromUtf8("▶ Hear"), hearOn, [this] { hearToggle(); }, "this clip alone");
    const QRectF orr(154, y, 164, h);
    if (beat) {
        frame(orr, GRAT, BG);
        txt(QRectF(orr.x(), orr.y() + 6, orr.width(), 16), "Octave", 12, LOW);
        txt(QRectF(orr.x(), orr.y() + 22, orr.width(), h - 26), "notes only", 13, LOW);
    } else {
        stepper(orr, "sound_octave", "Octave", pk.octave > 0 ? QString("+%1").arg(pk.octave) : QString::number(pk.octave),
                [this] { soundApply([](SoundPick &k) { k.octave = qMax(-2, k.octave - 1); }); },
                [this] { soundApply([](SoundPick &k) { k.octave = qMin(2, k.octave + 1); }); });
    }
    button(QRectF(326, y, 130, h), "sound_scope", only ? QString("Only this one") : QString("Every %1").arg(name), false, [this] { soundScope(); },
           only ? QString("tap for every one") : QString("tap for only this"));
    button(QRectF(464, y, 130, h), "sound_as_recorded", "As recorded", false, [this] { soundApply([](SoundPick &k) { k = SoundPick(); }); }, rec);
    button(QRectF(602, y, 102, h), "sound_done", "Done", false, [this] { soundOpen = false; }, "back to tools");
    txt(QRectF(18, 688, 684, 20), beat ? QString("Keys: 1-3 pick a kit, %1 hear it, %2 done.").arg(km.keyFor(A_SUSTAIN, false)).arg(km.keyFor(A_LOOP, false))
                                       : QString("Keys: left right pick a kind, 1-6 a sound, up down the octave, %1 hear it, %2 done.").arg(km.keyFor(A_SUSTAIN, false)).arg(km.keyFor(A_LOOP, false)),
        12, MID, L);
}

void MainWindow::paintMixer() {
    const int L = Qt::AlignLeft | Qt::AlignVCenter;
    mixLane = qBound(0, mixLane, kLanes - 1);
    QString solos;
    for (int ln = 0; ln < kLanes; ++ln)
        if (track.mix[ln].solo) solos += (solos.isEmpty() ? "" : ", ") + QString(laneLabel(ln));
    section(414, solos.isEmpty() ? QString("Mixer   %1 chosen   tap or drag a fader").arg(laneLabel(mixLane))
                                 : QString("Mixer   %1 chosen   only %2 heard").arg(laneLabel(mixLane)).arg(solos));

    const qreal W = 132, G = 7, y0 = 438, y1 = 638;
    for (int ln = 0; ln < kLanes; ++ln) {
        const LaneMix &m = track.mix[ln];
        const bool chosenLane = ln == mixLane, heard = laneHeard(track, ln);
        const qreal x = 16 + ln * (W + G);
        const QRectF r(x, y0, W, y1 - y0);
        frame(r, chosenLane ? INK : LOW, BG);
        const QRectF hd(x, y0, W, 24);
        if (chosenLane) g->fillRect(hd, INK);
        txt(hd, laneLabel(ln), 13, chosenLane ? BG : (heard ? INK : LOW), Qt::AlignCenter, true);
        g->fillRect(QRectF(x + 1, y0 + 24, W - 2, 1), GRAT);
        zone(hd, QString("mixer_lane%1").arg(ln), [this, ln](QPointF, int ph) { if (ph == PH_PRESS) mixLane = ln; });

        // the fader: the bar is the level; the notch beside it is 80, as recorded
        const qreal top = y0 + 36, bottom = y0 + 150;
        const QRectF fr(x + 24, top, 38, bottom - top);
        frame(fr, heard ? LOW : GRAT, BG);
        const qreal h = std::floor((fr.height() - 6) * m.level / 100.0);
        g->fillRect(QRectF(fr.x() + 3, fr.bottom() - 3 - h, fr.width() - 6, h), heard ? INK : LOW);
        const qreal ny = std::floor(fr.bottom() - 3 - (fr.height() - 6) * kLaneLevelNormal / 100.0);
        g->fillRect(QRectF(fr.x() - 7, ny, 5, 1), MID);
        txt(QRectF(x + 90, top, 40, 22), QString::number(m.level), 17, heard ? INK : LOW, L, true);
        txt(QRectF(x + 90, top + 22, 40, 16), m.level == kLaneLevelNormal ? "as is" : (m.level > kLaneLevelNormal ? "louder" : "softer"), 11, MID, L);

        // the meter: twenty steps from -48 dB to full scale, only while the track plays
        const QRectF mr(x + 72, top, 12, bottom - top);
        frame(mr, GRAT, BG);
        const float pk = u.trackPlaying ? u.lanePeak[ln] : 0.0f;
        const float db = pk > 1e-6f ? 20.0f * std::log10(pk) : -120.0f;
        const qreal seg = (mr.height() - 4) / 20.0;
        for (int sgm = 0; sgm < 20; ++sgm) {
            const bool lit = db > -48.0f + sgm * 2.4f;
            const qreal sy = std::floor(mr.bottom() - 2 - (sgm + 1) * seg);
            g->fillRect(QRectF(mr.x() + 2, sy + 1, mr.width() - 4, std::max<qreal>(1, std::floor(seg) - 1)), lit ? INK : GRAT);
        }
        zone(QRectF(x + 4, top - 8, 82, bottom - top + 16), QString("mixer_fader%1").arg(ln),
             [this, ln, top, bottom](QPointF p, int ph) { mixFader(ln, p.y(), top + 3, bottom - 3, ph); });

        const qreal by = y0 + 160, bw = 56;
        button(QRectF(x + 8, by, bw, 34), QString("mixer_mute%1").arg(ln), "M", m.mute,
               [this, ln] { mixApply(ln, [](LaneMix &mm) { mm.mute = !mm.mute; return true; }); });
        button(QRectF(x + 8 + bw + 4, by, bw, 34), QString("mixer_solo%1").arg(ln), "S", m.solo,
               [this, ln] { mixApply(ln, [](LaneMix &mm) { mm.solo = !mm.solo; return true; }); });
    }

    const qreal x = 16 + kLanes * (W + G), w = 704 - x;
    const bool cyc = track.cycleTo > track.cycleFrom;
    const int from = cyc && (playBar < track.cycleFrom || playBar >= track.cycleTo) ? track.cycleFrom : qMin(playBar, track.bars - 1);
    button(QRectF(x, y0, w, 62), "mixer_play", u.trackPlaying ? QString::fromUtf8("■ Stop") : QString::fromUtf8("▶ Play"), u.trackPlaying,
           [this] { trackPlayStop(); }, u.trackPlaying ? QString("bar %1").arg((int)(u.trackPos / 4.0) + 1) : QString("from bar %1").arg(from + 1));
    button(QRectF(x, y0 + 68, w, 62), "mixer_reset", "Reset", false, [this] { mixReset(); }, "all back to 80");
    button(QRectF(x, y0 + 136, w, 64), "mixer_done", "Done", false, [this] { mixOpen = false; }, "back to tools");

    txt(QRectF(18, 652, 684, 18), QString("Keys: left right choose a lane (or 1-4), up down its level, 0 back to 80,"), 12, MID, L);
    txt(QRectF(18, 670, 684, 18), QString("M mute, S solo, %1 play, %2 or X done. %3 + Z undoes a change.")
            .arg(km.keyFor(A_SUSTAIN, false)).arg(km.keyFor(A_HOME, false)).arg(km.keyFor(A_COMMAND, false)), 12, MID, L);
    txt(QRectF(18, 692, 684, 18), "Levels, mutes and solos are kept with the track and heard in an export.", 12, LOW, L);
}
