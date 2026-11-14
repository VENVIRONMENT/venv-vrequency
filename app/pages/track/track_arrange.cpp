// venv-vrequency — the Track page: repeat, split, join, mute, cycle, copy, moving, and dragging on the lanes.
#include "app/pages/track/track_page.h"
#include "app/pages/track/track_layout.h"
#include "app/ui/palette.h"
#include <QPainter>

using namespace td;
using namespace pal;

void TrackPage::trackRepeat() {
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

void TrackPage::trackSplit() {
    if (!chosen()) { toast("Choose a placed clip first", 1400); return; }
    const int ln = selLane, i = selIdx, at = playBar;
    if (!trackApply([&](TrackData &t) { return splitPlacement(t, ln, i, at); })) {
        toast("Put the play line inside the chosen clip, on a bar line", 2200);
        return;
    }
    chooseAt(ln, at);
}

void TrackPage::trackJoin() {
    Placement *p = chosen();
    if (!p) { toast("Choose a placed clip first", 1400); return; }
    const int ln = selLane, i = selIdx, bar = p->bar;
    if (!trackApply([&](TrackData &t) { return joinPlacements(t, ln, i); })) {
        toast("It only joins with what carries on from it, right after it", 2400);
        return;
    }
    chooseAt(ln, bar);
}

void TrackPage::trackMute() {
    Placement *p = chosen();
    if (!p) { toast("Choose a placed clip first", 1400); return; }
    const int ln = selLane, i = selIdx, bar = p->bar;
    trackApply([&](TrackData &t) { t.lane[ln][i].mute = !t.lane[ln][i].mute; return true; });
    chooseAt(ln, bar);
}

void TrackPage::trackCycle() {
    const bool on = track.cycleTo > track.cycleFrom;
    int from = 0, to = 0;
    if (!on) {
        if (Placement *p = chosen()) { from = p->bar; to = p->bar + p->bars; }
        else { from = qMin(playBar, track.bars - 1); to = qMin(from + 4, track.bars); }
    }
    const int ln = selLane, bar = chosen() ? chosen()->bar : -1;
    trackApply([&](TrackData &t) { t.cycleFrom = from; t.cycleTo = to; return true; });
    if (bar >= 0) chooseAt(ln, bar);
    toast(on ? QString("Cycle off") : QString("Cycle: %1").arg(tl::barsText(from, to - from)), 1200);
}

void TrackPage::trackCopy(bool cut) {
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

void TrackPage::trackSelect(int dx, int dy) {
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

void TrackPage::trackMove(int dbar, int dlane) {
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

void TrackPage::laneTouch(QPointF p, int ph) {
    int ln = -1;
    for (int i = 0; i < kLanes; ++i)
        if (p.y() >= tl::laneRect(i).y() - tl::LG / 2 && p.y() < tl::laneRect(i).bottom() + tl::LG / 2) ln = i;
    const int bar = tl::barAtX(p.x(), viewBar);
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
