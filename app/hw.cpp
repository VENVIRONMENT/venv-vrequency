#include "hw.h"
#include <QHash>
#include <QPainterPath>
#include <cmath>
#include <functional>

namespace hw {

const QColor CHASSIS(0xd3, 0xd0, 0xc4), CHASSIS_HI(0xf1, 0xef, 0xe6), CHASSIS_LO(0x8f, 0x8b, 0x7f);
const QColor INK(0x3a, 0x39, 0x37), INK_SOFT(0x6c, 0x6a, 0x63);
const QColor BAR_TIME(0x8c, 0x4f, 0x45), BAR_FILTER(0x6d, 0x8d, 0xb6), BAR_MENU(0x9a, 0x96, 0x8a), BAR_RUN(0x5d, 0x93, 0x6c), BAR_SPACE(0xa9, 0x86, 0x4a);
const QColor LAMP_GREEN(0x8d, 0xf0, 0x4a), LAMP_RED(0xff, 0x4a, 0x38), LAMP_AMBER(0xff, 0xb0, 0x30);
const QColor DECK(0xc2, 0xc4, 0xc1), STRIP(0xf0, 0xef, 0xea), LEG_BLUE(0x1f, 0x62, 0xc9), LEG_RED(0xd2, 0x3b, 0x2e), LEG_ORANGE(0xd9, 0x7a, 0x12), KEY_GREEN(0x24, 0xa3, 0x55);
const QColor CRT_BG(0x08, 0x19, 0x11), CRT_P(0x8c, 0xff, 0xb8), CRT_DIM(0x3b, 0x9a, 0x6c), CRT_GRAT(0x1e, 0x5a, 0x3e), CRT_BAND(0x5f, 0xdc, 0x9b), CRT_DARK(0x03, 0x16, 0x0c);

namespace {
QHash<QString, QPixmap> sprites;

QPixmap sprite(const QString &key, int w, int h, const std::function<void(QPainter &)> &draw) {
    auto it = sprites.constFind(key);
    if (it != sprites.constEnd()) return it.value();
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    draw(p);
    p.end();
    return *sprites.insert(key, pm);
}

QColor mix(QColor a, QColor b, qreal t) {
    return QColor((int)(a.red() + (b.red() - a.red()) * t), (int)(a.green() + (b.green() - a.green()) * t),
                  (int)(a.blue() + (b.blue() - a.blue()) * t), (int)(a.alpha() + (b.alpha() - a.alpha()) * t));
}
QColor withAlpha(QColor c, int a) { c.setAlpha(a); return c; }
} // namespace

QFont labelFont(int px, bool bold) {
    static QHash<int, QFont> cache;
    int key = px * 2 + bold;
    auto it = cache.constFind(key);
    if (it != cache.constEnd()) return it.value();
    QFont f("Nimbus Sans Narrow");
    f.setStyleHint(QFont::SansSerif);
    f.setPixelSize(px);
    f.setBold(bold);
    return *cache.insert(key, f);
}

QFont crtFont(int px, bool bold) {
    static QHash<int, QFont> cache;
    int key = px * 2 + bold;
    auto it = cache.constFind(key);
    if (it != cache.constEnd()) return it.value();
    QFont f("DejaVu Sans Mono");
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(px);
    f.setBold(bold);
    f.setStyleStrategy(QFont::NoAntialias);
    f.setHintingPreference(QFont::PreferFullHinting);
    return *cache.insert(key, f);
}

QFont wordmarkFont(int px) {
    QFont f("Nimbus Sans");
    f.setStyleHint(QFont::SansSerif);
    f.setPixelSize(px);
    f.setBold(true);
    f.setItalic(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, -0.4);
    return f;
}

QImage plastic(int w, int h, QColor base, int grain, quint32 seed, int falloff) {
    QImage img(w, h, QImage::Format_RGB32);
    quint32 s = seed | 1u;
    for (int y = 0; y < h; ++y) {
        QRgb *row = (QRgb *)img.scanLine(y);
        for (int x = 0; x < w; ++x) {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            int n = (int)(s % (2 * grain + 1)) - grain;
            // broad light falloff, brightest toward the top-left
            int f = falloff - (int)((x * 0.55 + y * 0.9) * 2 * falloff / (w * 0.55 + h * 0.9));
            int r = qBound(0, base.red() + n + f, 255), g = qBound(0, base.green() + n + f, 255), b = qBound(0, base.blue() + n + f, 255);
            row[x] = qRgb(r, g, b);
        }
    }
    return img;
}

void groove(QPainter &p, QRectF r, qreal rad) {
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(withAlpha(CHASSIS_HI, 200), 1));
    p.drawRoundedRect(r.translated(0.8, 0.8), rad, rad);
    p.setPen(QPen(withAlpha(CHASSIS_LO, 210), 1));
    p.drawRoundedRect(r, rad, rad);
}

