// venv-vrequency — the Freq page's three views, all drawn from the Freq tap (the tones' own output,
// after the warp stage, before the delay): the last 1.5 s as an envelope, two cycles of tone 1
// (trigger: a rising zero crossing) and the spectrum (spectrum.h, the Synth scope's FFT).
// Nothing is drawn that the output did not hold.
#include "app/pages/freq/freq_page.h"
#include "app/pages/synth/spectrum.h"
#include "app/ui/palette.h"
#include "app/ui/zones.h"
#include <QPainter>
#include <QPolygonF>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace td;
using namespace pal;

namespace {
spec::LogTable specTab;
const int kLeft = Qt::AlignLeft | Qt::AlignVCenter, kRight = Qt::AlignRight | Qt::AlignVCenter;
QString ms(double s) { return s < 1 ? QString("%1 ms").arg(s * 1000, 0, 'f', s < 0.01 ? 2 : 1) : QString("%1 s").arg(s, 0, 'f', 2); }
} // namespace

// The 1.5 s envelope: the highest and lowest sample within two periods of the lowest sounding tone
// (5 to 30 ms) around each pixel column, so a steady sound draws as a steady band and beating as
// swells; filled, with its edges in full ink.
void FreqPage::paintScope(QRectF r) {
    QPainter &g = *ui.g;
    const int n = (int)(1.5 * inst.sampleRate());
    const float *x = buf.data() + buf.size() - n;
    ui.frame(r, LOW, BG);
    const QRectF in = r.adjusted(8, 26, -8, -20);
    const int W = (int)in.width();
    float lowHz = kFreqHzMax;
    for (const FreqTone &t : u.freq.tone) if (t.on && t.level > 0) lowHz = std::min(lowHz, t.hz);
    const float span = std::clamp(2.0f / lowHz, 0.005f, 0.03f) * inst.sampleRate();
    const int k = std::max(1, (int)std::ceil(span * W / n)); // columns in the window
    std::vector<float> hi(W, 0.0f), lo(W, 0.0f);
    peak = 0;
    for (int c = 0; c < W; ++c) {
        const int a = (int)((long long)c * n / W), b = (int)((long long)(c + 1) * n / W);
        for (int i = a; i < b; ++i) { lo[c] = std::min(lo[c], x[i]); hi[c] = std::max(hi[c], x[i]); }
        peak = std::max(peak, std::max(hi[c], -lo[c]));
    }
    scale = std::max(peak * 1.1f, 0.05f);
    g.fillRect(QRectF(in.x(), std::floor(in.center().y()), in.width(), 1), GRAT);
    for (int s = 1; s < 3; ++s) // a mark every half second
        g.fillRect(QRectF(std::floor(in.right() - s * in.width() / 3), in.bottom() + 2, 1, 4), LOW);
    const qreal mid = in.center().y(), half = in.height() / 2;
    const QColor band(0x5a, 0x5a, 0x57);
    for (int c = 0; c < W; ++c) {
        float h = 0, l = 0;
        for (int j = std::max(0, c - k / 2); j <= std::min(W - 1, c + k / 2); ++j) { h = std::max(h, hi[j]); l = std::min(l, lo[j]); }
        const qreal y0 = std::floor(mid - h / scale * half), y1 = std::ceil(mid - l / scale * half);
        if (y1 - y0 < 1) continue;
        g.fillRect(QRectF(in.x() + c, y0, 1, y1 - y0), band);
        g.fillRect(QRectF(in.x() + c, y0, 1, 2), INK);
        g.fillRect(QRectF(in.x() + c, y1 - 2, 1, 2), INK);
    }
    const float beat = freqBeat(u.freq);
    ui.txt(QRectF(r.x() + 10, r.y() + 5, 330, 18), u.freqSounding ? "Freq output, last 1.5 s" : "Freq output, last 1.5 s: silent", 13, MID, kLeft);
    ui.txt(QRectF(r.right() - 340, r.y() + 5, 330, 18),
           beat > 0 ? QString("beat %1 Hz (%2 s)").arg(beat, 0, 'f', 2).arg(1.0 / beat, 0, 'f', 2) : QString("no beat"), 13, INK, kRight, true);
    ui.txt(QRectF(r.x() + 10, r.bottom() - 18, 200, 16), "-1.5 s", 11, MID, kLeft);
    ui.txt(QRectF(r.right() - 210, r.bottom() - 18, 200, 16), QString("now   peak %1").arg(peak, 0, 'f', 2), 11, MID, kRight);
    ui.zone(r, zn::freqscope, [](QPointF, int) {});
}

