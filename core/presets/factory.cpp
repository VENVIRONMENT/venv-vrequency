// venv-vrequency — the ten factory sounds and the five extra "dream" sounds.
#include "core/presets/patch_values.h"
#include "core/presets/presets.h"
#include <initializer_list>

namespace td {

namespace {

Patch make(const char *name, std::initializer_list<PV> vals) { return withValues(Patch(), name, vals.begin(), vals.size()); }

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

} // namespace td
