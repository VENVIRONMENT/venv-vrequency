// venv-vrequency — the chord pads (see chord_pads.h).
#include "app/pages/synth/chord_pads.h"
#include "app/ui/palette.h"
#include <QString>
#include <cmath>
#include <cstring>

using namespace td;
using namespace pal;

void ChordPads::paint(QRectF area) {
    const Tuning &t = u.set.tuning;
    const qreal y = area.y(), h = area.height();
    const qreal ctl = 92, pw = (area.width() - ctl - 7 * 6) / 7.0;
    for (int d = 0; d < 7; ++d) {
        QRectF r(std::floor(area.x() + d * (pw + 6)), y, std::floor(pw), h);
        QString name = QString("chord%1").arg(d);
        bool cur = u.curChord == d, dn = ui.isDown(name);
        bool drone = (u.droneMask >> d) & 1;
        bool inv = cur || dn;
        ui.frame(r, inv || drone ? INK : LOW, inv ? INK : BG);
        if (drone && !inv) ui.frame(r.adjusted(3, 3, -3, -3), INK);
        Chord c = buildChord(t, d, false);
        ui.txt(QRectF(r.x(), r.y() + h * 0.10, r.width(), h * 0.52), c.name, c.name[3] ? 16 : 20, inv ? BG : INK, Qt::AlignCenter, true);
        bool small = std::strchr(c.name + 1, 'm') && !std::strstr(c.name, "maj");
        ui.txt(QRectF(r.x(), r.y() + h * 0.62, r.width(), h * 0.30), romanNumeral(d, small), 12, inv ? BG : MID);
        ui.zone(r, name, [this, d](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_CHORD, d); });
    }
    QRectF c1(area.right() - ctl, y, ctl, h / 2 - 3), c2(area.right() - ctl, y + h / 2 + 3, ctl, h / 2 - 3);
    static const char *MN[PM_COUNT] = {"Hold", "Drone"};
    ui.button(c1, "padmode", MN[u.set.padMode], false, [this] { sh.set(S_PADMODE, (float)((u.set.padMode + 1) % PM_COUNT)); });
    ui.button(c2, "padplay", u.padsPlaying ? QString::fromUtf8("■ Stop") : QString::fromUtf8("▶ Play"), u.padsPlaying,
              [this] { inst.cmd(EV_PADS); });
}
