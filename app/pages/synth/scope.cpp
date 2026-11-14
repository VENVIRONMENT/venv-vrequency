// venv-vrequency — the oscilloscope on the play page. It shows the instrument's real
// output: triggered so the wave stands still, with its own time and voltage
// scales (they follow the signal automatically and never affect the pitch),
// measured frequency and level, and a fading white-phosphor trace. Touching it
// is the X-Y sound control.
#include "app/shell/mainwindow.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <cstring>

using namespace td;
using namespace pal;

namespace {
const int TB_COUNT = 7, VD_COUNT = 6, DIVS_X = 14, DIVS_Y = 4;
const float TB_MS[TB_COUNT] = {0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f}; // per division
const float VD_V[VD_COUNT] = {0.05f, 0.1f, 0.2f, 0.5f, 1.0f, 2.0f};
QString tbText(int i) { return TB_MS[i] < 1 ? QString("%1us/").arg((int)(TB_MS[i] * 1000)) : QString("%1ms/").arg(TB_MS[i], 0, 'f', 2); }
QString vdText(int i) { return VD_V[i] < 1 ? QString("%1mV/").arg((int)std::round(VD_V[i] * 1000)) : QString("%1V/").arg(VD_V[i], 0, 'f', 2); }
QString noteText(float midi) {
    int n = (int)std::lround(midi);
    return QString("%1%2").arg(noteName(n)).arg(n / 12 - 1);
}

// ---- the trace, drawn by hand ----
//
// The wave is a line that never goes backwards, so in every pixel column it covers one
// vertical span. Stroking it is then a matter of distance to those spans, which costs a
// fraction of a millisecond; the general path stroker took 30 ms a stroke on a busy wave.
const int TW = 644, TH = 184; // the trace area, in pixels

struct Trace {
    float lo[TW + 1], hi[TW + 1];
    bool any = false;
    float px = 0, py = 0;
    Trace() { for (int i = 0; i <= TW; ++i) { lo[i] = 1e9f; hi[i] = -1e9f; } }
    void span(int x, float a, float b) {
        if (x < 0 || x > TW) return;
        lo[x] = std::min(lo[x], std::min(a, b)); hi[x] = std::max(hi[x], std::max(a, b));
    }
    void to(float x, float y) { // the next point: in the same column, or the one after
        if (any) {
            const int x0 = (int)px, x1 = (int)x;
            if (x0 == x1) span(x0, py, y);
            else { const float mid = (py + y) * 0.5f; span(x0, py, mid); span(x1, mid, y); }
        }
        any = true; px = x; py = y;
    }
};

// Antialiased coverage (0..255) of a stroke `width` pixels wide along the trace.
void traceCoverage(const Trace &t, float width, uchar *plane) {
    std::memset(plane, 0, (size_t)TW * TH);
    const float reach = width * 0.5f + 0.5f; // coverage falls to nothing this far from the line
    for (int x = 0; x <= TW; ++x) {
        if (t.lo[x] > t.hi[x]) continue;
        const float lo = t.lo[x], hi = t.hi[x];
        const int r0 = std::max(0, (int)std::floor(lo - reach)), r1 = std::min(TH - 1, (int)std::floor(hi + reach));
        for (int c = std::max(0, (int)std::floor(x - reach)); c <= std::min(TW - 1, (int)std::floor(x + reach)); ++c) {
            const float dx = std::fabs(c + 0.5f - x);
            if (dx >= reach) continue;
            const int inside = std::min(255, (int)((reach - dx) * 255.0f));
            uchar *col = plane + c;
            for (int r = r0; r <= r1; ++r) {
                const float yc = r + 0.5f, dy = yc < lo ? lo - yc : (yc > hi ? yc - hi : 0.0f);
                int v = inside;
                if (dy > 0) {
                    const float d = std::sqrt(dx * dx + dy * dy);
                    if (d >= reach) continue;
                    v = std::min(255, (int)((reach - d) * 255.0f));
                }
                uchar &o = col[(size_t)r * TW];
                if (v > o) o = (uchar)v;
            }
        }
    }
}

// Paints a colour through the coverage onto an opaque RGB32 image (plain "source over").
void traceBlend(QImage &img, const uchar *plane, QColor colour, int alpha) {
    const int cr = colour.red(), cg = colour.green(), cb = colour.blue();
    for (int y = 0; y < TH; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        const uchar *cov = plane + (size_t)y * TW;
        for (int x = 0; x < TW; ++x) {
            if (!cov[x]) continue;
            const int a = cov[x] * alpha / 255, ia = 255 - a;
            const QRgb d = line[x];
            line[x] = qRgb((qRed(d) * ia + cr * a) / 255, (qGreen(d) * ia + cg * a) / 255, (qBlue(d) * ia + cb * a) / 255);
        }
    }
}

// The same, as a see-through image to lay over something else.
void traceOverlay(QImage &img, const uchar *plane, QColor colour, int alpha) {
    const int cr = colour.red(), cg = colour.green(), cb = colour.blue();
    for (int y = 0; y < TH; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        const uchar *cov = plane + (size_t)y * TW;
        for (int x = 0; x < TW; ++x) {
            const int a = cov[x] * alpha / 255;
            line[x] = qRgba(cr * a / 255, cg * a / 255, cb * a / 255, a); // premultiplied
        }
    }
}
} // namespace

