// venv-vrequency — the Freq tones on the audio thread (see freq_bank.h). No allocation, no locks.
#include "core/freq/freq_bank.h"
#include "core/pulse/pulse.h"
#include <cmath>

namespace td {

namespace {
constexpr float kToneGain = 0.25f;     // four tones at full level sum to full scale at most
constexpr float kNoiseReso = 0.95f;    // NOISE: the band's resonance (Q 10)
float coef(float seconds, float sr) { return 1.0f - std::exp(-1.0f / (seconds * sr)); }
} // namespace

void FreqBank::init(float sampleRate) {
    sr = sampleRate;
    glideC = coef(0.006f, sr);   // ~20 ms to settle
    fadeC = coef(0.0015f, sr);   // ~5 ms
    edgeC = coef(0.0003f, sr);   // ~1 ms
    warp.init(sr);
    warp.set(s.warp);
    tap.assign(kFreqScope, 0.0f);
    for (int i = 0; i < kFreqTones; ++i) { o[i].hz = s.tone[i].hz; o[i].lvl = 0; }
}

float FreqBank::rateOf(float bpm, bool sync, float *perBeat) const {
    if (perBeat) *perBeat = 0;
    return sync ? pulseSyncRate(s.rate, bpm, perBeat) : s.rate;
}

void FreqBank::set(int t, int field, float v) {
    if (!std::isfinite(v)) return;
    const bool tone = field <= FF_SNAP;
    if (tone && (t < 0 || t >= kFreqTones)) return;
    FreqTone *x = tone ? &s.tone[t] : nullptr;
    switch (field) {
    case FF_ON: x->on = v > 0.5f; break;
    case FF_WAVE: x->wave = (int)v; break;
    case FF_HZ: x->hz = v; break;
    case FF_LEVEL: x->level = v; break;
    case FF_NUDGE: x->hz += v; break;
    case FF_SEMI: x->hz *= std::pow(2.0f, v / 12.0f); break;
    case FF_SNAP: x->hz = t == 0 ? freqSnapNote(x->hz) : freqSnapRatio(x->hz, s.tone[0].hz); break;
    case FF_MOD: s.mod = (int)v; break;
    case FF_MODAMT: s.modAmt = v; break;
    case FF_FOLD: s.warp.fold = v; break;
    case FF_FILTER: s.warp.cutoff = v; break;
    case FF_DRIVE: s.warp.drive = v; break;
    case FF_CRUSH: s.warp.crush = v; break;
    case FF_DELAY: s.delay = v; break;
    case FF_HOLD: s.hold = v > 0.5f; break;
    case FF_GATE: if (v > 0.5f && !s.gate) gatePh = 0; s.gate = v > 0.5f; break;
    case FF_RATE: s.rate = v; break;
    case FF_KEYS: s.keys = v > 0.5f; if (!s.keys) keyHeld = false; break;
    case FF_KEYHELD: keyHeld = v > 0.5f; break;
    case FF_ROOT: freqMoveRoot(s, v); break;
    default: return;
    }
    freqClamp(s);
    warp.set(s.warp);
}

void FreqBank::silence() { s.hold = s.gate = keyHeld = false; }

void FreqBank::restore(const FreqState &in) {
    s = in;
    freqClamp(s);
    s.hold = s.gate = keyHeld = false;
    warp.set(s.warp);
}

float FreqBank::wave(int i, float ph, float dt) {
    switch (s.tone[i].wave) {
    case FW_SINE: return fastSin(ph);
    case FW_TRI: return oscSample(1, ph, dt, 0.5f);
    case FW_SQUARE: return oscSample(3, ph, dt, 0.5f);
    case FW_SAW: return oscSample(2, ph, dt, 0.5f);
    default: { // NOISE: a narrow band at the tone's Hz, about as loud as a sine, never past +-1
        Svf &b = o[i].band;
        const float k = b.k, enbw = 1.5708f * o[i].hz * k;
        const float g = 0.35f / (0.577f * std::sqrt(enbw / (0.5f * sr)));
        return fastTanh(b.tick(rng.bipolar(), 2) * k / (0.6f + 0.4f * k) * g);
    }
    }
}

void FreqBank::write(const float *x, int n) {
    const uint32_t w = tapW.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i) tap[(w + i) & (kFreqScope - 1)] = x[i];
    tapW.store(w + n, std::memory_order_release);
}

