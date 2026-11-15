// venv-vrequency — poly mode: every letter key is its own note, like a keyboard, instead of the
// three strings (one note per row that slides). Each held key owns one of the lead part's voice
// slots; the strings' held-key lists are still kept so the screen shows what is held.
#include "core/instrument/instrument.h"

namespace td {

namespace {
int keyId(int s, int col) { return (s << 16) | (col & 0xffff); }
} // namespace

// Switching mode lets every lead note go, so nothing is left hanging from the other way of playing.
void Instrument::setPoly(bool on) {
    if (on == st.poly) return;
    for (int s = 0; s < kSlots; ++s) {
        if (slotVoice[0][s] >= 0) leadOp(OP_OFF, s, 0);
        polyKey[s] = -1; polySound[s] = -1; polyPending[s] = false;
    }
    for (int s = 0; s < kStrings; ++s) { strings[s].n = 0; strings[s].pendingOff = false; }
    st.poly = on;
}

void Instrument::polyDown(int s, int col, float pitch) {
    const int id = keyId(s, col);
    int slot = -1;
    for (int i = 0; i < kSlots && slot < 0; ++i) {   // a free slot, starting after the last one used
        const int k = (polyNext + i) % kSlots;
        if (polyKey[k] < 0 && (slotVoice[0][k] < 0 || polyPending[k])) slot = k;
    }
    if (slot < 0) slot = polyNext % kSlots;           // all twelve held: the oldest gives way
    polyNext = (slot + 1) % kSlots;
    polyKey[slot] = id; polySound[slot] = id; polyPending[slot] = false;
    leadOp(OP_ON, slot, pitch, 1.0f);
}

void Instrument::polyUp(int s, int col) {
    const int id = keyId(s, col);
    for (int k = 0; k < kSlots; ++k) {
        if (polyKey[k] != id) continue;
        polyKey[k] = -1;
        if (sustain) polyPending[k] = true;
        else { leadOp(OP_OFF, k, 0); polySound[k] = -1; }
    }
}

void Instrument::polyRelease() {
    for (int k = 0; k < kSlots; ++k)
        if (polyPending[k]) { polyPending[k] = false; if (polyKey[k] < 0) { leadOp(OP_OFF, k, 0); polySound[k] = -1; } }
}

void Instrument::polyOctave(int delta) {
    for (int k = 0; k < kSlots; ++k)
        if (polyKey[k] >= 0 && slotVoice[0][k] >= 0) leadOp(OP_GLIDE, k, slotNote[0][k] + 12.0f * delta);
}

// For the screen: which keys are held and which notes still sound (sustain), per row.
void Instrument::polyMasks(int s, uint16_t &held, uint16_t &sound) const {
    held = sound = 0;
    const StringState &ss = strings[s];
    for (int i = 0; i < ss.n; ++i) if ((ss.col[i] & 0xff) < 16) held |= (uint16_t)(1u << (ss.col[i] & 0xff));
    for (int k = 0; k < kSlots; ++k)
        if (polySound[k] >= 0 && (polySound[k] >> 16) == s && slotVoice[0][k] >= 0 && (polySound[k] & 0xff) < 16)
            sound |= (uint16_t)(1u << (polySound[k] & 0xff));
}

} // namespace td
