// venv-vrequency — drums: three synthesised kits of eight sounds, and the voices that play them.
//
// Every sound is computed once when the instrument is built (no sample files), then played
// back from memory, so a hit costs almost nothing on the audio thread.
#pragma once
#include "dsp.h"
#include <cstdint>
#include <vector>

namespace td {

enum Drum { D_KICK, D_SNARE, D_HAT, D_OPENHAT, D_CLAP, D_RIM, D_TOM, D_COWBELL, kDrums };
enum Kit { KIT_SOFT, KIT_CRUNCHY, KIT_GLITCHY, kKits };
constexpr int kDrumSteps = 16;   // one bar of sixteenth notes
constexpr int kDrumVoices = 12;

inline const char *drumName(int d) {
    static const char *N[kDrums] = {"KICK", "SNARE", "HAT", "OPEN HAT", "CLAP", "RIM", "TOM", "COWBELL"};
    return N[d < 0 ? 0 : (d >= kDrums ? kDrums - 1 : d)];
}
inline const char *kitName(int k) {
    static const char *N[kKits] = {"SOFT", "CRUNCHY", "GLITCHY"};
    return N[k < 0 ? 0 : (k >= kKits ? kKits - 1 : k)];
}

class DrumKit {
public:
    void init(float sampleRate);                    // renders all kits (not on the audio thread)
    void trigger(int kit, int drum, float vel);     // audio thread
    void render(int kit, float *l, float *r, int n); // adds the drum mix to l / r
    bool active() const { return nActive > 0; }
    float reverbSend(int kit) const { return kit == KIT_SOFT ? 0.14f : (kit == KIT_CRUNCHY ? 0.03f : 0.0f); }
    void silence();                                  // fast fade of everything
    const std::vector<float> &sample(int kit, int drum) const { return smp[kit][drum]; }

private:
    struct V {
        const std::vector<float> *s = nullptr;
        int pos = 0, drum = 0;
        float gain = 0, gl = 0, gr = 0, fade = 1;
        bool fading = false;
    };
    float sr = 48000;
    std::vector<float> smp[kKits][kDrums];
    V v[kDrumVoices];
    int nActive = 0;
    float fadeCoef = 0.99f;
    // bus processing state (crunchy: saturation, glitchy: sample-and-hold + bit reduction)
    float holdPh = 1, holdL = 0, holdR = 0;
};

} // namespace td
