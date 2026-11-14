#include "core/presets/sound_shelf.h"
#include "core/presets/patch_values.h"
#include "core/presets/presets.h"
#include <cstring>

namespace td {

namespace {

struct Entry { const char *name, *hint; PV v[24]; };

// Every sound starts from the same dry, steady base, so each entry only lists what makes it itself.
Patch build(const Entry &e) {
    Patch p;
    p.p[P_VIBDEPTH] = 0; p.p[P_GLIDE] = 0; p.p[P_DLYMIX] = 0; p.p[P_REVMIX] = 0.1f;
    p.p[P_FMODE] = F_LP; p.p[P_FATK] = 0.001f;
    return withValues(p, e.name, e.v, sizeof e.v / sizeof e.v[0]);
}

const Entry kLib[kLibKinds][kLibSounds] = {
    { // ---- keys
        {"E-piano", "soft tines", {
            {P_WAVE1, W_SINE}, {P_WAVE2, W_SINE}, {P_MIX2, 0.1f}, {P_SEMI2, 12}, {P_DETUNE, 3}, {P_FM, 1.3f},
            {P_ATK, 0.002f}, {P_DEC, 1.6f}, {P_SUS, 0.12f}, {P_REL, 0.35f},
            {P_CUTOFF, 4200}, {P_RESO, 0.05f}, {P_FENV, 1.0f}, {P_FDEC, 0.4f}, {P_FSUS, 0.2f},
            {P_LFORATE, 4.5f}, {P_LFOAMP, 0.18f}, {P_CHORUS, 0.35f}, {P_DLYMIX, 0.05f}, {P_REVMIX, 0.18f}}},
        {"Soft keys", "felt piano", {
            {P_WAVE1, W_TRI}, {P_WAVE2, W_SAW}, {P_MIX2, 0.22f}, {P_DETUNE, 4},
            {P_ATK, 0.003f}, {P_DEC, 1.9f}, {P_SUS, 0.0f}, {P_REL, 0.45f},
            {P_CUTOFF, 900}, {P_RESO, 0.05f}, {P_FENV, 2.6f}, {P_FDEC, 0.55f}, {P_FSUS, 0.1f}, {P_KTRK, 0.7f},
            {P_DLYMIX, 0.04f}, {P_REVMIX, 0.22f}}},
        {"Organ", "steady drawbars", {
            {P_WAVE1, W_SINE}, {P_WAVE2, W_SINE}, {P_MIX2, 0.42f}, {P_SEMI2, 12}, {P_DETUNE, 2}, {P_SUB, 0.3f},
            {P_ATK, 0.004f}, {P_DEC, 0.1f}, {P_SUS, 1.0f}, {P_REL, 0.06f},
            {P_CUTOFF, 5200}, {P_RESO, 0.0f}, {P_FENV, 0}, {P_KTRK, 0.2f},
            {P_LFORATE, 6.2f}, {P_LFOAMP, 0.1f}, {P_CHORUS, 0.55f}, {P_DRIVE, 0.12f}, {P_REVMIX, 0.14f}}},
        {"Toy keys", "music box", {
            {P_WAVE1, W_SINE}, {P_WAVE2, W_SINE}, {P_MIX2, 0.22f}, {P_SEMI2, 24}, {P_DETUNE, 5}, {P_FM, 1.8f},
            {P_ATK, 0.001f}, {P_DEC, 0.7f}, {P_SUS, 0.0f}, {P_REL, 0.6f},
            {P_CUTOFF, 8000}, {P_RESO, 0.05f}, {P_FENV, 0}, {P_KTRK, 0.3f},
            {P_DLYMIX, 0.08f}, {P_REVMIX, 0.3f}}},
        {"Clav", "sharp and dry", {
            {P_WAVE1, W_SQUARE}, {P_WAVE2, W_SAW}, {P_MIX2, 0.2f}, {P_PW, 0.22f}, {P_DETUNE, 2},
            {P_ATK, 0.001f}, {P_DEC, 0.5f}, {P_SUS, 0.05f}, {P_REL, 0.08f},
            {P_CUTOFF, 1200}, {P_RESO, 0.35f}, {P_FENV, 2.8f}, {P_FDEC, 0.12f}, {P_FSUS, 0.1f}, {P_KTRK, 0.7f},
            {P_DRIVE, 0.15f}, {P_REVMIX, 0.05f}}},
        {"Strings", "slow and wide", {
            {P_WAVE1, W_SAW}, {P_WAVE2, W_SAW}, {P_MIX2, 0.5f}, {P_DETUNE, 12}, {P_UNISON, 0.6f}, {P_SPREAD, 14},
            {P_ATK, 0.3f}, {P_DEC, 1.0f}, {P_SUS, 0.9f}, {P_REL, 0.9f},
            {P_CUTOFF, 2200}, {P_RESO, 0.05f}, {P_FENV, 0.6f}, {P_FATK, 0.4f}, {P_FDEC, 1.2f}, {P_FSUS, 0.7f},
            {P_VIBDEPTH, 5}, {P_VIBDELAY, 0.5f}, {P_CHORUS, 0.5f}, {P_REVMIX, 0.35f}}},
    },
    { // ---- guitar
        {"Pluck", "soft nylon", {
            {P_WAVE1, W_SAW}, {P_WAVE2, W_SQUARE}, {P_MIX2, 0.25f}, {P_PW, 0.35f}, {P_DETUNE, 3}, {P_NOISE, 0.02f},
            {P_ATK, 0.001f}, {P_DEC, 1.1f}, {P_SUS, 0.0f}, {P_REL, 0.25f},
            {P_CUTOFF, 420}, {P_RESO, 0.18f}, {P_FENV, 3.6f}, {P_FDEC, 0.22f}, {P_FSUS, 0.08f}, {P_KTRK, 0.8f},
            {P_VIBDEPTH, 5}, {P_VIBDELAY, 0.5f}, {P_GLIDE, 70}, {P_DLYMIX, 0.06f}, {P_REVMIX, 0.14f}}},
        {"Steel", "bright picked", {
            {P_WAVE1, W_SAW}, {P_WAVE2, W_SQUARE}, {P_MIX2, 0.35f}, {P_PW, 0.3f}, {P_DETUNE, 4}, {P_NOISE, 0.04f},
            {P_ATK, 0.001f}, {P_DEC, 1.4f}, {P_SUS, 0.0f}, {P_REL, 0.3f},
            {P_CUTOFF, 700}, {P_RESO, 0.12f}, {P_FENV, 4.2f}, {P_FDEC, 0.3f}, {P_FSUS, 0.1f}, {P_KTRK, 0.9f},
            {P_GLIDE, 60}, {P_CHORUS, 0.15f}, {P_REVMIX, 0.15f}}},
        {"Clean", "electric chorus", {
            {P_WAVE1, W_TRI}, {P_WAVE2, W_SQUARE}, {P_MIX2, 0.35f}, {P_PW, 0.4f}, {P_DETUNE, 5},
            {P_ATK, 0.002f}, {P_DEC, 1.8f}, {P_SUS, 0.15f}, {P_REL, 0.35f},
            {P_CUTOFF, 1500}, {P_RESO, 0.1f}, {P_FENV, 2.2f}, {P_FDEC, 0.35f}, {P_FSUS, 0.25f}, {P_KTRK, 0.6f},
            {P_GLIDE, 60}, {P_CHORUS, 0.6f}, {P_DLYMIX, 0.12f}, {P_REVMIX, 0.2f}}},
        {"Drive", "overdriven lead", {
            {P_WAVE1, W_SQUARE}, {P_WAVE2, W_SAW}, {P_MIX2, 0.4f}, {P_PW, 0.42f}, {P_DETUNE, 6}, {P_SUB, 0.15f},
            {P_ATK, 0.004f}, {P_DEC, 2.5f}, {P_SUS, 0.45f}, {P_REL, 0.3f},
            {P_CUTOFF, 1900}, {P_RESO, 0.3f}, {P_FENV, 1.6f}, {P_FDEC, 0.5f}, {P_FSUS, 0.4f}, {P_KTRK, 0.5f},
            {P_VIBDEPTH, 12}, {P_VIBDELAY, 0.35f}, {P_GLIDE, 90}, {P_DRIVE, 0.85f}, {P_DLYMIX, 0.1f}, {P_REVMIX, 0.15f}}},
        {"Mute", "short palm", {
            {P_WAVE1, W_SAW}, {P_WAVE2, W_SQUARE}, {P_MIX2, 0.3f}, {P_PW, 0.4f}, {P_DETUNE, 3},
            {P_ATK, 0.001f}, {P_DEC, 0.16f}, {P_SUS, 0.0f}, {P_REL, 0.06f},
            {P_CUTOFF, 300}, {P_RESO, 0.1f}, {P_FENV, 2.6f}, {P_FDEC, 0.07f}, {P_FSUS, 0.0f}, {P_KTRK, 0.7f},
            {P_DRIVE, 0.35f}, {P_REVMIX, 0.05f}}},
        {"Twelve", "doubled octave", {
            {P_WAVE1, W_SAW}, {P_WAVE2, W_SAW}, {P_MIX2, 0.5f}, {P_SEMI2, 12}, {P_DETUNE, 10},
            {P_ATK, 0.001f}, {P_DEC, 1.5f}, {P_SUS, 0.0f}, {P_REL, 0.35f},
            {P_CUTOFF, 800}, {P_RESO, 0.1f}, {P_FENV, 3.8f}, {P_FDEC, 0.28f}, {P_FSUS, 0.1f}, {P_KTRK, 0.9f},
            {P_GLIDE, 60}, {P_CHORUS, 0.45f}, {P_REVMIX, 0.2f}}},
    },
    { // ---- bass
        {"Sub", "deep and plain", {
            {P_WAVE1, W_SINE}, {P_WAVE2, W_SINE}, {P_MIX2, 0.0f}, {P_SUB, 0.6f},
            {P_ATK, 0.004f}, {P_DEC, 0.3f}, {P_SUS, 0.85f}, {P_REL, 0.12f},
            {P_CUTOFF, 500}, {P_RESO, 0.0f}, {P_FENV, 0.5f}, {P_FDEC, 0.2f}, {P_KTRK, 0.3f},
            {P_GLIDE, 50}, {P_DRIVE, 0.1f}, {P_REVMIX, 0.0f}}},
        {"Round", "finger bass", {
            {P_WAVE1, W_TRI}, {P_WAVE2, W_SAW}, {P_MIX2, 0.25f}, {P_DETUNE, 2}, {P_SUB, 0.3f},
            {P_ATK, 0.003f}, {P_DEC, 0.5f}, {P_SUS, 0.4f}, {P_REL, 0.12f},
            {P_CUTOFF, 500}, {P_RESO, 0.1f}, {P_FENV, 1.8f}, {P_FDEC, 0.2f}, {P_FSUS, 0.2f}, {P_KTRK, 0.4f},
            {P_GLIDE, 40}, {P_REVMIX, 0.04f}}},
        {"Pick", "bright attack", {
            {P_WAVE1, W_SAW}, {P_WAVE2, W_SQUARE}, {P_MIX2, 0.2f}, {P_DETUNE, 2}, {P_SUB, 0.45f}, {P_NOISE, 0.02f},
            {P_ATK, 0.002f}, {P_DEC, 0.35f}, {P_SUS, 0.5f}, {P_REL, 0.12f},
            {P_CUTOFF, 600}, {P_RESO, 0.2f}, {P_FENV, 3.0f}, {P_FDEC, 0.14f}, {P_FSUS, 0.15f}, {P_KTRK, 0.3f},
            {P_GLIDE, 40}, {P_DRIVE, 0.2f}, {P_REVMIX, 0.04f}}},
        {"Acid", "squelchy", {
            {P_WAVE1, W_SAW}, {P_WAVE2, W_SAW}, {P_MIX2, 0.0f},
            {P_ATK, 0.001f}, {P_DEC, 0.4f}, {P_SUS, 0.6f}, {P_REL, 0.08f},
            {P_CUTOFF, 260}, {P_RESO, 0.7f}, {P_FENV, 3.5f}, {P_FDEC, 0.22f}, {P_FSUS, 0.1f}, {P_KTRK, 0.3f},
            {P_GLIDE, 90}, {P_DRIVE, 0.3f}, {P_REVMIX, 0.03f}}},
        {"Growl", "driven saws", {
            {P_WAVE1, W_SAW}, {P_WAVE2, W_SAW}, {P_MIX2, 0.5f}, {P_DETUNE, 14}, {P_SUB, 0.4f},
            {P_ATK, 0.003f}, {P_DEC, 0.6f}, {P_SUS, 0.8f}, {P_REL, 0.14f},
            {P_CUTOFF, 700}, {P_RESO, 0.15f}, {P_FENV, 1.2f}, {P_FDEC, 0.3f}, {P_FSUS, 0.4f}, {P_KTRK, 0.3f},
            {P_GLIDE, 50}, {P_DRIVE, 0.7f}, {P_REVMIX, 0.03f}}},
        {"Hollow", "square and sub", {
            {P_WAVE1, W_SQUARE}, {P_WAVE2, W_SQUARE}, {P_MIX2, 0.0f}, {P_PW, 0.5f}, {P_SUB, 0.5f},
            {P_ATK, 0.003f}, {P_DEC, 0.4f}, {P_SUS, 0.7f}, {P_REL, 0.1f},
            {P_CUTOFF, 800}, {P_RESO, 0.1f}, {P_FENV, 1.0f}, {P_FDEC, 0.2f}, {P_FSUS, 0.3f}, {P_KTRK, 0.3f},
            {P_GLIDE, 40}, {P_REVMIX, 0.03f}}},
    },
};

int clampi(int v, int hi) { return v < 0 ? 0 : (v >= hi ? hi - 1 : v); }

} // namespace

const Patch &librarySound(int kind, int index) {
    static Patch made[kLibKinds][kLibSounds];
    static bool have[kLibKinds][kLibSounds] = {{false}};
    kind = clampi(kind, kLibKinds); index = clampi(index, kLibSounds);
    if (!have[kind][index]) { made[kind][index] = build(kLib[kind][index]); have[kind][index] = true; }
    return made[kind][index];
}

const char *libraryHint(int kind, int index) { return kLib[clampi(kind, kLibKinds)][clampi(index, kLibSounds)].hint; }

int libraryFind(const char *name, int *index) {
    for (int k = 0; k < kLibKinds; ++k)
        for (int i = 0; i < kLibSounds; ++i)
            if (std::strcmp(kLib[k][i].name, name) == 0) { if (index) *index = i; return k; }
    return -1;
}

} // namespace td
