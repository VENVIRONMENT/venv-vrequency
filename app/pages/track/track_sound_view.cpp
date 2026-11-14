// venv-vrequency — the Track page: drawing the sound picker.
#include "app/pages/track/track_page.h"
#include "app/pages/track/track_layout.h"
#include "app/ui/palette.h"
#include <QPainter>

using namespace td;
using namespace pal;

void TrackPage::paintClipSound() {
    const int L = Qt::AlignLeft | Qt::AlignVCenter;
    if (selLane != soundForLane || selIdx != soundForIdx) soundSetup(); // another clip was chosen
    Placement &pl = *chosen();
    const Clip &c = *track.clips[pl.clip];
    const bool beat = c.kind == CK_BEAT;
    if (beat) soundKind = PK_DRUMS;
    else if (soundKind == PK_DRUMS) soundKind = PK_SYNTH;
    const SoundPick &pk = pickFor(c, pl);
    const bool only = pl.pick.own || pl.pick.any();
    auto shown = [beat](const char *s) { return beat ? nice(s) : QString::fromUtf8(s); };
    const QString name = QString::fromUtf8(c.name), now = shown(soundName(c, pk)), rec = shown(recordedSoundName(c));
    ui.section(414, pk.on || pk.kit >= 0 ? QString("Sound for %1, %2   plays %3, recorded as %4").arg(name).arg(tl::barsText(pl.bar, pl.bars)).arg(now).arg(rec)
                                      : QString("Sound for %1, %2   plays %3, as recorded").arg(name).arg(tl::barsText(pl.bar, pl.bars)).arg(rec));

    // kinds of sound
    static const char *KN[PK_COUNT] = {"Keys", "Synth", "Guitar", "Bass", "Drums"};
    const QString KS[PK_COUNT] = {"pianos organs", QString("your %1 sounds").arg(soundCount(PK_SYNTH)), "plucked driven", "low end", "three kits"};
    const qreal kw = (688 - 4 * 6) / 5.0;
    for (int k = 0; k < PK_COUNT; ++k) {
        const QRectF r(std::floor(16 + k * (kw + 6)), 438, std::floor(kw), 52);
        const bool fits = beat ? k == PK_DRUMS : k != PK_DRUMS;
        if (fits) {
            ui.button(r, QString("sound_kind%1").arg(k), KN[k], soundKind == k, [this, k] { if (soundKind != k) { soundKind = k; soundPage = 0; } }, KS[k]);
        } else { // not for this kind of clip
            ui.frame(r, GRAT, BG);
            ui.txt(QRectF(r.x(), r.y() + r.height() * 0.13, r.width(), r.height() * 0.46), KN[k], 16, LOW, Qt::AlignCenter, true);
            ui.txt(QRectF(r.x() + 2, r.y() + r.height() * 0.58, r.width() - 4, r.height() * 0.32), beat ? "notes only" : "beat clips only", 11, LOW);
        }
    }

    // six cells: the sounds of that kind, or five and a way to the next page
    const int n = soundCount(soundKind), per = tl::cellsPerPage(n), pages = qMax(1, (n + per - 1) / per);
    soundPage = qBound(0, soundPage, pages - 1);
    const qreal cw = (688 - 2 * 8) / 3.0;
    for (int cell = 0; cell < 6; ++cell) {
        const QRectF r(std::floor(16 + (cell % 3) * (cw + 8)), 498 + (cell / 3) * 60, std::floor(cw), 54);
        const QString zn = QString("sound_cell%1").arg(cell);
        const int i = soundPage * per + cell;
        QString label, sub;
        bool on = false;
        if (n > 6 && cell == 5) { label = "More"; sub = QString("page %1 of %2").arg(soundPage + 1).arg(pages); }
        else if (i >= n) { ui.frame(r, GRAT, BG); continue; }
        else if (soundKind == PK_DRUMS) {
            label = nice(kitName(i)); sub = i == c.kit ? "as recorded" : "kit";
            on = i == (pk.kit >= 0 ? pk.kit : c.kit);
        } else if (soundKind == PK_SYNTH) {
            label = QString::fromUtf8(soundPatch(PK_SYNTH, i).name);
            sub = i < 10 ? QString("slot %1").arg((i + 1) % 10) : QString("favourite");
            on = label == now;
        } else {
            label = QString::fromUtf8(librarySound(tl::libKind(soundKind), i).name);
            sub = libraryHint(tl::libKind(soundKind), i);
            on = pk.on && label == now;
        }
        ui.button(r, zn, label, on, [this, cell] { soundPickAt(cell); }, sub);
        ui.txt(QRectF(r.x() + 5, r.y() + 3, 12, 14), QString::number(cell + 1), 11, on || ui.isDown(zn) ? BG : LOW, L);
    }

    const qreal y = 622, h = 56;
    ui.button(QRectF(16, y, 130, h), "sound_hear", hearOn ? QString::fromUtf8("■ Stop") : QString::fromUtf8("▶ Hear"), hearOn, [this] { hearToggle(); }, "this clip alone");
    const QRectF orr(154, y, 164, h);
    if (beat) {
        ui.frame(orr, GRAT, BG);
        ui.txt(QRectF(orr.x(), orr.y() + 6, orr.width(), 16), "Octave", 12, LOW);
        ui.txt(QRectF(orr.x(), orr.y() + 22, orr.width(), h - 26), "notes only", 13, LOW);
    } else {
        ui.stepper(orr, "sound_octave", "Octave", pk.octave > 0 ? QString("+%1").arg(pk.octave) : QString::number(pk.octave),
                [this] { soundApply([](SoundPick &k) { k.octave = qMax(-2, k.octave - 1); }); },
                [this] { soundApply([](SoundPick &k) { k.octave = qMin(2, k.octave + 1); }); });
    }
    ui.button(QRectF(326, y, 130, h), "sound_scope", only ? QString("Only this one") : QString("Every %1").arg(name), false, [this] { soundScope(); },
           only ? QString("tap for every one") : QString("tap for only this"));
    ui.button(QRectF(464, y, 130, h), "sound_as_recorded", "As recorded", false, [this] { soundApply([](SoundPick &k) { k = SoundPick(); }); }, rec);
    ui.button(QRectF(602, y, 102, h), "sound_done", "Done", false, [this] { soundOpen = false; }, "back to tools");
    ui.txt(QRectF(18, 688, 684, 20), beat ? QString("Keys: 1-3 pick a kit, %1 hear it, %2 done.").arg(sh.km.keyFor(A_SUSTAIN, false)).arg(sh.km.keyFor(A_LOOP, false))
                                       : QString("Keys: left right pick a kind, 1-6 a sound, up down the octave, %1 hear it, %2 done.").arg(sh.km.keyFor(A_SUSTAIN, false)).arg(sh.km.keyFor(A_LOOP, false)),
        12, MID, L);
}
