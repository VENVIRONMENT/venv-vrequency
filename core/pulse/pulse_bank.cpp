// venv-vrequency — the pulse pads on the audio thread (see pulse_bank.h). No allocation, no locks.
#include "core/pulse/pulse_bank.h"
#include <algorithm>
#include <cmath>

namespace td {

void PulseBank::init(float sampleRate) {
    sr = sampleRate;
    for (int p = 0; p < kPulsePads; ++p) pad[p] = pulseDefault(p);
}

float PulseBank::rateOf(int p, float bpm) const {
    return sync ? pulseSyncRate(pad[p].rate, bpm) : pad[p].rate;
}

bool PulseBank::active() const {
    for (const PulseVoice &x : v) if (x.active()) return true;
    for (int p = 0; p < kPulsePads; ++p) if (going(p)) return true;
    return false;
}

// Live (part 0) when the player keeps it going, else the first loop layer holding it.
int PulseBank::ownerOf(int p) const {
    if (latched[p] || held[p] || sustained[p] || !loopHeld[p]) return 0;
    int l = 0;
    while (!(loopHeld[p] & (1u << l))) ++l;
    return 2 + l;
}

void PulseBank::start(int p, float velocity, float bpm, bool endless, int delay, int owner) {
    for (PulseVoice &x : v) // the same pad again: the hit before fades instead of piling up
        if (x.active() && x.pad() == p && !x.releasing()) x.release(1.0f);
    int slot = -1;
    for (int i = 0; i < kPulseVoices; ++i) if (!v[i].active()) { slot = i; break; }
    if (slot < 0) { // all busy: take the one furthest through its sound
        slot = 0;
        for (int i = 1; i < kPulseVoices; ++i) if (v[i].age() > v[slot].age()) slot = i;
    }
    v[slot].start(p, pad[p], sr, rateOf(p, bpm), endless, delay, owner < 0 ? ownerOf(p) : owner);
    vel[slot] = clampf(velocity, 0, 1.5f);
    ++hits[p];
}

void PulseBank::trigger(int p, float velocity, float bpm, int owner) {
    if (p < 0 || p >= kPulsePads) return;
    start(p, velocity, bpm, false, 0, owner);
}

void PulseBank::set(int p, int field, float val, float bpm) {
    if (p < 0 || p >= kPulsePads || !std::isfinite(val)) return;
    PulsePad &d = pad[p];
    switch (field) {
    case PF_TYPE: d.type = (int)val; break;
    case PF_TONE: d.tone = val; break;
    case PF_LENGTH: d.lengthMs = val; break;
    case PF_RATE: d.rate = val; break;
    case PF_LEVEL: d.level = val; break;
    default: return;
    }
    pulseClamp(d);
    for (PulseVoice &x : v) // a latched pad that runs on follows the change at once
        if (x.active() && x.endless() && x.pad() == p && !x.releasing()) x.retune(d, rateOf(p, bpm));
}

void PulseBank::begin(int p, double beat, float bpm) {
    if (runsOn(p)) { start(p, 1.0f, bpm, true, 0); return; }
    start(p, 1.0f, bpm, false, 0); // the first hit is now; the rest follow at the rate
    float perBeat = 1;
    const float r = sync ? pulseSyncRate(pad[p].rate, bpm, &perBeat) : pad[p].rate;
    nextHit[p] = sync ? (std::floor(beat * perBeat + 1e-9) + 1) / perBeat : sr / r;
}

void PulseBank::stop(int p) {
    for (PulseVoice &x : v) if (x.active() && x.pad() == p && x.endless()) x.release(3.0f);
}

void PulseBank::follow(int p, bool was, double beat, float bpm) {
    const bool now = going(p);
    if (now && !was) begin(p, beat, bpm);
    else if (!now && was) stop(p);
}

void PulseBank::latch(int p, bool on, double beat, float bpm) {
    if (p < 0 || p >= kPulsePads) return;
    const bool was = going(p);
    latched[p] = on;
    follow(p, was, beat, bpm);
}

void PulseBank::hold(int p, bool on, double beat, float bpm) {
    if (p < 0 || p >= kPulsePads) return;
    const bool was = going(p);
    if (on) { held[p] = true; sustained[p] = false; if (was) start(p, 1.0f, bpm, runsOn(p), 0); }
    else if (held[p]) { held[p] = false; if (sustainOn) sustained[p] = true; }
    follow(p, was, beat, bpm);
}

void PulseBank::setSustain(bool on, double beat, float bpm) {
    sustainOn = on;
    if (on) return;
    for (int p = 0; p < kPulsePads; ++p)
        if (sustained[p]) { const bool was = going(p); sustained[p] = false; follow(p, was, beat, bpm); }
}

void PulseBank::loopHold(int p, int layer, bool on, double beat, float bpm) {
    if (p < 0 || p >= kPulsePads || layer < 0 || layer >= 32) return;
    const bool was = going(p);
    if (on) loopHeld[p] |= 1u << layer; else loopHeld[p] &= ~(1u << layer);
    follow(p, was, beat, bpm);
}

void PulseBank::dropLoop(int layer) {
    if (layer < 0 || layer >= 32) return;
    for (int p = 0; p < kPulsePads; ++p)
        if (loopHeld[p] & (1u << layer)) { const bool was = going(p); loopHeld[p] &= ~(1u << layer); follow(p, was, 0, 120); }
}

void PulseBank::silence() {
    for (int p = 0; p < kPulsePads; ++p) { latched[p] = held[p] = sustained[p] = false; loopHeld[p] = 0; }
    sustainOn = false;
    for (PulseVoice &x : v) x.release(3.0f);
}

void PulseBank::tickLatches(int n, double beat0, double beat1, float bpm) {
    for (int p = 0; p < kPulsePads; ++p) {
        if (!going(p)) continue;
        if (runsOn(p)) {
            bool running = false;
            for (const PulseVoice &x : v) if (x.active() && x.endless() && x.pad() == p && !x.releasing()) running = true;
            if (!running) start(p, 1.0f, bpm, true, 0);
            continue;
        }
        if (sync) { // on the grid of the beat clock
            float perBeat = 1;
            pulseSyncRate(pad[p].rate, bpm, &perBeat);
            const double step = 1.0 / perBeat;
            if (nextHit[p] < beat0 - step || nextHit[p] > beat1 + step) // tempo, rate or mode changed
                nextHit[p] = std::ceil(beat0 * perBeat - 1e-9) / perBeat;
            while (nextHit[p] < beat1) {
                const int delay = std::clamp((int)((nextHit[p] - beat0) / (beat1 - beat0) * n), 0, n - 1);
                start(p, 1.0f, bpm, false, delay);
                nextHit[p] += step;
            }
        } else { // free: every sr / rate samples, to the sample
            const double period = sr / pad[p].rate;
            if (nextHit[p] > period || nextHit[p] < 0) nextHit[p] = std::min(std::max(nextHit[p], 0.0), period);
            while (nextHit[p] < n) {
                start(p, 1.0f, bpm, false, (int)nextHit[p]);
                nextHit[p] += period;
            }
            nextHit[p] -= n;
        }
    }
}

void PulseBank::logTick(uint64_t frame, uint32_t dur, int p) {
    const uint32_t w = ringW.load(std::memory_order_relaxed);
    PulseTick &t = ring[w & (kPulseTickRing - 1)];
    t.frame = frame; t.dur = dur; t.pad = (uint8_t)p;
    ringW.store(w + 1, std::memory_order_release);
}

void PulseBank::render(float *out, int n, double beat0, double beat1, float bpm, uint64_t frame0, const float *bend) {
    tickLatches(n, beat0, beat1, bpm);
    for (int k = 0; k < kPulseVoices; ++k) {
        PulseVoice &x = v[k];
        if (!x.active()) continue;
        x.setBend(bend ? bend[x.owner()] : 0.0f);
        const float g = vel[k];
        bool onset; int dur;
        for (int i = 0; i < n && x.active(); ++i) {
            out[i] += x.next(onset, dur) * g;
            if (onset) logTick(frame0 + (uint64_t)i, (uint32_t)dur, x.pad());
        }
    }
}

int PulseBank::readTicks(uint32_t &cursor, PulseTick *dst, int max) const {
    const uint32_t w = ringW.load(std::memory_order_acquire);
    if (w - cursor > (uint32_t)kPulseTickRing) cursor = w - kPulseTickRing;
    if (w - cursor > (uint32_t)max) cursor = w - (uint32_t)max;
    int n = 0;
    for (; cursor != w; ++cursor) dst[n++] = ring[cursor & (kPulseTickRing - 1)];
    return n;
}

} // namespace td
