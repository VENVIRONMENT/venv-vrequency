// venv-vrequency — arranging: placing clips on the lanes.
#include "core/track/track.h"
#include <algorithm>

namespace td {

Placement placeClip(const Clip &c, int bar) {
    Placement p;
    p.bar = bar; p.bars = c.bars;
    resetPieces(p, c);
    return p;
}

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

} // namespace td
