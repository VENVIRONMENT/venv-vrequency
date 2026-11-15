// venv-vrequency — the four Freq tones on the audio thread: oscillators, 2 > 1, GATE, the warp
// stage, and a tap of their own output for the Freq page's views.
//
// Frequencies and levels glide (about 20 ms) so a change never clicks; the sound starts and stops
// with a 5 ms fade, GATE's edges with a 1 ms one. Saw and square are band-limited (polyBLEP,
// core/dsp.h); NOISE is white noise through a narrow band-pass at the tone's Hz.
// Safe level: each tone is at most 1/4 of full scale and the warp stage keeps the peak, so all
// four at full level with every warp maxed stay inside +-kFreqOut.
// No allocation and no locks after init().
#pragma once
#include "core/dsp.h"
#include "core/freq/freq.h"
#include <atomic>
#include <cstdint>
#include <vector>

namespace td {

constexpr int kFreqScope = 1 << 17;   // 2.7 s at 48 kHz (a power of two)
constexpr float kFreqOut = 0.9f;      // the bank's output peak at most

class FreqBank {
public:
    void init(float sampleRate);
    FreqState s;
    bool keyHeld = false;              // KEYS: a letter key is down

    // ---- audio thread ----
    void set(int tone, int field, float v);       // one EV_FREQSET
    void silence();                               // panic: HOLD, GATE and the held key let go
    void restore(const FreqState &in);            // a session: the settings, nothing sounding
    // writes n samples of the Freq sound (mono) to out, and into the tap; beat0..beat1 is the beat
    // clock over the block (GATE with SYNC follows it)
    void render(float *out, int n, double beat0, double beat1, float bpm, bool sync);
    void idle(int n);                             // nothing to play: the tap gets silence
    bool active() const { return wanted() || amp > 1e-5f; }
    bool sounding() const { return wanted(); }    // something asks the tones to play
    float rateOf(float bpm, bool sync, float *perBeat = nullptr) const;   // GATE's rate, SYNC applied

    // ---- any thread: the newest n samples of the Freq output (one writer, the audio thread) ----
    void readScope(float *dst, int n) const;

private:
    struct Osc {
        float ph = 0, hz = 220, lvl = 0, lastPh = 0;
        Svf band;                                 // NOISE: the band-pass
        float bandHz = 0;
    };
    float sr = 48000, glideC = 0.003f, fadeC = 0.004f, edgeC = 0.02f;
    Osc o[kFreqTones];
    Rng rng;
    Warp warp;
    float amp = 0, gateAmp = 0;
    double gatePh = 0;
    std::vector<float> tap;
    std::atomic<uint32_t> tapW{0};

    bool wanted() const { return s.keys ? (s.hold || keyHeld) : (s.hold || s.gate); }
    float wave(int i, float ph, float dt);
    void write(const float *x, int n);
};

} // namespace td
