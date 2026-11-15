// venv-vrequency — the eight pulse pads on the audio thread: their settings, the voices that play
// them, LATCH (a pad that keeps going) and a record of every hit for the Pulse page's strip.
//
// LATCH: a latched gated tone or pulse train runs without end; a latched one-shot is hit again
// RATE times a second. With SYNC the rate is snapped to a division of the tempo, and latched
// one-shots then fall exactly on that grid of the beat clock.
#pragma once
#include "core/pulse/pulse_voice.h"
#include <atomic>
#include <cstdint>

namespace td {

constexpr int kPulseVoices = 24;
constexpr int kPulseTickRing = 1024;   // must be a power of two

// one hit, one click of a train or one gate of a gated tone, as the strip draws it
struct PulseTick {
    uint64_t frame = 0;   // the engine's sample clock when it began
    uint32_t dur = 0;     // samples it lasts (0: a click)
    uint8_t pad = 0;
};

class PulseBank {
public:
    void init(float sampleRate);
    PulsePad pad[kPulsePads];
    bool latched[kPulsePads] = {false};
    bool sync = false;
    uint32_t hits[kPulsePads] = {0};   // counts every hit (taps, latch repeats, loop playback)

    // ---- audio thread ----
    void trigger(int p, float vel, float bpm);        // a tap: plays at once
    void set(int p, int field, float v, float bpm);   // one setting of a pad
    void latch(int p, bool on, double beat, float bpm);
    void silence();                                   // fade everything, drop every latch
    // adds n samples of the pulses (mono) to out; beat0..beat1 is the beat clock over the block,
    // frame0 the sample clock at its start
    void render(float *out, int n, double beat0, double beat1, float bpm, uint64_t frame0);
    bool active() const;
    float rateOf(int p, float bpm) const;             // RATE with SYNC applied

    // ---- any thread: the record of hits (one writer, the audio thread) ----
    // copies the ticks after `cursor` into dst (at most max), moves cursor on; old ones are lost
    // when the reader falls more than kPulseTickRing behind
    int readTicks(uint32_t &cursor, PulseTick *dst, int max) const;

private:
    float sr = 48000;
    PulseVoice v[kPulseVoices];
    double nextHit[kPulsePads] = {0};   // latched one-shots: samples to the next hit, or (SYNC) its beat
    float vel[kPulseVoices] = {0};
    PulseTick ring[kPulseTickRing];
    std::atomic<uint32_t> ringW{0};
    void start(int p, float velocity, float bpm, bool endless, int delay);
    void logTick(uint64_t frame, uint32_t dur, int p);
    void tickLatches(int n, double beat0, double beat1, float bpm);
};

} // namespace td
