// venv-vrequency — the drums: eight sounds in each of three kits, played on a bar of sixteen steps.
#pragma once

namespace td {

enum Drum { D_KICK, D_SNARE, D_HAT, D_OPENHAT, D_CLAP, D_RIM, D_TOM, D_COWBELL, kDrums };
enum Kit { KIT_SOFT, KIT_CRUNCHY, KIT_GLITCHY, kKits };
constexpr int kDrumSteps = 16;   // one bar of sixteenth notes

// Straight sixteenths played by a machine sound stiff: the hats off the beat are a little quieter.
inline float stepVelocity(int drum, int step) { return (drum == D_HAT && (step & 3) != 0) ? 0.7f : 1.0f; }

inline const char *drumName(int d) {
    static const char *N[kDrums] = {"KICK", "SNARE", "HAT", "OPEN HAT", "CLAP", "RIM", "TOM", "COWBELL"};
    return N[d < 0 ? 0 : (d >= kDrums ? kDrums - 1 : d)];
}
inline const char *kitName(int k) {
    static const char *N[kKits] = {"SOFT", "CRUNCHY", "GLITCHY"};
    return N[k < 0 ? 0 : (k >= kKits ? kKits - 1 : k)];
}

} // namespace td
