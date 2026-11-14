// venv-vrequency — the Beat page's bar (every drum, every step) and the eight pads.
#include "app/pages/beat/beat_page.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <cmath>

using namespace td;
using namespace pal;

void BeatPage::paintGrid() {
    ui.section(60, QString("%1  %2 bpm%3").arg(beats.empty() ? QString("Pattern") : beats[cur].name).arg((int)std::round(u.set.bpm)).arg(u.beatRec ? "  recording" : (u.beatPlaying ? "  playing" : "")));
    const qreal ox = 104, cw = 37.5, sy = 84; const int ch = 12;
    for (int d = 0; d < kDrums; ++d) {
        qreal y = sy + d * (ch + 3);
        ui.txt(QRectF(16, y - 3, 84, ch + 6), nice(drumName(d)), Ui::pixelFont() ? 20 : 11, u.beatPat[d] ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter);
        for (int s = 0; s < kDrumSteps; ++s) {
            QRectF c(std::floor(ox + s * cw), y, std::floor(ox + (s + 1) * cw) - std::floor(ox + s * cw) - 4, ch);
            if ((u.beatPat[d] >> s) & 1) ui.g->fillRect(c, INK);
            else ui.frame(c, s % 4 == 0 ? LOW : GRAT);
        }
    }
    const qreal sb = sy + kDrums * (ch + 3);
    if (u.beatStep >= 0)
        ui.frame(QRectF(std::floor(ox + u.beatStep * cw) - 3, sy - 4, std::floor(ox + (u.beatStep + 1) * cw) - std::floor(ox + u.beatStep * cw) + 2, sb - sy + 5), INK);
    for (int b = 0; b < 4; ++b)
        ui.txt(QRectF(std::floor(ox + b * 4 * cw), sb + 1, cw - 4, 14), QString::number(b + 1), 11, MID);
}

void BeatPage::paintPads(qint64 now) {
    ui.section(222, erase ? "Pads  tap the drum to erase" : "Pads  tap to play");
    const qreal pw = (688 - 3 * 8) / 4.0;
    for (int d = 0; d < kDrums; ++d) {
        QRectF r(std::floor(16 + (d % 4) * (pw + 8)), 244 + (d / 4) * 108, std::floor(pw), 100);
        const QString name = QString("pad%1").arg(d);
        const bool lit = now - padLit[d] < 110 || ui.isDown(name);
        ui.frame(r, INK, lit ? INK : BG);
        if (!lit) {
            ui.frame(r.adjusted(1, 1, -1, -1), INK);
            ui.frame(r.adjusted(6, 6, -6, -6), erase ? INK : GRAT);
        }
        const QColor fg = lit ? BG : INK, dim = lit ? BG : MID;
        ui.txt(QRectF(r.x() + 12, r.y() + 10, 30, 18), QString::number(d + 1), 12, lit ? BG : LOW, Qt::AlignLeft | Qt::AlignVCenter, true);
        ui.txt(QRectF(r.x() + 40, r.y() + 10, r.width() - 52, 18), sh.km.drumKeys(d), 12, dim, Qt::AlignRight | Qt::AlignVCenter, true);
        ui.txt(QRectF(r.x(), r.bottom() - 40, r.width(), 26), nice(drumName(d)), 18, fg, Qt::AlignCenter, true);
        if (erase && !lit) ui.txt(QRectF(r.x(), r.y() + 34, r.width(), 18), "erase", 12, MID);
        ui.zone(r, name, [this, d](QPointF, int ph) { if (ph == PH_PRESS) hitPad(d); });
    }
}
