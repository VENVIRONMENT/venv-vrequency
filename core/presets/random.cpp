// venv-vrequency — the sound randomiser: brand-new sounds, and nudges to an existing one.
#include "core/presets/presets.h"
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace td {

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
        if (i >= P_LEVEL) continue; // LEVEL, and the warp after it, stay as they are
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
