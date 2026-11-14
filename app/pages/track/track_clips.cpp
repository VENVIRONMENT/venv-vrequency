// venv-vrequency — the Track page: the shelf of clips, keeping what plays, placing and removing.
#include "app/pages/track/track_page.h"
#include "app/pages/track/track_layout.h"
#include "app/ui/palette.h"
#include <QPainter>

using namespace td;
using namespace pal;

void TrackPage::trackTake(int ci) {
    if (ci < 0 || ci >= (int)track.clips.size()) return;
    removeArmed = -1;
    if (handOn && handFromShelf && hand.clip == ci) { handOn = false; return; }
    hand = placeClip(*track.clips[ci], 0);
    hand.clip = ci;
    handOn = true; handFromShelf = true;
    selLane = selIdx = -1; chopOpen = false;
}

// Copies of what is live now go on the shelf: the beat, the backing's chords, the loop.
void TrackPage::trackKeep() {
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
            if ((int)t.clips.size() >= tl::kMaxClips) { full = true; break; }
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
        shelfPage = lastNew / tl::SHELF;
        handOn = false;
        trackTake(lastNew);
    }
}

bool TrackPage::trackPlace(int lane, int bar) {
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

void TrackPage::trackPlaceAtLine() {
    if (!handOn) { toast("Pick a clip on the left first", 1600); return; }
    int lane, bar;
    if (!placeSpot(lane, bar)) { toast("No room left on that lane", 1600); return; }
    if (trackPlace(lane, bar)) { // the line moves on, so placing again carries on from there
        playBar = qMin(bar + hand.bars, kTrackMaxBars - 1);
        showBar(bar);
    }
}

void TrackPage::trackRemove() {
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
        if (removeArmed != ci || sh.clock.elapsed() - removeArmedAt > 4000) {
            removeArmed = ci; removeArmedAt = sh.clock.elapsed();
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