void plate(QPainter &p, QRectF r, qreal rad, QColor face, int lift) {
    bool up = lift > 0;
    int d = std::abs(lift);
    // cast shadow / lit lip
    for (int i = d; i >= 1; --i) {
        p.setPen(Qt::NoPen);
        p.setBrush(withAlpha(Qt::black, up ? 26 : 34));
        if (up) p.drawRoundedRect(r.translated(i * 0.9, i * 1.1), rad, rad);
        else p.drawRoundedRect(r.adjusted(-i * 0.5, -i * 0.5, i * 0.5, i * 0.5), rad + i * 0.5, rad + i * 0.5);
    }
    if (face.isValid()) {
        p.setBrush(face);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(r, rad, rad);
    }
    // rim light: top-left bright for raised, bottom-right bright for sunk
    QPainterPath tl, br;
    p.setBrush(Qt::NoBrush);
    p.save();
    p.setClipRect(QRectF(r.x() - 2, r.y() - 2, r.width() + 4, r.height() * 0.5 + 2));
    p.setPen(QPen(up ? withAlpha(Qt::white, 170) : withAlpha(Qt::black, 120), 1.2));
    p.drawRoundedRect(r.adjusted(0.6, 0.6, -0.6, -0.6), rad, rad);
    p.restore();
    p.save();
    p.setClipRect(QRectF(r.x() - 2, r.y() + r.height() * 0.5, r.width() + 4, r.height() * 0.5 + 2));
    p.setPen(QPen(up ? withAlpha(Qt::black, 90) : withAlpha(Qt::white, 150), 1.2));
    p.drawRoundedRect(r.adjusted(0.6, 0.6, -0.6, -0.6), rad, rad);
    p.restore();
}

void legend(QPainter &p, QRectF r, const QString &s, int px, int align, QColor c, bool bold) {
    p.setFont(labelFont(px, bold));
    p.setPen(c.isValid() ? c : INK);
    p.drawText(r, align | Qt::TextDontClip, s);
}

void sectionTitle(QPainter &p, QRectF f, const QString &title, QColor bar) {
    // the reference prints each group's name between two coloured bars on the top edge
    QFont ft = labelFont(11, false);
    int tw = QFontMetrics(ft).horizontalAdvance(title) + 10;
    qreal cx = f.center().x(), y = f.y() + 3;
    p.setPen(Qt::NoPen);
    p.setBrush(bar);
    p.drawRect(QRectF(f.x() + 6, y, cx - tw / 2.0 - f.x() - 6, 4));
    p.drawRect(QRectF(cx + tw / 2.0, y, f.right() - 6 - cx - tw / 2.0, 4));
    legend(p, QRectF(cx - tw / 2.0, y - 4, tw, 12), title, 11, Qt::AlignCenter, INK_SOFT);
}

void screw(QPainter &p, QPointF c, qreal r, qreal slotDeg) {
    QRadialGradient g(c + QPointF(-r * 0.3, -r * 0.3), r * 1.4);
    g.setColorAt(0, QColor(0xe8, 0xe6, 0xdd));
    g.setColorAt(1, QColor(0x8a, 0x87, 0x7d));
    p.setPen(QPen(withAlpha(Qt::black, 110), 0.8));
    p.setBrush(g);
    p.drawEllipse(c, r, r);
    qreal a = slotDeg * M_PI / 180;
    QPointF d(std::cos(a) * r * 0.75, std::sin(a) * r * 0.75);
    p.setPen(QPen(QColor(0x3c, 0x3a, 0x36), 1.3, Qt::SolidLine, Qt::FlatCap));
    p.drawLine(c - d, c + d);
}

