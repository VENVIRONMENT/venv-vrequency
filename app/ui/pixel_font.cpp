// venv-vrequency — the MGS1 pixel fonts, the default type: two faces, five roles.
// VREQUENCY_FONT=classic brings back the old DejaVu Sans Mono look.
// The faces are bundled in assets/fonts (MGS1 Fonts by Andrew Gleeson, CC BY-SA 4.0).
#include "app/ui/ui.h"
#include <QFontMetrics>
#include <QPainter>
#include <cmath>

bool Ui::pixelFont() {
    static const bool on = !qEnvironmentVariable("VREQUENCY_FONT").startsWith("classic", Qt::CaseInsensitive);
    return on;
}

// Old pixel sizes become one of five roles:
//   0 Codec 48 (huge numbers)  1 Codec 32 (titles)  2 Codec 24 (title, stepped down)
//   3 HUD 32 (button labels)   4 HUD 24 (small text)
int Ui::pixelRole(int px, bool bold) {
    if (px >= 40) return 0;
    if (px >= 17) return 1;
    if (px >= 14 || (px == 13 && bold)) return 3;
    return 4;
}

// Try the role's size, step down a size until it fits the box (width and letter height), then
// shorten. Placement uses the real letter height, not the font's line height, so tall line
// metrics never push letters out of short boxes.
void Ui::pixelTxt(QRectF r, const QString &s0, int role, QColor c, int align) {
    static const int next[] = {1, 2, 4, 4, 4}; // 48 -> 32 -> 24, HUD 32 -> HUD 24
    QString s = s0;
    for (;;) {
        QFont f = font(1000 + role, false);
        QFontMetrics fm(f);
        int capH = fm.tightBoundingRect("H").height();
        int w = fm.horizontalAdvance(s);
        bool fits = w <= r.width() && capH <= r.height() + 1;
        if (!fits && next[role] != role) { role = next[role]; continue; }
        if (w > r.width()) {
            while (s.size() > 1 && fm.horizontalAdvance(s + "..") > r.width()) s.chop(1);
            s += "..";
            w = fm.horizontalAdvance(s);
        }
        qreal x = r.x();
        if (align & Qt::AlignHCenter) x = r.x() + std::floor((r.width() - w) / 2);
        else if (align & Qt::AlignRight) x = r.right() - w;
        qreal base;
        if (align & Qt::AlignTop) base = r.y() + capH;
        else if (align & Qt::AlignBottom) base = r.bottom();
        else base = r.y() + std::floor((r.height() + capH) / 2);
        g->setFont(f);
        g->setPen(c);
        g->drawText(QPointF(x, base), s);
        break;
    }
    if (recordText && !s0.trimmed().isEmpty()) drawnText.push_back({r, s0});
}
