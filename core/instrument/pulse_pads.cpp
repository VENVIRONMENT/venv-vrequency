// venv-vrequency — the pulse pads inside the instrument: taps, the loop, and the mix.
//
// A tap is written into the loop while it records (OP_PULSE) and played back by the layer with the
// pad's settings of the moment. Latched repeats are not written: the loop keeps what was tapped.
// A held pad is written as its key down and up, so the layer holds it as long as the player did.
#include "core/instrument/instrument.h"

namespace td {

void Instrument::pulseHit(int pad, float vel, bool record, int part) {
    if (pad < 0 || pad >= kPulsePads) return;
    pulses.trigger(pad, vel, st.bpm, part);
    if (record) loopRecord(OP_PULSE, pad, vel, 0);
}

void Instrument::holdPulse(int pad, bool down, int layer) {
    if (pad < 0 || pad >= kPulsePads) return;
    if (layer >= 0) { pulses.loopHold(pad, layer, down, beat, st.bpm); return; }
    if (!down && !pulses.held[pad]) return;
    pulses.hold(pad, down, beat, st.bpm);
    if (down || !pulses.sustained[pad]) loopRecord(OP_PULSE, pad, 1.0f, down ? 1.0f : 2.0f); // sustained: it ends when sustain lifts
}

void Instrument::renderPulses(float *l, float *r, int n, double b0, double b1) {
    float m[kBlock];
    for (int i = 0; i < n; ++i) m[i] = 0;
    float bend[kParts]; // each part's bend: the pulses bend with the lead, a loop's with its layer
    for (int p = 0; p < kParts; ++p) bend[p] = parts[p].bend;
    pulses.render(m, n, b0, b1, st.bpm, frames, bend);
    const float g = 0.8f; // in the middle, dry: a pulse is meant to be exact
    for (int i = 0; i < n; ++i) { l[i] += m[i] * g; r[i] += m[i] * g; }
}

} // namespace td
