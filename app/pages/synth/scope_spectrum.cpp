// venv-vrequency — the scope's spectrum and waterfall: what frequencies the output is made of.
//
// The newest 4096 real output samples get a Hann window and a radix-2 FFT. The magnitudes become
// dB (a full-scale sine is 0 dB) and are drawn on a log frequency axis, 30 Hz to 16 kHz. The
// waterfall keeps one such spectrum per frame (128 log-spaced bins) in the 3D stack's layers.
#include "app/pages/synth/scope.h"
#include "app/sounds/sounds.h"
#include "app/ui/palette.h"
#include "audio/audio.h"
#include <QPainter>
#include <QPolygonF>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace td;

namespace {
constexpr float kLoHz = 30, kHiHz = 16000, kDbLo = -90;

// The FFT's tables, made once: bit reversal, twiddles, Hann window.
struct Fft {
    static constexpr int N = Scope::kFft;
    uint16_t rev[N];
    float cs[N / 2], sn[N / 2], win[N];
    Fft() {
        for (int i = 0; i < N; ++i) {
            int r = 0;
            for (int b = 0; (1 << b) < N; ++b) r |= ((i >> b) & 1) << (11 - b);
            rev[i] = (uint16_t)r;
            win[i] = 0.5f - 0.5f * std::cos(6.2831853f * i / N);
        }
        for (int i = 0; i < N / 2; ++i) { cs[i] = std::cos(6.2831853f * i / N); sn[i] = -std::sin(6.2831853f * i / N); }
    }
    void run(const float *x, float *re, float *im) const { // x: N samples, in; re/im: N, out
        for (int i = 0; i < N; ++i) { re[rev[i]] = x[i] * win[i]; im[rev[i]] = 0; }
        for (int len = 2; len <= N; len <<= 1) {
            const int half = len / 2, step = N / len;
            for (int a = 0; a < N; a += len)
                for (int k = 0; k < half; ++k) {
                    const float wr = cs[k * step], wi = sn[k * step];
                    const int p = a + k, q = p + half;
                    const float tr = re[q] * wr - im[q] * wi, ti = re[q] * wi + im[q] * wr;
                    re[q] = re[p] - tr; im[q] = im[p] - ti; re[p] += tr; im[p] += ti;
                }
        }
    }
};

// For n log-spaced points: the FFT bin range (fractional) each one covers. Cached per sample rate.
struct Table {
    int n = 0; float sr = 0; std::vector<float> lo, hi;
    void make(int count, float rate) {
        if (n == count && sr == rate) return;
        n = count; sr = rate; lo.resize(n); hi.resize(n);
        const float bw = rate / Scope::kFft, r = std::pow(kHiHz / kLoHz, 1.0f / (n - 1));
        for (int i = 0; i < n; ++i) {
            const float f = kLoHz * std::pow(r, (float)i);
            lo[i] = f / std::sqrt(r) / bw; hi[i] = f * std::sqrt(r) / bw;
        }
    }
    float at(const float *dbv, int i) const { // the loudest bin inside the point, or the line between two bins
        if (hi[i] - lo[i] < 1.0f) {
            const float c = 0.5f * (lo[i] + hi[i]);
            const int b = (int)c;
            return dbv[b] + (dbv[b + 1] - dbv[b]) * (c - b);
        }
        float m = kDbLo;
        for (int b = (int)std::ceil(lo[i]); b <= (int)hi[i]; ++b) m = std::max(m, dbv[b]);
        return m;
    }
};
Table specTab, fallTab;

qreal xOfHz(float f, qreal w) { return w * std::log(f / kLoHz) / std::log(kHiHz / kLoHz); }
const QColor kDim(0x58, 0x58, 0x56), kMid(0x80, 0x80, 0x7d), kInk(255, 255, 251);
} // namespace

QString hzText(float f) {
    return f >= 1000 ? QString("%1kHz").arg(f / 1000, 0, 'f', 2) : QString("%1Hz").arg(f, 0, 'f', f < 100 ? 1 : 0);
}