void knobSeat(QPainter &p, QPointF c, qreal r, int ticks, bool endless) {
    // soft cast shadow, down-right
    QPointF sc = c + QPointF(r * 0.30, r * 0.46);
    QRadialGradient sh(sc, r * 1.5);
    sh.setColorAt(0, QColor(0, 0, 0, 120));
    sh.setColorAt(0.62, QColor(0, 0, 0, 85));
    sh.setColorAt(1, QColor(0, 0, 0, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(sh);
    p.drawEllipse(sc, r * 1.5, r * 1.5);
    // raised collar the shaft passes through
    QLinearGradient cg(c - QPointF(r, r), c + QPointF(r, r));
    cg.setColorAt(0, QColor(0xdd, 0xda, 0xcf));
    cg.setColorAt(1, QColor(0x9a, 0x96, 0x8b));
    p.setBrush(cg);
    p.setPen(QPen(QColor(0x6f, 0x6c, 0x64, 150), 0.8));
    p.drawEllipse(c, r + 2.6, r + 2.6);
    // printed scale
    p.setPen(QPen(INK, 1.0, Qt::SolidLine, Qt::FlatCap));
    if (endless) {
        p.setBrush(INK_SOFT);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 12; ++i) {
            qreal a = i * M_PI / 6;
            p.drawEllipse(c + QPointF(std::sin(a), -std::cos(a)) * (r + 7.5), 0.9, 0.9);
        }
    } else {
        for (int i = 0; i < ticks; ++i) {
            qreal a = (-135.0 + 270.0 * i / (ticks - 1)) * M_PI / 180;
            bool major = i == 0 || i == ticks - 1 || (ticks % 2 == 1 && i == ticks / 2);
            QPointF d(std::sin(a), -std::cos(a));
            p.setPen(QPen(major ? INK : INK_SOFT, major ? 1.3 : 0.9, Qt::SolidLine, Qt::FlatCap));
            p.drawLine(c + d * (r + 5.2), c + d * (r + (major ? 10.0 : 8.4)));
        }
    }
}

void knob(QPainter &p, QPointF c, qreal r, qreal angleDeg, bool grabbed) {
    int R = (int)std::ceil(r) + 6;
    QPixmap body = sprite(QString("knob/%1").arg((int)(r * 10)), 2 * R, 2 * R, [r, R](QPainter &q) {
        QPointF o(R, R);
        // the knob stands proud of the panel: its side wall shows below and to the right of the top
        QPointF w = o + QPointF(r * 0.07, r * 0.17);
        QLinearGradient wall(w - QPointF(r, 0), w + QPointF(r, 0));
        wall.setColorAt(0.00, QColor(0x8d, 0x89, 0x7f));
        wall.setColorAt(0.35, QColor(0x73, 0x70, 0x67));
        wall.setColorAt(0.75, QColor(0x52, 0x4f, 0x49));
        wall.setColorAt(1.00, QColor(0x3d, 0x3b, 0x36));
        q.setPen(QPen(QColor(0x2f, 0x2d, 0x29, 200), 0.9));
        q.setBrush(wall);
        q.drawEllipse(w, r * 1.02, r * 1.02);
        // flutes on the wall
        q.setPen(QPen(QColor(0, 0, 0, 70), 1.0));
        for (int i = -8; i <= 8; ++i) {
            qreal fx = w.x() + i * r * 0.115;
            qreal half = std::sqrt(qMax(0.0, r * r * 1.04 - (fx - w.x()) * (fx - w.x())));
            q.drawLine(QPointF(fx, w.y() + half * 0.55), QPointF(fx, w.y() + half));
        }
        // top rim, lit from the top-left
        QConicalGradient side(o, 135);
        side.setColorAt(0.00, QColor(0xc6, 0xc2, 0xb7));
        side.setColorAt(0.20, QColor(0x9b, 0x97, 0x8c));
        side.setColorAt(0.50, QColor(0x67, 0x64, 0x5c));
        side.setColorAt(0.80, QColor(0x93, 0x8f, 0x85));
        side.setColorAt(1.00, QColor(0xc6, 0xc2, 0xb7));
        q.setPen(QPen(QColor(0x4e, 0x4b, 0x45, 170), 0.8));
        q.setBrush(side);
        q.drawEllipse(o, r, r);
        // chamfer between rim and face
        QLinearGradient ch(o - QPointF(r, r) * 0.7, o + QPointF(r, r) * 0.7);
        ch.setColorAt(0, QColor(0xfa, 0xf9, 0xf3));
        ch.setColorAt(1, QColor(0x8f, 0x8b, 0x81));
        q.setPen(Qt::NoPen);
        q.setBrush(ch);
        q.drawEllipse(o, r * 0.80, r * 0.80);
        // face, slightly dished
        QRadialGradient face(o + QPointF(-r * 0.25, -r * 0.30), r * 1.05);
        face.setColorAt(0, QColor(0xf3, 0xf1, 0xe9));
        face.setColorAt(0.5, QColor(0xdd, 0xda, 0xcf));
        face.setColorAt(1, QColor(0xb1, 0xad, 0xa2));
        q.setBrush(face);
        q.setPen(QPen(QColor(0x8a, 0x86, 0x7c, 120), 0.7));
        q.drawEllipse(o, r * 0.72, r * 0.72);
    });
    p.drawPixmap(QPointF(c.x() - R, c.y() - R), body);

    p.save();
    p.translate(c);
    p.rotate(angleDeg);
    // knurl ridges turn with the knob
    int n = qMax(20, (int)(r * 1.5));
    p.setPen(QPen(QColor(0, 0, 0, 58), 1.0, Qt::SolidLine, Qt::FlatCap));
    for (int i = 0; i < n; ++i) {
        qreal a = 2 * M_PI * i / n;
        QPointF d(std::sin(a), -std::cos(a));
        p.drawLine(d * (r * 0.84), d * (r * 0.985));
    }
    // indicator: a milled notch on the face and a white index on the skirt
    p.setPen(QPen(QColor(0x2e, 0x2c, 0x29), qMax(1.8, r * 0.10), Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(0, -r * 0.26), QPointF(0, -r * 0.63));
    p.setPen(QPen(QColor(0xfb, 0xfa, 0xf4), qMax(1.6, r * 0.09), Qt::SolidLine, Qt::FlatCap));
    p.drawLine(QPointF(0, -r * 0.82), QPointF(0, -r * 0.985));
    p.restore();
    if (grabbed) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 22));
        p.drawEllipse(c, r * 0.71, r * 0.71);
    }
}

