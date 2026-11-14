// venv-vrequency — the instrument's voice pool: allocation, stealing, and the note operations every part uses.
#include "core/instrument/instrument.h"
#include <cstdint>

namespace td {

float Instrument::panFor(int part, int slot) const {
    if (part == 1) {
        static const float P[kChordSlots] = {0.0f, -0.5f, 0.5f, -0.25f, 0.25f};
        return slot < kChordSlots ? P[slot] : 0.0f;
    }
    static const float L[kStrings] = {-0.3f, 0.0f, 0.3f};
    return L[slot % kStrings];
}

float Instrument::glideMs(int part) const { return parts[part].patch.p[P_GLIDE] * st.glideScale; }

int Instrument::allocVoice() {
    for (int i = 0; i < kVoices; ++i)
        if (!voices[i].active()) return i;
    int best = -1; float bestV = 1e9f;
    for (int i = 0; i < kVoices; ++i)
        if (!voices[i].gate && voices[i].amp.v < bestV) { bestV = voices[i].amp.v; best = i; }
    if (best < 0) { // everything is held: take the oldest
        uint64_t old = ~0ull;
        for (int i = 0; i < kVoices; ++i)
            if (voices[i].serial < old) { old = voices[i].serial; best = i; }
        Voice &v = voices[best];
        if (v.part >= 0 && slotVoice[v.part][v.slot] == best) slotVoice[v.part][v.slot] = -1;
    }
    return best;
}

void Instrument::opOn(int part, int slot, float pitch, float vel) {
    int old = slotVoice[part][slot];
    if (old >= 0) voices[old].release();
    int v = allocVoice();
    voices[v].start(part, slot, pitch + partShift[part], vel, panFor(part, slot), serial++);
    slotVoice[part][slot] = v;
    slotNote[part][slot] = pitch;
}

void Instrument::opGlide(int part, int slot, float pitch) {
    int v = slotVoice[part][slot];
    if (v < 0) { opOn(part, slot, pitch, 1.0f); return; }
    voices[v].glideTo(pitch + partShift[part], glideMs(part), sr);
    slotNote[part][slot] = pitch;
}

void Instrument::opOff(int part, int slot) {
    int v = slotVoice[part][slot];
    if (v >= 0) voices[v].release();
    slotVoice[part][slot] = -1;
}

void Instrument::partOff(int part, bool hard) {
    for (int s = 0; s < kSlots; ++s) {
        int v = slotVoice[part][s];
        if (v >= 0) { if (hard) voices[v].kill(sr); else voices[v].release(); }
        slotVoice[part][s] = -1;
    }
    if (hard)
        for (auto &v : voices)
            if (v.part == part) v.kill(sr);
}

} // namespace td