void Scope::computeSpectrum(const View &v) {
    static const Fft fft;
    static float re[kFft], im[kFft];
    const float sr = (float)sh.audio.sampleRate();
    db.resize(kFft / 2 + 2);
    fft.run(buf.data() + v.total - kFft, re, im);
    for (int i = 0; i <= kFft / 2; ++i) // a full-scale sine reads kFft/4 through the window
        db[i] = std::max(kDbLo, 20.0f * std::log10(std::sqrt(re[i] * re[i] + im[i] * im[i]) * (4.0f / kFft) + 1e-6f));
    db[kFft / 2 + 1] = db[kFft / 2];
    specTab.make(kSpecW, sr);
    for (int i = 0; i < kSpecW; ++i) {
        const float d = specTab.at(db.data(), i);
        specLine[i] = d > specLine[i] ? d : specLine[i] * 0.75f + d * 0.25f; // quick up, easy down
    }
    const int b0 = std::max(2, (int)std::ceil(kLoHz * kFft / sr)), b1 = std::min(kFft / 2 - 1, (int)(kHiHz * kFft / sr));
    int best = b0; // the strongest peak, with a parabola through its neighbours for a finer frequency
    for (int b = b0; b <= b1; ++b) if (db[b] > db[best]) best = b;
    peakDb = db[best]; peakHz = 0;
    if (peakDb > -75.0f) {
        const float a = db[best - 1], b = db[best], c = db[best + 1], den = a - 2 * b + c;
        const float off = std::fabs(den) > 1e-6f ? std::clamp(0.5f * (a - c) / den, -0.5f, 0.5f) : 0;
        peakHz = (best + off) * sr / kFft;
    }
}

void Scope::captureSpectrumRow() {
    std::memmove(layers[1], layers[0], sizeof(float) * kPts * (kLayers - 1));
    layerCount = std::min(layerCount + 1, (int)kLayers);
    fallTab.make(kPts, (float)sh.audio.sampleRate());
    for (int k = 0; k < kPts; ++k) layers[0][k] = std::clamp((fallTab.at(db.data(), k) + 90.0f) / 80.0f, 0.0f, 1.0f);
}

void Scope::paintSpectrum(QRectF gr) {
    QPainter &g = *ui.g;
    g.save();
    g.setClipRect(gr.adjusted(1, 1, -1, -1));
    g.translate(gr.topLeft());
    const qreal W = gr.width(), top = 42, bot = gr.height() - 5;
    auto yOf = [&](float d) { return bot - (bot - top) * std::clamp((d - kDbLo) / -kDbLo, 0.0f, 1.0f); };
    g.setPen(QPen(QColor(0x3a, 0x3a, 0x38), 1, Qt::DotLine)); // dB rungs
    for (int d : {-30, -60}) {
        g.drawLine(QPointF(0, yOf((float)d)), QPointF(W, yOf((float)d)));
        ui.txt(QRectF(W - 84, yOf((float)d) - 12, 80, 11), QString("%1dB").arg(d), 11, kDim, Qt::AlignRight | Qt::AlignVCenter);
    }
    for (float f : {100.0f, 1000.0f, 10000.0f}) { // frequency marks
        const qreal x = std::floor(xOfHz(f, W)) + 0.5;
        g.setPen(QPen(kDim, 1, Qt::DotLine));
        g.drawLine(QPointF(x, top), QPointF(x, bot));
        ui.txt(QRectF(x + 2, 29, 40, 11), f < 1000 ? "100" : f < 10000 ? "1K" : "10K", 11, kMid, Qt::AlignLeft | Qt::AlignVCenter);
    }
    const float cutoff = sh.soundShelf().lead.p[P_CUTOFF]; // the filter cutoff of the lead sound
    if (cutoff >= kLoHz && cutoff <= kHiHz) {
        const qreal x = std::floor(xOfHz(cutoff, W)) + 0.5;
        g.setPen(QPen(kInk, 1, Qt::DotLine));
        g.drawLine(QPointF(x, top), QPointF(x, bot));
        ui.txt(QRectF(x + 3, 29, 70, 11), "cutoff", 11, kInk, Qt::AlignLeft | Qt::AlignVCenter);
    }
    QPolygonF pts(kSpecW);
    for (int i = 0; i < kSpecW; ++i) pts[i] = QPointF(W * i / (kSpecW - 1), yOf(specLine[i]));
    QPolygonF fill = pts;
    fill << QPointF(W, bot) << QPointF(0, bot);
    g.setPen(Qt::NoPen);
    g.setBrush(QColor(255, 255, 255, 40));
    g.drawPolygon(fill);
    g.setRenderHint(QPainter::Antialiasing, true);
    g.setBrush(Qt::NoBrush);
    g.setPen(QPen(kInk, 1.5, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin));
    g.drawPolyline(pts);
    g.restore();
    g.setPen(QColor(0x46, 0x46, 0x44));
    g.setBrush(Qt::NoBrush);
    g.drawRect(gr);
}

// Frequency marks under the front row of the waterfall.
void Scope::paintFallLabels(QRectF gr) {
    const qreal x0 = gr.x() + 24, w = 430, y = gr.y() + 124 + 1.2 * 0.85 * 46 + 2;
    for (float f : {1000.0f, 10000.0f}) // (100 would sit under the X legend)
        ui.txt(QRectF(x0 + xOfHz(f, w) - 14, y, 28, 10), f < 10000 ? "1K" : "10K", 11, kMid);
}