void cap(QPainter &p, QRectF r, const QString &label, bool down, QColor lit, int style) {
    int w = (int)std::ceil(r.width()), h = (int)std::ceil(r.height());
    const int M = 6; // margin for housing and glow
    QString key = QString("cap/%1/%2x%3/%4/%5/%6").arg(style).arg(w).arg(h).arg(label).arg(down).arg(lit.isValid() ? lit.rgb() : 0u);
    QPixmap pm = sprite(key, w + 2 * M, h + 2 * M, [=](QPainter &q) {
        QRectF b(M, M, w, h);
        bool oval = style == CAP_OVAL || style == CAP_SOFT;
        qreal rad = oval ? h / 2.0 : (style == CAP_RUN ? 5 : 3);
        // housing: a slot in the panel the cap sits in
        QRectF well = b.adjusted(-1.8, -1.8, 1.8, 1.8);
        q.setPen(Qt::NoPen);
        q.setBrush(QColor(255, 255, 255, 120));
        q.drawRoundedRect(well.translated(0.7, 0.9), rad + 1.8, rad + 1.8);
        q.setBrush(QColor(0x4c, 0x4a, 0x44));
        q.drawRoundedRect(well, rad + 1.8, rad + 1.8);
        if (lit.isValid()) { // light leaking round the cap
            q.setBrush(QColor(lit.red(), lit.green(), lit.blue(), 70));
            q.drawRoundedRect(b.adjusted(-4.5, -4.5, 4.5, 4.5), rad + 4.5, rad + 4.5);
            q.setBrush(QColor(0x4c, 0x4a, 0x44));
            q.drawRoundedRect(well, rad + 1.8, rad + 1.8);
        }
        // cap
        QRectF c = down ? b.adjusted(0.6, 1.6, -0.6, 0.4) : b;
        QColor top, bot;
        switch (style) {
        case CAP_WHITE: top = QColor(0xfa, 0xf9, 0xf4); bot = QColor(0xd8, 0xd6, 0xcc); break;
        case CAP_SOFT: top = QColor(0xb9, 0xb6, 0xab); bot = QColor(0x8e, 0x8b, 0x81); break;
        case CAP_RUN: top = QColor(0xe2, 0xdf, 0xd4); bot = QColor(0xb4, 0xb0, 0xa4); break;
        default: top = QColor(0xcf, 0xcc, 0xc0); bot = QColor(0xa7, 0xa3, 0x97); break;
        }
        if (lit.isValid()) { top = mix(QColor(255, 255, 255), lit, 0.45); bot = mix(lit, QColor(0, 0, 0), 0.12); }
        if (down) { top = top.darker(112); bot = bot.darker(108); }
        if (!down) { // side wall showing below the cap
            q.setBrush(bot.darker(140));
            q.drawRoundedRect(c.translated(0, 1.4), rad, rad);
        }
        QLinearGradient g(c.topLeft(), c.bottomLeft());
        g.setColorAt(0, top);
        g.setColorAt(1, bot);
        q.setBrush(g);
        q.drawRoundedRect(c, rad, rad);
        if (!down) {
            q.setBrush(Qt::NoBrush);
            q.setPen(QPen(QColor(255, 255, 255, lit.isValid() ? 150 : 175), 1.0));
            q.save();
            q.setClipRect(QRectF(c.x(), c.y(), c.width(), qMin(c.height() * 0.5, 5.0)));
            q.drawRoundedRect(c.adjusted(0.6, 0.6, -0.6, -0.6), rad, rad);
            q.restore();
        }
        if (!label.isEmpty()) {
            int px = style == CAP_RUN ? 11 : 10;
            q.setFont(labelFont(px, true));
            q.setPen(lit.isValid() ? QColor(0x1c, 0x24, 0x10) : INK);
            q.drawText(c.adjusted(1, 0, -1, 0), Qt::AlignCenter | Qt::TextDontClip, label);
        }
    });
    p.drawPixmap(QPointF(r.x() - M, r.y() - M), pm);
}

