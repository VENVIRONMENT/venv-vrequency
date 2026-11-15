// venv-vrequency — the Freq page (see freq_page.h): its keys, actions, section line and soft keys.
#include "app/pages/freq/freq_page.h"
#include "app/ui/palette.h"
#include "app/ui/zones.h"
#include "core/pulse/pulse.h"
#include "core/synth/scale.h"
#include <QJsonObject>
#include <cmath>

using namespace td;
using namespace pal;

namespace {
// GATE's free rates, Hz: a tap of + or - moves one along
const float kRates[] = {0.25f, 0.5f, 1, 1.5f, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 25, 30, 40, 50};
} // namespace

// Arrows move the selected tone, 1-4 select one, and with KEYS on the letters set tone 1. While an Hz is
// being typed the digits, Backspace, Enter and Esc go to it first.
// Everything else (Ctrl, Enter, Backspace, Space, Tab, Esc, 5-0) is left to the window.
bool FreqPage::key(int qtKey, int plainAct) {
    if (sh.keys.command) return false;       // Ctrl+key belongs to the command layer
    if (hzEdit >= 0 && hzKey(qtKey)) return true;
    const bool shift = sh.keys.chromatic;
    if (plainAct == A_BEND_DOWN || plainAct == A_BEND_UP) {
        setField(sel, FF_NUDGE, (plainAct == A_BEND_UP ? 1.0f : -1.0f) * (shift ? 10.0f : 0.5f));
        return true;
    }
    if (plainAct == A_OCT_DOWN || plainAct == A_OCT_UP) { setField(sel, FF_SEMI, plainAct == A_OCT_UP ? 1 : -1); return true; }
    if (plainAct >= A_SOUND_1 && plainAct <= A_SOUND_4) { select(plainAct - A_SOUND_1); return true; }
    int row, col;
    if (u.freq.keys && sh.km.stringKey(qtKey, row, col)) {
        const int midi = u.set.tuning.noteFor(row, col, shift);
        inst.freqSet(-1, FF_ROOT, (float)midiToHz(midi));
        if (heldKeys.isEmpty()) inst.freqSet(-1, FF_KEYHELD, 1);
        heldKeys.insert(qtKey);
        sh.saveSoon();
        sh.redraw();
        return true;
    }
    return false;
}

void FreqPage::typeHz(int t) {
    if (t < 0 || t >= kFreqTones) return;
    select(t);
    hzEdit = t;
    hzText.clear();
    sh.redraw();
}

// Typing an Hz: digits and a point add, Backspace takes one off, Enter sets it, Esc leaves it.
// Any other key ends the typing and then does what it always does.
bool FreqPage::hzKey(int k) {
    const QString tx = sh.keys.text;
    if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        bool ok = false;
        const float v = hzText.toFloat(&ok);
        const int t = hzEdit;
        hzEdit = -1;
        if (ok && v > 0) {
            const float hz = qBound(kFreqHzMin, v, kFreqHzMax);
            setField(t, FF_HZ, hz);
            sh.toast(QString("Tone %1: %2").arg(t + 1).arg(freqHz(hz)), 900);
        }
        sh.redraw();
        return true;
    }
    if (k == Qt::Key_Escape) { hzEdit = -1; sh.redraw(); return true; }
    if (k == Qt::Key_Backspace) { hzText.chop(1); sh.redraw(); return true; }
    const bool digit = (k >= Qt::Key_0 && k <= Qt::Key_9) || (tx.size() == 1 && tx[0].isDigit());
    const bool point = k == Qt::Key_Period || tx == ".";
    if (digit || (point && !hzText.contains('.'))) {
        if (hzText.size() < 8) hzText += digit ? (tx.size() == 1 && tx[0].isDigit() ? tx : QString(QChar('0' + (k - Qt::Key_0)))) : QString(".");
        sh.redraw();
        return true;
    }
    if (point) return true;
    hzEdit = -1;
    sh.redraw();
    return false;
}

// Every frame: a letter key let go (the window keeps every key it saw go down until it comes up).
void FreqPage::tick() {
    if (heldKeys.isEmpty()) return;
    for (auto it = heldKeys.begin(); it != heldKeys.end();)
        it = sh.keys.held.contains(*it) ? std::next(it) : heldKeys.erase(it);
    if (heldKeys.isEmpty()) inst.freqSet(-1, FF_KEYHELD, 0);
}

void FreqPage::select(int t) {
    if (t < 0 || t >= kFreqTones || t == sel) return;
    sel = t;
    sh.saveSoon();
    sh.redraw();
}

void FreqPage::tapRow(int t) {
    if (t < 0 || t >= kFreqTones) return;
    select(t);
    if (!u.freq.tone[t].on) setField(t, FF_ON, 1);
}

void FreqPage::cycleWave(int t) {
    if (t < 0 || t >= kFreqTones) return;
    select(t);
    const int w = (u.freq.tone[t].wave + 1) % kFreqWaves;
    setField(t, FF_WAVE, (float)w);
    sh.toast(QString("Tone %1: %2").arg(t + 1).arg(nice(freqWaveName(w))), 900);
}

void FreqPage::setField(int t, int field, float v) {
    inst.freqSet(t, field, v);
    sh.saveSoon();
    sh.redraw();
}

