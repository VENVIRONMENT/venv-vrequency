// venv-vrequency — the Synth page's bottom bar: the loop button and the soft keys beside it.
#include "app/pages/synth/synth_page.h"
#include "app/sounds/sounds.h"
#include "app/ui/palette.h"
#include <QPainter>

using namespace td;
using namespace pal;

void SynthPage::paintLoopButton(QRectF r) {
    bool dn = ui.isDown("loop_pedal");
    QString label = "Loop", sub = "tap to record";
    bool inv = dn, armed = false;
    float prog = 0;
    bool blink = (sh.clock.elapsed() / 300) % 2 == 0;
    switch (u.loopState) {
    case L_COUNTIN: label = QString::number(qMax(1, u.countBeat)); sub = "count-in"; armed = true; break;
    case L_REC: label = "REC"; sub = QString("bar %1").arg((int)u.loopPos + 1); inv = true; break;
    case L_PLAY:
        if (u.loopClosing) { label = "REC"; sub = QString::fromUtf8("closing bar…"); inv = true; }
        else { label = u.layers > 1 ? QString::fromUtf8("Loop ×%1").arg(u.layers) : QString("Loop"); sub = QString("%1 bar%2, tap = dub").arg(u.loopBars).arg(u.loopBars == 1 ? "" : "s"); armed = true; }
        prog = u.loopPos; break;
    case L_OVERDUB: label = "DUB"; sub = u.loopFull ? "memory full" : "tap to finish"; inv = blink; armed = true; prog = u.loopPos; break;
    case L_STOPPED: label = "Loop"; sub = "stopped, tap = play"; break;
    default: break;
    }
    ui.frame(r, inv || armed ? INK : LOW, inv ? INK : BG);
    if (armed && !inv) ui.frame(r.adjusted(3, 3, -3, -3), INK);
    QColor tc = inv ? BG : INK;
    ui.txt(QRectF(r.x(), r.y() + 9, r.width(), 32), label, 24, u.loopState == L_STOPPED ? MID : tc, Qt::AlignCenter, true);
    ui.txt(QRectF(r.x(), r.y() + 45, r.width(), 16), ui.fit(sub, 11, r.width() - 10), 11, inv ? BG : MID);
    if (prog > 0) { // position in the loop
        QRectF b(r.x() + 9, r.bottom() - 14, r.width() - 18, 5);
        ui.frame(b, inv ? BG : MID);
        ui.g->fillRect(QRectF(b.x(), b.y(), b.width() * qMin(1.0f, prog), b.height()), tc);
    }
    ui.zone(r, "loop_pedal", [this](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_LOOP, LC_PEDAL); });
}

void SynthPage::paintSoftKeys() {
    qreal x = 16, y = 628, h = 80;
    paintLoopButton(QRectF(x, y, 150, h)); x += 158;
    ui.button(QRectF(x, y, 66, h), "loop_undo", "Undo", false, [this] { inst.cmd(EV_LOOP, LC_UNDO); }, "layer"); x += 74;
    bool lp = u.loopState == L_PLAY || u.loopState == L_OVERDUB;
    ui.button(QRectF(x, y, 66, h), "loop_playstop", lp ? QString::fromUtf8("■") : QString::fromUtf8("▶"), false, [this] { inst.cmd(EV_LOOP, LC_PLAYSTOP); }, "loop"); x += 74;
    ui.button(QRectF(x, y, 66, h), "sustain", "Sus", u.sustain, [this] { KeyState &keys = sh.keys; keys.susLatch = !keys.susLatch; inst.cmd(EV_SUSTAIN, keys.susLatch || keys.susKey ? 1 : 0); }, "sustain"); x += 74;
    ui.button(QRectF(x, y, 54, h), "octave_down", QString::fromUtf8("−"), false, [this] { inst.cmd(EV_OCTAVE, -1); }, "oct"); x += 62;
    ui.button(QRectF(x, y, 54, h), "octave_up", "+", false, [this] { inst.cmd(EV_OCTAVE, 1); }, "oct"); x += 62;
    ui.button(QRectF(x, y, 86, h), "new_sound", "New", false, [this] { sounds.randomise(); }, "sound"); x += 94;
    ui.button(QRectF(x, y, 704 - x, h), "menu", "Menu", false, [this] { sh.goPage(PG_SOUND); }, QString::fromUtf8("≡"));
}
