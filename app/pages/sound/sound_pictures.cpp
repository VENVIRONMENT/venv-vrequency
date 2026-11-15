// venv-vrequency — the settings pictures: the shared frame, the cache and the oscillator picture
// (see sound_pictures.h). The filter and envelope pictures are in sound_pictures_filter.cpp.
#include "app/pages/sound/sound_pictures.h"
#include "app/pages/sound/sound_pictures_draw.h"
#include <cstring>

using namespace td;
using namespace pal;
using namespace picdraw;

void SoundPictures::note(bool g, double now) {
    if (g && !gate) { tOn = now; tOff = -1; }
    if (!g && gate) tOff = now;
    gate = g; tNow = now;
}

bool SoundPictures::stale(const float *p, const float *key, const int *ids, int n) const {
    for (int i = 0; i < n; ++i) if (p[ids[i]] != key[ids[i]]) return true;
    return false;
}

QRectF SoundPictures::panel(QRectF r, const QString &title, const QString &value) {
    ui.frame(r, GRAT, QColor(0x0a, 0x0a, 0x0a));
    ui.txt(QRectF(r.x() + 8, r.y() + 3, r.width() * 0.5 - 8, 16), title, 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.txt(QRectF(r.x() + r.width() * 0.4, r.y() + 3, r.width() * 0.6 - 8, 16), value, 12, INK, Qt::AlignRight | Qt::AlignVCenter);
    return r.adjusted(8, 24, -8, -8);
}

void SoundPictures::oscillators(QRectF r, const float *p) {
    static const int ids[] = {P_WAVE1, P_WAVE2, P_MIX2, P_SEMI2, P_DETUNE, P_SUB, P_PW, P_FM};
    if (!oscOk || stale(p, oscKey, ids, 8)) { oscCurve(p, osc); std::memcpy(oscKey, p, sizeof oscKey); oscOk = true; }
    QString b = osc.bOn ? QString("B %1 %2%3").arg(waveName((int)p[P_WAVE2])).arg(p[P_SEMI2] > 0 ? "+" : "").arg((int)p[P_SEMI2]) : QString("B OFF");
    if (osc.bOn && (int)p[P_SEMI2] == 0) b = QString("B %1").arg(waveName((int)p[P_WAVE2]));
    QRectF a = panel(r, "OSCILLATORS", QString("A %1  %2").arg(waveName((int)p[P_WAVE1]), b));
    ui.zone(r, "picosc", [](QPointF, int) {});
    Smooth sm(ui.g);
    const qreal cy = a.center().y(), h = a.height() * 0.5 - 2;
    auto X = [&](int i) { return a.x() + a.width() * i / OscCurve::N; };
    auto Ya = [&](int i) { return cy - osc.a[i] / osc.scale * h; };
    auto Yb = [&](int i) { return cy - osc.b[i] / osc.scale * h; };
    ui.g->setPen(dashed(grey(70), 1.0, 1, 3));
    ui.g->drawLine(QPointF(a.x(), cy), QPointF(a.right(), cy));                 // the centre line
    ui.g->setPen(Qt::NoPen);
    ui.g->setBrush(grey(48));
    QPainterPath fill = line(OscCurve::N + 1, X, Ya, true, cy);                 // filled down to the centre line
    ui.g->drawPath(fill);
    ui.g->setBrush(Qt::NoBrush);
    if (osc.bOn) {
        ui.g->setPen(dashed(grey(150), 1.1, 3, 3));
        ui.g->drawPath(line(OscCurve::N + 1, X, Yb));
    }
    ui.g->setPen(pen(INK, 1.5));
    ui.g->drawPath(line(OscCurve::N + 1, X, Ya));
}
