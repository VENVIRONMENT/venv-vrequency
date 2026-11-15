// venv-vrequency — the Freq page's four tone rows and its six bend-and-warp cells.
#include "app/pages/freq/freq_page.h"
#include "app/ui/layout.h"
#include "app/ui/palette.h"
#include "app/ui/zones.h"
#include <QPainter>
#include <cmath>

using namespace td;
using namespace pal;

namespace {
const int kLeft = Qt::AlignLeft | Qt::AlignVCenter, kRight = Qt::AlignRight | Qt::AlignVCenter;
QString pct(float v) { return QString("%1%").arg((int)std::round(v * 100)); }
float toLog(float v, float lo, float hi) { return std::log(v / lo) / std::log(hi / lo); }
float fromLog(float n, float lo, float hi) { return lo * std::pow(hi / lo, qBound(0.0f, n, 1.0f)); }
} // namespace

QString freqHz(float hz) { return QString("%1 Hz").arg(hz, 0, 'f', 2); }

// number | WAVE | Hz | note (or +offset from tone 1) | ratio to tone 1 | level bar | ON
void FreqPage::paintRow(int i, QRectF r) {
    const FreqTone &t = u.freq.tone[i], &t1 = u.freq.tone[0];
    const bool on = t.on, me = i == sel;
    const QColor tc = on ? INK : LOW;
    ui.zone(r, zn::at(zn::freqtone, i), [this, i](QPointF, int ph) { if (ph == PH_PRESS) tapRow(i); });
    ui.frame(r, me ? INK : (on ? LOW : GRAT), BG);
    if (me) ui.frame(r.adjusted(2, 2, -2, -2), INK);
    const qreal x = r.x(), cy = r.y(), h = r.height();
    ui.txt(QRectF(x + 8, cy, 26, h), QString::number(i + 1), 14, me ? INK : MID, kLeft, true);
    const QRectF wave(x + 36, cy + 6, 86, h - 12);
    const bool wd = ui.isDown(zn::at(zn::freqwave, i));
    ui.frame(wave, wd ? INK : (on ? MID : GRAT), wd ? INK : BG);
    ui.txt(wave, freqWaveName(t.wave), 12, wd ? BG : tc, Qt::AlignCenter, true);
    ui.zone(wave, zn::at(zn::freqwave, i), [this, i](QPointF, int ph) { if (ph == PH_PRESS) cycleWave(i); });
    ui.txt(QRectF(x + 128, cy, 118, h), freqHz(t.hz), 14, tc, kRight, true);
    QString note;
    if (i > 0 && std::fabs(t.hz - t1.hz) <= kFreqNear) note = QString("%1%2").arg(t.hz >= t1.hz ? "+" : "-").arg(std::fabs(t.hz - t1.hz), 0, 'f', 2);
    else { char n[8]; freqNoteName(t.hz, n, sizeof n); note = n; }
    ui.txt(QRectF(x + 256, cy, 70, h), note, 13, on ? MID : LOW, kLeft);
    int p = 1, q = 1;
    const int fit = i == 0 ? 2 : freqRatio(t.hz, t1.hz, p, q);
    ui.txt(QRectF(x + 326, cy, 60, h), fit == 0 ? QString("-") : QString("%1%2:%3").arg(fit == 1 ? "~" : "").arg(p).arg(q), 13, on ? MID : LOW, kLeft);
    const QRectF lv(x + 392, cy + 4, 228, h - 8), bar(lv.x() + 4, lv.center().y() - 4, lv.width() - 52, 8);
    ui.frame(lv, ui.isDown(zn::at(zn::freqlevel, i)) ? INK : GRAT, BG);
    ui.g->fillRect(QRectF(bar.x(), bar.center().y(), bar.width(), 1), LOW);
    ui.g->fillRect(QRectF(bar.x(), bar.y(), std::floor(bar.width() * t.level), bar.height()), on ? INK : LOW);
    ui.txt(QRectF(bar.right() + 2, cy, 44, h), pct(t.level), 12, tc, kRight);
    ui.zone(lv, zn::at(zn::freqlevel, i), [this, i, bar](QPointF pt, int ph) {
        if (ph != PH_RELEASE) setField(i, FF_LEVEL, qBound(0.0f, (float)((pt.x() - bar.x()) / bar.width()), 1.0f));
    });
    const QRectF ob(x + 626, cy + 5, 56, h - 10);
    ui.frame(ob, on ? INK : LOW, on ? INK : BG);
    ui.txt(ob, on ? "ON" : "off", 12, on ? BG : MID, Qt::AlignCenter, true);
    ui.zone(ob, zn::at(zn::freqon, i), [this, i, on](QPointF, int ph) { if (ph == PH_PRESS) { select(i); setField(i, FF_ON, on ? 0 : 1); } });
}

