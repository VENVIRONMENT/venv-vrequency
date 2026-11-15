// venv-vrequency — engine tests: what the Freq tests share (the bank rendered alone, the loudness measures).
#pragma once
#include "tests/support.h"
#include "core/freq/freq_bank.h"
#include <algorithm>
#include <vector>

namespace freqt {
// the Freq bank alone, rendered offline in blocks of kBlock
inline std::vector<float> renderBank(FreqBank &b, double seconds, double bpm = 120, bool sync = false) {
    const int n = (int)(seconds * SR);
    std::vector<float> out(n);
    double beat = 0;
    for (int i = 0; i < n; i += kBlock) {
        const int m = std::min(kBlock, n - i);
        const double b1 = beat + bpm / 60.0 / SR * m;
        b.render(out.data() + i, m, beat, b1, (float)bpm, sync);
        beat = b1;
    }
    return out;
}
// rising crossings of the loudness (50 ms RMS) through its mean, per second
inline float envelopeHz(const std::vector<float> &x, int from) {
    const int w = 2400; std::vector<float> e;
    for (int i = from; i + w <= (int)x.size(); i += w) e.push_back(rms(x.data() + i, w));
    float mean = 0; for (float v : e) mean += v; mean /= std::max<size_t>(1, e.size());
    double first = -1, last = -1; int count = 0;
    for (size_t i = 1; i < e.size(); ++i)
        if (e[i - 1] < mean && e[i] >= mean) {
            const double t = (i - 1) + (mean - e[i - 1]) / (double)(e[i] - e[i - 1]);
            if (first < 0) first = t;
            last = t; ++count;
        }
    return count < 2 ? 0 : (float)((count - 1) / ((last - first) * w / SR));
}
inline int onsets(const std::vector<float> &x, float thr) { // the gate opening: loudness crossing thr upwards
    int c = 0; bool open = false;
    for (size_t i = 0; i + 48 <= x.size(); i += 48) {
        const float r = rms(x.data() + i, 48);
        if (!open && r > thr) { ++c; open = true; } else if (open && r < thr * 0.5f) open = false;
    }
    return c;
}
inline float maxStep(const std::vector<float> &x, int from, int to) {
    float m = 0;
    for (int i = std::max(1, from); i < to; ++i) m = std::max(m, std::fabs(x[i] - x[i - 1]));
    return m;
}
inline void solo(FreqBank &b, int wave, float hz) {
    b.set(0, FF_WAVE, (float)wave); b.set(0, FF_HZ, hz); b.set(0, FF_LEVEL, 1);
    for (int i = 1; i < kFreqTones; ++i) b.set(i, FF_ON, 0);
    b.set(-1, FF_HOLD, 1);
}
} // namespace freqt
using namespace freqt;
