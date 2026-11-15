// venv-vrequency — the Pulse page's eight pads, and the selected pad: its picture and four sliders.
#include "app/pages/pulse/pulse_page.h"
#include "app/ui/palette.h"
#include "app/ui/zones.h"
#include "core/pulse/pulse_voice.h"
#include <QPainter>
#include <cmath>

using namespace td;
using namespace pal;

namespace {
QString hz(float f) {
    if (f >= 10000) return QString("%1 kHz").arg(f / 1000, 0, 'f', 0);
    if (f >= 1000) return QString("%1 kHz").arg(f / 1000, 0, 'f', 1);
    if (f >= 10) return QString("%1 Hz").arg((int)std::round(f));
    return QString("%1 Hz").arg(f, 0, 'f', f < 1 ? 2 : 1);
}
QString ms(double m) {
    if (m < 1) return QString("%1 ms").arg(m, 0, 'f', 2);
    if (m < 10) return QString("%1 ms").arg(m, 0, 'f', 1);
    return QString("%1 ms").arg((int)std::round(m));
}
QString division(float perBeat) { // hits per beat as a note value
    if (perBeat < 0.25f) return QString("%1 bars").arg((int)std::round(0.25f / perBeat));
    if (perBeat == 0.25f) return "1 bar";
    return QString("1/%1").arg((int)std::round(perBeat * 4));
}
// sliders: tone, length and rate move in octaves, level in a straight line
float toNorm(float v, float lo, float hi) { return std::log(v / lo) / std::log(hi / lo); }
float fromNorm(float n, float lo, float hi) { return lo * std::pow(hi / lo, qBound(0.0f, n, 1.0f)); }
QString what(const PulsePad &p, int onsets, float sr) { // the top right of the big picture
    switch (p.type) {
    case PT_TICK: return QString("%1 sample%2").arg(PulseVoice::tickWidth(p.tone, sr)).arg(PulseVoice::tickWidth(p.tone, sr) == 1 ? "" : "s");
    case PT_NOISE: return QString("low-pass %1").arg(hz(p.tone));
    case PT_BLIP: return hz(p.tone);
    case PT_GATED: return QString("%1 gate%2").arg(onsets).arg(onsets == 1 ? "" : "s");
    case PT_TRAIN: return QString("%1 click%2").arg(onsets).arg(onsets == 1 ? "" : "s");
    case PT_THUMP: return QString("%1 > %2").arg((int)std::round(p.tone * 4)).arg(hz(p.tone));
    case PT_HISS: return QString("band %1").arg(hz(p.tone));
    default: return QString("ring %1").arg(hz(p.tone));
    }
}
} // namespace

void PulsePage::paintPads(qint64 now) {
    const float sr = inst.sampleRate();
    for (int p = 0; p < kPulsePads; ++p) {
        const QRectF r(16 + (p % 4) * 173, 84 + (p / 4) * 111, 168, 106);
        const bool lit = now - padLit[p] < 110 || ui.isDown(zn::at(zn::pad, p)), on = p == sel;
        ui.frame(r, on ? INK : LOW, lit ? GRAT : BG);
        if (on) ui.frame(r.adjusted(1, 1, -1, -1), INK);
        if (u.pulseLatched[p]) ui.frame(r.adjusted(4, 4, -4, -4), INK);
        ui.txt(QRectF(r.x() + 10, r.y() + 7, 30, 16), QString::number(p + 1), 12, on ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter, true);
        ui.txt(QRectF(r.x() + 40, r.y() + 7, r.width() - 50, 16), u.pulseLatched[p] ? QString("latch") : sh.km.pulseKeys(p), 12,
               u.pulseLatched[p] ? INK : LOW, Qt::AlignRight | Qt::AlignVCenter, true);
        const QRect pic((int)r.x() + 10, (int)r.y() + 27, (int)r.width() - 20, 44);
        ui.g->drawImage(pic.topLeft(), small[p].get(u.pulse[p], rateNow(p), sr, pic.size(), false));
        ui.txt(QRectF(r.x() + 4, r.bottom() - 30, r.width() - 8, 22), pulseTypeName(u.pulse[p].type), 14, on ? INK : MID, Qt::AlignCenter, true);
        ui.zone(r, zn::at(zn::pad, p), [this, p](QPointF, int ph) { if (ph == PH_PRESS) hitPad(p); });
    }
}

