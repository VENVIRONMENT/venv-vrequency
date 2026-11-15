// venv-vrequency — the backing: tapped chords played as a progression, a held chord or drones.
#include "core/instrument/instrument.h"
#include "core/synth/scale.h"
#include <cmath>

namespace td {

void Instrument::chordTap(int degree) {
    degree = ((degree % 7) + 7) % 7;
    bool wasActive = backingActive();
    switch (st.backMode) {
    case BM_PROG:
        if (selSlot >= 0 && selSlot < progLen) { prog[selSlot] = degree; selSlot = -1; }
        else if (progLen < kProgMax) prog[progLen++] = degree;
        if (!backPlaying || !wasActive) {
            if (loopState == L_EMPTY && !wasActive && !beatPlaying) { beat = 0; progOrigin = 0; }
            else progOrigin = std::floor(beat / st.chordBeats) * st.chordBeats;
            lastPulseStep = lastArpStep = -1;
            backPlaying = true;
        }
        break;
    case BM_HOLD:
        if (holdChord == degree && backPlaying) holdChord = -1;
        else { holdChord = degree; backPlaying = true; }
        break;
    case BM_DRONE:
        droneMask ^= 1 << degree;
        if (droneMask) backPlaying = true;
        break;
    }
    // The backing keeps the sound that was live when it started.
    if (!wasActive && backingActive()) parts[1].patch = parts[0].patch;
}

void Instrument::tickBacking(double b0, double b1) {
    for (int s = 0; s < kSlots; ++s) backDesired[s] = -1;
    int deg = -1;
    progPos = -1;
    if (backPlaying) {
        if (progLen > 0) {
            double rel = (b0 - progOrigin) / st.chordBeats;
            long idx = (long)std::floor(rel);
            idx %= progLen; if (idx < 0) idx += progLen;
            progPos = (int)idx;
            deg = prog[idx];
        }
        if (holdChord >= 0) deg = holdChord;
        for (int d = 0; d < 7; ++d)
            if (droneMask & (1 << d)) {
                Chord c = buildChord(st.tuning, d, false, 0, false);
                backDesired[kDroneSlot0 + d] = c.notes[0] + 12;
            }
    }
    bool chordChanged = deg != curChord;
    curChord = deg;
    bool managed = st.backStyle != BS_SUSTAIN; // slots 1..4 triggered rhythmically
    if (deg >= 0) {
        Chord c = buildChord(st.tuning, deg, st.seventh, 0, false);
        backDesired[0] = c.notes[0];
        if (st.backStyle == BS_SUSTAIN) {
            for (int i = 1; i < c.count; ++i) backDesired[i] = c.notes[i];
        } else if (st.backStyle == BS_PULSE) {
            long step = (long)std::floor(b1);
            if (step != lastPulseStep || chordChanged) {
                lastPulseStep = step;
                for (int i = 1; i < kChordSlots; ++i) {
                    if (i < c.count) opOn(1, i, (float)c.notes[i], 0.55f);
                    else opOff(1, i);
                }
                pulseOffBeat = step + 0.55;
            }
            if (pulseOffBeat >= 0 && b1 >= pulseOffBeat) {
                for (int i = 1; i < kChordSlots; ++i) opOff(1, i);
                pulseOffBeat = -1;
            }
        } else { // arpeggio on slot 1, two-octave span
            int tones[8], nt = 0;
            for (int i = 1; i < c.count; ++i) tones[nt++] = c.notes[i];
            int base = nt;
            for (int i = 0; i < base; ++i) tones[nt++] = tones[i] + 12;
            long step = (long)std::floor(b1 * st.arpRate);
            if (step != lastArpStep) {
                lastArpStep = step;
                if (st.backStyle == BS_ARP_UP) arpIdx = (arpIdx + 1) % nt;
                else if (st.backStyle == BS_ARP_DOWN) arpIdx = (arpIdx - 1 + nt) % nt;
                else {
                    if (arpIdx >= nt - 1) arpDir = -1; else if (arpIdx <= 0) arpDir = 1;
                    arpIdx += arpDir;
                }
                if (arpIdx < 0) arpIdx = 0;
                if (arpIdx >= nt) arpIdx = nt - 1;
                opOn(1, 1, (float)tones[arpIdx], 0.7f);
                for (int i = 2; i < kChordSlots; ++i) opOff(1, i);
                arpOffBeat = (step + 0.8) / st.arpRate;
            }
            if (arpOffBeat >= 0 && b1 >= arpOffBeat) { opOff(1, 1); arpOffBeat = -1; }
        }
    }
    for (int s = 0; s < kSlots; ++s) {
        bool rhythmic = managed && deg >= 0 && s >= 1 && s < kChordSlots;
        if (rhythmic) continue;
        int cur = slotVoice[1][s] >= 0 ? (int)slotNote[1][s] : -1;
        int want = backDesired[s];
        if (want < 0) { if (cur >= 0) opOff(1, s); }
        else if (cur < 0) opOn(1, s, (float)want, s == 0 ? 0.8f : 0.55f);
        else if (cur != want) opGlide(1, s, (float)want);
    }
}

} // namespace td