void lamp(QPainter &p, QPointF c, qreal r, QColor col, bool on) {
    int R = (int)std::ceil(r * 3.2);
    QString key = QString("lamp/%1/%2/%3").arg((int)(r * 10)).arg(col.rgb()).arg(on);
    QPixmap pm = sprite(key, 2 * R, 2 * R, [=](QPainter &q) {
        QPointF o(R, R);
        q.setPen(Qt::NoPen);
        if (on) {
            QRadialGradient halo(o, r * 3.0);
            halo.setColorAt(0, QColor(col.red(), col.green(), col.blue(), 150));
            halo.setColorAt(1, QColor(col.red(), col.green(), col.blue(), 0));
            q.setBrush(halo);
            q.drawEllipse(o, r * 3.0, r * 3.0);
        }
        q.setBrush(QColor(0x3d, 0x3b, 0x36));
        q.drawEllipse(o, r + 1.1, r + 1.1);
        QRadialGradient g(o + QPointF(-r * 0.3, -r * 0.35), r * 1.3);
        if (on) {
            g.setColorAt(0, QColor(255, 255, 255));
            g.setColorAt(0.35, mix(col, QColor(255, 255, 255), 0.35));
            g.setColorAt(1, col);
        } else {
            QColor d = mix(col, QColor(0x30, 0x2e, 0x2a), 0.72);
            g.setColorAt(0, d.lighter(150));
            g.setColorAt(1, d);
        }
        q.setBrush(g);
        q.drawEllipse(o, r, r);
    });
    p.drawPixmap(QPointF(c.x() - R, c.y() - R), pm);
}

