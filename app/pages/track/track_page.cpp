// venv-vrequency — the Track page: layout of the shelf, the ruler, the lanes and the play line.
#include "app/pages/track/track_page.h"
#include "app/pages/track/track_layout.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <QPainterPath>

using namespace td;
using namespace pal;

void TrackPage::paint() {
    const int L = Qt::AlignLeft | Qt::AlignVCenter;
    const bool cyc = track.cycleTo > track.cycleFrom;
    const int nClips = (int)track.clips.size();
    int placed = 0;
    for (int ln = 0; ln < kLanes; ++ln) placed += (int)track.lane[ln].size();
    if (u.trackPlaying && !dragOn) { // follow the play line a screenful at a time
        const int pb = (int)(u.trackPos / 4.0);
        if (pb < viewBar || pb >= viewBar + tl::VIEW) viewBar = (pb / tl::VIEW) * tl::VIEW;
    }
    viewBar = qBound(0, viewBar, maxViewBar());

    // headings
    ui.txt(QRectF(18, 60, 110, 20), "Clips", 13, MID, L);
    QString head = QString("Track  %1 bpm  %2 bar%3").arg((int)std::round(u.set.bpm)).arg(track.bars).arg(track.bars == 1 ? "" : "s");
    if (exportState.load() == 1) head += QString("  exporting %1%").arg(exportPct.load());
    else if (u.trackPlaying) head += QString("  playing bar %1").arg((int)(u.trackPos / 4.0) + 1);
    ui.txt(QRectF(136, 60, 568, 20), head, 13, MID, L);

    // the shelf of clips
    const int pages = qMax(1, (nClips + tl::SHELF - 1) / tl::SHELF);
    shelfPage = qBound(0, shelfPage, pages - 1);
    for (int k = 0; k < tl::SHELF; ++k) {
        const QRectF r(16, 84 + k * 54, 112, 48);
        const int ci = shelfPage * tl::SHELF + k;
        if (ci >= nClips) { ui.frame(r, GRAT); continue; }
        const Clip &c = *track.clips[ci];
        const QString name = QString("shelf_clip%1").arg(k);
        const bool on = (handOn && hand.clip == ci) || ui.isDown(name);
        ui.frame(r, on ? INK : LOW, on ? INK : BG);
        if (removeArmed == ci && !ui.isDown(name)) ui.frame(r.adjusted(3, 3, -3, -3), on ? BG : INK);
        ui.txt(QRectF(r.x() + 4, r.y() + 3, 12, 14), QString::number((k + 1) % 10), 11, on ? BG : LOW, L);
        ui.txt(QRectF(r.x(), r.y() + 5, r.width(), 22), ui.fit(QString::fromUtf8(c.name), 14, 92), 14, on ? BG : INK, Qt::AlignCenter, true);
        ui.txt(QRectF(r.x() + 2, r.y() + 28, r.width() - 4, 16), ui.fit(tl::clipSub(c), 11, 106), 11, on ? BG : MID);
        ui.zone(r, name, [this, ci](QPointF, int ph) { if (ph == PH_PRESS) trackTake(ci); });
    }
    if (pages > 1) {
        ui.button(QRectF(16, 354, 62, 48), "track_keep", "+ Keep", false, [this] { trackKeep(); }, "what plays");
        ui.button(QRectF(82, 354, 46, 48), "track_shelf_page", QString("%1/%2").arg(shelfPage + 1).arg(pages), false, [this, pages] { shelfPage = (shelfPage + 1) % pages; }, "more");
    } else {
        ui.button(QRectF(16, 354, 112, 48), "track_keep", "+ Keep", false, [this] { trackKeep(); }, "beat chord loop");
    }

    // the ruler of bars
    for (int i = 0; i < tl::VIEW; ++i) {
        const int bar = viewBar + i;
        const qreal x = std::floor(tl::GX + i * tl::BW), x2 = std::floor(tl::GX + (i + 1) * tl::BW);
        const bool in = cyc && bar >= track.cycleFrom && bar < track.cycleTo;
        const QRectF nr(x, 86, x2 - x, 16);
        if (in) ui.g->fillRect(nr.adjusted(0, 0, 1, 0), INK);
        ui.txt(nr, QString::number(bar + 1), 12, in ? BG : (bar >= track.bars ? LOW : (bar % 4 == 0 ? INK : MID)), Qt::AlignCenter, bar % 4 == 0 || in);
    }
    ui.zone(QRectF(tl::GX, 78, tl::GW, 26), "track_ruler", [this](QPointF p, int ph) {
        if (ph == PH_RELEASE) return;
        const int b = qBound(0, tl::barAtX(p.x(), viewBar), qMin(track.bars, kTrackMaxBars - 1));
        if (b == playBar && ph == PH_MOVE) return;
        playBar = b;
        if (inst.uiState().trackPlaying && b < track.bars) inst.trackDo(TC_PLAY, b);
    });

    // lanes
    const qreal lanesBottom = tl::laneRect(kLanes - 1).bottom();
    const qreal endX = std::floor(tl::GX + (track.bars - viewBar) * tl::BW);
    for (int ln = 0; ln < kLanes; ++ln) {
        const QRectF lr = tl::laneRect(ln);
        const bool target = handOn && laneTakes(ln, track.clips[hand.clip]->kind);
        const bool strong = target || selLane == ln;
        const bool heard = laneHeard(track, ln);
        const QString state = track.mix[ln].mute ? "muted" : (!heard ? "off" : (track.mix[ln].solo ? "solo" : ""));
        if (state.isEmpty()) ui.txt(QRectF(134, lr.y(), 44, lr.height()), tl::laneLabel(ln), 11, strong ? INK : MID, L, strong);
        else {
            ui.txt(QRectF(134, lr.y() + lr.height() / 2 - 15, 44, 15), tl::laneLabel(ln), 11, heard ? (strong ? INK : MID) : LOW, L, strong);
            ui.txt(QRectF(134, lr.y() + lr.height() / 2 + 1, 44, 15), state, 11, heard ? INK : LOW, L);
        }
        ui.frame(QRectF(lr.x(), lr.y(), lr.width() + 1, lr.height()), GRAT);
        for (int i = 1; i < tl::VIEW; ++i)
            if (viewBar + i < track.bars) ui.g->fillRect(QRectF(std::floor(tl::GX + i * tl::BW), lr.y() + 1, 1, lr.height() - 2), GRAT);
        if (endX > tl::GX && endX < tl::GX + tl::GW) hatch(QRectF(endX + 1, lr.y() + 1, tl::GX + tl::GW - endX - 1, lr.height() - 2), QColor(0x1c, 0x1c, 0x1c));
    }
    ui.g->save();
    ui.g->setClipRect(QRectF(tl::GX, tl::LY - 1, tl::GW + 1, lanesBottom - tl::LY + 2), Qt::IntersectClip);
    for (int ln = 0; ln < kLanes; ++ln)
        for (int i = 0; i < (int)track.lane[ln].size(); ++i) drawPlacement(ln, i);
    for (int ln = 0; ln < kLanes; ++ln) // a lane the mixer silences is drawn faint
        if (!laneHeard(track, ln)) ui.g->fillRect(tl::laneRect(ln).adjusted(1, 1, -1, -1), QColor(4, 4, 4, 160));
    int spotLane = -1, spotBar = -1;
    if (handOn && placeSpot(spotLane, spotBar)) { // where Place would put the clip in hand
        const QRectF lr = tl::laneRect(spotLane);
        const qreal x0 = std::floor(tl::GX + (spotBar - viewBar) * tl::BW), x1 = std::floor(tl::GX + (spotBar + hand.bars - viewBar) * tl::BW);
        ui.g->setPen(QPen(MID, 1, Qt::DashLine));
        ui.g->setBrush(Qt::NoBrush);
        ui.g->drawRect(QRectF(x0 + 2.5, lr.y() + 2.5, x1 - x0 - 4, lr.height() - 5));
        ui.txt(QRectF(x0, lr.y(), x1 - x0, lr.height()), x1 - x0 > 60 ? "place" : "+", 11, MID);
    }
    if (endX > tl::GX && endX <= tl::GX + tl::GW) ui.g->fillRect(QRectF(endX, tl::LY, 2, lanesBottom - tl::LY), MID); // the end of the track
    ui.g->restore();
    ui.zone(QRectF(tl::GX, tl::LY - 2, tl::GW, lanesBottom - tl::LY + 4), "track_lanes", [this](QPointF p, int ph) { laneTouch(p, ph); });

    if (nClips == 0 && placed == 0) {
        const QRectF m(tl::GX + 40, tl::LY + 76, tl::GW - 80, 124);
        ui.frame(m, LOW, BG);
        ui.tag(QRectF(m.x() + 1, m.y() + 1, 118, 18), "An empty track", 12, L);
        ui.txt(QRectF(m.x() + 16, m.y() + 30, m.width() - 32, 20), "1  Make a beat, a loop or some chords", 13, INK, L);
        ui.txt(QRectF(m.x() + 16, m.y() + 56, m.width() - 32, 20), "2  Tap + Keep: they become clips", 13, INK, L);
        ui.txt(QRectF(m.x() + 16, m.y() + 82, m.width() - 32, 20), "3  Tap a clip, then tap a bar", 13, INK, L);
    }

    // the play line
    {
        const double pos = u.trackPlaying ? u.trackPos / 4.0 : (double)playBar;
        const qreal x = std::floor(tl::GX + (pos - viewBar) * tl::BW);
        if (x >= tl::GX - 1 && x <= tl::GX + tl::GW + 1) {
            ui.g->fillRect(QRectF(x, 84, u.trackPlaying ? 2 : 1, lanesBottom - 84), INK);
            QPainterPath m;
            m.moveTo(x - 5, 76); m.lineTo(x + 6, 76); m.lineTo(x + 0.5, 85); m.closeSubpath();
            ui.g->setRenderHint(QPainter::Antialiasing, true);
            ui.g->setPen(Qt::NoPen); ui.g->setBrush(INK);
            ui.g->drawPath(m);
            ui.g->setRenderHint(QPainter::Antialiasing, false);
        }
    }

    if (mixOpen) paintMixer();
    else if (soundOpen && chosen()) paintClipSound();
    else if (chopOpen && chosen()) paintChop();
    else { chopOpen = soundOpen = false; paintTrackTools(); }
}
