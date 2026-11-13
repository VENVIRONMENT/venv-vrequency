#include "presets.h"
#include "voice.h"
#include <cstdio>
#include <vector>

namespace td {

namespace {

struct PV { int id; float v; };

Patch make(const char *name, std::initializer_list<PV> vals) {
    Patch p;
    p.setName(name);
    for (const PV &x : vals) p.p[x.id] = x.v;
    p.clamp();
    normalisePatch(p);
    return p;
}

} // namespace

Patch factoryPatch(int index) {
    switch (index) {
    case 0: // two detuned saws + sub through a warm low-pass
        return make("Warm Lead", {
            {P_WAVE1, W_SAW}, {P_WAVE2, W_SAW}, {P_MIX2, 0.5f}, {P_DETUNE, 9}, {P_SUB, 0.25f},
            {P_ATK, 0.008f}, {P_DEC, 0.4f}, {P_SUS, 0.8f}, {P_REL, 0.25f},
            {P_FMODE, F_LP}, {P_CUTOFF, 1700}, {P_RESO, 0.25f}, {P_FENV, 1.5f}, {P_FDEC, 0.5f}, {P_FSUS, 0.5f},
            {P_VIBDEPTH, 9}, {P_VIBDELAY, 0.3f}, {P_GLIDE, 110}, {P_DRIVE, 0.15f},
            {P_DLYMIX, 0.18f}, {P_DLYDIV, 3}, {P_REVMIX, 0.16f}});
    case 1: // sine carrier, FM from a partial near 3x: glassy bell
        return make("Glass Bells", {
            {P_WAVE1, W_SINE}, {P_WAVE2, W_SINE}, {P_MIX2, 0.08f}, {P_SEMI2, 19}, {P_DETUNE, 14}, {P_FM, 2.6f},
            {P_ATK, 0.001f}, {P_DEC, 1.8f}, {P_SUS, 0.0f}, {P_REL, 1.6f},
            {P_FMODE, F_LP}, {P_CUTOFF, 9000}, {P_RESO, 0.05f}, {P_FENV, 0}, {P_KTRK, 0.3f},
            {P_VIBDEPTH, 0}, {P_GLIDE, 60},
            {P_DLYMIX, 0.2f}, {P_DLYDIV, 3}, {P_REVMIX, 0.42f}, {P_REVSIZE, 0.86f}});
    case 2: // slow unison saws, octave layer, chorus
        return make("Soft Pad", {
            {P_WAVE1, W_SAW}, {P_WAVE2, W_TRI}, {P_MIX2, 0.3f}, {P_SEMI2, 12}, {P_DETUNE, 7},
            {P_UNISON, 0.8f}, {P_SPREAD, 18},
            {P_ATK, 0.9f}, {P_DEC, 1.5f}, {P_SUS, 0.85f}, {P_REL, 2.2f},
            {P_FMODE, F_LP}, {P_CUTOFF, 850}, {P_RESO, 0.1f}, {P_FENV, 0.9f}, {P_FATK, 1.2f}, {P_FDEC, 2.0f}, {P_FSUS, 0.7f}, {P_FREL, 2.0f},
            {P_LFORATE, 0.25f}, {P_LFOCUT, 0.4f}, {P_VIBDEPTH, 4}, {P_VIBDELAY, 0.8f}, {P_GLIDE, 250},
            {P_CHORUS, 0.7f}, {P_DLYMIX, 0.12f}, {P_REVMIX, 0.5f}, {P_REVSIZE, 0.85f}});
    case 3: // narrow pulse with width wobble: 8-bit console
        return make("Retro Game", {
            {P_WAVE1, W_SQUARE}, {P_WAVE2, W_SQUARE}, {P_MIX2, 0.2f}, {P_SEMI2, 12}, {P_DETUNE, 0}, {P_PW, 0.25f},
            {P_ATK, 0.001f}, {P_DEC, 0.15f}, {P_SUS, 0.6f}, {P_REL, 0.08f},
            {P_FMODE, F_LP}, {P_CUTOFF, 9000}, {P_RESO, 0.0f}, {P_FENV, 0}, {P_KTRK, 0},
            {P_LFORATE, 5.0f}, {P_LFOPW, 0.12f},
            {P_VIBRATE, 6.5f}, {P_VIBDEPTH, 16}, {P_VIBDELAY, 0.15f}, {P_GLIDE, 40},
            {P_DLYMIX, 0.14f}, {P_DLYDIV, 2}, {P_DLYFB, 0.3f}, {P_REVMIX, 0.03f}});
    case 4: // bright burst closing fast: plucked string
        return make("Plucked Strings", {
            {P_WAVE1, W_SAW}, {P_WAVE2, W_SQUARE}, {P_MIX2, 0.15f}, {P_DETUNE, 4}, {P_NOISE, 0.1f},
            {P_ATK, 0.001f}, {P_DEC, 0.9f}, {P_SUS, 0.0f}, {P_REL, 0.5f},
            {P_FMODE, F_LP}, {P_CUTOFF, 420}, {P_RESO, 0.15f}, {P_FENV, 4.5f}, {P_FATK, 0.001f}, {P_FDEC, 0.18f}, {P_FSUS, 0.0f}, {P_FREL, 0.3f}, {P_KTRK, 0.8f},
            {P_VIBDEPTH, 3}, {P_VIBDELAY, 0.5f}, {P_GLIDE, 70},
            {P_CHORUS, 0.2f}, {P_DLYMIX, 0.1f}, {P_REVMIX, 0.24f}});
    case 5: // saw + octave-down square + sub, driven, dark
        return make("Deep Bass", {
            {P_WAVE1, W_SAW}, {P_WAVE2, W_SQUARE}, {P_MIX2, 0.4f}, {P_SEMI2, -12}, {P_DETUNE, 3}, {P_SUB, 0.7f},
            {P_ATK, 0.003f}, {P_DEC, 0.3f}, {P_SUS, 0.75f}, {P_REL, 0.15f},
            {P_FMODE, F_LP}, {P_CUTOFF, 300}, {P_RESO, 0.35f}, {P_FENV, 2.4f}, {P_FDEC, 0.25f}, {P_FSUS, 0.2f}, {P_KTRK, 0.3f},
            {P_VIBDEPTH, 0}, {P_GLIDE, 90}, {P_DRIVE, 0.3f},
            {P_DLYMIX, 0.0f}, {P_REVMIX, 0.06f}});
    case 6: // gentle 2:1 FM electric piano with tremolo
        return make("Dreamy Keys", {
            {P_WAVE1, W_TRI}, {P_WAVE2, W_SINE}, {P_MIX2, 0.3f}, {P_SEMI2, 12}, {P_DETUNE, 5}, {P_FM, 0.7f},
            {P_ATK, 0.004f}, {P_DEC, 1.4f}, {P_SUS, 0.25f}, {P_REL, 0.9f},
            {P_FMODE, F_LP}, {P_CUTOFF, 3000}, {P_RESO, 0.1f}, {P_FENV, 1.2f}, {P_FDEC, 0.5f}, {P_FSUS, 0.2f},
            {P_LFORATE, 4.2f}, {P_LFOAMP, 0.35f}, {P_VIBDEPTH, 0}, {P_GLIDE, 100},
            {P_CHORUS, 0.5f}, {P_DLYMIX, 0.24f}, {P_DLYDIV, 3}, {P_REVMIX, 0.35f}});
    case 7: // inharmonic FM on a square through a resonant band-pass
        return make("Metallic Chimes", {
            {P_WAVE1, W_SQUARE}, {P_WAVE2, W_SINE}, {P_MIX2, 0.0f}, {P_SEMI2, 17}, {P_DETUNE, 31}, {P_FM, 4.2f}, {P_PW, 0.5f},
            {P_ATK, 0.001f}, {P_DEC, 1.1f}, {P_SUS, 0.0f}, {P_REL, 1.2f},
            {P_FMODE, F_BP}, {P_CUTOFF, 2600}, {P_RESO, 0.45f}, {P_FENV, 0.6f}, {P_FDEC, 0.8f}, {P_KTRK, 1.0f},
            {P_VIBDEPTH, 0}, {P_GLIDE, 30},
            {P_DLYMIX, 0.25f}, {P_DLYDIV, 1}, {P_REVMIX, 0.45f}, {P_REVSIZE, 0.8f}});
    case 8: // stacked square/saw unison into heavy saturation
        return make("Dirty Lead", {
            {P_WAVE1, W_SQUARE}, {P_WAVE2, W_SAW}, {P_MIX2, 0.5f}, {P_DETUNE, 12}, {P_UNISON, 0.6f}, {P_SPREAD, 14},
            {P_ATK, 0.004f}, {P_DEC, 0.3f}, {P_SUS, 0.8f}, {P_REL, 0.2f},
            {P_FMODE, F_LP}, {P_CUTOFF, 2400}, {P_RESO, 0.4f}, {P_FENV, 2.0f}, {P_FDEC, 0.4f}, {P_FSUS, 0.4f},
            {P_VIBDEPTH, 11}, {P_VIBDELAY, 0.25f}, {P_GLIDE, 140}, {P_DRIVE, 0.85f},
            {P_DLYMIX, 0.22f}, {P_DLYDIV, 3}, {P_REVMIX, 0.14f}});
    default: // slow breathing band-pass over a fifth: endless drone
        return make("Ether Drone", {
            {P_WAVE1, W_SINE}, {P_WAVE2, W_SAW}, {P_MIX2, 0.35f}, {P_SEMI2, 7}, {P_DETUNE, 6},
            {P_UNISON, 0.7f}, {P_SPREAD, 22}, {P_NOISE, 0.07f},
            {P_ATK, 2.2f}, {P_DEC, 2.0f}, {P_SUS, 1.0f}, {P_REL, 4.5f},
            {P_FMODE, F_BP}, {P_CUTOFF, 700}, {P_RESO, 0.5f}, {P_FENV, 0.5f}, {P_FATK, 2.0f}, {P_KTRK, 0.2f},
            {P_LFORATE, 0.12f}, {P_LFOCUT, 1.6f}, {P_VIBDEPTH, 4}, {P_VIBDELAY, 1.0f}, {P_GLIDE, 600},
            {P_CHORUS, 0.6f}, {P_DLYMIX, 0.3f}, {P_DLYDIV, 4}, {P_DLYFB, 0.5f}, {P_REVMIX, 0.65f}, {P_REVSIZE, 0.93f}});
    }
}

// Sine-based "dream" sounds. Each adds a different kind of buzz to the same soft,
// slowly wobbling, heavily washed sine.
Patch extraPatch(int index) {
    switch (index) {
    case 0: // sine plus a quiet sine an octave up
        return make("Haze", {
            {P_WAVE1, W_SINE}, {P_WAVE2, W_SINE}, {P_MIX2, 0.2f}, {P_SEMI2, 12}, {P_DETUNE, 7},
            {P_ATK, 0.25f}, {P_DEC, 2.0f}, {P_SUS, 0.9f}, {P_REL, 2.5f},
            {P_FMODE, F_LP}, {P_CUTOFF, 2500}, {P_RESO, 0.05f}, {P_FENV, 0},
            {P_VIBRATE, 0.6f}, {P_VIBDEPTH, 14}, {P_VIBDELAY, 0}, {P_GLIDE, 300},
            {P_CHORUS, 0.8f}, {P_DLYMIX, 0.25f}, {P_DLYDIV, 3}, {P_DLYFB, 0.45f}, {P_REVMIX, 0.6f}, {P_REVSIZE, 0.92f}});
    case 1: // sine into soft saturation, with a sub
        return make("Fuzz", {
            {P_WAVE1, W_SINE}, {P_MIX2, 0}, {P_SUB, 0.2f}, {P_DRIVE, 0.3f},
            {P_ATK, 0.2f}, {P_DEC, 2.0f}, {P_SUS, 0.9f}, {P_REL, 2.2f},
            {P_FMODE, F_LP}, {P_CUTOFF, 1800}, {P_RESO, 0.05f}, {P_FENV, 0},
            {P_VIBRATE, 0.6f}, {P_VIBDEPTH, 12}, {P_VIBDELAY, 0}, {P_GLIDE, 300},
            {P_CHORUS, 0.7f}, {P_DLYMIX, 0.22f}, {P_DLYDIV, 3}, {P_DLYFB, 0.45f}, {P_REVMIX, 0.55f}, {P_REVSIZE, 0.92f}});
    case 2: // sine with a quiet saw two octaves up riding on it
        return make("Teeth", {
            {P_WAVE1, W_SINE}, {P_WAVE2, W_SAW}, {P_MIX2, 0.14f}, {P_SEMI2, 24}, {P_DETUNE, 5},
            {P_ATK, 0.2f}, {P_DEC, 2.0f}, {P_SUS, 0.9f}, {P_REL, 2.2f},
            {P_FMODE, F_LP}, {P_CUTOFF, 5000}, {P_RESO, 0.05f}, {P_FENV, 0}, {P_KTRK, 0.5f},
            {P_VIBRATE, 0.6f}, {P_VIBDEPTH, 12}, {P_VIBDELAY, 0}, {P_GLIDE, 300},
            {P_CHORUS, 0.6f}, {P_DLYMIX, 0.22f}, {P_DLYDIV, 3}, {P_DLYFB, 0.45f}, {P_REVMIX, 0.55f}, {P_REVSIZE, 0.92f}});
    case 3: // sine gently bent by FM from a sine an octave up; the detune makes the shape drift
        return make("Bend", {
            {P_WAVE1, W_SINE}, {P_WAVE2, W_SINE}, {P_MIX2, 0}, {P_SEMI2, 12}, {P_DETUNE, 6}, {P_FM, 0.4f},
            {P_ATK, 0.2f}, {P_DEC, 2.0f}, {P_SUS, 0.9f}, {P_REL, 2.2f},
            {P_FMODE, F_LP}, {P_CUTOFF, 2500}, {P_RESO, 0.05f}, {P_FENV, 0},
            {P_VIBRATE, 0.6f}, {P_VIBDEPTH, 10}, {P_VIBDELAY, 0}, {P_GLIDE, 300},
            {P_CHORUS, 0.6f}, {P_DLYMIX, 0.22f}, {P_DLYDIV, 3}, {P_DLYFB, 0.45f}, {P_REVMIX, 0.55f}, {P_REVSIZE, 0.92f}});
    default: // triangle and sine with a little hiss and a slow tremolo
        return make("Grain", {
            {P_WAVE1, W_TRI}, {P_WAVE2, W_SINE}, {P_MIX2, 0.5f}, {P_SEMI2, 0}, {P_DETUNE, 9}, {P_NOISE, 0.06f},
            {P_ATK, 0.3f}, {P_DEC, 2.0f}, {P_SUS, 0.9f}, {P_REL, 2.5f},
            {P_FMODE, F_LP}, {P_CUTOFF, 1500}, {P_RESO, 0.1f}, {P_FENV, 0},
            {P_LFORATE, 3.5f}, {P_LFOAMP, 0.3f}, {P_VIBRATE, 0.6f}, {P_VIBDEPTH, 10}, {P_VIBDELAY, 0}, {P_GLIDE, 300},
            {P_CHORUS, 0.7f}, {P_DLYMIX, 0.25f}, {P_DLYDIV, 3}, {P_DLYFB, 0.45f}, {P_REVMIX, 0.6f}, {P_REVSIZE, 0.92f}});
    }
}

// --------------------------------------------------------------- loudness

float normalisePatch(Patch &p, float sr) {
    Part pt;
    pt.patch = p;
    pt.patch.p[P_LEVEL] = 1.0f;
    // measure the sound at its loudest: don't wait for multi-second attacks
    if (pt.patch.p[P_ATK] > 0.03f) pt.patch.p[P_ATK] = 0.03f;
    if (pt.patch.p[P_FATK] > 0.25f) pt.patch.p[P_FATK] = 0.25f;
    pt.patch.p[P_VIBDELAY] = 0;
    Voice v[2];
    v[0].start(0, 0, 57, 1.0f, 0, 1);
    v[1].start(0, 1, 69, 1.0f, 0, 2);
    const int total = (int)(sr * 1.5f);
    const int win = (int)(sr * 0.05f);
    float tmp[kBlock], mix[kBlock];
    double acc = 0; int accN = 0;
    float maxRms = 0, peak = 0;
    for (int done = 0; done < total; done += kBlock) {
        for (int i = 0; i < kBlock; ++i) mix[i] = 0;
        for (auto &vv : v) {
            if (!vv.active()) continue;
            vv.render(pt, sr, tmp, kBlock);
            for (int i = 0; i < kBlock; ++i) mix[i] += tmp[i];
        }
        for (int i = 0; i < kBlock; ++i) {
            float a = std::fabs(mix[i]);
            if (a > peak) peak = a;
            acc += (double)mix[i] * mix[i];
            if (++accN >= win) {
                float r = (float)std::sqrt(acc / accN);
                if (r > maxRms) maxRms = r;
                acc = 0; accN = 0;
            }
        }
    }
    const float targetRms = 0.11f, peakCap = 0.42f;
    float g = maxRms > 1e-6f ? targetRms / maxRms : 1.0f;
    if (peak * g > peakCap) g = peakCap / peak;
    const ParamInfo &m = paramInfo(P_LEVEL);
    p.p[P_LEVEL] = clampf(g, m.min, m.max);
    return peak * p.p[P_LEVEL];
}

// ------------------------------------------------------------- randomiser

namespace {

struct Gen {
    Rng &r;
    float u(float lo, float hi) { return lo + (hi - lo) * r.uni(); }
    float lg(float lo, float hi) { return lo * std::pow(hi / lo, r.uni()); }
    int pick(int n) { return (int)(r.next() % (uint32_t)n); }
    bool chance(float p) { return r.uni() < p; }
    template <int N> int of(const int (&a)[N]) { return a[pick(N)]; }
};

const char *kAdj[] = {"Amber", "Hollow", "Velvet", "Brittle", "Lunar", "Rusty", "Silk", "Feral", "Paper",
                      "Copper", "Misty", "Neon", "Tidal", "Woven", "Ghost", "Ember", "Frost", "Mossy",
                      "Static", "Honey", "Cobalt", "Dusty", "Solar", "Murky", "Glassy", "Tin", "Plush",
                      "Crooked", "Distant", "Salt"};
const char *kNoun[][6] = {
    {"Pluck", "Thumb", "Harp", "Pick", "Twang", "Koto"},
    {"Bell", "Chime", "Gong", "Tine", "Glock", "Spire"},
    {"Pad", "Haze", "Cloud", "Veil", "Bloom", "Drift"},
    {"Bass", "Growl", "Root", "Thud", "Rumble", "Depth"},
    {"Lead", "Whistle", "Reed", "Voice", "Siren", "Flare"},
    {"Keys", "Piano", "Clav", "Organ", "Rhodes", "Lullaby"},
    {"Chip", "Pixel", "Blip", "Cart", "Arcade", "Byte"},
    {"Drone", "Hum", "Tide", "Engine", "Choir", "Breath"},
    {"Brass", "Horn", "Fanfare", "Swell", "Tuba", "Bugle"},
    {"Wobble", "Pulse", "Flutter", "Wave", "Sway", "Throb"},
    {"Thing", "Critter", "Machine", "Signal", "Oddity", "Relic"},
};

} // namespace

Patch randomPatch(Rng &rng) {
    Gen g{rng};
    Patch p;
    float *P = p.p;
    static const int semis[] = {0, 0, 0, 0, 12, 12, -12, 7, 19, 24, 5};

    // a safe, fully random baseline…
    P[P_WAVE1] = (float)g.pick(4);
    P[P_WAVE2] = (float)g.pick(4);
    P[P_MIX2] = g.chance(0.7f) ? g.u(0.1f, 0.6f) : 0;
    P[P_SEMI2] = (float)g.of(semis);
    P[P_DETUNE] = g.u(0, 18);
    P[P_UNISON] = g.chance(0.4f) ? g.u(0.3f, 0.9f) : 0;
    P[P_SPREAD] = g.u(6, 26);
    P[P_SUB] = g.chance(0.35f) ? g.u(0.2f, 0.7f) : 0;
    P[P_NOISE] = g.chance(0.2f) ? g.u(0.02f, 0.15f) : 0;
    P[P_PW] = g.u(0.15f, 0.5f);
    P[P_FM] = 0;
    P[P_ATK] = g.lg(0.001f, 0.05f);
    P[P_DEC] = g.lg(0.1f, 2.0f);
    P[P_SUS] = g.u(0.3f, 1.0f);
    P[P_REL] = g.lg(0.08f, 1.5f);
    P[P_FMODE] = g.chance(0.75f) ? F_LP : (g.chance(0.6f) ? F_BP : F_HP);
    P[P_CUTOFF] = g.lg(300, 5000);
    P[P_RESO] = g.u(0.0f, 0.55f);
    P[P_FENV] = g.u(0, 3.5f);
    P[P_FATK] = g.lg(0.001f, 0.2f);
    P[P_FDEC] = g.lg(0.08f, 1.5f);
    P[P_FSUS] = g.u(0, 0.7f);
    P[P_FREL] = g.lg(0.08f, 1.5f);
    P[P_KTRK] = g.u(0.2f, 1.0f);
    P[P_VIBRATE] = g.u(4, 7);
    P[P_VIBDEPTH] = g.chance(0.6f) ? g.u(3, 16) : 0;
    P[P_VIBDELAY] = g.u(0.1f, 0.8f);
    P[P_LFORATE] = g.lg(0.1f, 8);
    P[P_LFOCUT] = g.chance(0.3f) ? g.u(0.2f, 1.5f) : 0;
    P[P_LFOAMP] = g.chance(0.15f) ? g.u(0.2f, 0.6f) : 0;
    P[P_LFOPW] = g.chance(0.25f) ? g.u(0.05f, 0.3f) : 0;
    P[P_GLIDE] = g.u(40, 260);
    P[P_DRIVE] = g.chance(0.35f) ? g.u(0.1f, 0.7f) : 0;
    P[P_CHORUS] = g.chance(0.4f) ? g.u(0.2f, 0.8f) : 0;
    P[P_DLYMIX] = g.chance(0.6f) ? g.u(0.08f, 0.35f) : 0;
    P[P_DLYDIV] = (float)g.pick(5);
    P[P_DLYFB] = g.u(0.2f, 0.55f);
    P[P_REVMIX] = g.u(0.05f, 0.45f);
    P[P_REVSIZE] = g.u(0.5f, 0.92f);

    // …then pushed towards one of several characters so it plays like an instrument.
    int ch = g.pick(11);
    switch (ch) {
    case 0: // pluck
        P[P_ATK] = 0.001f; P[P_SUS] = g.u(0, 0.15f); P[P_DEC] = g.lg(0.25f, 1.4f); P[P_REL] = g.lg(0.2f, 0.9f);
        P[P_FMODE] = F_LP; P[P_CUTOFF] = g.lg(250, 900); P[P_FENV] = g.u(2.5f, 5); P[P_FATK] = 0.001f;
        P[P_FDEC] = g.lg(0.08f, 0.4f); P[P_FSUS] = 0; P[P_LFOAMP] = 0;
        break;
    case 1: { // bell
        static const int ratios[] = {12, 19, 24, 17, 14, 21, 28};
        P[P_WAVE1] = g.chance(0.7f) ? W_SINE : W_TRI; P[P_WAVE2] = W_SINE;
        P[P_SEMI2] = (float)g.of(ratios); P[P_DETUNE] = g.chance(0.5f) ? g.u(0, 40) : 0;
        P[P_FM] = g.u(1.2f, 4.5f); P[P_MIX2] = g.u(0, 0.15f); P[P_UNISON] = 0; P[P_SUB] = 0; P[P_NOISE] = 0;
        P[P_ATK] = 0.001f; P[P_SUS] = 0; P[P_DEC] = g.lg(0.8f, 3); P[P_REL] = g.lg(0.8f, 2.5f);
        P[P_FMODE] = F_LP; P[P_CUTOFF] = g.lg(4000, 12000); P[P_FENV] = 0; P[P_RESO] = g.u(0, 0.2f);
        P[P_DRIVE] = 0; P[P_VIBDEPTH] = 0; P[P_REVMIX] = g.u(0.25f, 0.55f); P[P_LFOCUT] = 0;
        break;
    }
    case 2: // pad
        P[P_ATK] = g.lg(0.3f, 2); P[P_DEC] = g.u(1, 3); P[P_SUS] = g.u(0.7f, 1); P[P_REL] = g.lg(1, 4);
        P[P_UNISON] = g.u(0.5f, 0.9f); P[P_CUTOFF] = g.lg(400, 2200); P[P_FATK] = g.lg(0.3f, 2);
        P[P_FENV] = g.u(0.3f, 1.8f); P[P_CHORUS] = g.u(0.4f, 0.9f); P[P_REVMIX] = g.u(0.35f, 0.65f);
        P[P_GLIDE] = g.u(150, 500); P[P_DRIVE] = 0; P[P_LFOCUT] = g.u(0, 0.8f); P[P_LFORATE] = g.lg(0.08f, 0.8f);
        break;
    case 3: // bass
        P[P_WAVE1] = g.chance(0.5f) ? W_SAW : W_SQUARE; P[P_SEMI2] = g.chance(0.5f) ? -12 : 0;
        P[P_SUB] = g.u(0.4f, 0.9f); P[P_FMODE] = F_LP; P[P_CUTOFF] = g.lg(160, 600); P[P_FENV] = g.u(1.5f, 3.5f);
        P[P_FDEC] = g.lg(0.1f, 0.5f); P[P_ATK] = 0.003f; P[P_REL] = g.lg(0.08f, 0.3f); P[P_KTRK] = g.u(0.1f, 0.5f);
        P[P_REVMIX] = g.u(0, 0.1f); P[P_DLYMIX] = 0; P[P_VIBDEPTH] = 0; P[P_NOISE] = 0; P[P_LFOAMP] = 0;
        break;
    case 4: // lead
        P[P_ATK] = g.lg(0.003f, 0.03f); P[P_SUS] = g.u(0.7f, 1); P[P_REL] = g.lg(0.12f, 0.5f);
        P[P_FMODE] = F_LP; P[P_CUTOFF] = g.lg(1200, 4500); P[P_VIBDEPTH] = g.u(6, 18);
        P[P_GLIDE] = g.u(80, 220); P[P_DLYMIX] = g.u(0.12f, 0.3f); P[P_LFOAMP] = 0;
        break;
    case 5: // keys
        P[P_WAVE1] = g.chance(0.5f) ? W_TRI : W_SINE; P[P_WAVE2] = W_SINE; P[P_SEMI2] = g.chance(0.6f) ? 12 : 24;
        P[P_FM] = g.u(0.3f, 1.4f); P[P_ATK] = 0.003f; P[P_DEC] = g.lg(0.7f, 2); P[P_SUS] = g.u(0.1f, 0.4f);
        P[P_REL] = g.lg(0.4f, 1.2f); P[P_FMODE] = F_LP; P[P_CUTOFF] = g.lg(1800, 6000); P[P_DRIVE] = 0;
        P[P_LFOAMP] = g.chance(0.5f) ? g.u(0.2f, 0.45f) : 0; P[P_LFORATE] = g.u(3, 6); P[P_VIBDEPTH] = 0;
        P[P_CHORUS] = g.u(0.2f, 0.6f); P[P_NOISE] = 0; P[P_LFOCUT] = 0;
        break;
    case 6: // chip
        P[P_WAVE1] = W_SQUARE; P[P_WAVE2] = g.chance(0.5f) ? W_SQUARE : W_TRI; P[P_PW] = g.u(0.12f, 0.5f);
        P[P_UNISON] = 0; P[P_SUB] = 0; P[P_NOISE] = 0; P[P_FMODE] = F_LP; P[P_CUTOFF] = g.lg(5000, 12000);
        P[P_FENV] = 0; P[P_RESO] = 0; P[P_ATK] = 0.001f; P[P_REL] = g.lg(0.03f, 0.15f); P[P_DRIVE] = 0;
        P[P_VIBDEPTH] = g.u(8, 25); P[P_VIBRATE] = g.u(5.5f, 8); P[P_VIBDELAY] = g.u(0, 0.25f);
        P[P_LFOPW] = g.u(0, 0.25f); P[P_LFORATE] = g.u(2, 9); P[P_REVMIX] = g.u(0, 0.1f); P[P_CHORUS] = 0;
        P[P_GLIDE] = g.u(20, 90); P[P_LFOCUT] = 0; P[P_LFOAMP] = 0;
        break;
    case 7: // drone
        P[P_ATK] = g.lg(1, 3.5f); P[P_SUS] = 1; P[P_REL] = g.lg(2.5f, 6); P[P_UNISON] = g.u(0.5f, 0.9f);
        P[P_SPREAD] = g.u(14, 34); P[P_FMODE] = g.chance(0.5f) ? F_BP : F_LP; P[P_CUTOFF] = g.lg(350, 1500);
        P[P_RESO] = g.u(0.2f, 0.6f); P[P_LFOCUT] = g.u(0.6f, 2.0f); P[P_LFORATE] = g.lg(0.06f, 0.35f);
        P[P_REVMIX] = g.u(0.45f, 0.75f); P[P_REVSIZE] = g.u(0.82f, 0.96f); P[P_GLIDE] = g.u(350, 1100);
        P[P_CHORUS] = g.u(0.3f, 0.8f); P[P_NOISE] = g.chance(0.5f) ? g.u(0.03f, 0.12f) : 0; P[P_LFOAMP] = 0;
        break;
    case 8: // brass
        P[P_WAVE1] = W_SAW; P[P_WAVE2] = W_SAW; P[P_MIX2] = g.u(0.3f, 0.5f); P[P_SEMI2] = 0; P[P_DETUNE] = g.u(5, 14);
        P[P_ATK] = g.lg(0.03f, 0.12f); P[P_SUS] = g.u(0.7f, 0.9f); P[P_FMODE] = F_LP; P[P_CUTOFF] = g.lg(350, 900);
        P[P_FENV] = g.u(2, 3.5f); P[P_FATK] = g.lg(0.04f, 0.2f); P[P_FDEC] = g.lg(0.3f, 0.9f); P[P_FSUS] = g.u(0.4f, 0.7f);
        P[P_RESO] = g.u(0.05f, 0.25f); P[P_NOISE] = 0; P[P_LFOAMP] = 0;
        break;
    case 9: // wobble
        P[P_FMODE] = F_LP; P[P_CUTOFF] = g.lg(300, 900); P[P_RESO] = g.u(0.35f, 0.65f);
        P[P_LFOCUT] = g.u(1.5f, 3); P[P_LFORATE] = g.lg(1.5f, 8); P[P_SUS] = g.u(0.7f, 1); P[P_FENV] = g.u(0, 1);
        P[P_SUB] = g.u(0.2f, 0.6f); P[P_ATK] = g.lg(0.003f, 0.03f);
        break;
    default: // wild: keep the unconstrained baseline, add something odd
        if (g.chance(0.5f)) { P[P_FM] = g.u(0.5f, 5); P[P_SEMI2] = (float)(g.pick(31) - 7); }
        if (g.chance(0.4f)) { P[P_FMODE] = F_BP; P[P_RESO] = g.u(0.4f, 0.75f); P[P_LFOCUT] = g.u(1, 3); }
        if (g.chance(0.3f)) P[P_NOISE] = g.u(0.1f, 0.35f);
        break;
    }
    char nm[28];
    std::snprintf(nm, sizeof nm, "%s %s", kAdj[g.pick((int)(sizeof kAdj / sizeof *kAdj))], kNoun[ch][g.pick(6)]);
    p.setName(nm);
    p.clamp();
    normalisePatch(p);
    return p;
}

Patch mutatePatch(const Patch &src, Rng &rng, float amount) {
    Gen g{rng};
    Patch p = src;
    for (int i = 0; i < P_COUNT; ++i) {
        if (i == P_LEVEL) continue;
        const ParamInfo &m = paramInfo(i);
        if (m.integer) {
            if (g.chance(amount * 0.2f) && i != P_SEMI2) p.p[i] = m.min + (float)g.pick((int)(m.max - m.min) + 1);
            continue;
        }
        float d = (g.r.uni() * 2 - 1) * amount;
        if (m.logScale) p.p[i] = p.p[i] * std::pow(m.max / m.min, d * 0.25f);
        else p.p[i] += d * 0.3f * (m.max - m.min);
    }
    // keep mutations inside the same safe window the randomiser uses
    if (p.p[P_RESO] > 0.8f) p.p[P_RESO] = 0.8f;
    if (p.p[P_NOISE] > 0.35f) p.p[P_NOISE] = 0.35f;
    p.clamp();
    normalisePatch(p);
    return p;
}

} // namespace td
