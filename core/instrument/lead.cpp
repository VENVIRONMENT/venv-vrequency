// venv-vrequency — the lead part: keys and strings, sustain, octave, the XY pad, panic.
#include "core/instrument/instrument.h"
#include "core/dsp.h"

namespace td {

void Instrument::leadOp(uint8_t op, int slot, float a, float b) {
    switch (op) {
    case OP_ON: opOn(0, slot, a, b); break;
    case OP_GLIDE: opGlide(0, slot, a); break;
    case OP_OFF: opOff(0, slot); break;
    default: break;
    }
    loopRecord(op, slot, a, b);
}

void Instrument::keyDown(int s, int col, bool chromatic) {
    if (s < 0 || s >= kStrings) return;
    StringState &ss = strings[s];
    for (int i = 0; i < ss.n; ++i)
        if (ss.col[i] == col) return; // already held (repeat)
    if (ss.n >= kMaxHeld) return;
    // low byte = column (pitch); high bits identify the source (keyboard / touch)
    float pitch = (float)st.tuning.noteFor(s, col & 0xff, chromatic);
    ss.col[ss.n] = col; ss.pitch[ss.n] = pitch; ++ss.n;
    ss.pendingOff = false;
    // A string that is already sounding slides; a silent one attacks.
    if (slotVoice[0][s] >= 0) leadOp(OP_GLIDE, s, pitch);
    else leadOp(OP_ON, s, pitch, 1.0f);
}

void Instrument::noteDown(int s, int id, float pitch, float vel) {
    if (s < 0 || s >= kStrings) return;
    StringState &ss = strings[s];
    for (int i = 0; i < ss.n; ++i)
        if (ss.col[i] == id) return;
    if (ss.n >= kMaxHeld) return;
    pitch = clampf(pitch, 12.0f, 120.0f);
    ss.col[ss.n] = id; ss.pitch[ss.n] = pitch; ++ss.n;
    ss.pendingOff = false;
    if (slotVoice[0][s] >= 0) leadOp(OP_GLIDE, s, pitch);
    else leadOp(OP_ON, s, pitch, clampf(vel, 0.05f, 1.0f));
}

void Instrument::keyUp(int s, int col) {
    if (s < 0 || s >= kStrings) return;
    StringState &ss = strings[s];
    int idx = -1;
    for (int i = 0; i < ss.n; ++i)
        if (ss.col[i] == col) { idx = i; break; }
    if (idx < 0) return;
    bool wasTop = idx == ss.n - 1;
    for (int i = idx; i < ss.n - 1; ++i) { ss.col[i] = ss.col[i + 1]; ss.pitch[i] = ss.pitch[i + 1]; }
    --ss.n;
    if (ss.n == 0) {
        if (sustain) ss.pendingOff = true;
        else leadOp(OP_OFF, s, 0);
    } else if (wasTop) {
        leadOp(OP_GLIDE, s, ss.pitch[ss.n - 1]); // pull-off: slide back to the held key
    }
}

void Instrument::setSustain(bool on) {
    sustain = on;
    if (!on)
        for (int s = 0; s < kStrings; ++s)
            if (strings[s].pendingOff) { strings[s].pendingOff = false; leadOp(OP_OFF, s, 0); }
}

void Instrument::shiftOctave(int delta) {
    int o = st.tuning.octave + delta;
    if (o < -3 || o > 3) return;
    st.tuning.octave = o;
    for (int s = 0; s < kStrings; ++s) {
        StringState &ss = strings[s];
        for (int i = 0; i < ss.n; ++i) ss.pitch[i] += 12.0f * delta;
        if (slotVoice[0][s] >= 0) leadOp(OP_GLIDE, s, slotNote[0][s] + 12.0f * delta);
    }
}

void Instrument::panic() {
    crush.set(false, 0, 0); glitchOwner = -1;
    for (int s = 0; s < kStrings; ++s) { strings[s].n = 0; strings[s].pendingOff = false; }
    sustain = false;
    for (int p = 0; p < kParts; ++p) {
        bendTarget[p] = 0; parts[p].bend = 0;
        for (int s = 0; s < kSlots; ++s) slotVoice[p][s] = -1;
    }
    for (auto &v : voices)
        if (v.active()) v.kill(sr);
    backPlaying = false;
    beatPlaying = beatRec = false; drums.silence();
    if (beatSwitching) beatSwitchNow(); // a beat waiting for the bar line arrives now
    if (loopState == L_COUNTIN || loopState == L_REC || closing) loopClear();
    else if (loopState == L_PLAY || loopState == L_OVERDUB) {
        if (recLayer >= 0) recLayer = -1;
        loopState = L_STOPPED;
    }
}

void Instrument::applyXY(int part, int map, float x, float y, bool touching, bool hold) {
    Part &p = parts[part];
    if (map == XY_GLITCH) { // a master effect: it crushes everything you hear, not one part
        p.clearMod();
        if (touching || hold) { crush.set(true, x, y); glitchOwner = part; }
        else if (glitchOwner == part) { crush.set(false, x, y); glitchOwner = -1; }
        return;
    }
    if (glitchOwner == part) { crush.set(false, 0, 0); glitchOwner = -1; } // this pad moved to another mapping
    if (!touching && !hold) {
        p.clearMod();
        if (map == XY_BEND) bendTarget[part] = 0;
        return;
    }
    p.clearMod();
    float cx = (clampf(x, 0, 1) - 0.5f) * 2.0f;
    y = clampf(y, 0, 1);
    switch (map) {
    case XY_FILTER: p.modCut = cx * 3.0f; p.modRes = y * 0.6f; break;
    case XY_WOBBLE: p.modCut = cx * 3.0f; p.modVib = y * 70.0f; break;
    case XY_MORPH: p.modMix = cx; p.modDrive = y * 0.8f; break;
    case XY_SPACE: p.modDly = clampf(x, 0, 1) * 0.6f; p.modRev = y * 0.7f; break;
    case XY_BEND: bendTarget[part] = cx * st.bendRange; p.modCut = (y - 0.5f) * 5.0f; break;
    default: break;
    }
}

void Instrument::setLeadPatch(const Patch &p) {
    parts[0].patch = p;
    if (recLayer >= 0) {
        Layer &ly = layers[recLayer];
        if (ly.nPatches < kLayerPatches) {
            ly.patches[ly.nPatches] = p;
            loopRecord(OP_PATCH, ly.nPatches, 0, 0);
            ++ly.nPatches;
        }
    }
}

} // namespace td