void FreqPage::toggle(int field) {
    const FreqState &f = u.freq;
    const bool on = field == FF_HOLD ? f.hold : field == FF_GATE ? f.gate : f.keys;
    setField(-1, field, on ? 0 : 1);
    if (field == FF_KEYS && on && !heldKeys.isEmpty()) heldKeys.clear();
}

void FreqPage::stepRate(int dir) {
    const float r = u.freq.rate;
    float next = r;
    if (u.set.pulseSync) { // a tempo division up or down
        const float now = pulseSyncRate(r, u.set.bpm);
        for (float k = 1.2f; k < 8 && next == r; k *= 1.2f) {
            const float t = pulseSyncRate(dir > 0 ? now * k : now / k, u.set.bpm);
            if (std::fabs(t - now) > 1e-3f) next = t;
        }
    } else if (dir > 0) {
        for (float x : kRates) if (x > r + 1e-3f) { next = x; break; }
    } else {
        for (float x : kRates) if (x < r - 1e-3f) next = x;
    }
    setField(-1, FF_RATE, next);
}

void FreqPage::load(const QJsonObject &st) { sel = qBound(0, st["freqSelected"].toInt(0), kFreqTones - 1); }
void FreqPage::save(QJsonObject &o) const { o["freqSelected"] = sel; }

QString FreqPage::sectionLine() const {
    int i = -1, j = -1;
    const float b = freqBeat(u.freq, &i, &j);
    if (b <= 0) return "Freq  4 tones   no beat: no two tones within 20 Hz";
    return QString("Freq  4 tones  tone %1 %2 / tone %3 %4  beat %5 Hz").arg(i + 1).arg(freqHz(u.freq.tone[i].hz))
        .arg(j + 1).arg(freqHz(u.freq.tone[j].hz)).arg(b, 0, 'f', 2);
}

void FreqPage::paint() {
    buf.resize((size_t)(1.5 * inst.sampleRate()) + 64);
    inst.readFreqScope(buf.data(), (int)buf.size());
    ui.section(60, sectionLine());
    paintScope(QRectF(16, 84, 688, 170));
    paintCycles(QRectF(16, 260, 342, 74));
    paintSpectrum(QRectF(362, 260, 342, 74));
    ui.section(340, hzEdit >= 0 ? QString("Tone %1  type the Hz, Enter sets it, Esc leaves it").arg(hzEdit + 1)
                                : QString("Tones  tap an Hz to type it, drag it to scrub; arrows move the selected one"));
    for (int i = 0; i < kFreqTones; ++i) paintRow(i, QRectF(16, 362 + i * 42, 688, 38));
    paintWarp(530);
    paintSoftKeys(QRectF(16, 642, 688, 70));
}

void FreqPage::paintSoftKeys(QRectF r) {
    const FreqState &f = u.freq;
    const qreal w = std::floor((r.width() - 3 * 5) / 4.0);
    auto at = [&](int i) { return QRectF(std::floor(r.x() + i * (w + 5)), r.y(), w, r.height()); };
    ui.button(at(0), zn::freqhold, "Hold", f.hold, [this] { toggle(FF_HOLD); }, f.hold ? "sounding" : "tones keep going");
    float perBeat = 0;
    const float rate = u.set.pulseSync ? pulseSyncRate(f.rate, u.set.bpm, &perBeat) : f.rate;
    QString rt = QString("%1 Hz").arg(rate, 0, 'f', rate < 10 ? 2 : 1);
    if (u.set.pulseSync) rt = perBeat >= 0.25f ? QString("1/%1 note").arg((int)std::round(perBeat * 4)) : QString("%1 bars").arg((int)std::round(0.25f / perBeat));
    const QRectF g = at(1), gb(g.x(), g.y(), g.width() - 44, g.height());
    ui.button(gb, zn::freqgate, "Gate", f.gate, [this] { toggle(FF_GATE); }, rt);
    const QRectF up(gb.right() + 4, g.y(), 40, 33), dn(gb.right() + 4, g.y() + 37, 40, 33);
    ui.button(up, zn::plus(zn::freqrate), "+", false, [this] { stepRate(1); });
    ui.button(dn, zn::minus(zn::freqrate), QString::fromUtf8("−"), false, [this] { stepRate(-1); });
    const QString key = QString("%1 %2").arg(noteName(u.set.tuning.root)).arg(nice(scaleDef(u.set.tuning.scale).shortName));
    ui.button(at(2), zn::freqkeys, "Keys", f.keys, [this] { toggle(FF_KEYS); },
              f.keys ? (u.freqKeyHeld ? QString("held: %1").arg(freqHz(f.tone[0].hz)) : QString("letters: %1").arg(key)) : QString("letters set tone 1"));
    QString to;
    if (sel == 0) { char n[8]; freqNoteName(f.tone[0].hz, n, sizeof n); to = QString("tone 1 to %1").arg(n); }
    else {
        int p, q;
        freqRatio(freqSnapRatio(f.tone[sel].hz, f.tone[0].hz), f.tone[0].hz, p, q);
        to = QString("tone %1 to %2:%3").arg(sel + 1).arg(p).arg(q);
    }
    ui.button(at(3), zn::freqsnap, "Snap", false, [this] { snap(); }, to);
}
