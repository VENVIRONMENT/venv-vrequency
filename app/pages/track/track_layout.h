// venv-vrequency — the Track page's measures and small helpers, shared by its files.
#pragma once
#include "app/ui/palette.h"
#include "core/presets/sound_shelf.h"
#include "core/track/track.h"
#include <QRectF>
#include <QString>
#include <cmath>

namespace tl {
const qreal GX = 180, GW = 524;        // the grid of bars
const int VIEW = 8;                    // bars on screen
const qreal BW = GW / VIEW;
const qreal LY = 106, LH = 66, LG = 4; // lanes
const int SHELF = 5;                   // clips shown on the shelf at once
const int kMaxClips = 40;

inline const char *laneLabel(int ln) {
    static const char *N[td::kLanes] = {"Beat", "Chords", "Loop A", "Loop B"};
    return N[qBound(0, ln, td::kLanes - 1)];
}
inline QRectF laneRect(int ln) { return QRectF(GX, LY + ln * (LH + LG), GW, LH); }
inline int barAtX(qreal x, int viewBar) { return viewBar + (int)std::floor((x - GX) / BW); }

inline QString clipSub(const td::Clip &c) {
    const QString what = c.kind == td::CK_BEAT ? nice(td::soundName(c, c.pick)) : QString::fromUtf8(td::soundName(c, c.pick));
    return QString("%1 bar%2 %3").arg(c.bars).arg(c.bars == 1 ? "" : "s").arg(what).trimmed();
}
inline QString barsText(int bar, int bars) {
    return bars == 1 ? QString("bar %1").arg(bar + 1) : QString("bars %1 to %2").arg(bar + 1).arg(bar + bars);
}
// how fine a cut may be: finer for short clips, where there is room to tap
inline double chopGrid(const td::Clip &c) { return c.bars <= 1 ? 0.5 : (c.bars <= 4 ? 1.0 : (c.bars <= 8 ? 2.0 : 4.0)); }
// the sound picker: which library a picker kind is, and how many cells a page shows
inline int libKind(int pickKind) { return pickKind == 0 ? td::SK_KEYS : (pickKind == 2 ? td::SK_GUITAR : td::SK_BASS); }
inline int cellsPerPage(int count) { return count > 6 ? 5 : 6; } // a sixth cell turns the page when there are more
} // namespace tl