void MainWindow::drawScope(QRectF xy) {
    const int GW = TW, GH = TH;
    const qreal div = 46;
    const QRectF GR(xy.x() + 22, xy.y() + 21, GW, GH);
    const int L = Qt::AlignLeft | Qt::AlignVCenter;
    frame(xy, downNames.contains("xy") ? INK : LOW, BG);

    // ---- graticule (drawn once): dotted grid, ticks on the centre axes ----
    if (gratPix.isNull()) {
        gratPix = QPixmap(GW + 1, GH + 1);
        gratPix.fill(Qt::transparent);
        QPainter q(&gratPix);
        q.setPen(QColor(0x58, 0x58, 0x56));
        for (int i = 0; i <= DIVS_X; ++i)
            for (qreal y = 0; y <= GH; y += div / 5) q.drawPoint(QPointF(i * div, std::floor(y)));
        for (int j = 0; j <= DIVS_Y; ++j)
            for (qreal x = 0; x <= GW; x += div / 5) q.drawPoint(QPointF(std::floor(x), j * div));
        q.setPen(QColor(0x80, 0x80, 0x7d));
        for (qreal x = 0; x <= GW; x += div / 5) q.drawLine(QPointF(std::floor(x), GH / 2 - 2), QPointF(std::floor(x), GH / 2 + 2));
        for (qreal y = 0; y <= GH; y += div / 5) q.drawLine(QPointF(GW / 2 - 2, std::floor(y)), QPointF(GW / 2 + 2, std::floor(y)));
        q.setPen(QColor(0x46, 0x46, 0x44));
        q.drawRect(0, 0, GW, GH);
    }
    g->drawPixmap(GR.topLeft(), gratPix);

    // ---- acquire the newest samples ----
    static float buf[kScopeSize];
    const float sr = (float)audio.sampleRate();
    int need = qBound(64, (int)(TB_MS[tbIdx] * DIVS_X * sr / 1000.0f), kScopeSize - 2048);
    int total = qMin((int)kScopeSize, need + 6000);
    inst.readScope(buf, total);
    float lo = 1e9f, hi = -1e9f;
    for (int i = total - need; i < total; ++i) { lo = qMin(lo, buf[i]); hi = qMax(hi, buf[i]); }
    float span = hi - lo;

    // ---- frequency: autocorrelation of the last 85 ms (decimated by four). Counting zero
    // crossings misreads any wave with more than one wiggle per cycle. ----
    measFreq = 0;
    float period = 0; // samples
    if (span > 0.004f) {
        static float d[1024];
        const float *x = buf + total - 4096;
        float mean = 0;
        for (int i = 0; i < 1024; ++i) { d[i] = (x[4 * i] + x[4 * i + 1] + x[4 * i + 2] + x[4 * i + 3]) * 0.25f; mean += d[i]; }
        mean /= 1024;
        for (int i = 0; i < 1024; ++i) d[i] -= mean;
        static float ac[402];
        float r0 = 1e-12f;
        for (int i = 0; i < 1024; ++i) r0 += d[i] * d[i];
        float best = 0;
        for (int lag = 5; lag <= 401; ++lag) {
            float a = 0;
            const int n = 1024 - lag;
            for (int i = 0; i < n; ++i) a += d[i] * d[i + lag];
            ac[lag] = a * 1024.0f / (n * r0); // unbiased, normalised
            if (lag >= 6 && lag <= 400 && ac[lag] > best) best = ac[lag];
        }
        if (best > 0.3f) {
            for (int lag = 6; lag <= 400; ++lag) // the shortest lag that is nearly as good: the true period, not a multiple
                if (ac[lag] >= 0.92f * best && ac[lag] >= ac[lag - 1] && ac[lag] >= ac[lag + 1]) {
                    float den = ac[lag - 1] - 2 * ac[lag] + ac[lag + 1];
                    float frac = std::fabs(den) > 1e-9f ? 0.5f * (ac[lag - 1] - ac[lag + 1]) / den : 0;
                    period = (lag + qBound(-0.5f, frac, 0.5f)) * 4.0f;
                    measFreq = sr / period;
                    break;
                }
        }
    }

    // ---- trigger: rising zero crossing after a deep negative swing, at most once per cycle ----
    float hyst = qMax(0.003f, -lo * 0.55f);
    int trig = -1, lastX = -100000;
    bool armed = false;
    for (int i = 1; i < total; ++i) {
        if (buf[i] < -hyst) armed = true;
        else if (armed && buf[i - 1] < 0 && buf[i] >= 0) {
            armed = false;
            if (i - lastX < period * 0.8f) continue; // hold-off
            lastX = i;
            if (i >= need / 2 && i <= total - need / 2) trig = i;
        }
    }
    trigd = trig >= 0 && span > 0.004f;
    int start = trigd ? trig - need / 2 : total - need;
    lo = 1e9f; hi = -1e9f;
    for (int i = start; i < start + need; ++i) { lo = qMin(lo, buf[i]); hi = qMax(hi, buf[i]); }
    measPk = qMax(0.0f, hi - lo);

    // ---- auto-range: keep four to nine cycles and a comfortable height on screen. The scales
    // step only after the picture has been wrong for a third of a second, so slides do not flicker. ----
    if (measFreq > 0 && period > 0) {
        float cycles = need / period;
        if (cycles < 2.5f || cycles > 12.0f) {
            if (++rangeT > 8) {
                float want = 5.5f * 1000.0f / measFreq / DIVS_X; // ms per division
                int best = 0;
                for (int i = 1; i < TB_COUNT; ++i)
                    if (std::fabs(std::log(TB_MS[i] / want)) < std::fabs(std::log(TB_MS[best] / want))) best = i;
                tbIdx = best; rangeT = 0;
            }
        } else rangeT = 0;
    }
    if (span > 0.004f) {
        float pkDiv = measPk * 0.5f / VD_V[vdIdx];
        if (pkDiv > 1.9f || pkDiv < 0.55f) {
            if (++rangeV > 8) {
                vdIdx = VD_COUNT - 1;
                for (int i = 0; i < VD_COUNT; ++i)
                    if (measPk * 0.5f <= VD_V[i] * 1.6f) { vdIdx = i; break; }
                rangeV = 0;
            }
        } else rangeV = 0;
    }

    // ---- phosphor: the old trace fades, the new one is written on top ----
    if (phos.size() != QSize(GW, GH)) { phos = QImage(GW, GH, QImage::Format_RGB32); phos.fill(Qt::black); }
    // Glitch: while the crusher is on, old traces are not simply faded: they are pushed along a
    // diagonal, and each new trace is written in the next colour round the wheel. The result is
    // many coloured outlines at once, streaked like a long exposure. The newest trace is drawn
    // white on top, so the wave on screen is still the wave you hear.
    glitchVis += ((u.glitch ? 1.0f : 0.0f) - glitchVis) * (u.glitch ? 0.5f : 0.2f);
    const bool gl = glitchVis > 0.03f;
    static uchar cover[TW * TH];
    Trace tr;
    {
        if (gl) {
            QImage prev = phos.copy();
            QPainter q0(&phos);
            q0.fillRect(phos.rect(), Qt::black);
            q0.setOpacity(u.glitch ? 0.87 + 0.10 * u.glitchY : 0.6);
            const int step = 4 + (int)(7 * u.glitchX);
            q0.drawImage(QPoint(step, (step * 2 + 1) / 3), prev);
        } else {
            QPainter q(&phos);
            q.fillRect(phos.rect(), QColor(0, 0, 0, 112));
        }
        const float vdiv = VD_V[vdIdx];
        auto Y = [&](float v) { return (float)(GH / 2.0 - qBound(-2.1f, v / vdiv, 2.1f) * div); };
        double per = (double)need / GW; // samples per pixel column
        if (per <= 1.5) {
            for (int x = 0; x <= GW; ++x) {
                double pos = start + x * per;
                int i0 = qBound(0, (int)pos, total - 2);
                float fr = (float)(pos - i0);
                tr.to(x, Y(buf[i0] + (buf[i0 + 1] - buf[i0]) * fr));
            }
        } else { // peak detect: each column spans from its lowest to its highest sample
            for (int x = 0; x < GW; ++x) {
                int a = start + (int)(x * per), b = qMin(total, start + (int)((x + 1) * per) + 1);
                float mn = buf[a], mx = buf[a];
                for (int i = a + 1; i < b; ++i) { mn = qMin(mn, buf[i]); mx = qMax(mx, buf[i]); }
                bool up = buf[b - 1] >= buf[a];
                tr.to(x, Y(up ? mn : mx));
                tr.to(x, Y(up ? mx : mn));
            }
        }
        if (gl) {
            glitchHue = std::fmod(glitchHue + 0.083f, 1.0f);
            const QColor c = QColor::fromHsvF(glitchHue, qBound(0.0f, glitchVis, 1.0f), 1.0f);
            traceCoverage(tr, 6.0f, cover);
            traceBlend(phos, cover, c, 70); // bloom, in this frame's colour
            traceCoverage(tr, 2.0f, cover);
            traceBlend(phos, cover, c, 255);
        } else {
            traceCoverage(tr, 7.0f, cover);
            traceBlend(phos, cover, QColor(255, 255, 255), 46); // bloom
            traceCoverage(tr, 2.0f, cover);
            traceBlend(phos, cover, QColor(0xff, 0xff, 0xfb), 255);
        }
    }
    g->setCompositionMode(QPainter::CompositionMode_Plus);
    g->drawImage(GR.topLeft(), phos);
    g->setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (gl) { // the present wave, white, over its own coloured history
        static QImage top;
        if (top.size() != QSize(GW, GH)) top = QImage(GW, GH, QImage::Format_ARGB32_Premultiplied);
        traceCoverage(tr, 1.6f, cover);
        traceOverlay(top, cover, QColor(255, 255, 251), (int)(255 * qBound(0.0f, glitchVis, 1.0f)));
        g->drawImage(GR.topLeft(), top);
    }

    // ---- markers: trigger point (top centre), channel ground (left margin) ----
    g->setRenderHint(QPainter::Antialiasing, true);
    g->setPen(Qt::NoPen);
    g->setBrush(INK);
    QPainterPath tm;
    tm.moveTo(GR.center().x() - 5, GR.y() + 1); tm.lineTo(GR.center().x() + 5, GR.y() + 1); tm.lineTo(GR.center().x(), GR.y() + 8); tm.closeSubpath();
    g->drawPath(tm);
    QPainterPath gm;
    qreal gy = GR.center().y();
    gm.moveTo(xy.x() + 4, gy - 7); gm.lineTo(xy.x() + 15, gy - 7); gm.lineTo(xy.x() + 21, gy); gm.lineTo(xy.x() + 15, gy + 7); gm.lineTo(xy.x() + 4, gy + 7); gm.closeSubpath();
    g->drawPath(gm);
    g->setRenderHint(QPainter::Antialiasing, false);
    txt(QRectF(xy.x() + 4, gy - 7, 12, 14), "1", 11, BG, Qt::AlignCenter, true);

    // ---- X-Y cursors (touch anywhere on the scope) ----
    const QString xn = nice(xyAxisName(u.set.xyMap, false)), yn = nice(xyAxisName(u.set.xyMap, true));
    bool xyActive = u.xyTouch || u.set.xyHold;
    if (xyActive) {
        QPointF c(std::floor(xy.x() + xy.width() * u.xyX), std::floor(xy.bottom() - xy.height() * u.xyY));
        c.setX(qBound(GR.x(), c.x(), GR.right()));
        c.setY(qBound(GR.y(), c.y(), GR.bottom()));
        g->setPen(QPen(u.xyTouch ? INK : MID, 1, Qt::DashLine));
        g->drawLine(QPointF(c.x(), GR.y()), QPointF(c.x(), GR.bottom()));
        g->drawLine(QPointF(GR.x(), c.y()), QPointF(GR.right(), c.y()));
        frame(QRectF(c.x() - 7, c.y() - 7, 15, 15), INK, u.xyTouch ? INK : BG);
    }
    // axis legends sit on the graticule like the scope's own annotations
    g->fillRect(QRectF(GR.x() + 6, GR.y() + 12, (yn.size() + 3) * chW(12) + 4, 15), BG);
    txt(QRectF(GR.x() + 8, GR.y() + 12, 300, 15), QString::fromUtf8("Y↑ %1").arg(yn), 12, MID, L);
    g->fillRect(QRectF(GR.x() + 6, GR.bottom() - 19, (xn.size() + 3) * chW(12) + 4, 15), BG);
    txt(QRectF(GR.x() + 8, GR.bottom() - 19, 300, 15), QString::fromUtf8("X→ %1").arg(xn), 12, MID, L);

    // ---- status line (inverse video), left of the two keys ----
    QRectF band(xy.x() + 1, xy.y() + 1, xy.width() - 206, 18);
    g->fillRect(band, INK);
    g->fillRect(QRectF(band.x() + 3, band.y() + 2, 14, 14), BG);
    txt(QRectF(band.x() + 3, band.y() + 2, 14, 14), "1", 12, INK, Qt::AlignCenter, true);
    txt(QRectF(band.x() + 24, band.y(), 80, 18), vdText(vdIdx), 13, BG, L, true);
    txt(QRectF(band.x() + 104, band.y(), 90, 18), tbText(tbIdx), 13, BG, L, true);
    QString trg = !audio.streaming() ? "Stop" : (trigd ? "Trig'd" : "Auto");
    txt(QRectF(band.x() + 200, band.y(), 70, 18), trg, 13, BG, L, true);
    txt(QRectF(band.x() + 270, band.y(), band.width() - 276, 18), QString("%1 kSa/s").arg(audio.sampleRate() / 1000), 13, BG, Qt::AlignRight | Qt::AlignVCenter, true);

    // ---- measurement line ----
    QRectF ml(xy.x() + 8, GR.bottom() + 2, xy.width() - 16, xy.bottom() - GR.bottom() - 3);
    QString fq = measFreq > 0 ? (measFreq >= 1000 ? QString("%1kHz").arg(measFreq / 1000, 0, 'f', 3) : QString("%1Hz").arg(measFreq, 0, 'f', 1)) : QString("----");
    QString pk = measPk >= 1 ? QString("%1V").arg(measPk, 0, 'f', 2) : QString("%1mV").arg((int)std::round(measPk * 1000));
    QString nt = "--";
    for (int r = kStrings - 1; r >= 0; --r)
        if (u.str[r].gate) { nt = noteText(u.str[r].note + u.bend); break; }
    txt(ml, QString("Freq(1): %1").arg(fq), 13, INK, L);
    txt(ml.adjusted(170, 0, 0, 0), QString("Pk-Pk(1): %1").arg(pk), 13, INK, L);
    txt(ml.adjusted(340, 0, 0, 0), QString("Note: %1").arg(nt), 13, INK, L);
    if (xyActive)
        txt(ml, QString("X %1%  Y %2%").arg((int)(u.xyX * 100)).arg((int)(u.xyY * 100)), 13, INK, Qt::AlignRight | Qt::AlignVCenter);
}
