// venv-vrequency — the Backing page (see backing_page.h).
#include "app/pages/backing/backing_page.h"
#include "app/sounds/sounds.h"
#include "app/ui/palette.h"
#include <cmath>

using namespace td;
using namespace pal;

BackingPage::BackingPage(Shell &s, ChordPads &c) : Page(s), sounds(s.soundShelf()), pads(c) {}

void BackingPage::paint() {
    ui.section(62, QString::fromUtf8("A chord tap…"));
    static const char *MS[BM_COUNT] = {"adds to the loop", "latches one chord", "latches a drone note"};
    qreal w3 = (688 - 2 * 8) / 3.0;
    for (int i = 0; i < BM_COUNT; ++i)
        ui.button(QRectF(std::floor(16 + i * (w3 + 8)), 86, std::floor(w3), 54), QString("bmode%1").arg(i), nice(backModeName(i)), u.set.backMode == i, [this, i] { sh.set(S_BACKMODE, (float)i); }, MS[i]);
    qreal w5 = (688 - 4 * 6) / 5.0;
    for (int i = 0; i < BS_COUNT; ++i)
        ui.button(QRectF(std::floor(16 + i * (w5 + 6)), 148, std::floor(w5), 46), QString("bstyle%1").arg(i), nice(backStyleName(i)), u.set.backStyle == i, [this, i] { sh.set(S_BACKSTYLE, (float)i); });

    // progression slots
    ui.section(202, "Progression  tap a slot, then a chord, to replace it");
    qreal sw = (688 - 7 * 6) / 8.0;
    for (int i = 0; i < kProgMax; ++i) {
        QRectF r(std::floor(16 + i * (sw + 6)), 226, std::floor(sw), 58);
        bool has = i < u.progLen, cur = has && i == u.progPos && u.backPlaying, sel = i == u.selSlot;
        ui.frame(r, sel || cur ? INK : (has ? MID : GRAT), cur ? INK : BG);
        if (sel && !cur) ui.frame(r.adjusted(3, 3, -3, -3), INK);
        ui.txt(QRectF(r.x(), r.y() + 5, r.width(), 14), QString::number(i + 1), 11, cur ? BG : (has ? MID : LOW));
        if (has) ui.txt(QRectF(r.x(), r.y() + 18, r.width(), 34), buildChord(u.set.tuning, u.prog[i], u.set.seventh).name, 18, cur ? BG : INK, Qt::AlignCenter, true);
        if (has) ui.zone(r, QString("bslot%1").arg(i), [this, i](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_BACK, BC_SELECT_SLOT, i); });
    }
    qreal w4 = (688 - 3 * 8) / 4.0;
    auto c4 = [w4](int i, qreal y, qreal h) { return QRectF(std::floor(16 + i * (w4 + 8)), y, std::floor(w4), h); };
    ui.button(c4(0, 292, 52), "bremove", "Remove last", false, [this] { inst.cmd(EV_BACK, BC_REMOVE_LAST); });
    ui.button(c4(1, 292, 52), "bclear", "Clear all", false, [this] { inst.cmd(EV_BACK, BC_CLEAR); });
    ui.button(c4(2, 292, 52), "bseventh", "7th chords", u.set.seventh, [this] { sh.set(S_SEVENTH, u.set.seventh ? 0 : 1); });
    ui.button(c4(3, 292, 52), "bmetro", "Metronome", u.set.metro, [this] { sh.set(S_METRO, u.set.metro ? 0 : 1); });

    pads.paint(QRectF(16, 356, 688, 84), false);

    ui.stepper(QRectF(16, 452, 340, 60), "chordbeats", "Beats per chord", QString::number(u.set.chordBeats),
            [this] { sh.set(S_CHORDBEATS, (float)(u.set.chordBeats - 1)); }, [this] { sh.set(S_CHORDBEATS, (float)(u.set.chordBeats + 1)); });
    ui.stepper(QRectF(364, 452, 340, 60), "arprate", "Arp steps per beat", QString::number(u.set.arpRate),
            [this] { sh.set(S_ARPRATE, (float)(u.set.arpRate - 1)); }, [this] { sh.set(S_ARPRATE, (float)(u.set.arpRate + 1)); });
    ui.stepper(QRectF(16, 520, 340, 60), "bpm2", "Tempo", QString("%1 bpm").arg((int)std::round(u.set.bpm)),
            [this] { sh.set(S_BPM, std::round(u.set.bpm) - 1); }, [this] { sh.set(S_BPM, std::round(u.set.bpm) + 1); });
    ui.slider(QRectF(364, 520, 340, 60), "backvol", "Backing volume", u.set.backVol, QString("%1%").arg((int)(u.set.backVol * 100)), [this](float n) { sh.set(S_BACKVOL, n); });
    ui.button(QRectF(16, 590, 688, 56), "bresync", "Use the current lead sound for the backing", false, [this] { inst.cmd(EV_BACK, BC_RESYNC_SOUND); },
           QString("backing now plays: %1    lead: %2").arg(u.backName[0] ? u.backName : "(nothing yet)").arg(sounds.lead.name));
    ui.txt(QRectF(18, 654, 684, 20), "The backing keeps the sound it started with, so the lead can change freely.", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.txt(QRectF(18, 676, 684, 20), QString::fromUtf8("Keyboard: hold %1 + Z‥M for chords, or press $ for a chord row.").arg(sh.km.keyFor(A_CHORD_LAYER, false)), 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
}
