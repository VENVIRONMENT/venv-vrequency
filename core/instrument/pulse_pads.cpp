// venv-vrequency — the pulse pads inside the instrument: taps, the loop, and the mix.
//
// A tap is written into the loop while it records (OP_PULSE) and played back by the layer with the
// pad's settings of the moment. Latched repeats are not written: the loop keeps what was tapped.
#include "core/instrument/instrument.h"

namespace td {

void Instrument::pulseHit(int pad, float vel, bool record) {
    if (pad < 0 || pad >= kPulsePads) return;
    pulses.trigger(pad, vel, st.bpm);
    if (record) loopRecord(OP_PULSE, pad, vel, 0);
}

void Instrument::renderPulses(float *l, float *r, int n, double b0, double b1) {
    float m[kBlock];
    for (int i = 0; i < n; ++i) m[i] = 0;
    pulses.render(m, n, b0, b1, st.bpm, frames);
    const float g = 0.8f; // in the middle, dry: a pulse is meant to be exact
    for (int i = 0; i < n; ++i) { l[i] += m[i] * g; r[i] += m[i] * g; }
}

} // namespace td