// One warp cell: a name, its value and a bar; a touch sets it by where it lands (and drags).
void FreqPage::cell(QRectF r, const char *zone, const QString &label, const QString &value, float norm, int field,
                    float lo, float hi, bool logScale) {
    const bool dn = ui.isDown(zone);
    ui.frame(r, dn ? INK : LOW, BG);
    ui.txt(QRectF(r.x() + 6, r.y() + 6, r.width() - 12, 16), label, 12, MID, Qt::AlignCenter, true);
    ui.txt(QRectF(r.x() + 2, r.y() + 26, r.width() - 4, 22), ui.fit(value, 13, r.width() - 6), 13, INK, Qt::AlignCenter, true);
    const QRectF bar(r.x() + 8, r.bottom() - 18, r.width() - 16, 6);
    ui.g->fillRect(QRectF(bar.x(), bar.bottom() - 1, bar.width(), 1), LOW);
    ui.g->fillRect(QRectF(bar.x(), bar.y(), std::floor(bar.width() * qBound(0.0f, norm, 1.0f)), bar.height()), INK);
    if (field < 0) return; // the caller registers the zone
    ui.zone(r, zone, [this, bar, field, lo, hi, logScale](QPointF p, int ph) {
        if (ph == PH_RELEASE) return;
        const float n = qBound(0.0f, (float)((p.x() - bar.x()) / bar.width()), 1.0f);
        setField(-1, field, logScale ? fromLog(n, lo, hi) : lo + n * (hi - lo));
    });
}

void FreqPage::paintWarp(qreal y) {
    const FreqState &f = u.freq;
    const WarpParams &w = f.warp;
    ui.section(y, "Bend and warp  tone 2 into tone 1, then the whole Freq sound");
    const qreal cy = y + 22, ch = 80;
    const QRectF m = lay::cell(0, 6, cy, ch);
    const QString modText = f.mod == FM_OFF ? QString("OFF") : QString("%1 %2").arg(freqModName(f.mod)).arg(pct(f.modAmt));
    cell(m, zn::freqmod, "2 > 1", modText, f.mod == FM_OFF ? 0 : f.modAmt, -1, 0, 1, false);
    ui.zone(m, zn::freqmod, [this, m](QPointF p, int ph) { // a tap cycles OFF / FM / RING, a drag sets the amount
        if (ph == PH_PRESS) { modDragX = (float)p.x(); modDragAmt = u.freq.modAmt; modDragged = false; return; }
        const float dx = (float)p.x() - modDragX;
        if (ph == PH_MOVE && (modDragged || std::fabs(dx) > 6)) {
            modDragged = true;
            setField(-1, FF_MODAMT, qBound(0.0f, modDragAmt + dx / (float)(m.width() - 16), 1.0f));
        } else if (ph == PH_RELEASE && !modDragged) {
            setField(-1, FF_MOD, (float)((u.freq.mod + 1) % kFreqMods));
        }
    });
    cell(lay::cell(1, 6, cy, ch), zn::freqfold, "Fold", pct(w.fold), w.fold, FF_FOLD, 0, 1, false);
    const bool open = w.cutoff >= kWarpCutMax - 1;
    const QString cut = open ? QString("open") : w.cutoff >= 1000 ? QString("%1 kHz").arg(w.cutoff / 1000, 0, 'f', 1) : QString("%1 Hz").arg((int)std::round(w.cutoff));
    cell(lay::cell(2, 6, cy, ch), zn::freqfilter, "Filter", cut, toLog(w.cutoff, kWarpCutMin, kWarpCutMax), FF_FILTER, kWarpCutMin, kWarpCutMax, true);
    cell(lay::cell(3, 6, cy, ch), zn::freqdrive, "Drive", pct(w.drive), w.drive, FF_DRIVE, 0, 1, false);
    const float sr = inst.sampleRate(), bits = warpCrushBits(w.crush), khz = warpCrushHz(w.crush, sr) / 1000;
    const QString cr = w.crush <= 0 ? QString("off") : QString("%1b %2k").arg((int)std::round(bits)).arg(khz, 0, 'f', khz < 10 ? 1 : 0);
    cell(lay::cell(4, 6, cy, ch), zn::freqcrush, "Crush", cr, w.crush, FF_CRUSH, 0, 1, false);
    cell(lay::cell(5, 6, cy, ch), zn::freqdelay, "Delay", f.delay <= 0 ? QString("dry") : QString("send %1").arg(pct(f.delay)), f.delay, FF_DELAY, 0, 1, false);
}
