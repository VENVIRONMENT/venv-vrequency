// venv-vrequency — the scope's trace: strokes the wave by hand and keeps the fading phosphor image.
//
// The wave is a line that never goes backwards, so in every pixel column it covers one
// vertical span. Stroking it is then a matter of distance to those spans, which costs a
// fraction of a millisecond; the general path stroker took 30 ms a stroke on a busy wave.
#include "app/pages/synth/scope.h"
#include "app/ui/ui.h"
#include <QPainter>
#include <cmath>
#include <cstring>

using namespace td;

namespace {
const int TW = Scope::kTraceW, TH = Scope::kTraceH;

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

// The old trace fades, the new one is written on top.
void Scope::paintTrace(const View &v, QRectF gr, qreal div) {
    const int GW = TW, GH = TH;
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
        const float vdiv = kVoltsPerDiv[vdIdx];
        auto Y = [&](float val) { return (float)(GH / 2.0 - qBound(-2.1f, val / vdiv, 2.1f) * div); };
        double per = (double)v.need / GW; // samples per pixel column
        if (per <= 1.5) {
            for (int x = 0; x <= GW; ++x) {
                double pos = v.start + x * per;
                int i0 = qBound(0, (int)pos, v.total - 2);
                float fr = (float)(pos - i0);
                tr.to(x, Y(buf[i0] + (buf[i0 + 1] - buf[i0]) * fr));
            }
        } else { // peak detect: each column spans from its lowest to its highest sample
            for (int x = 0; x < GW; ++x) {
                int a = v.start + (int)(x * per), b = qMin(v.total, v.start + (int)((x + 1) * per) + 1);
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
    ui.g->setCompositionMode(QPainter::CompositionMode_Plus);
    ui.g->drawImage(gr.topLeft(), phos);
    ui.g->setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (gl) { // the present wave, white, over its own coloured history
        static QImage top;
        if (top.size() != QSize(GW, GH)) top = QImage(GW, GH, QImage::Format_ARGB32_Premultiplied);
        traceCoverage(tr, 1.6f, cover);
        traceOverlay(top, cover, QColor(255, 255, 251), (int)(255 * qBound(0.0f, glitchVis, 1.0f)));
        ui.g->drawImage(gr.topLeft(), top);
    }
}
