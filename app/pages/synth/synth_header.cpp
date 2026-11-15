// venv-vrequency — the Synth page's header: the lead sound, the key, tempo and octave, the output level.
#include "app/pages/synth/synth_page.h"
#include "app/shell/recorder.h"
#include "app/sounds/sounds.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <cmath>

using namespace td;
using namespace pal;

void SynthPage::paintHeader() {
    const Tuning &t = u.set.tuning;
    const int L = Qt::AlignLeft | Qt::AlignVCenter, R = Qt::AlignRight | Qt::AlignVCenter;
    const Recorder &rec = sh.recorderNow();
    ui.txt(QRectF(18, 6, 430, 44), ui.fit(sounds.lead.name, 30, 428), 30, INK, L, true);
    QString slotTxt = sounds.curSlot >= 0 ? QString("Sound %1").arg(sounds.curSlot + 1) : QString("Unsaved sound");
    ui.txt(QRectF(20, 52, 230, 20), QString("%1   %2").arg(slotTxt, u.set.poly ? "Poly" : "Strings"), 13, MID, L); // the keyboard mode (Ctrl+P)
    if (rec.recording()) {
        if ((sh.clock.elapsed() / 400) % 2 == 0) ui.tag(QRectF(250, 54, 104, 17), QString("REC %1 s").arg(rec.seconds()), 12);
    } else if (sh.noAudioDevice()) {
        ui.tag(QRectF(250, 54, 150, 17), "No audio device", 12);
    } else if (u.limiter < 0.85f) {
        ui.tag(QRectF(250, 54, 60, 17), "Limit", 12);
    }
    ui.zone(QRectF(16, 4, 430, 72), "hdr_sound", [this](QPointF, int ph) { if (ph == PH_PRESS) sh.goPage(PG_SOUND); });

    ui.g->fillRect(QRectF(444, 10, 1, 60), GRAT);
    ui.txt(QRectF(456, 6, 60, 44), noteName(t.root), 32, INK, L, true);
    ui.txt(QRectF(456, 52, 110, 20), QString(scaleDef(t.scale).shortName).toLower(), 13, MID, L);
    ui.txt(QRectF(552, 6, 86, 44), QString::number((int)std::round(u.set.bpm)), 28, INK, R, true);
    ui.txt(QRectF(552, 52, 86, 20), "bpm", 13, MID, R);
    double frac = u.beat - std::floor(u.beat);
    bool running = u.backPlaying || u.loopState != L_EMPTY || u.pulsing;
    ui.frame(QRectF(552, 24, 9, 9), running ? INK : LOW, running && frac < 0.25 ? INK : BG); // beat lamp
    ui.txt(QRectF(644, 6, 60, 44), QString("%1%2").arg(t.octave > 0 ? "+" : "").arg(t.octave), 28, INK, R, true);
    ui.txt(QRectF(644, 52, 60, 20), "oct", 13, MID, R);
    ui.zone(QRectF(450, 4, 254, 72), "hdr_tune", [this](QPointF, int ph) { if (ph == PH_PRESS) sh.goPage(PG_TUNE); });
    // the readouts inside it, named so they can be pointed at; they open the Tune page too
    for (const auto &rd : {std::make_pair(QRectF(452, 4, 96, 72), "key_readout"), std::make_pair(QRectF(550, 4, 90, 72), "tempo_readout"), std::make_pair(QRectF(642, 4, 62, 72), "octave_readout")})
        ui.zone(rd.first, rd.second, [this](QPointF, int ph) { if (ph == PH_PRESS) sh.goPage(PG_TUNE); });

    // output level: a row of segments
    int lit = (int)std::ceil(qMin(1.0f, u.peak) * 86);
    for (int i = 0; i < 86; ++i) ui.g->fillRect(QRectF(16 + i * 8, 80, 6, 4), i < lit ? INK : GRAT);
}
