// venv-vrequency — the scope's level view: how loud the output has been over the last four
// seconds. Every 10 ms of real output samples gives one peak; the shape is mirrored around the
// centre, newest at the right. The history is advanced by the time that has passed, so it stays
// true when the frame rate drops to a slow tick.
#include "app/pages/synth/scope.h"
#include "audio/audio.h"
#include <QPainter>
#include <QPolygonF>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace td;

namespace {
constexpr float kFloorDb = -54;
float frac(float peak) { return std::clamp((20.0f * std::log10(peak + 1e-6f) - kFloorDb) / -kFloorDb, 0.0f, 1.0f); }
} // namespace

void Scope::updateLevel() {
    const float sr = (float)sh.audio.sampleRate();
    const int binN = std::max(1, (int)(sr * 0.01f));
    const double n = lvlClock.isValid() ? lvlClock.nsecsElapsed() * 1e-9 * sr + lvlCarry : 0;
    lvlClock.restart();
    int count = (int)n;
    lvlCarry = (float)(n - count);
    auto push = [&](float peak) {
        std::memmove(lvl, lvl + 1, sizeof(float) * (kLvl - 1));
        lvl[kLvl - 1] = peak;
    };
    if (count > kScopeSize) { // a longer gap than the buffer holds: the rest was not seen, call it silence
        for (int i = (count - kScopeSize) / binN; i > 0; --i) push(0);
        count = kScopeSize;
    }
    if (count <= 0) return;
    lvlBuf.resize(kScopeSize);
    inst.readScope(lvlBuf.data(), count);
    for (int i = 0; i < count; ++i) {
        lvlAcc = std::max(lvlAcc, std::fabs(lvlBuf[i]));
        if (++lvlAccN >= binN) { push(lvlAcc); lvlAcc = 0; lvlAccN = 0; }
    }
}

void Scope::paintLevel(QRectF gr) {
    QPainter &g = *ui.g;
    g.save();
    g.setClipRect(gr.adjusted(1, 1, -1, -1));
    g.translate(gr.topLeft());
    const qreal W = gr.width(), cy = gr.height() / 2, H = cy - 8, right = W - 8;
    g.setPen(QPen(QColor(0x3a, 0x3a, 0x38), 1, Qt::DotLine));
    for (int s = 1; s <= 3; ++s) { // one line a second back
        const qreal x = std::floor(right - (right + 4) * s / 4.0) + 0.5;
        g.drawLine(QPointF(x, 28), QPointF(x, gr.height() - 4));
        ui.txt(QRectF(x + 2, 29, 30, 11), QString("-%1s").arg(s), 11, QColor(0x58, 0x58, 0x56), Qt::AlignLeft | Qt::AlignVCenter);
    }
    g.setPen(QPen(QColor(0x58, 0x58, 0x56), 1, Qt::DotLine));
    g.drawLine(QPointF(0, cy), QPointF(W, cy));
    constexpr int n = kLvl + 1; // the history, then the bin still filling
    QPolygonF top(n), bot(n);
    float sm[n];
    for (int i = 0; i < kLvl; ++i) sm[i] = frac(lvl[i]);
    sm[kLvl] = frac(lvlAcc);
    for (int i = 0; i < n; ++i) {
        const float a = sm[std::max(0, i - 1)], b = sm[i], c = sm[std::min(n - 1, i + 1)];
        const float h = (a + 2 * b + c) * 0.25f * (float)H;
        const qreal x = right - (right + 4) * (n - 1 - i) / (qreal)(n - 1);
        top[i] = QPointF(x, cy - h); bot[i] = QPointF(x, cy + h);
    }
    QPolygonF shape = top;
    for (int i = n - 1; i >= 0; --i) shape << bot[i];
    g.setPen(Qt::NoPen);
    g.setBrush(QColor(255, 255, 255, 40));
    g.drawPolygon(shape);
    g.setRenderHint(QPainter::Antialiasing, true);
    g.setBrush(Qt::NoBrush);
    g.setPen(QPen(QColor(255, 255, 251), 1.5, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin)); // bevel joins: round ones cost 20 ms here
    g.drawPolyline(top);
    g.drawPolyline(bot);
    g.setRenderHint(QPainter::Antialiasing, false);
    g.setPen(QColor(0xc8, 0xc8, 0xc4)); // "now"
    g.drawLine(QPointF(std::floor(right) + 0.5, 28), QPointF(std::floor(right) + 0.5, gr.height() - 3));
    ui.txt(QRectF(right - 32, 29, 30, 11), "now", 11, QColor(0xc8, 0xc8, 0xc4), Qt::AlignRight | Qt::AlignVCenter);
    g.restore();
    g.setPen(QColor(0x46, 0x46, 0x44));
    g.setBrush(Qt::NoBrush);
    g.drawRect(gr);
}
