// venv-vrequency — the scope's phase view: the output plotted against itself a quarter of a
// period later (a Lissajous figure). A pure tone is an ellipse, harmonics fold it into loops, a
// stable pitch holds still. The last few figures fade out behind the newest.
#include "app/pages/synth/scope.h"
#include "app/sounds/sounds.h"
#include "audio/audio.h"
#include <QPainter>
#include <QPolygonF>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace td;

// One closed figure of the newest whole period (the newest 20 ms when there is no pitch).
void Scope::capturePhase(const View &v) {
    std::memmove(figs[1], figs[0], sizeof(figs[0]) * (kFigs - 1));
    figCount = std::min(figCount + 1, (int)kFigs);
    const float sr = (float)sh.audio.sampleRate();
    const double P = period > 8 ? period : 0.02 * sr, d = period > 8 ? period / 4 : 0.002 * sr;
    figDelayMs = (float)(d * 1000 / sr);
    float pk = 0;
    for (int k = 0; k < kFigPts; ++k) {
        const double pos = v.total - 2 - d - P + P * k / (kFigPts - 1); // x: now, y: a quarter period later
        const int i0 = std::clamp((int)pos, 0, v.total - 3);
        const float now = buf[i0] + (buf[i0 + 1] - buf[i0]) * (float)(pos - i0);
        const double lag = pos + d;
        const int j0 = std::clamp((int)lag, 0, v.total - 3);
        const float later = buf[j0] + (buf[j0 + 1] - buf[j0]) * (float)(lag - j0);
        figs[0][k][0] = now; figs[0][k][1] = later;
        pk = std::max(pk, std::max(std::fabs(now), std::fabs(later)));
    }
    const float want = 80.0f / std::max(pk, 0.05f); // grows slowly, shrinks at once: a fading note stays honest
    figScale = want < figScale || figScale <= 0 ? want : figScale + (want - figScale) * 0.04f;
}

// The lead sound's second oscillator against the first, as set in the patch (not measured).
QString Scope::ratioText() const {
    const Patch &p = sh.soundShelf().lead;
    const double r = std::pow(2.0, (p.p[P_SEMI2] + p.p[P_DETUNE] / 100.0) / 12.0);
    for (int q = 1; q <= 4; ++q) {
        const int n = (int)std::lround(r * q);
        if (n >= 1 && n <= 8 && std::fabs(r * q / n - 1) < 0.006) return QString("B:A %1:%2 set").arg(n).arg(q);
    }
    return QString("B:A %1 set").arg(r, 0, 'f', 3);
}

void Scope::paintPhase(QRectF gr, qreal) {
    QPainter &g = *ui.g;
    g.save();
    g.setClipRect(gr.adjusted(1, 1, -1, -1));
    g.translate(gr.center());
    g.setPen(QPen(QColor(0x3a, 0x3a, 0x38), 1, Qt::DotLine));
    g.drawLine(QPointF(-gr.width() / 2 + 2, 0), QPointF(gr.width() / 2 - 2, 0));
    g.drawLine(QPointF(0, -gr.height() / 2 + 2), QPointF(0, gr.height() / 2 - 2));
    g.setRenderHint(QPainter::Antialiasing, true);
    g.setBrush(Qt::NoBrush);
    QPolygonF pts(kFigPts);
    for (int f = figCount - 1; f >= 0; --f) {
        for (int k = 0; k < kFigPts; ++k) pts[k] = QPointF(figs[f][k][0] * figScale, -figs[f][k][1] * figScale);
        if (f > 0) {
            g.setPen(QPen(QColor(255, 255, 255, 120 - 20 * f), 0));
            g.drawPolyline(pts);
        } else {
            g.setPen(Qt::NoPen);
            g.setBrush(QColor(255, 255, 255, 30));
            g.drawPolygon(pts);
            g.setBrush(Qt::NoBrush);
            g.setPen(QPen(QColor(255, 255, 251), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            g.drawPolyline(pts);
        }
    }
    g.restore();
    g.setPen(QColor(0x46, 0x46, 0x44));
    g.setBrush(Qt::NoBrush);
    g.drawRect(gr);
}
