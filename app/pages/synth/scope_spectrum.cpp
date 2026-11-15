// venv-vrequency — the scope's spectrum and waterfall: what frequencies the output is made of.
//
// The FFT and its log axis are shared with the Freq page (spectrum.h). The waterfall keeps one
// spectrum per frame (128 log-spaced bins) in the 3D stack's layers.
#include "app/pages/synth/scope.h"
#include "app/pages/synth/spectrum.h"
#include "app/sounds/sounds.h"
#include "app/ui/palette.h"
#include "audio/audio.h"
#include <QPainter>
#include <QPolygonF>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace td;
using namespace spec;

namespace {
LogTable specTab, fallTab;
const QColor kDim(0x58, 0x58, 0x56), kMid(0x80, 0x80, 0x7d), kInk(255, 255, 251);
} // namespace

QString hzText(float f) {
    return f >= 1000 ? QString("%1kHz").arg(f / 1000, 0, 'f', 2) : QString("%1Hz").arg(f, 0, 'f', f < 100 ? 1 : 0);
}

void Scope::computeSpectrum(const View &v) {
    const float sr = (float)sh.audio.sampleRate();
    dbSpectrum(buf.data() + v.total - kFft, db);
    specTab.make(kSpecW, sr);
    for (int i = 0; i < kSpecW; ++i) {
        const float d = specTab.at(db.data(), i);
        specLine[i] = d > specLine[i] ? d : specLine[i] * 0.75f + d * 0.25f; // quick up, easy down
    }
    peakHz = spec::peakHz(db, sr, &peakDb); // the strongest peak, with a parabola for a finer frequency
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
