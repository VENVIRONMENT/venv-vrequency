// venv-vrequency — the chord pads (see chord_pads.h).
#include "app/pages/synth/chord_pads.h"
#include "app/ui/palette.h"
#include <QString>
#include <cmath>
#include <cstring>

using namespace td;
using namespace pal;

void ChordPads::paint(QRectF area, bool withSlots) {
    const Tuning &t = u.set.tuning;
    qreal y = area.y();
    if (withSlots) {
        // progression strip
        qreal sw = 62;
        for (int i = 0; i < kProgMax; ++i) {
            QRectF r(area.x() + i * (sw + 4), y, sw, 22);
            bool has = i < u.progLen, cur = has && i == u.progPos && u.backPlaying, sel = i == u.selSlot;
            ui.frame(r, sel || cur ? INK : (has ? MID : GRAT), cur ? INK : BG);
            if (sel && !cur) ui.frame(r.adjusted(2, 2, -2, -2), INK);
            if (has) ui.txt(r, buildChord(t, u.prog[i], u.set.seventh).name, 13, cur ? BG : INK, Qt::AlignCenter, true);
            else ui.txt(r, QString::number(i + 1), 11, GRAT);
            if (has) ui.zone(r.adjusted(0, -6, 0, 6), QString("pslot%1").arg(i), [this, i](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_BACK, BC_SELECT_SLOT, i); });
        }
        QString info;
        if (u.backPlaying && (u.progLen || u.holdChord >= 0 || u.droneMask)) info = QString::fromUtf8("♪ %1").arg(u.backName);
        else if (u.progLen) info = "stopped";
        else info = sh.keys.chordRow ? QString::fromUtf8("Z‥M = chords") : QString("tap chords");
        qreal ix = area.x() + 8 * (sw + 4);
        ui.txt(QRectF(ix, y, area.right() - ix, 22), ui.fit(info, 12, area.right() - ix), 12, MID, Qt::AlignRight | Qt::AlignVCenter);
        y += 28;
    }
    qreal h = area.bottom() - y;
    qreal ctl = 92, pw = (area.width() - ctl - 7 * 6) / 7.0;
    for (int d = 0; d < 7; ++d) {
        QRectF r(std::floor(area.x() + d * (pw + 6)), y, std::floor(pw), h);
        QString name = QString("chord%1").arg(d);
        bool cur = u.curChord == d, dn = ui.isDown(name);
        bool drone = (u.droneMask >> d) & 1;
        bool inv = cur || dn;
        ui.frame(r, inv || drone ? INK : LOW, inv ? INK : BG);
        if (drone && !inv) ui.frame(r.adjusted(3, 3, -3, -3), INK);
        Chord c = buildChord(t, d, u.set.seventh);
        ui.txt(QRectF(r.x(), r.y() + h * 0.10, r.width(), h * 0.52), c.name, c.name[3] ? 16 : 20, inv ? BG : INK, Qt::AlignCenter, true);
        bool small = std::strchr(c.name + 1, 'm') && !std::strstr(c.name, "maj");
        ui.txt(QRectF(r.x(), r.y() + h * 0.62, r.width(), h * 0.30), romanNumeral(d, small), 12, inv ? BG : MID);
        ui.zone(r, name, [this, d](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_CHORD, d); });
    }
    QRectF c1(area.right() - ctl, y, ctl, h / 2 - 3), c2(area.right() - ctl, y + h / 2 + 3, ctl, h / 2 - 3);
    static const char *MN[BM_COUNT] = {"Prog", "Hold", "Drone"};
    ui.button(c1, "backmode", MN[u.set.backMode], false, [this] { sh.perform(A_BACK_MODE); });
    ui.button(c2, "backplay", u.backPlaying ? QString::fromUtf8("■ Stop") : QString::fromUtf8("▶ Play"), u.backPlaying,
           [this] { inst.cmd(EV_BACK, BC_PLAYSTOP); });
}
