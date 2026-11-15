// venv-vrequency — the Freq tones inside the instrument: the mix, and the send into the master delay.
//
// The Freq sound goes in the middle, dry, like the pulse pads; DELAY sends it into the same tempo
// delay the synth uses. Its own tap (FreqBank::readScope) is taken before the delay, so the Freq
// page draws exactly what the four tones and the warp stage make.
#include "core/instrument/instrument.h"

namespace td {

void Instrument::renderFreq(float *l, float *r, float *send, int n, double b0, double b1) {
    float m[kBlock];
    freq.render(m, n, b0, b1, st.bpm, st.pulseSync);
    const float d = freq.s.delay;
    for (int i = 0; i < n; ++i) { l[i] += m[i]; r[i] += m[i]; send[i] += m[i] * d; }
}

} // namespace td
