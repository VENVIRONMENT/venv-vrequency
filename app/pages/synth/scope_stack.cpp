// venv-vrequency — the scope's 3D view: the latest real cycles of the output, stacked in depth.
//
// Every frame the newest whole cycle of what you hear is cut out of the output (one measured
// period, starting where the wave rises through zero) and put at the front; older cycles move
// back and fade. A held note makes a calm block of the same shape, a filter sweep or an LFO
// rolls back through the stack, a dying note shrinks. Chords and noise have no single cycle:
// then the stack shows the newest 20 ms instead, which is busier but still the real sound.
#include "app/pages/synth/scope.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace td;

namespace {
// Where layer `z` (0 front .. 1 back) sits inside the trace area.
struct StackRow { qreal x, y, w, amp; };
StackRow place(float z, qreal div) {
    const qreal shrink = 1.0 - 0.22 * z;
    return {24 + 200 * z, 124 - 80 * z, 430 * shrink, div * 0.85 * shrink};
}
} // namespace

void Scope::captureCycle(const View &v) {
    std::memmove(layers[1], layers[0], sizeof(float) * kPts * (kLayers - 1));
    layerCount = std::min(layerCount + 1, (int)kLayers);
    float *dst = layers[0];
    const int total = v.total;
    if (v.span <= 0.004f || total < 2048) { std::fill(dst, dst + kPts, 0.0f); return; }

    double start = total - 961, len = 960; // no clear pitch: the newest 20 ms
    if (period > 8) {
        const int P = (int)std::ceil(period);
        float lo = 0;
        for (int i = total - 2 * P; i < total; ++i) lo = std::min(lo, buf[i]);
        const float hyst = std::max(0.003f, -lo * 0.5f);
        for (int i = total - P - 2; i > std::max(1, total - 4 * P); --i) {
            if (!(buf[i - 1] < 0 && buf[i] >= 0)) continue;
            bool armed = false; // a deep negative swing just before: the cycle's own start, not a wiggle
            for (int j = std::max(0, i - P); j < i && !armed; ++j) armed = buf[j] < -hyst;
            if (!armed) continue;
            start = i - 1 + buf[i - 1] / (buf[i - 1] - buf[i]);
            len = period;
            break;
        }
    }
    for (int k = 0; k < kPts; ++k) {
        const double pos = start + len * k / (kPts - 1);
        const int i0 = std::clamp((int)pos, 0, total - 2);
        const float fr = (float)(pos - i0);
        dst[k] = buf[i0] + (buf[i0 + 1] - buf[i0]) * fr;
    }
}

// Back to front: each cycle first blacks out what lies under it (so nearer cycles hide farther
// ones, like a landscape), then draws its line, greyer the older it is. The newest is filled to
// its centre line and drawn brightest.
void Scope::paintStack(QRectF gr, qreal div) {
    QPainter &g = *ui.g;
    g.save();
    g.setClipRect(gr.adjusted(1, 1, -1, -1));
    g.translate(gr.topLeft());
    g.setRenderHint(QPainter::Antialiasing, true);
    const float vdiv = kVoltsPerDiv[vdIdx];
    auto path = [&](const float *ys, const StackRow &l, QPolygonF &pts) {
        pts.resize(kPts);
        for (int k = 0; k < kPts; ++k)
            pts[k] = QPointF(l.x + l.w * k / (kPts - 1), l.y - qBound(-2.2f, ys[k] / vdiv, 2.2f) * l.amp);
    };
    QPolygonF pts;
    for (int i = layerCount - 1; i >= 0; --i) {
        const float z = (float)i / (kLayers - 1);
        const StackRow l = place(z, div);
        path(layers[i], l, pts);
        if (i > 0) {
            QPolygonF under = pts; // hide what is behind this cycle
            under << QPointF(l.x + l.w, l.y + 2.3 * l.amp) << QPointF(l.x, l.y + 2.3 * l.amp);
            g.setRenderHint(QPainter::Antialiasing, false); // the hiding fill needs no smoothing: cheaper
            g.setPen(Qt::NoPen);
            g.setBrush(pal::BG);
            g.drawPolygon(under);
            g.setRenderHint(QPainter::Antialiasing, true);
            const int grey = (int)(215 - 165 * z);
            g.setPen(QPen(QColor(grey, grey, grey), 0)); // cosmetic 1 px: Qt's fast line path
            g.setBrush(Qt::NoBrush);
            g.drawPolyline(pts);
        }
    }
    if (layerCount > 0) { // the newest cycle
        const StackRow l = place(0, div);
        path(layers[0], l, pts);
        QPolygonF fill = pts;
        fill << QPointF(l.x + l.w, l.y) << QPointF(l.x, l.y);
        g.setPen(Qt::NoPen);
        g.setBrush(QColor(255, 255, 255, 38));
        g.drawPolygon(fill);
        g.setPen(QPen(QColor(0x80, 0x80, 0x7d), 1, Qt::DotLine));
        g.drawLine(QPointF(l.x, l.y), QPointF(l.x + l.w, l.y)); // the centre line, 0 V
        g.setPen(QPen(QColor(255, 255, 251), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        g.drawPolyline(pts);
    }
    g.restore();
    g.setPen(QColor(0x46, 0x46, 0x44));
    g.setBrush(Qt::NoBrush);
    g.drawRect(QRectF(gr.x(), gr.y(), gr.width(), gr.height()));
}
