#include "drums.h"
#include <algorithm>
#include <cmath>

namespace td {

namespace {

using Buf = std::vector<float>;

struct Biquad { // second-order Butterworth low- or high-pass
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    Biquad(bool high, double hz, double sr) {
        hz = std::min(hz, sr * 0.45);
        double w = 2.0 * M_PI * hz / sr, c = std::cos(w), alpha = std::sin(w) / (2.0 * 0.70710678);
        double a0 = 1.0 + alpha;
        if (high) { b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = b0; }
        else { b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = b0; }
        a1 = -2 * c / a0; a2 = (1 - alpha) / a0;
    }
    double tick(double x) {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

struct Maker {
    double sr;
    Rng rng;
    explicit Maker(double sampleRate) : sr(sampleRate) { rng.s = 0x5EED1234u; }

    int len(double sec) const { return (int)(sec * sr); }
    // white noise with unit standard deviation, optionally high- and/or low-passed (0 = off)
    Buf noise(double sec, double hp = 0, double lp = 0) {
        Buf b(len(sec));
        Biquad h(true, hp > 0 ? hp : 20, sr), l(false, lp > 0 ? lp : sr * 0.45, sr);
        for (float &x : b) {
            double v = rng.bipolar() * 1.7320508;
            if (hp > 0) v = h.tick(v);
            if (lp > 0) v = l.tick(v);
            x = (float)v;
        }
        return b;
    }
    // sine whose pitch falls from f0 to f1 with time constant tau
    Buf sweep(double sec, double f0, double f1, double tau) const {
        Buf b(len(sec));
        double ph = 0;
        for (size_t i = 0; i < b.size(); ++i) {
            double t = i / sr;
            ph += (f1 + (f0 - f1) * std::exp(-t / tau)) / sr;
            b[i] = (float)std::sin(2.0 * M_PI * ph);
        }
        return b;
    }
    // sum of square waves (the 808's metallic hats and cowbell)
    Buf squares(double sec, std::initializer_list<double> hz) const {
        Buf b(len(sec), 0.0f);
        for (double f : hz)
            for (size_t i = 0; i < b.size(); ++i) b[i] += std::sin(2.0 * M_PI * f * i / sr) >= 0 ? 1.0f : -1.0f;
        for (float &x : b) x /= (float)hz.size();
        return b;
    }
    // phase-modulated sine: sin(2 pi fc t + index * sin(2 pi fm t))
    Buf fm(double sec, double fc, double fmHz, double index) const {
        Buf b(len(sec));
        for (size_t i = 0; i < b.size(); ++i) {
            double t = i / sr;
            b[i] = (float)std::sin(2.0 * M_PI * fc * t + index * std::sin(2.0 * M_PI * fmHz * t));
        }
        return b;
    }
    Buf &filter(Buf &b, double hp, double lp) const {
        Biquad h(true, hp > 0 ? hp : 20, sr), l(false, lp > 0 ? lp : sr * 0.45, sr);
        for (float &x : b) {
            double v = x;
            if (hp > 0) v = h.tick(v);
            if (lp > 0) v = l.tick(v);
            x = (float)v;
        }
        return b;
    }
    Buf &decay(Buf &b, double tau, double gain = 1.0) const {
        for (size_t i = 0; i < b.size(); ++i) b[i] *= (float)(gain * std::exp(-(i / sr) / tau));
        return b;
    }
    // a hand clap: three short bursts, then a tail
    Buf &clapEnv(Buf &b, double burstTau, double tailTau, double burstGain) const {
        static const double off[3] = {0.0, 0.0105, 0.0215};
        for (size_t i = 0; i < b.size(); ++i) {
            double t = i / sr, e = 0;
            for (double o : off) if (t >= o) e += burstGain * std::exp(-(t - o) / burstTau);
            if (t >= 0.03) e += std::exp(-(t - 0.03) / tailTau);
            b[i] *= (float)e;
        }
        return b;
    }
    Buf &drive(Buf &b, double pre, double post) const {
        for (float &x : b) x = (float)(std::tanh(x * pre) * post);
        return b;
    }
    Buf &gain(Buf &b, double g) const { for (float &x : b) x *= (float)g; return b; }
    // sample-and-hold at `rate` Hz, then round to `bits`
    Buf &crush(Buf &b, double rate, int bits) const {
        int hold = std::max(1, (int)std::lround(sr / rate));
        float q = (float)(1 << (bits - 1)), h = 0;
        for (size_t i = 0; i < b.size(); ++i) {
            if (i % hold == 0) h = b[i];
            b[i] = std::round(h * q) / q;
        }
        return b;
    }
    static Buf mix(const Buf &a, double ga, const Buf &b, double gb) {
        Buf o(std::max(a.size(), b.size()), 0.0f);
        for (size_t i = 0; i < a.size(); ++i) o[i] += (float)(a[i] * ga);
        for (size_t i = 0; i < b.size(); ++i) o[i] += (float)(b[i] * gb);
        return o;
    }
    // every sound ends in silence, so a voice can simply stop at the end of its buffer
    Buf done(Buf b) const {
        int n = std::min((int)b.size(), len(0.004));
        for (int i = 0; i < n; ++i) b[b.size() - 1 - i] *= (float)i / n;
        return b;
    }
};

} // namespace

void DrumKit::init(float sampleRate) {
    sr = sampleRate;
    fadeCoef = std::exp(-1.0f / (0.003f * sr));
    Maker m(sr);

    // ---- soft: round, quiet, a little room (the room comes from the master reverb send)
    {
        Buf *k = smp[KIT_SOFT];
        Buf a = m.sweep(0.5, 150, 46, 0.040); k[D_KICK] = m.done(m.decay(a, 0.22, 0.9));
        Buf tone = m.sweep(0.3, 260, 180, 0.02); m.decay(tone, 0.07, 0.5);
        Buf nz = m.noise(0.3, 900, 5500); m.decay(nz, 0.11, 0.22);
        Buf sn = Maker::mix(tone, 0.7, nz, 0.7); k[D_SNARE] = m.done(sn);
        a = m.noise(0.12, 6000, 11000); k[D_HAT] = m.done(m.decay(a, 0.028, 0.16));
        a = m.noise(0.7, 6000, 11000); k[D_OPENHAT] = m.done(m.decay(a, 0.20, 0.12));
        a = m.noise(0.3, 800, 3200); k[D_CLAP] = m.done(m.gain(m.clapEnv(a, 0.012, 0.09, 0.6), 0.14));
        Buf r1 = m.fm(0.08, 480, 0, 0), r2 = m.fm(0.08, 1650, 0, 0);
        a = Maker::mix(r1, 0.5, r2, 0.35); k[D_RIM] = m.done(m.decay(a, 0.013, 0.5));
        a = m.sweep(0.5, 190, 108, 0.06); k[D_TOM] = m.done(m.decay(a, 0.19, 0.6));
        a = m.squares(0.4, {540, 800}); m.filter(a, 500, 2400);
        Buf cb = a; m.decay(a, 0.015, 0.6); m.decay(cb, 0.11, 0.4);
        a = Maker::mix(a, 0.3, cb, 0.3); k[D_COWBELL] = m.done(a);
    }
    // ---- crunchy: driven and lo-fi (14 kHz, 8 bits)
    {
        Buf *k = smp[KIT_CRUNCHY];
        Buf a = m.sweep(0.4, 280, 52, 0.028); m.decay(a, 0.17);
        int click = m.len(0.002);
        for (int i = 0; i < click && i < (int)a.size(); ++i) a[i] += 0.5f * 1.732f * m.rng.bipolar() * (1.0f - (float)i / click);
        k[D_KICK] = m.drive(a, 3.2, 0.75);
        Buf tone = m.sweep(0.25, 330, 200, 0.015); m.decay(tone, 0.05, 0.6);
        Buf nz = m.noise(0.25, 1400, 9000); m.decay(nz, 0.085, 0.5);
        a = Maker::mix(tone, 1, nz, 1); k[D_SNARE] = m.drive(a, 2.2, 0.6);
        a = m.noise(0.1, 7500); k[D_HAT] = m.decay(a, 0.022, 0.2);
        a = m.noise(0.6, 7500); k[D_OPENHAT] = m.decay(a, 0.15, 0.16);
        a = m.noise(0.3, 1000, 4500); m.clapEnv(a, 0.009, 0.07, 0.7); k[D_CLAP] = m.drive(a, 0.9, 0.4);
        Buf r1 = m.fm(0.07, 520, 0, 0), r2 = m.fm(0.07, 1800, 0, 0);
        a = Maker::mix(r1, 0.5, r2, 0.5); m.decay(a, 0.010); k[D_RIM] = m.drive(a, 3.0, 0.42);
        a = m.sweep(0.45, 240, 118, 0.05); m.decay(a, 0.16); k[D_TOM] = m.drive(a, 2.5, 0.5);
        a = m.squares(0.4, {540, 800}); m.filter(a, 600, 4000);
        Buf cb = a; m.decay(a, 0.012, 0.7); m.decay(cb, 0.09, 0.4);
        a = Maker::mix(a, 1, cb, 1); k[D_COWBELL] = m.drive(a, 2.0, 0.3);
        for (int d = 0; d < kDrums; ++d) k[d] = m.done(m.crush(k[d], 14000, 8));
    }
    // ---- glitchy: zaps, crushed noise and blips
    {
        Buf *k = smp[KIT_GLITCHY];
        Buf a = m.sweep(0.3, 900, 44, 0.012); m.drive(a, 2.5, 1.0); k[D_KICK] = m.done(m.decay(a, 0.12, 0.75));
        a = m.noise(0.14); m.crush(a, 5200, 3); k[D_SNARE] = m.done(m.decay(a, 0.045, 0.3));
        a = m.squares(0.06, {2110, 3130, 4270, 5410, 6230, 8170}); m.filter(a, 6500, 0); k[D_HAT] = m.done(m.decay(a, 0.012, 0.5));
        a = m.squares(0.4, {2110, 3130, 4270, 5410, 6230, 8170}); m.filter(a, 6500, 0); m.decay(a, 0.085, 0.4);
        k[D_OPENHAT] = m.done(m.crush(a, 16000, 5));
        a = m.fm(0.09, 1480, 610, 3.0); m.crush(a, 9000, 4); k[D_CLAP] = m.done(m.decay(a, 0.03, 0.3));
        a = m.fm(0.04, 2400, 0, 0); m.crush(a, 12000, 2); k[D_RIM] = m.done(m.decay(a, 0.006, 0.4));
        a = m.sweep(0.3, 700, 90, 0.05); m.crush(a, 8000, 4); k[D_TOM] = m.done(m.decay(a, 0.10, 0.45));
        a = m.fm(0.25, 830, 1245, 2.5); m.crush(a, 11000, 5); k[D_COWBELL] = m.done(m.decay(a, 0.07, 0.3));
    }
}

void DrumKit::trigger(int kit, int drum, float vel) {
    if (kit < 0 || kit >= kKits || drum < 0 || drum >= kDrums) return;
    // the same drum again (or a closed hat over an open one) fades the old hit out instead of piling up
    for (V &x : v)
        if (x.s && (x.drum == drum || (x.drum == D_OPENHAT && drum == D_HAT))) x.fading = true;
    V *slot = nullptr;
    for (V &x : v) if (!x.s) { slot = &x; break; }
    if (!slot) { // all busy: take the one furthest through its sound
        slot = &v[0];
        for (V &x : v) if (x.pos > slot->pos) slot = &x;
    }
    static const float pan[kDrums] = {0.0f, 0.0f, 0.25f, 0.25f, -0.15f, -0.3f, -0.35f, 0.35f};
    slot->s = &smp[kit][drum];
    slot->pos = 0; slot->drum = drum; slot->gain = clampf(vel, 0, 1.5f);
    slot->gl = std::sqrt(1.0f - pan[drum]); slot->gr = std::sqrt(1.0f + pan[drum]);
    slot->fade = 1; slot->fading = false;
    nActive = 0;
    for (V &x : v) if (x.s) ++nActive;
}

void DrumKit::silence() {
    for (V &x : v) if (x.s) x.fading = true;
}

void DrumKit::render(int kit, float *l, float *r, int n) {
    if (nActive == 0) return;
    float bl[kBlock], br[kBlock];
    if (n > kBlock) n = kBlock;
    for (int i = 0; i < n; ++i) bl[i] = br[i] = 0;
    nActive = 0;
    for (V &x : v) {
        if (!x.s) continue;
        const float *s = x.s->data();
        int size = (int)x.s->size();
        for (int i = 0; i < n && x.pos < size; ++i) {
            float y = s[x.pos++] * x.gain * x.fade;
            if (x.fading) x.fade *= fadeCoef;
            bl[i] += y * x.gl; br[i] += y * x.gr;
        }
        if (x.pos >= size || x.fade < 1e-3f) x.s = nullptr; else ++nActive;
    }
    if (kit == KIT_CRUNCHY) {
        for (int i = 0; i < n; ++i) { bl[i] = fastTanh(bl[i] * 1.6f) * (1.0f / 1.3f); br[i] = fastTanh(br[i] * 1.6f) * (1.0f / 1.3f); }
    } else if (kit == KIT_GLITCHY) {
        float inc = 22000.0f / sr;
        for (int i = 0; i < n; ++i) {
            holdPh += inc;
            if (holdPh >= 1.0f) { holdPh -= 1.0f; holdL = std::round(bl[i] * 64.0f) * (1.0f / 64.0f); holdR = std::round(br[i] * 64.0f) * (1.0f / 64.0f); }
            bl[i] = holdL; br[i] = holdR;
        }
        if (nActive == 0) holdL = holdR = 0;
    }
    for (int i = 0; i < n; ++i) { l[i] += bl[i]; r[i] += br[i]; }
}

} // namespace td
