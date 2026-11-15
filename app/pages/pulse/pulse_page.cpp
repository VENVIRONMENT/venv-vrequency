// venv-vrequency — the Pulse page (see pulse_page.h): its keys, taps, LATCH, TYPE and the soft keys.
#include "app/pages/pulse/pulse_page.h"
#include "app/ui/palette.h"
#include "app/ui/zones.h"
#include <QJsonObject>
#include <cmath>

using namespace td;
using namespace pal;

// On this page the two upper letter rows are the pads; the low string still plays.
bool PulsePage::key(int qtKey, int plainAct) {
    if (sh.keys.command) return false;       // Ctrl+key belongs to the command layer
    int pad, r, c;
    if (sh.km.pulseKey(qtKey, pad)) { hitPad(pad); return true; }
    if (sh.km.stringKey(qtKey, r, c) && r != 0) return true;   // between the pad keys: nothing
    if (plainAct == A_BEND_DOWN || plainAct == A_BEND_UP) {    // the pad before or after
        select((sel + (plainAct == A_BEND_UP ? 1 : kPulsePads - 1)) % kPulsePads);
        return true;
    }
    return false;                            // Enter is the loop pedal, as everywhere
}

void PulsePage::hitPad(int pad) {
    if (pad < 0 || pad >= kPulsePads) return;
    if (pad != sel) { sel = pad; sh.saveSoon(); }
    if (u.pulseLatched[pad]) { setLatch(pad, false); return; } // tapped again: a latched pad lets go
    inst.hit(pad);
    padLit[pad] = sh.clock.elapsed();
    sh.redraw();
}

void PulsePage::setLatch(int pad, bool on) {
    if (pad < 0 || pad >= kPulsePads) return;
    sel = pad;
    inst.pulseLatch(pad, on);
    sh.redraw();
}

void PulsePage::select(int pad) {
    if (pad < 0 || pad >= kPulsePads || pad == sel) return;
    sel = pad;
    sh.saveSoon();
    sh.redraw();
}

void PulsePage::cycleType() {
    // taps faster than the engine's state reaches the page step on from the kind last asked for
    const qint64 now = sh.clock.elapsed();
    const int from = typePad == sel && now - typeAt < 250 ? typeSent : u.pulse[sel].type;
    typePad = sel; typeSent = (from + 1) % kPulseTypes; typeAt = now;
    setField(PF_TYPE, (float)typeSent);
    sh.toast(QString("Pad %1: %2").arg(sel + 1).arg(pulseTypeName(typeSent)), 900);
}

void PulsePage::setField(int field, float v) {
    inst.pulseSet(sel, field, v);
    sh.saveSoon();
}

float PulsePage::rateNow(int pad) const {
    return u.set.pulseSync ? pulseSyncRate(u.pulse[pad].rate, u.set.bpm) : u.pulse[pad].rate;
}

void PulsePage::load(const QJsonObject &st) { sel = qBound(0, st["pulseSelected"].toInt(0), kPulsePads - 1); }
void PulsePage::save(QJsonObject &o) const { o["pulseSelected"] = sel; }

void PulsePage::paint() {
    const qint64 now = sh.clock.elapsed();
    for (int p = 0; p < kPulsePads; ++p)
        if (u.pulseHits[p] != seenHits[p]) { seenHits[p] = u.pulseHits[p]; padLit[p] = now; }
    const KeyMap &km = sh.km;
    auto four = [](const QString &row) { return row.left(4).split("", Qt::SkipEmptyParts).join(" "); };
    ui.section(60, QString("Pulse  8 pads   tap, or keys %1 / %2").arg(four(km.rows[2]), four(km.rows[1])));
    paintPads(now);
    paintSelected(306);
    ui.section(530, "Last 4 s  every hit, one lane per pad (oldest left)");
    paintStrip(QRectF(16, 552, 688, 82));
    paintSoftKeys(QRectF(16, 642, 688, 70));
}

void PulsePage::paintSoftKeys(QRectF r) {
    const qreal w = (r.width() - 3 * 5) / 4.0;
    auto at = [&](int i) { return QRectF(std::floor(r.x() + i * (w + 5)), r.y(), std::floor(w), r.height()); };
    QString loopSub = "record pulses";
    bool loopOn = false;
    switch (u.loopState) {
    case L_COUNTIN: loopSub = QString("count-in %1").arg(qMax(1, u.countBeat)); loopOn = true; break;
    case L_REC: loopSub = QString("recording, bar %1").arg((int)u.loopPos + 1); loopOn = true; break;
    case L_PLAY: loopSub = u.loopClosing ? QString("closing the bar") : QString("playing, tap = dub"); loopOn = u.loopClosing; break;
    case L_OVERDUB: loopSub = "overdub, tap = done"; loopOn = true; break;
    case L_STOPPED: loopSub = "stopped, tap = play"; break;
    default: break;
    }
    ui.button(at(0), zn::pulseloop, "Loop", loopOn, [this] { sh.perform(A_LOOP); }, loopSub,
              u.loopState == L_PLAY && !u.loopClosing ? LOOK_ARMED : LOOK_PLAIN);
    const bool lat = u.pulseLatched[sel];
    ui.button(at(1), zn::pulselatch, "Latch", lat, [this, lat] { setLatch(sel, !lat); },
              lat ? QString("pad %1 keeps going").arg(sel + 1) : QString("pad keeps going"));
    float perBeat = 1;
    pulseSyncRate(u.pulse[sel].rate, u.set.bpm, &perBeat);
    ui.button(at(2), zn::pulsesync, "Sync", u.set.pulseSync, [this] { sh.set(S_PULSESYNC, u.set.pulseSync ? 0 : 1); sh.saveSoon(); },
              u.set.pulseSync ? QString("%1 bpm").arg((int)std::round(u.set.bpm)) : QString("rate = tempo"));
    ui.button(at(3), zn::pulsetype, "Type", false, [this] { cycleType(); },
              QString("next: %1").arg(nice(pulseTypeName((u.pulse[sel].type + 1) % kPulseTypes))));
}