// Two periods of tone 1 from the newest rising zero crossing that leaves room for them.
void FreqPage::paintCycles(QRectF r) {
    QPainter &g = *ui.g;
    ui.frame(r, LOW, BG);
    const float f1 = u.freq.tone[0].hz, sr = inst.sampleRate();
    const int len = std::min((int)std::lround(2 * sr / f1), (int)buf.size() / 2);
    const QRectF in = r.adjusted(6, 20, -6, -4);
    g.fillRect(QRectF(in.x(), std::floor(in.center().y()), in.width(), 1), GRAT);
    int at = -1;
    const int last = (int)buf.size() - len, first = std::max(1, last - 2 * len - 4800);
    for (int i = last; i > first && at < 0; --i) if (buf[i - 1] < 0 && buf[i] >= 0) at = i;
    const bool live = at > 0 && peak > 1e-4f;
    if (live) {
        const int W = (int)in.width(), pts = std::min(W, len);
        QPolygonF line(pts);
        const qreal mid = in.center().y(), half = in.height() / 2;
        for (int k = 0; k < pts; ++k) {
            const float v = buf[at + (int)((long long)k * len / pts)];
            line[k] = QPointF(in.x() + in.width() * k / std::max(1, pts - 1), mid - v / scale * half);
        }
        g.save();
        g.setClipRect(in);
        g.setRenderHint(QPainter::Antialiasing, true);
        g.setPen(QPen(INK, 1.2));
        g.drawPolyline(line);
        g.restore();
    }
    ui.txt(QRectF(r.x() + 8, r.y() + 3, 160, 16), live ? "2 cycles" : "2 cycles: silent", 12, MID, kLeft);
    ui.txt(QRectF(r.right() - 190, r.y() + 3, 182, 16), QString("tone 1  %1").arg(ms(2.0 / f1)), 12, INK, kRight);
    ui.zone(r, zn::freqcycles, [](QPointF, int) {});
}

// The newest 4096 samples, log frequency across, -90 to 0 dB up.
void FreqPage::paintSpectrum(QRectF r) {
    QPainter &g = *ui.g;
    ui.frame(r, LOW, BG);
    const float sr = inst.sampleRate();
    const QRectF in = r.adjusted(6, 20, -6, -4);
    float pk = 0;
    if (peak > 1e-5f && (int)buf.size() >= spec::kN) {
        spec::dbSpectrum(buf.data() + buf.size() - spec::kN, db);
        specTab.make(kSpecW, sr);
        for (int i = 0; i < kSpecW; ++i) {
            const float d = specTab.at(db.data(), i);
            specLine[i] = d > specLine[i] ? d : specLine[i] * 0.6f + d * 0.4f;
        }
        pk = spec::peakHz(db, sr);
    } else {
        for (float &v : specLine) v = spec::kDbLo;
    }
    g.save();
    g.setClipRect(in);
    g.translate(in.topLeft());
    const qreal W = in.width(), H = in.height();
    for (float f : {100.0f, 1000.0f, 10000.0f}) {
        const qreal xx = std::floor(spec::xOfHz(f, W)) + 0.5;
        g.setPen(QPen(GRAT, 1, Qt::DotLine));
        g.drawLine(QPointF(xx, 0), QPointF(xx, H));
        if (f < 10000) ui.txt(QRectF(xx + 2, H - 12, 30, 11), f < 1000 ? "100" : "1K", 11, LOW, kLeft);
        else ui.txt(QRectF(xx - 32, H - 12, 30, 11), "10K", 11, LOW, kRight);
    }
    QPolygonF line(kSpecW);
    for (int i = 0; i < kSpecW; ++i) line[i] = QPointF(W * i / (kSpecW - 1), H * std::clamp(specLine[i] / spec::kDbLo, 0.0f, 1.0f));
    g.setRenderHint(QPainter::Antialiasing, true);
    g.setPen(QPen(INK, 1.2));
    g.drawPolyline(line);
    g.restore();
    ui.txt(QRectF(r.x() + 8, r.y() + 3, 160, 16), "spectrum", 12, MID, kLeft);
    ui.txt(QRectF(r.right() - 190, r.y() + 3, 182, 16), pk > 0 ? QString("peak %1").arg(freqHz(pk)) : QString("silent"), 12, INK, kRight);
    ui.zone(r, zn::freqspectrum, [](QPointF, int) {});
}
