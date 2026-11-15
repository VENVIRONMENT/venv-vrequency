// venv-vrequency — the performance settings and the names of their choices.
#pragma once
#include "core/drums/drums.h"
#include "core/synth/scale.h"

namespace td {

enum Setting {
    S_ROOT, S_SCALE, S_OCTAVE, S_ROWINT, S_BPM, S_VOLUME, S_XYMAP, S_XYHOLD,
    S_BACKMODE, S_BACKSTYLE, S_CHORDBEATS, S_ARPRATE, S_SEVENTH, S_METRO,
    S_BENDRANGE, S_BENDTIME, S_GLIDESCALE, S_BACKVOL, S_LOOPVOL, S_LATENCY,
    S_KIT, S_BEATVOL,
    S_POLY,
    S_COUNT
};

enum BackMode { BM_PROG, BM_HOLD, BM_DRONE, BM_COUNT };
enum BackStyle { BS_SUSTAIN, BS_PULSE, BS_ARP_UP, BS_ARP_DOWN, BS_ARP_UPDOWN, BS_COUNT };
enum XyMap { XY_FILTER, XY_WOBBLE, XY_MORPH, XY_SPACE, XY_BEND, XY_GLITCH, XY_COUNT };

inline const char *xyMapName(int m) {
    static const char *N[XY_COUNT] = {"FILTER", "WOBBLE", "MORPH", "SPACE", "BEND", "GLITCH"};
    return N[m < 0 ? 0 : (m >= XY_COUNT ? XY_COUNT - 1 : m)];
}
inline const char *xyAxisName(int m, bool yAxis) {
    static const char *X[XY_COUNT] = {"CUTOFF", "CUTOFF", "TIMBRE", "DELAY", "PITCH", "COARSE"};
    static const char *Y[XY_COUNT] = {"RESONANCE", "VIBRATO", "DRIVE", "REVERB", "CUTOFF", "GRIT"};
    m = m < 0 ? 0 : (m >= XY_COUNT ? XY_COUNT - 1 : m);
    return yAxis ? Y[m] : X[m];
}
inline const char *backModeName(int m) {
    static const char *N[BM_COUNT] = {"PROGRESSION", "HOLD", "DRONE"};
    return N[m < 0 ? 0 : (m >= BM_COUNT ? BM_COUNT - 1 : m)];
}
inline const char *backStyleName(int m) {
    static const char *N[BS_COUNT] = {"SUSTAIN", "PULSE", "ARP UP", "ARP DOWN", "ARP UP-DN"};
    return N[m < 0 ? 0 : (m >= BS_COUNT ? BS_COUNT - 1 : m)];
}

struct Settings {
    Tuning tuning;
    float bpm = 100;
    float volume = 0.7f;
    int xyMap = XY_FILTER;
    bool xyHold = false;
    int backMode = BM_PROG;
    int backStyle = BS_SUSTAIN;
    int chordBeats = 4;
    int arpRate = 2;       // steps per beat
    bool seventh = false;
    bool metro = false;
    float bendRange = 2;   // semitones
    float bendTime = 120;  // ms
    float glideScale = 1;  // multiplies each patch's glide time
    float backVol = 0.7f;
    float loopVol = 0.9f;
    float latencyMs = 0;   // output delay (e.g. Bluetooth): loop recordings are shifted earlier by this
    int kit = KIT_SOFT;    // drum kit
    float beatVol = 0.7f;
    bool poly = false;     // the letter keys: false = three strings (one note per row, slides), true = every key its own note
};

} // namespace td
