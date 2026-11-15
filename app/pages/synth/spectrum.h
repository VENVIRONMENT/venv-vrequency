// venv-vrequency — the spectrum both scopes draw (the Synth page's and the Freq page's): the newest
// 4096 real output samples, a Hann window, a radix-2 FFT, magnitudes in dB (a full-scale sine is
// 0 dB), read off on a log frequency axis from 30 Hz to 16 kHz.
#pragma once
#include <QtGlobal>
#include <vector>

namespace spec {
constexpr int kN = 4096;                         // samples per spectrum
constexpr float kLoHz = 30, kHiHz = 16000, kDbLo = -90;

// dB of bins 0..kN/2 of the kN samples at x (db gets kN/2 + 2 values)
void dbSpectrum(const float *x, std::vector<float> &db);
// For n log-spaced points from kLoHz to kHiHz: the FFT bins each one covers. Make it once per size.
struct LogTable {
    int n = 0; float sr = 0; std::vector<float> lo, hi;
    void make(int count, float rate);
    float at(const float *db, int i) const;      // the loudest bin inside point i, or the line between two
};
qreal xOfHz(float f, qreal w);                   // where f sits across a width w
// the strongest peak between kLoHz and kHiHz (a parabola through its neighbours); 0 when below -75 dB
float peakHz(const std::vector<float> &db, float sr, float *peakDb = nullptr);
} // namespace spec
