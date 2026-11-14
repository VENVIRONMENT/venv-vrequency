// venv-vrequency — the Track page: the master copy, one undoable edit at a time, undo and redo.
#include "app/pages/track/track_page.h"
#include "app/pages/track/track_layout.h"
#include "app/ui/palette.h"
#include "app/store/store.h"
#include <QJsonObject>
#include <memory>
#include <QPainter>

using namespace td;
using namespace pal;

Placement *TrackPage::chosen() {
    if (selLane < 0 || selLane >= kLanes || selIdx < 0 || selIdx >= (int)track.lane[selLane].size()) return nullptr;
    return &track.lane[selLane][selIdx];
}

int TrackPage::maxViewBar() const { return qMin(kTrackMaxBars - tl::VIEW, ((track.bars + 3) / 4) * 4); }

void TrackPage::showBar(int bar) {
    if (bar < viewBar) viewBar = (bar / 4) * 4;
    else if (bar >= viewBar + tl::VIEW) viewBar = ((bar - tl::VIEW) / 4 + 1) * 4;
    viewBar = qBound(0, viewBar, maxViewBar());
}

void TrackPage::chooseAt(int lane, int bar) {
    selLane = lane;
    selIdx = placementAt(track, lane, bar);
    if (selIdx < 0) selLane = -1;
    chopPiece = 0;
}

// Tidy the master copy and give the engine its own.
void TrackPage::trackSync() {
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
    sh.saveSoon();
    sh.redraw();
}

bool TrackPage::trackApply(const std::function<bool(TrackData &)> &fn) {
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
bool TrackPage::placeSpot(int &lane, int &bar) const {
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

void TrackPage::trackUndoDo() {
    if (trackUndo.empty()) { toast("Nothing to undo", 1000); return; }
    trackRedo.push_back(track);
    track = trackUndo.back();
    trackUndo.pop_back();
    selLane = selIdx = -1; chopOpen = false; dashCache.clear();
    trackSync();
}

void TrackPage::trackRedoDo() {
    if (trackRedo.empty()) { toast("Nothing to redo", 1000); return; }
    trackUndo.push_back(track);
    track = trackRedo.back();
    trackRedo.pop_back();
    selLane = selIdx = -1; chopOpen = false; dashCache.clear();
    trackSync();
}

void TrackPage::trackPlayStop() {
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

void TrackPage::trackLine(int dbar) {
    playBar = qBound(0, playBar + dbar, qMin(track.bars, kTrackMaxBars - 1));
    showBar(playBar);
    if (inst.uiState().trackPlaying && playBar < track.bars) inst.trackDo(TC_PLAY, playBar);
}

// A saved session's track replaces this one; the old one is an undo step.
void TrackPage::loadJson(const QJsonObject &o) {
    trackUndo.push_back(track);
    trackRedo.clear();
    store::trackFromJson(o, track);
    selLane = selIdx = -1; handOn = false; chopOpen = false; dashCache.clear();
    playBar = viewBar = 0;
    trackSync();
}

// Every frame, whichever page is showing.
void TrackPage::tick() {
    pollExport();
    hearWatch();
}
