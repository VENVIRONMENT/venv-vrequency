// venv-vrequency — drawing a pulse pad's real sound (see pulse_picture.h).
#include "app/pages/pulse/pulse_picture.h"
#include "app/ui/palette.h"
#include "core/pulse/pulse_voice.h"
#include <QPainter>
#include <QPolygonF>
#include <algorithm>
#include <vector>

using namespace td;

int PulsePicture::windowSamples(const PulsePad &p, float sr) {
    const int len = PulseVoice::lengthSamples(p, sr);
    return p.type == PT_TICK ? std::max(len, 24) : len;
}

const QImage &PulsePicture::get(const PulsePad &p, float rateHz, float sr, QSize size, bool centreLine) {
    if (img.isNull() || !samePulse(p, key) || rateHz != keyRate || sr != keySr || size != keySize || centreLine != keyLine) {
        draw(p, rateHz, sr, size, centreLine);
        key = p; keyRate = rateHz; keySr = sr; keySize = size; keyLine = centreLine;
    }
    return img;
}

void PulsePicture::draw(const PulsePad &p, float rateHz, float sr, QSize size, bool centreLine) {
    const int n = windowSamples(p, sr), w = std::max(2, size.width()), h = std::max(2, size.height());
    std::vector<float> s((size_t)n);
    nOnsets = pulseRender(p, sr, rateHz, s.data(), n);
    nSamples = n;
    img = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter g(&img);
    g.setRenderHint(QPainter::Antialiasing, true);
    const qreal mid = h * 0.5, amp = h * 0.5 - 1.5;
    if (centreLine) { // dotted
        for (int x = 0; x < w; x += 4) g.fillRect(QRectF(x, std::floor(mid), 1, 1), pal::LOW);
    }
    auto Y = [&](float v) { return mid - std::clamp(v, -1.0f, 1.0f) * amp; };
    QPen pen(pal::INK, 1.0);
    pen.setJoinStyle(Qt::RoundJoin);
    QColor fill = pal::INK; fill.setAlpha(56);
    if (n < w) { // fewer samples than pixels: every sample is a step of its own
        QPolygonF line;
        for (int i = 0; i < n; ++i) {
            const qreal x0 = (qreal)i * w / n, x1 = (qreal)(i + 1) * w / n;
            line << QPointF(x0, Y(s[i])) << QPointF(x1, Y(s[i]));
        }
        QPolygonF area = line;
        area << QPointF(w, mid) << QPointF(0, mid);
        g.setPen(Qt::NoPen); g.setBrush(fill); g.drawPolygon(area);
        g.setPen(pen); g.setBrush(Qt::NoBrush); g.drawPolyline(line);
        return;
    }
    // one column a pixel: the highest and lowest sample in it, filled between, outlined on both edges
    QPolygonF top, bot;
    top.reserve(w); bot.reserve(w);
    for (int x = 0; x < w; ++x) {
        const int a = (int)((long long)x * n / w), b = std::max(a + 1, (int)((long long)(x + 1) * n / w));
        float lo = s[a], hi = s[a];
        for (int i = a + 1; i < b && i < n; ++i) { lo = std::min(lo, s[i]); hi = std::max(hi, s[i]); }
        top << QPointF(x + 0.5, Y(hi));
        bot << QPointF(x + 0.5, Y(lo));
    }
    QPolygonF area = top;
    for (int i = bot.size() - 1; i >= 0; --i) area << bot[i];
    g.setPen(Qt::NoPen); g.setBrush(fill); g.drawPolygon(area);
    g.setPen(pen); g.setBrush(Qt::NoBrush);
    g.drawPolyline(top); g.drawPolyline(bot);
}