void rubberKey(QPainter &p, QRectF r, bool down, QColor glow, QColor body) {
    int w = (int)std::ceil(r.width()), h = (int)std::ceil(r.height());
    const int M = 8;
    QString key = QString("rkey/%1x%2/%3/%4/%5").arg(w).arg(h).arg(down).arg(glow.isValid() ? glow.rgb() : 0u).arg(body.rgb());
    QPixmap pm = sprite(key, w + 2 * M, h + 2 * M, [=](QPainter &q) {
        QRectF b(M, M, w, h);
        qreal rad = h / 2.0;
        q.setPen(Qt::NoPen);
        if (glow.isValid()) { // backlight spilling round a pressed key
            for (int i = 4; i >= 1; --i) {
                q.setBrush(QColor(glow.red(), glow.green(), glow.blue(), 34 + (4 - i) * 26));
                q.drawRoundedRect(b.adjusted(-i * 1.5, -i * 1.5, i * 1.5, i * 1.5), rad + i * 1.5, rad + i * 1.5);
            }
        }
        if (!down) {
            q.setBrush(QColor(0, 0, 0, 70));
            q.drawRoundedRect(b.translated(1.4, 2.6), rad, rad);
            q.setBrush(QColor(0, 0, 0, 55));
            q.drawRoundedRect(b.translated(0.6, 1.4).adjusted(-0.8, -0.4, 0.8, 0.6), rad, rad);
        }
        QRectF c = down ? b.adjusted(1.0, 2.0, -1.0, 0.0) : b;
        QLinearGradient g(c.topLeft(), c.bottomLeft());
        if (down) {
            g.setColorAt(0, body.darker(190));
            g.setColorAt(1, body.darker(120));
        } else {
            g.setColorAt(0, body.lighter(210));
            g.setColorAt(0.45, body);
            g.setColorAt(1, body.darker(170));
        }
        q.setBrush(g);
        q.drawRoundedRect(c, c.height() / 2.0, c.height() / 2.0);
        if (!down) { // soft specular streak along the top
            QLinearGradient s(c.topLeft(), QPointF(c.x(), c.y() + c.height() * 0.55));
            s.setColorAt(0, QColor(255, 255, 255, 70));
            s.setColorAt(1, QColor(255, 255, 255, 0));
            q.setBrush(s);
            q.drawRoundedRect(c.adjusted(c.height() * 0.28, 1.6, -c.height() * 0.28, -c.height() * 0.48), c.height() * 0.25, c.height() * 0.25);
        }
    });
    p.drawPixmap(QPointF(r.x() - M, r.y() - M), pm);
}

QPixmap glass(int w, int h, qreal cornerRad) {
    QPixmap pm(w, h);
    pm.fill(Qt::transparent);
    QPainter q(&pm);
    q.setRenderHint(QPainter::Antialiasing, true);
    // raster lines
    q.setPen(QPen(QColor(0, 0, 0, 34), 1));
    for (int y = 1; y < h; y += 2) q.drawLine(0, y, w, y);
    // the tube's corners fall away into the dark surround
    QPainterPath outer, inner;
    outer.addRect(QRectF(0, 0, w, h));
    inner.addRoundedRect(QRectF(0, 0, w, h), cornerRad, cornerRad);
    q.setPen(Qt::NoPen);
    q.setBrush(QColor(0x08, 0x0a, 0x09));
    q.drawPath(outer.subtracted(inner));
    // edge vignette (thick glass, curved face)
    for (int i = 0; i < 10; ++i) {
        q.setBrush(Qt::NoBrush);
        q.setPen(QPen(QColor(0, 0, 0, 150 - i * 15), 1.4));
        q.drawRoundedRect(QRectF(i + 0.5, i + 0.5, w - 2 * i - 1, h - 2 * i - 1), qMax(1.0, cornerRad - i), qMax(1.0, cornerRad - i));
    }
    // room light catching the glass, top-left
    QLinearGradient refl(QPointF(0, 0), QPointF(w * 0.55, h * 0.75));
    refl.setColorAt(0, QColor(220, 255, 235, 26));
    refl.setColorAt(0.4, QColor(220, 255, 235, 7));
    refl.setColorAt(1, QColor(220, 255, 235, 0));
    q.setPen(Qt::NoPen);
    q.setBrush(refl);
    q.drawRoundedRect(QRectF(0, 0, w, h), cornerRad, cornerRad);
    return pm;
}

} // namespace hw
