// venv-vrequency — the toolkit's text, rules, zones and touches (see ui.h).
//
// The whole screen is drawn the way a bench oscilloscope draws its own display:
// white phosphor on black, one-pixel rules, raster type, and inverse video for
// whatever is selected. There is no colour; state is shown by inversion, by a
// double frame, or by blinking.
#include "app/ui/ui.h"
#include "app/ui/palette.h"
#include <QFontMetrics>
#include <QPainter>

using namespace pal;

void Ui::beginFrame(QPainter &painter) {
    g = &painter;
    zoneList.clear();
    drawnText.clear();
}

QFont Ui::font(int px, bool bold) const {
    // building a QFont is surprisingly costly; every label reuses a cached one
    quint32 key = (quint32)px << 1 | (bold ? 1u : 0u);
    auto it = fontCache.constFind(key);
    if (it != fontCache.constEnd()) return it.value();
    // MOCKUP SWITCH: VREQUENCY_FONT=MGS1 draws text in two pixel faces by role (pixel_font.cpp)
    QString family = "DejaVu Sans Mono";
    int size = px;
    if (pixelFont()) {
        // the key is a role, not a size: 1000 + n = role n
        static const char *fam[] = {"MGS1 Codec", "MGS1 Codec", "MGS1 Codec", "MGS1 HUD", "MGS1 HUD"};
        static const int sz[] = {48, 32, 24, 32, 24};
        int role = px >= 1000 ? qBound(0, px - 1000, 4) : pixelRole(px, bold);
        family = fam[role];
        size = sz[role];
    }
    QFont f(family);
    f.setStyleHint(QFont::Monospace);
    f.setStyleStrategy(QFont::NoAntialias); // raster text, like the tube's character generator
    f.setHintingPreference(QFont::PreferFullHinting);
    f.setPixelSize(size);
    f.setBold(bold && !pixelFont());
    fontCache.insert(key, f);
    return f;
}

int Ui::chW(int px) const {
    if (pixelFont()) px = 1000 + pixelRole(px, true);
    auto it = widthCache.constFind(px);
    if (it != widthCache.constEnd()) return it.value();
    int v = QFontMetrics(font(px, true)).horizontalAdvance(QLatin1Char('M'));
    widthCache.insert(px, v);
    return v;
}

QString Ui::fit(const QString &s, int px, qreal width) const {
    if (pixelFont()) return s; // txt() shrinks, then shortens, by itself
    int n = qMax(1, (int)(width / chW(px)));
    return s.size() <= n ? s : s.left(n - 1) + QString::fromUtf8("…");
}

void Ui::txt(QRectF r, const QString &s, int px, QColor c, int align, bool bold) {
    if (pixelFont()) { pixelTxt(r, s, pixelRole(px, bold), c, align); return; }
    g->setFont(font(px, bold));
    g->setPen(c);
    g->drawText(r, align | Qt::TextSingleLine, s);
    if (recordText && !s.trimmed().isEmpty()) drawnText.push_back({r, s});
}

// a one-pixel frame on the pixel grid, optionally filled
void Ui::frame(QRectF r, QColor line, QColor fill) {
    if (fill.isValid()) g->fillRect(r, fill);
    if (!line.isValid()) return;
    g->fillRect(QRectF(r.x(), r.y(), r.width(), 1), line);
    g->fillRect(QRectF(r.x(), r.bottom() - 1, r.width(), 1), line);
    g->fillRect(QRectF(r.x(), r.y(), 1, r.height()), line);
    g->fillRect(QRectF(r.right() - 1, r.y(), 1, r.height()), line);
}

// inverse-video label: a white block with black text
void Ui::tag(QRectF r, const QString &s, int px, int align) {
    g->fillRect(r, INK);
    txt(r.adjusted(4, 0, -4, 0), s, px, BG, align, true);
}

// small heading with a rule running to the right edge
void Ui::section(qreal y, const QString &s) {
    txt(QRectF(18, y, 680, 20), s, 13, MID, Qt::AlignLeft | Qt::AlignVCenter);
    qreal x = 18 + s.size() * chW(13) + 10;
    if (x < 700) g->fillRect(QRectF(x, y + 10, 704 - x, 1), GRAT);
}

void Ui::zone(QRectF r, const QString &name, std::function<void(QPointF, int)> fn) {
    zoneList.push_back({r, name, std::move(fn)});
}
bool Ui::touch(int id, QPointF c, int phase, const std::function<void(const QString &)> &pressed) {
    if (phase == PH_PRESS) {
        for (int i = (int)zoneList.size() - 1; i >= 0; --i)
            if (zoneList[i].r.contains(c)) {
                captured[id] = zoneList[i];
                downNames.insert(zoneList[i].name);
                if (pressed) pressed(zoneList[i].name);
                zoneList[i].fn(c, PH_PRESS);
                break;
            }
        return true;
    }
    auto it = captured.find(id);
    if (it == captured.end()) return false;
    Zone z = it.value();
    if (phase == PH_RELEASE) { captured.erase(it); downNames.remove(z.name); }
    z.fn(c, phase);
    return true;
}

QRectF Ui::zoneRect(const QString &name) const {
    for (const Zone &z : zoneList) if (z.name == name) return z.r;
    return QRectF();
}

QStringList Ui::zoneNames() const {
    QStringList n;
    for (const Zone &z : zoneList) n << z.name;
    return n;
}
QString Ui::zoneAt(QPointF c) const {
    for (int i = (int)zoneList.size() - 1; i >= 0; --i)
        if (zoneList[i].r.contains(c)) return zoneList[i].name;
    return QString();
}
