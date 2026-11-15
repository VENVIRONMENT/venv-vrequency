// venv-vrequency — the filter and envelope settings pictures (see sound_pictures.h).
#include "app/pages/sound/sound_pictures.h"
#include "app/pages/sound/sound_pictures_draw.h"
#include "core/dsp.h"
#include <cmath>
#include <cstring>

using namespace td;
using namespace pal;
using namespace picdraw;

namespace {
QString ms(float s) { return s < 1 ? QString("%1MS").arg((int)std::lround(s * 1000)) : QString("%1S").arg(s, 0, 'f', 1); }
}

void SoundPictures::filter(QRectF r, const float *p) {
    static const int ids[] = {P_FMODE, P_CUTOFF, P_RESO, P_FENV};
    if (!fltOk || stale(p, fltKey, ids, 4)) { filterCurve(p, flt); std::memcpy(fltKey, p, sizeof fltKey); fltOk = true; }
    static const char *mode[3] = {"LOW-PASS", "HIGH-PASS", "BAND-PASS"};
    const float c = p[P_CUTOFF];
    QString hz = c >= 1000 ? QString("%1 KHZ").arg(c / 1000, 0, 'f', 1) : QString("%1 HZ").arg((int)c);
    QRectF a = panel(r, QString("FILTER  %1").arg(mode[qBound(0, (int)p[P_FMODE], 2)]),
                     QString("%1  %2%").arg(hz).arg((int)std::lround(p[P_RESO] / 0.95f * 100)));
    ui.zone(r, "picfilter", [](QPointF, int) {});
    Smooth sm(ui.g);
    const float top = 24.0f, bot = -48.0f;                       // dB at the top and the bottom of the plot
    const qreal lg = std::log(kFreqHi / kFreqLo);
    auto xOfHz = [&](float f) { return a.x() + a.width() * std::log(f / kFreqLo) / lg; };
    auto yOfDb = [&](float d) { return a.y() + a.height() * (1 - (clampf(d, bot, top) - bot) / (top - bot)); };
    ui.g->setPen(dashed(grey(60), 1.0, 1, 3));
    for (float f : {100.0f, 1000.0f, 10000.0f}) ui.g->drawLine(QPointF(xOfHz(f), a.y()), QPointF(xOfHz(f), a.bottom()));
    ui.g->drawLine(QPointF(a.x(), yOfDb(0)), QPointF(a.right(), yOfDb(0)));        // 0 dB
    auto X = [&](int i) { return xOfHz(freqAt(i, FilterCurve::N)); };
    auto Y = [&](int i) { return yOfDb(flt.db[i]); };
    auto Ye = [&](int i) { return yOfDb(flt.dbEnv[i]); };
    ui.g->setPen(Qt::NoPen);
    ui.g->setBrush(grey(48));
    ui.g->drawPath(line(FilterCurve::N + 1, X, Y, true, a.bottom()));
    ui.g->setBrush(Qt::NoBrush);
    if (flt.envOn) {
        ui.g->setPen(dashed(grey(150), 1.1, 3, 3));
        ui.g->drawPath(line(FilterCurve::N + 1, X, Ye));
    }
    ui.g->setPen(pen(INK, 1.5));
    ui.g->drawPath(line(FilterCurve::N + 1, X, Y));
    dot(ui.g, QPointF(xOfHz(flt.cutoff), yOfDb(flt.cutDb)), 3.2);
}

void SoundPictures::envelope(QRectF r, const float *p) {
    static const int ids[] = {P_ATK, P_DEC, P_SUS, P_REL};
    if (!envOk || stale(p, envKey, ids, 4)) { envCurve(p, env); std::memcpy(envKey, p, sizeof envKey); envOk = true; }
    QRectF a = panel(r, "ENVELOPE", QString("A %1  R %2").arg(ms(p[P_ATK]), ms(p[P_REL])));
    ui.zone(r, "picenv", [](QPointF, int) {});
    Smooth sm(ui.g);
    a.adjust(0, 4, 0, 0);
    auto X = [&](int i) { return a.x() + a.width() * i / EnvCurve::N; };
    auto Y = [&](int i) { return a.bottom() - env.y[i] * a.height(); };
    auto XofX = [&](float x) { return a.x() + a.width() * x; };
    ui.g->setPen(dashed(grey(80), 1.0, 1, 3));
    for (float x : {env.xA(), env.xD(), env.xH()}) ui.g->drawLine(QPointF(XofX(x), a.y()), QPointF(XofX(x), a.bottom()));
    ui.g->setPen(Qt::NoPen);
    ui.g->setBrush(grey(48));
    ui.g->drawPath(line(EnvCurve::N + 1, X, Y, true, a.bottom()));
    ui.g->setBrush(Qt::NoBrush);
    ui.g->setPen(pen(INK, 1.5));
    ui.g->drawPath(line(EnvCurve::N + 1, X, Y));
    // Where the note is now. The engine does not publish the stage of its voices, so this is the
    // time since the gate was seen to open (or close), laid on the curve: it assumes the key was
    // held through the decay before it was let go.
    const double now = tNow;
    const float held = env.tA + env.tD + env.tH;
    float t = -1;
    if (gate) t = qMin((float)(now - tOn), held);
    else if (tOff >= 0) { t = held + (float)(now - tOff); if (t > env.total) t = -1; }
    if (t >= 0) { float x = env.xOf(t); dot(ui.g, QPointF(XofX(x), a.bottom() - env.yOf(x) * a.height()), 3.2); }
}
