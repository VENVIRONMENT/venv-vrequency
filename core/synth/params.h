// venv-vrequency — patch parameter table.
// A Patch is a flat array of floats described by a metadata table, so the
// engine, the randomiser, the UI sliders and the JSON files all share one
// definition of what a sound is.
#pragma once
#include <cstring>

namespace td {

enum Wave { W_SINE, W_TRI, W_SAW, W_SQUARE, W_COUNT };
enum FilterMode { F_LP, F_HP, F_BP, F_COUNT };

enum Param {
    // oscillators
    P_WAVE1, P_WAVE2, P_MIX2, P_SEMI2, P_DETUNE, P_UNISON, P_SPREAD,
    P_SUB, P_NOISE, P_PW, P_FM,
    // amp envelope
    P_ATK, P_DEC, P_SUS, P_REL,
    // filter
    P_FMODE, P_CUTOFF, P_RESO, P_FENV, P_FATK, P_FDEC, P_FSUS, P_FREL, P_KTRK,
    // modulation
    P_VIBRATE, P_VIBDEPTH, P_VIBDELAY, P_LFORATE, P_LFOCUT, P_LFOAMP, P_LFOPW,
    // performance
    P_GLIDE,
    // effects
    P_DRIVE, P_CHORUS, P_DLYMIX, P_DLYDIV, P_DLYFB, P_REVMIX, P_REVSIZE,
    P_LEVEL,
    P_COUNT
};

struct ParamInfo {
    const char *key;    // stable name used in JSON files
    const char *label;  // short UI label
    float min, max, def;
    bool integer;       // stepped value (wave, mode, semitones...)
    bool logScale;      // UI slider + randomiser use a log curve
};

inline const ParamInfo &paramInfo(int p) {
    static const ParamInfo T[P_COUNT] = {
        {"wave1", "WAVE A", 0, 3, 2, true, false},
        {"wave2", "WAVE B", 0, 3, 2, true, false},
        {"mix2", "B MIX", 0, 1, 0.0f, false, false},
        {"semi2", "B PITCH", -12, 24, 0, true, false},
        {"detune", "DETUNE", 0, 50, 6, false, false},        // cents
        {"unison", "UNISON", 0, 1, 0, false, false},
        {"spread", "SPREAD", 0, 40, 12, false, false},       // cents
        {"sub", "SUB", 0, 1, 0, false, false},
        {"noise", "NOISE", 0, 0.6f, 0, false, false},
        {"pw", "WIDTH", 0.1f, 0.9f, 0.5f, false, false},
        {"fm", "FM", 0, 6, 0, false, false},
        {"attack", "ATTACK", 0.001f, 4, 0.005f, false, true},
        {"decay", "DECAY", 0.01f, 6, 0.3f, false, true},
        {"sustain", "SUSTAIN", 0, 1, 0.7f, false, false},
        {"release", "RELEASE", 0.01f, 8, 0.3f, false, true},
        {"fmode", "FILTER", 0, 2, 0, true, false},
        {"cutoff", "CUTOFF", 40, 16000, 2400, false, true},  // Hz
        {"reso", "RESO", 0, 0.95f, 0.2f, false, false},
        {"fenv", "F.ENV", -4, 6, 1.5f, false, false},        // octaves
        {"fattack", "F.ATK", 0.001f, 4, 0.005f, false, true},
        {"fdecay", "F.DEC", 0.01f, 6, 0.35f, false, true},
        {"fsustain", "F.SUS", 0, 1, 0.3f, false, false},
        {"frelease", "F.REL", 0.01f, 8, 0.3f, false, true},
        {"keytrack", "K.TRACK", 0, 1, 0.5f, false, false},
        {"vibrate", "VIB HZ", 0.5f, 9, 5.2f, false, false},
        {"vibdepth", "VIBRATO", 0, 60, 6, false, false},     // cents
        {"vibdelay", "VIB DLY", 0, 2, 0.35f, false, false},  // seconds
        {"lforate", "LFO HZ", 0.05f, 12, 0.6f, false, true},
        {"lfocut", "LFO>CUT", 0, 3, 0, false, false},        // octaves
        {"lfoamp", "LFO>AMP", 0, 1, 0, false, false},
        {"lfopw", "LFO>WID", 0, 0.4f, 0, false, false},
        {"glide", "GLIDE", 0, 2000, 120, false, false},      // ms
        {"drive", "DRIVE", 0, 1, 0, false, false},
        {"chorus", "CHORUS", 0, 1, 0, false, false},
        {"delaymix", "DELAY", 0, 0.7f, 0.15f, false, false},
        {"delaydiv", "DLY TIME", 0, 4, 2, true, false},      // index for delayBeats()
        {"delayfb", "DLY FB", 0, 0.8f, 0.35f, false, false},
        {"reverbmix", "REVERB", 0, 0.8f, 0.2f, false, false},
        {"reverbsize", "REV SIZE", 0.2f, 0.98f, 0.7f, false, false},
        {"level", "LEVEL", 0.02f, 4, 1, false, true},
    };
    return T[p];
}

// Delay times in beats for P_DLYDIV.
inline float delayBeats(int idx) {
    static const float B[5] = {0.25f, 0.375f, 0.5f, 0.75f, 1.0f};
    return B[idx < 0 ? 0 : (idx > 4 ? 4 : idx)];
}
inline const char *delayDivName(int idx) {
    static const char *N[5] = {"1/16", "1/16.", "1/8", "1/8.", "1/4"};
    return N[idx < 0 ? 0 : (idx > 4 ? 4 : idx)];
}
inline const char *waveName(int w) {
    static const char *N[4] = {"SINE", "TRI", "SAW", "SQUARE"};
    return N[w & 3];
}
inline const char *filterName(int f) {
    static const char *N[3] = {"LOW", "HIGH", "BAND"};
    return N[f < 0 ? 0 : (f > 2 ? 2 : f)];
}

struct Patch {
    char name[28];
    float p[P_COUNT];

    Patch() {
        std::memset(name, 0, sizeof name);
        std::strcpy(name, "Init");
        for (int i = 0; i < P_COUNT; ++i) p[i] = paramInfo(i).def;
    }
    void setName(const char *n) {
        std::memset(name, 0, sizeof name);
        for (size_t i = 0; i + 1 < sizeof name && n[i]; ++i) name[i] = n[i];
    }
    void clamp() {
        for (int i = 0; i < P_COUNT; ++i) {
            const ParamInfo &m = paramInfo(i);
            float v = p[i];
            if (!(v == v)) v = m.def; // NaN guard
            if (v < m.min) v = m.min;
            if (v > m.max) v = m.max;
            if (m.integer) v = (float)(int)(v + (v >= 0 ? 0.5f : -0.5f));
            p[i] = v;
        }
    }
};

} // namespace td
