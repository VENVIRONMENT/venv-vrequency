// venv-vrequency — the toolkit's three controls: a button, a slider and a stepper (see ui.h).
#include "app/ui/ui.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <cmath>

using namespace pal;

void Ui::button(QRectF r, const QString &name, const QString &label, bool on, std::function<void()> fn,
                        const QString &sub, int look) {
    bool inv = on || downNames.contains(name);
    frame(r, inv || look == LOOK_ARMED ? INK : LOW, inv ? INK : BG);
    if (look == LOOK_ARMED && !inv) frame(r.adjusted(3, 3, -3, -3), INK);
    QColor tc = inv ? BG : INK;
    int px = r.height() >= 50 ? 16 : 14;
    while (px > 11 && label.size() * chW(px) > r.width() - 6) --px;
    if (sub.isEmpty()) {
        txt(r, fit(label, px, r.width() - 4), px, tc, Qt::AlignCenter, true);
    } else {
        txt(QRectF(r.x(), r.y() + r.height() * 0.13, r.width(), r.height() * 0.46), fit(label, px, r.width() - 4), px, tc, Qt::AlignCenter, true);
        txt(QRectF(r.x() + 2, r.y() + r.height() * 0.58, r.width() - 4, r.height() * 0.32), fit(sub, 11, r.width() - 6), 11, inv ? BG : MID);
    }
    zone(r, name, [fn](QPointF, int ph) { if (ph == PH_PRESS && fn) fn(); });
}

// a level readout: value bar over a printed tick scale, with a marker
void Ui::slider(QRectF r, const QString &name, const QString &label, float norm, const QString &value,
                        std::function<void(float)> fn) {
    bool dn = downNames.contains(name);
    frame(r, dn ? INK : LOW, BG);
    norm = norm < 0 ? 0 : (norm > 1 ? 1 : norm);
    QRectF bar(r.x() + 10, r.bottom() - 19, r.width() - 20, 6);
    for (int i = 0; i <= 20; ++i) {
        qreal x = std::floor(bar.x() + (bar.width() - 1) * i / 20.0);
        bool major = i % 5 == 0;
        g->fillRect(QRectF(x, bar.bottom() + 2, 1, major ? 6 : 3), major ? MID : LOW);
    }
    g->fillRect(QRectF(bar.x(), bar.bottom() - 1, bar.width(), 1), LOW);
    qreal vx = std::floor(bar.x() + (bar.width() - 1) * norm);
    g->fillRect(QRectF(bar.x(), bar.y(), vx - bar.x(), bar.height()), INK);
    g->fillRect(QRectF(vx - 1, bar.y() - 4, 3, bar.height() + 8), INK);
    txt(QRectF(r.x() + 10, r.y() + 5, r.width() * 0.6, 18), label, 13, MID, Qt::AlignLeft | Qt::AlignVCenter);
    txt(QRectF(r.x() + r.width() * 0.4, r.y() + 5, r.width() * 0.6 - 10, 18), value, 13, INK, Qt::AlignRight | Qt::AlignVCenter, true);
    zone(r, name, [fn, bar](QPointF p, int ph) {
        if (ph == PH_RELEASE) return;
        fn((float)((p.x() - bar.x()) / bar.width()));
    });
}

void Ui::stepper(QRectF r, const QString &name, const QString &label, const QString &value,
                         std::function<void()> minus, std::function<void()> plus) {
    frame(r, LOW, BG);
    qreal bw = r.width() < 200 ? 44 : 56;
    QRectF lm(r.x(), r.y(), bw, r.height()), rp(r.right() - bw, r.y(), bw, r.height());
    bool dm = downNames.contains(name + "-"), dp = downNames.contains(name + "+");
    if (dm) g->fillRect(lm, INK);
    if (dp) g->fillRect(rp, INK);
    g->fillRect(QRectF(lm.right() - 1, r.y() + 1, 1, r.height() - 2), LOW);
    g->fillRect(QRectF(rp.x(), r.y() + 1, 1, r.height() - 2), LOW);
    txt(lm, QString::fromUtf8("−"), 24, dm ? BG : INK, Qt::AlignCenter, true);
    txt(rp, "+", 24, dp ? BG : INK, Qt::AlignCenter, true);
    txt(QRectF(r.x() + bw, r.y() + 6, r.width() - 2 * bw, 16), fit(label, 12, r.width() - 2 * bw - 4), 12, MID);
    txt(QRectF(r.x() + bw, r.y() + 22, r.width() - 2 * bw, r.height() - 26), value, 18, INK, Qt::AlignCenter, true);
    zone(lm, name + "-", [minus](QPointF, int ph) { if (ph == PH_PRESS) minus(); });
    zone(rp, name + "+", [plus](QPointF, int ph) { if (ph == PH_PRESS) plus(); });
}
