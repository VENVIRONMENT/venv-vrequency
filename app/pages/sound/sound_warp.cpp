// venv-vrequency — the Sound page's Warp view: the IN > OUT picture and the six warp sliders.
// The picture is a ramp from -1 to 1 run through the voice's own warp code (warpCurve), so the line
// is what the voices do to a signal. RING MOD moves with the note and time, so it is not drawn.
#include "app/pages/sound/sound_page.h"
#include "app/sounds/sounds.h"
#include "app/ui/palette.h"
#include "app/ui/zones.h"
#include "core/synth/voice_warp.h"
#include <QPainter>
#include <QPolygonF>
#include <algorithm>
#include <cmath>

using namespace td;
using namespace pal;

const int kWarpParams[6] = {P_DRIVE, P_FOLD, P_CRUSH, P_DSAMP, P_RING, P_BENDIN};

void SoundPage::paintWarp(qreal y) {
    const float *p = sounds.lead.p;
    const QRectF box(16, y, 200, 226);
    ui.frame(box, LOW, BG);
    ui.txt(QRectF(box.x() + 8, box.y() + 5, 120, 16), "IN > OUT", 12, MID, Qt::AlignLeft | Qt::AlignVCenter, true);
    const QRectF plot(box.x() + 10, box.y() + 26, box.width() - 20, box.height() - 52);
    // faint axes: zero in and zero out, and the full-scale edges
    ui.g->fillRect(QRectF(plot.x(), std::floor(plot.center().y()), plot.width(), 1), GRAT);
    ui.g->fillRect(QRectF(std::floor(plot.center().x()), plot.y(), 1, plot.height()), GRAT);
    for (int x = 0; x < (int)plot.width(); x += 4) {
        ui.g->fillRect(QRectF(plot.x() + x, plot.y(), 1, 1), GRAT);
        ui.g->fillRect(QRectF(plot.x() + x, plot.bottom() - 1, 1, 1), GRAT);
    }
    constexpr int kN = 360;
    float out[kN];
    warpCurve(p, out, kN);
    QPolygonF line;
    line.reserve(kN);
    for (int i = 0; i < kN; ++i)
        line << QPointF(plot.x() + plot.width() * i / (kN - 1), plot.center().y() - std::clamp(out[i], -1.0f, 1.0f) * (plot.height() * 0.5 - 1));
    ui.g->save();
    ui.g->setRenderHint(QPainter::Antialiasing, true);
    ui.g->setPen(QPen(INK, 1.5));
    ui.g->drawPolyline(line);
    ui.g->restore();
    QString foot = p[P_RING] > 0 ? QString("+ ring %1%").arg((int)std::round(p[P_RING] * 100)) : QString();
    if (p[P_BENDIN] != 0) foot += QString(foot.isEmpty() ? "" : "  ") + QString("+ bend in");
    ui.txt(QRectF(box.x() + 8, box.bottom() - 22, box.width() - 16, 16), foot.isEmpty() ? QString("-1  in  +1") : foot, 12,
           foot.isEmpty() ? LOW : MID, Qt::AlignCenter);
    ui.zone(box, zn::warpcurve, [](QPointF, int) {});
    for (int k = 0; k < 6; ++k) paramSlider(QRectF(224 + (k % 2) * 244, y + (k / 2) * 78, 236, 70), kWarpParams[k]);
}
