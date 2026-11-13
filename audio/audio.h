// Tendril — audio output: PipeWire (preferred), ALSA, or a silent clock when
// no device can be opened (so the instrument still runs and can be tested).
#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

struct td_pw;

class AudioOut {
public:
    using Render = std::function<void(float *l, float *r, int frames)>;

    ~AudioOut() { stop(); }
    // prefer: "auto", "pipewire", "alsa" or "null"
    bool start(Render render, int rate, int quantum, const std::string &prefer = "auto");
    void stop();

    const std::string &backend() const { return backendName; }
    const std::string &lastError() const { return error; }
    int sampleRate() const { return rate; }
    int quantum() const { return quant; }
    bool streaming() const;
    float load() const { return loadAvg.load(); }      // render time / real time, smoothed
    float peakLoad() const { return loadPeak.load(); } // worst block since last resetPeak()
    void resetPeak() { loadPeak.store(0); }
    unsigned overruns() const { return over.load(); }  // blocks slower than real time

    void renderInterleaved(float *dst, int frames);    // used by the backends

private:
    Render render;
    std::string backendName = "none", error;
    int rate = 48000, quant = 256;
    td_pw *pw = nullptr;
    void *alsa = nullptr;
    std::thread th;
    std::atomic<bool> running{false};
    std::atomic<float> loadAvg{0}, loadPeak{0};
    std::atomic<unsigned> over{0};
    std::vector<float> bl, br;
    bool ftz = false;

    bool startAlsa();
    void alsaLoop();
    void nullLoop();
};

// Record mono audio from the default input for the "listen" feature.
// Returns frames captured; 0 with `err` set when nothing could be recorded.
int captureMono(std::vector<float> &dst, int rate, double seconds, std::string &err);