void PulsePage::paintSelected(qreal y) {
    const PulsePad &d = u.pulse[sel];
    const float sr = inst.sampleRate();
    ui.section(y, QString("Pad %1  %2   %3").arg(sel + 1).arg(pulseTypeName(d.type)).arg(nice(pulseTypeLine(d.type))));
    const QRectF box(16, y + 22, 340, 196);
    ui.frame(box, LOW, BG);
    const QRect pic((int)box.x() + 10, (int)box.y() + 28, (int)box.width() - 20, 136);
    const QImage &img = big.get(d, rateNow(sel), sr, pic.size(), true);
    ui.g->drawImage(pic.topLeft(), img);
    const double lenMs = PulseVoice::lengthSamples(d, sr) * 1000.0 / sr, winMs = big.samples() * 1000.0 / sr;
    ui.txt(QRectF(box.x() + 10, box.y() + 6, 150, 18), ms(lenMs), 13, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    ui.txt(QRectF(box.x() + 150, box.y() + 6, 180, 18), what(d, big.onsets(), sr), 13, INK, Qt::AlignRight | Qt::AlignVCenter, true);
    ui.g->fillRect(QRectF(pic.x(), pic.bottom() + 3, pic.width(), 1), GRAT);
    ui.txt(QRectF(box.x() + 10, box.bottom() - 26, 80, 18), "0", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.txt(QRectF(box.right() - 130, box.bottom() - 26, 120, 18), ms(winMs), 12, MID, Qt::AlignRight | Qt::AlignVCenter);
    ui.zone(box, zn::pulsewave, [](QPointF, int) {});
    paintSliders(QRectF(364, y + 22, 340, 196));
}

void PulsePage::paintSliders(QRectF r) {
    const PulsePad &d = u.pulse[sel];
    const float sr = inst.sampleRate();
    auto row = [&](int i) { return QRectF(r.x(), r.y() + i * 50, r.width(), 46); };
    QString toneText = hz(d.tone);
    if (d.type == PT_TICK) toneText += QString(", %1 smp").arg(PulseVoice::tickWidth(d.tone, sr));
    ui.slider(row(0), zn::pulsetone, "Tone", toNorm(d.tone, kPulseToneMin, kPulseToneMax), toneText,
              [this](float n) { setField(PF_TONE, fromNorm(n, kPulseToneMin, kPulseToneMax)); });
    ui.slider(row(1), zn::pulselength, "Length", toNorm(d.lengthMs, kPulseLenMin, kPulseLenMax),
              pulseUsesLength(d.type) ? ms(d.lengthMs) : QString("not used"),
              [this](float n) { setField(PF_LENGTH, fromNorm(n, kPulseLenMin, kPulseLenMax)); });
    QString rateText = hz(d.rate);
    if (u.set.pulseSync) { float k = 1; rateText = hz(pulseSyncRate(d.rate, u.set.bpm, &k)) + " " + division(k); }
    ui.slider(row(2), zn::pulserate, pulseContinuous(d.type) ? "Rate" : "Rate (latch repeat)", toNorm(d.rate, kPulseRateMin, kPulseRateMax), rateText,
              [this](float n) { setField(PF_RATE, fromNorm(n, kPulseRateMin, kPulseRateMax)); });
    ui.slider(row(3), zn::pulselevel, "Level", d.level, QString("%1%").arg((int)std::round(d.level * 100)),
              [this](float n) { setField(PF_LEVEL, qBound(0.0f, n, 1.0f)); });
}
