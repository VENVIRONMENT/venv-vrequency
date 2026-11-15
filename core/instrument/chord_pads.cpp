// venv-vrequency — the chord pads: a tapped chord held, or drone notes, on part 1.
#include "core/instrument/instrument.h"
#include "core/synth/scale.h"

namespace td {

void Instrument::chordTap(int degree) {
    degree = ((degree % 7) + 7) % 7;
    bool wasActive = padsActive();
    switch (st.padMode) {
    case PM_HOLD:
        if (holdChord == degree && padsPlaying) holdChord = -1;
        else { holdChord = degree; padsPlaying = true; }
        break;
    case PM_DRONE:
        droneMask ^= 1 << degree;
        if (droneMask) padsPlaying = true;
        break;
    }
    // The pads keep the sound that was live when they started.
    if (!wasActive && padsActive()) parts[1].patch = parts[0].patch;
}

// Slots 0..4 hold the chord (root and triad), slots 5..11 one drone each (degree 0..6).
void Instrument::tickPads() {
    for (int s = 0; s < kSlots; ++s) padDesired[s] = -1;
    const int deg = padsPlaying ? holdChord : -1;
    if (padsPlaying)
        for (int d = 0; d < 7; ++d)
            if (droneMask & (1 << d)) padDesired[kDroneSlot0 + d] = buildChord(st.tuning, d, false, 0, false).notes[0] + 12;
    curChord = deg;
    if (deg >= 0) {
        Chord c = buildChord(st.tuning, deg, false, 0, false);
        for (int i = 0; i < c.count && i < kChordSlots; ++i) padDesired[i] = c.notes[i];
    }
    for (int s = 0; s < kSlots; ++s) {
        int cur = slotVoice[1][s] >= 0 ? (int)slotNote[1][s] : -1;
        int want = padDesired[s];
        if (want < 0) { if (cur >= 0) opOff(1, s); }
        else if (cur < 0) opOn(1, s, (float)want, s == 0 ? 0.8f : 0.55f);
        else if (cur != want) opGlide(1, s, (float)want);
    }
}

} // namespace td