void FreqBank::idle(int n) {
    float z[kBlock] = {0};
    for (int done = 0; done < n; done += kBlock) write(z, n - done < kBlock ? n - done : kBlock);
}

void FreqBank::render(float *out, int n, double beat0, double beat1, float bpm, bool sync) {
    const float want = wanted() ? 1.0f : 0.0f;
    float tHz[kFreqTones], tLvl[kFreqTones];
    for (int i = 0; i < kFreqTones; ++i) {
        tHz[i] = s.tone[i].hz; tLvl[i] = s.tone[i].on ? s.tone[i].level : 0;
        if (amp < 1e-5f) { o[i].hz = tHz[i]; o[i].lvl = tLvl[i]; } // from silence: no glide in
        if (s.tone[i].wave == FW_NOISE && std::fabs(o[i].bandHz - o[i].hz) > 0.01f) {
            o[i].band.set(o[i].hz, kNoiseReso, sr); o[i].bandHz = o[i].hz;
        }
    }
    float perBeat = 0;
    const float rate = rateOf(bpm, sync, &perBeat);
    const bool mod = s.mod != FM_OFF && s.tone[1].on;
    const float amt = s.modAmt;
    for (int k = 0; k < n; ++k) {
        float raw[kFreqTones];
        for (int i = 1; i < kFreqTones; ++i) { // tones 2-4, then tone 1 (tone 2 may bend it)
            Osc &x = o[i];
            x.hz += glideC * (tHz[i] - x.hz); x.lvl += glideC * (tLvl[i] - x.lvl);
            const float dt = x.hz / sr;
            raw[i] = clampf(wave(i, x.ph, dt), -1, 1);
            x.ph += dt; if (x.ph >= 1.0f) x.ph -= 1.0f;
        }
        Osc &a = o[0];
        a.hz += glideC * (tHz[0] - a.hz); a.lvl += glideC * (tLvl[0] - a.lvl);
        const float dt = a.hz / sr;
        float p = a.ph, dtEff = dt;
        if (mod && s.mod == FM_FM) { // phase modulation, up to 1.5 cycles each way
            p += amt * 1.5f * raw[1];
            p -= std::floor(p);
            float d = std::fabs(p - a.lastPh); if (d > 0.5f) d = 1.0f - d;
            dtEff = d > 1e-6f ? d : 1e-6f;
        }
        a.lastPh = p;
        raw[0] = clampf(wave(0, p, dtEff), -1, 1);
        if (mod && s.mod == FM_RING) raw[0] *= 1.0f - amt + amt * raw[1];
        a.ph += dt; if (a.ph >= 1.0f) a.ph -= 1.0f;
        float sum = 0;
        for (int i = 0; i < kFreqTones; ++i) sum += raw[i] * o[i].lvl * kToneGain;
        out[k] = sum;
    }
    warp.process(out, n);
    for (int k = 0; k < n; ++k) {
        float open = 1;
        if (s.gate) {
            if (sync && perBeat > 0) {
                const double b = (beat0 + (beat1 - beat0) * k / n) * perBeat;
                open = b - std::floor(b) < 0.5 ? 1.0f : 0.0f;
            } else {
                gatePh += rate / sr; if (gatePh >= 1.0) gatePh -= 1.0;
                open = gatePh < 0.5 ? 1.0f : 0.0f;
            }
        }
        amp += fadeC * (want - amp);
        gateAmp += edgeC * (open - gateAmp);
        out[k] *= amp * gateAmp * kFreqOut;
    }
    write(out, n);
}

void FreqBank::readScope(float *dst, int n) const {
    if (n > kFreqScope) n = kFreqScope;
    const uint32_t w = tapW.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i) dst[i] = tap[(w - n + i) & (kFreqScope - 1)];
}

} // namespace td
