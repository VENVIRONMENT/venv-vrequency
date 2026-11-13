#include "instrument.h"
#include <algorithm>
#include <cstring>

namespace td {

Instrument::Instrument(float sampleRate) : sr(sampleRate) {
    chorus.init(sr); delay.init(sr); reverb.init(sr); limiter.init(sr);
    for (int p = 0; p < kParts; ++p)
        for (int s = 0; s < kSlots; ++s) { slotVoice[p][s] = -1; slotNote[p][s] = 0; }
    for (int s = 0; s < kSlots; ++s) backDesired[s] = -1;
    for (auto &ly : layers) ly.ev.resize(kLayerEvents);
    for (auto &a : sendSm) a[0] = a[1] = a[2] = 0;
    std::memset(scope, 0, sizeof scope);
    recBuf.assign((1u << 18) * 2, 0.0f);
    volSm = st.volume;
    publish();
}

Instrument::~Instrument() {}

// ------------------------------------------------------------------ voices

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
    voices[v].start(part, slot, pitch, vel, panFor(part, slot), serial++);
    slotVoice[part][slot] = v;
    slotNote[part][slot] = pitch;
}

void Instrument::opGlide(int part, int slot, float pitch) {
    int v = slotVoice[part][slot];
    if (v < 0) { opOn(part, slot, pitch, 1.0f); return; }
    voices[v].glideTo(pitch, glideMs(part), sr);
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

// ------------------------------------------------------------- lead strings

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
    for (int s = 0; s < kStrings; ++s) { strings[s].n = 0; strings[s].pendingOff = false; }
    sustain = false;
    for (int p = 0; p < kParts; ++p) {
        bendTarget[p] = 0; parts[p].bend = 0;
        for (int s = 0; s < kSlots; ++s) slotVoice[p][s] = -1;
    }
    for (auto &v : voices)
        if (v.active()) v.kill(sr);
    backPlaying = false;
    if (loopState == L_COUNTIN || loopState == L_REC || closing) loopClear();
    else if (loopState == L_PLAY || loopState == L_OVERDUB) {
        if (recLayer >= 0) recLayer = -1;
        loopState = L_STOPPED;
    }
}

void Instrument::applyXY(int part, int map, float x, float y, bool touching, bool hold) {
    Part &p = parts[part];
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

void Instrument::setPatch(int part, const Patch &p) {
    PatchMsg m; m.part = part; m.patch = p; m.patch.clamp();
    patchq.push(m);
}

// ------------------------------------------------------------------ events

void Instrument::applySetting(int s, float v) {
    switch (s) {
    case S_ROOT: st.tuning.root = ((int)v % 12 + 12) % 12; break;
    case S_SCALE: st.tuning.scale = std::clamp((int)v, 0, SC_COUNT - 1); break;
    case S_OCTAVE: st.tuning.octave = std::clamp((int)v, -3, 3); break;
    case S_ROWINT: st.tuning.rowInterval = std::clamp((int)v, 1, 12); break;
    case S_BPM: st.bpm = clampf(v, 40, 240); break;
    case S_VOLUME: st.volume = clampf(v, 0, 1); break;
    case S_XYMAP: st.xyMap = std::clamp((int)v, 0, XY_COUNT - 1); applyXY(0, st.xyMap, xyX, xyY, false, false); break;
    case S_XYHOLD: st.xyHold = v > 0.5f; if (!st.xyHold && !xyTouch) applyXY(0, st.xyMap, xyX, xyY, false, false); break;
    case S_BACKMODE: st.backMode = std::clamp((int)v, 0, BM_COUNT - 1); break;
    case S_BACKSTYLE: st.backStyle = std::clamp((int)v, 0, BS_COUNT - 1); break;
    case S_CHORDBEATS: st.chordBeats = std::clamp((int)v, 1, 8); break;
    case S_ARPRATE: st.arpRate = std::clamp((int)v, 1, 4); break;
    case S_SEVENTH: st.seventh = v > 0.5f; break;
    case S_METRO: st.metro = v > 0.5f; break;
    case S_BENDRANGE: st.bendRange = clampf(v, 1, 12); break;
    case S_BENDTIME: st.bendTime = clampf(v, 10, 1000); break;
    case S_GLIDESCALE: st.glideScale = clampf(v, 0, 4); break;
    case S_BACKVOL: st.backVol = clampf(v, 0, 1); break;
    case S_LOOPVOL: st.loopVol = clampf(v, 0, 1); break;
    case S_LATENCY: st.latencyMs = clampf(v, 0, 500); break;
    default: break;
    }
}

void Instrument::handle(const Event &e) {
    switch (e.type) {
    case EV_KEY:
        if (e.c) keyDown(e.a, e.b, e.d != 0); else keyUp(e.a, e.b);
        break;
    case EV_CHORD: chordTap(e.a); break;
    case EV_SUSTAIN: setSustain(e.a != 0); break;
    case EV_BEND:
        bendTarget[0] = clampf(e.f, -1, 1) * st.bendRange;
        loopRecord(OP_BEND, 0, bendTarget[0], 0);
        break;
    case EV_XY: {
        xyX = e.f; xyY = e.g; xyTouch = e.a != 0;
        applyXY(0, st.xyMap, xyX, xyY, xyTouch, st.xyHold);
        int flags = st.xyMap | (xyTouch ? 0x80 : 0) | (st.xyHold ? 0x40 : 0);
        loopRecord(OP_XY, flags, xyX, xyY);
        break;
    }
    case EV_PANIC: panic(); break;
    case EV_SET: applySetting(e.a, e.f); break;
    case EV_PATCHPARAM:
        if (e.a >= 0 && e.a < P_COUNT) {
            const ParamInfo &m = paramInfo(e.a);
            float v = clampf(e.f, m.min, m.max);
            parts[0].patch.p[e.a] = v;
            loopRecord(OP_PARAM, e.a, v, 0);
        }
        break;
    case EV_LOOP:
        switch (e.a) {
        case LC_PEDAL: loopPedal(); break;
        case LC_UNDO: loopUndo(); break;
        case LC_CLEAR: loopClear(); break;
        case LC_PLAYSTOP: loopPlayStop(); break;
        }
        break;
    case EV_BACK:
        switch (e.a) {
        case BC_PLAYSTOP:
            backPlaying = !backPlaying;
            if (backPlaying) {
                if (loopState == L_EMPTY) { beat = 0; progOrigin = 0; }
                else progOrigin = std::floor(beat / st.chordBeats) * st.chordBeats;
                lastPulseStep = lastArpStep = -1;
            }
            break;
        case BC_CLEAR:
            progLen = 0; holdChord = -1; droneMask = 0; selSlot = -1; backPlaying = false;
            break;
        case BC_REMOVE_LAST:
            if (progLen > 0) --progLen;
            if (selSlot >= progLen) selSlot = -1;
            break;
        case BC_RESYNC_SOUND: parts[1].patch = parts[0].patch; break;
        case BC_SELECT_SLOT: selSlot = (e.b == selSlot || e.b < 0 || e.b >= progLen) ? -1 : e.b; break;
        }
        break;
    case EV_OCTAVE: shiftOctave(e.a); break;
    }
}

// ----------------------------------------------------------------- backing

void Instrument::chordTap(int degree) {
    degree = ((degree % 7) + 7) % 7;
    bool wasActive = backingActive();
    switch (st.backMode) {
    case BM_PROG:
        if (selSlot >= 0 && selSlot < progLen) { prog[selSlot] = degree; selSlot = -1; }
        else if (progLen < kProgMax) prog[progLen++] = degree;
        if (!backPlaying || !wasActive) {
            if (loopState == L_EMPTY && !wasActive) { beat = 0; progOrigin = 0; }
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

// ------------------------------------------------------------------ looper

void Instrument::trigClick(bool accent) {
    clickEnv = 1.0f; clickPh = 0;
    clickFreq = accent ? 2200.0f : 1500.0f;
}

void Instrument::loopRecord(uint8_t op, int slot, float a, float b) {
    if (recLayer < 0) return;
    Layer &ly = layers[recLayer];
    bool isOff = op == OP_OFF;
    if (ly.n >= kLayerEvents - 16 && !isOff) { loopFull = true; return; }
    if (ly.n >= kLayerEvents) return;
    // The player reacts to what they hear, which left the engine latencyMs ago:
    // place the event where it was meant to be.
    double pos = beat - loopStart - st.latencyMs * 0.001 * st.bpm / 60.0;
    if (loopState != L_REC && loopLen > 0) { pos = std::fmod(pos, loopLen); if (pos < 0) pos += loopLen; }
    else if (pos < 0) pos = 0;
    LoopEv &e = ly.ev[ly.n++];
    e.beat = pos; e.op = op; e.slot = (uint8_t)slot; e.a = a; e.b = b; e.seq = evSeq++;
}

void Instrument::loopBeginLayer() {
    recLayer = nLayers;
    Layer &ly = layers[recLayer];
    ly.n = 0; ly.cursor = 0;
    ly.patches[0] = parts[0].patch; ly.nPatches = 1;
    loopFull = false;
    // notes already sounding become part of the take
    for (int s = 0; s < kStrings; ++s)
        if (slotVoice[0][s] >= 0) loopRecord(OP_ON, s, slotNote[0][s], 1.0f);
}

void Instrument::resetLayerPart(int li) {
    int p = 2 + li;
    partOff(p, false);
    parts[p].patch = layers[li].patches[0];
    parts[p].clearMod();
    bendTarget[p] = 0;
    layers[li].cursor = 0;
}

void Instrument::loopCommit(bool atLoopEnd) {
    if (recLayer < 0) return;
    Layer &ly = layers[recLayer];
    double pos = 0;
    if (!atLoopEnd && loopLen > 0) { pos = std::fmod(beat - loopStart, loopLen); if (pos < 0) pos += loopLen; }
    // close any note still open in the recorded stream
    bool on[kStrings] = {false};
    std::sort(ly.ev.begin(), ly.ev.begin() + ly.n, [](const LoopEv &x, const LoopEv &y) { return x.seq < y.seq; });
    for (int i = 0; i < ly.n; ++i) {
        const LoopEv &e = ly.ev[i];
        if (e.slot < kStrings) {
            if (e.op == OP_ON || e.op == OP_GLIDE) on[e.slot] = true;
            else if (e.op == OP_OFF) on[e.slot] = false;
        }
    }
    for (int s = 0; s < kStrings; ++s)
        if (on[s] && ly.n < kLayerEvents) {
            LoopEv &e = ly.ev[ly.n++];
            e.beat = atLoopEnd ? loopLen - 1e-6 : pos;
            e.op = OP_OFF; e.slot = (uint8_t)s; e.a = e.b = 0; e.seq = evSeq++;
        }
    std::sort(ly.ev.begin(), ly.ev.begin() + ly.n, [](const LoopEv &x, const LoopEv &y) {
        return x.beat < y.beat || (x.beat == y.beat && x.seq < y.seq);
    });
    int li = recLayer;
    recLayer = -1;
    nLayers = li + 1;
    resetLayerPart(li);
    parts[2 + li].gain = st.loopVol;
    int c = 0;
    while (c < ly.n && ly.ev[c].beat < pos) ++c;
    ly.cursor = c;
}

void Instrument::loopPedal() {
    switch (loopState) {
    case L_EMPTY:
        if (backingActive()) recStartBeat = std::ceil((beat + 2.0) / 4.0) * 4.0;
        else { beat = 0; progOrigin = 0; recStartBeat = 4; lastPulseStep = lastArpStep = -1; }
        loopState = L_COUNTIN;
        break;
    case L_COUNTIN:
        loopState = L_EMPTY;
        break;
    case L_REC: {
        double t = beat - loopStart;
        double bars = std::floor(t / 4.0 + 0.5);
        if (bars < 1) bars = 1;
        loopLen = std::min(bars * 4.0, kMaxLoopBeats);
        loopState = L_PLAY;
        if (t >= loopLen) { // pressed just after the bar line: fold the overshoot back
            Layer &ly = layers[recLayer];
            for (int i = 0; i < ly.n; ++i)
                if (ly.ev[i].beat >= loopLen) ly.ev[i].beat -= loopLen;
            loopStart += loopLen;
            loopCommit(false);
        } else {
            closing = true; // keep recording until the bar completes
        }
        break;
    }
    case L_PLAY:
        if (closing) break;
        if (nLayers >= kLayers) { loopFull = true; break; }
        loopBeginLayer();
        loopState = L_OVERDUB;
        break;
    case L_OVERDUB:
        loopCommit(false);
        loopState = L_PLAY;
        break;
    case L_STOPPED:
        loopPlayStop();
        break;
    }
}

void Instrument::loopUndo() {
    if (loopState == L_OVERDUB) { recLayer = -1; loopState = L_PLAY; loopFull = false; return; }
    if (loopState == L_COUNTIN || loopState == L_REC || closing) { loopClear(); return; }
    if (nLayers > 1) {
        --nLayers;
        partOff(2 + nLayers, false);
        loopFull = false;
    } else if (nLayers == 1) {
        loopClear();
    }
}

void Instrument::loopClear() {
    for (int i = 0; i < kLayers; ++i) partOff(2 + i, false);
    nLayers = 0; recLayer = -1; closing = false; loopFull = false;
    loopLen = 0; loopState = L_EMPTY;
}

void Instrument::loopPlayStop() {
    if (loopState == L_PLAY || loopState == L_OVERDUB) {
        if (closing) { loopCommit(true); closing = false; }
        else if (recLayer >= 0) loopCommit(false);
        for (int i = 0; i < kLayers; ++i) partOff(2 + i, false);
        loopState = L_STOPPED;
    } else if (loopState == L_STOPPED && nLayers > 0) {
        loopStart = backingActive() ? std::ceil(beat / 4.0) * 4.0 : beat;
        for (int i = 0; i < nLayers; ++i) resetLayerPart(i);
        loopState = L_PLAY;
    }
}

void Instrument::applyOp(int part, Layer &ly, const LoopEv &e) {
    switch (e.op) {
    case OP_ON: opOn(part, e.slot, e.a, e.b); break;
    case OP_GLIDE: opGlide(part, e.slot, e.a); break;
    case OP_OFF: opOff(part, e.slot); break;
    case OP_BEND: bendTarget[part] = e.a; break;
    case OP_XY: applyXY(part, e.slot & 0x0f, e.a, e.b, (e.slot & 0x80) != 0, (e.slot & 0x40) != 0); break;
    case OP_PATCH: if (e.slot < ly.nPatches) parts[part].patch = ly.patches[e.slot]; break;
    case OP_PARAM: if (e.slot < P_COUNT) parts[part].patch.p[e.slot] = e.a; break;
    }
}

void Instrument::playLayers(double p0, double p1) {
    for (int li = 0; li < nLayers; ++li) {
        Layer &ly = layers[li];
        parts[2 + li].gain = st.loopVol;
        while (ly.cursor < ly.n && ly.ev[ly.cursor].beat < p1) {
            if (ly.ev[ly.cursor].beat >= p0 - 1e-9) applyOp(2 + li, ly, ly.ev[ly.cursor]);
            ++ly.cursor;
        }
    }
}

void Instrument::tickLoop(double b0, double b1) {
    // beat clicks (count-in always, metronome when enabled)
    double m = std::ceil(b0);
    bool onBeat = m < b1;
    if (onBeat && (loopState == L_COUNTIN || st.metro)) trigClick(((long)m) % 4 == 0);

    switch (loopState) {
    case L_COUNTIN:
        countBeat = (int)std::ceil(recStartBeat - b1);
        if (b1 >= recStartBeat) {
            loopState = L_REC;
            loopStart = recStartBeat;
            loopBeginLayer();
        }
        break;
    case L_REC:
        if (b1 - loopStart >= kMaxLoopBeats) loopPedal();
        break;
    case L_PLAY:
    case L_OVERDUB: {
        double rel0 = b0 - loopStart;
        if (rel0 < 0) { // waiting for a bar line after restart
            if (b1 > loopStart) playLayers(0, b1 - loopStart);
            break;
        }
        double p0 = std::fmod(rel0, loopLen);
        double p1 = p0 + (b1 - b0);
        if (p1 >= loopLen) {
            playLayers(p0, loopLen + 1.0);
            if (closing) { loopCommit(true); closing = false; }
            for (int i = 0; i < nLayers; ++i) resetLayerPart(i);
            loopStart += loopLen;
            playLayers(0, p1 - loopLen);
        } else {
            playLayers(p0, p1);
        }
        break;
    }
    default: break;
    }
}

// ------------------------------------------------------------------- audio

void Instrument::block(float *l, float *r, int n) {
    PatchMsg pm;
    while (patchq.pop(pm)) {
        if (pm.part == 0) setLeadPatch(pm.patch);
        else if (pm.part > 0 && pm.part < kParts) parts[pm.part].patch = pm.patch;
    }
    Event e;
    while (evq.pop(e)) handle(e);

    double b0 = beat, b1 = beat + (double)st.bpm / 60.0 / sr * n;
    tickLoop(b0, b1);
    tickBacking(b0, b1);
    beat = b1;

    parts[0].gain = 1.0f;
    parts[1].gain = st.backVol;
    float bc = 1.0f - std::exp(-(float)n / (st.bendTime * 0.001f * sr * 0.33f));
    for (int p = 0; p < kParts; ++p) {
        parts[p].bend += bc * (bendTarget[p] - parts[p].bend);
        const float *P = parts[p].patch.p;
        float tc = P[P_CHORUS], tdl = clampf(P[P_DLYMIX] + parts[p].modDly, 0, 1), trv = clampf(P[P_REVMIX] + parts[p].modRev, 0, 1);
        sendSm[p][0] += 0.2f * (tc - sendSm[p][0]);
        sendSm[p][1] += 0.2f * (tdl - sendSm[p][1]);
        sendSm[p][2] += 0.2f * (trv - sendSm[p][2]);
    }

    float dryL[kBlock], dryR[kBlock], sC[kBlock], sD[kBlock], sR[kBlock], tmp[kBlock];
    for (int i = 0; i < n; ++i) dryL[i] = dryR[i] = sC[i] = sD[i] = sR[i] = 0;

    int nActive = 0;
    for (int vi = 0; vi < kVoices; ++vi) {
        Voice &v = voices[vi];
        if (!v.active()) continue;
        ++nActive;
        int p = v.part;
        v.render(parts[p], sr, tmp, n);
        float gl = v.panL, gr = v.panR;
        float c = sendSm[p][0], d = sendSm[p][1], rv = sendSm[p][2];
        for (int i = 0; i < n; ++i) {
            float x = tmp[i];
            dryL[i] += x * gl; dryR[i] += x * gr;
            sC[i] += x * c; sD[i] += x * d; sR[i] += x * rv;
        }
    }

    // Nothing sounding and every effect tail has died away: skip the effects
    // entirely (saves battery while the instrument sits idle).
    bool recording = recOn.load(std::memory_order_relaxed);
    if (nActive == 0 && clickEnv <= 1e-4f && silentBlocks > 600 && !recording) {
        for (int i = 0; i < n; ++i) l[i] = r[i] = 0;
        uint32_t sw0 = scopeW.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) scope[(sw0 + i) & (kScopeSize - 1)] = 0;
        scopeW.store(sw0 + n, std::memory_order_release);
        peakHold *= 0.98f;
        frames += n;
        return;
    }

    const float *LP = parts[0].patch.p;
    float wetL[kBlock], wetR[kBlock];
    for (int i = 0; i < n; ++i) wetL[i] = wetR[i] = 0;
    chorus.process(sC, wetL, wetR, n);
    for (int i = 0; i < n; ++i) { wetL[i] *= 0.7f; wetR[i] *= 0.7f; }
    float dsec = delayBeats((int)LP[P_DLYDIV]) * 60.0f / st.bpm;
    delay.process(sD, wetL, wetR, n, dsec, LP[P_DLYFB]);
    reverb.process(sR, wetL, wetR, n, LP[P_REVSIZE]);

    float clickDec = std::exp(-1.0f / (0.012f * sr));
    float pk = 0;
    for (int i = 0; i < n; ++i) {
        volSm += 0.002f * (st.volume - volSm);
        float ol = (dryL[i] + wetL[i]) * volSm, orr = (dryR[i] + wetR[i]) * volSm;
        if (clickEnv > 1e-4f) {
            clickPh += clickFreq / sr; if (clickPh >= 1.0f) clickPh -= 1.0f;
            float c = fastSin(clickPh) * clickEnv * 0.22f;
            clickEnv *= clickDec;
            ol += c; orr += c;
        }
        l[i] = ol; r[i] = orr;
    }
    limiter.process(l, r, n);

    uint32_t sw = scopeW.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i) {
        float mono = 0.5f * (l[i] + r[i]);
        scope[(sw + i) & (kScopeSize - 1)] = mono;
        float a = std::fabs(l[i]) > std::fabs(r[i]) ? std::fabs(l[i]) : std::fabs(r[i]);
        if (a > pk) pk = a;
    }
    scopeW.store(sw + n, std::memory_order_release);
    if (pk > peakHold) peakHold = pk; else peakHold *= 0.98f;
    if (nActive == 0 && pk < 1e-6f) ++silentBlocks; else silentBlocks = 0;

    if (recording) {
        uint32_t w = recW.load(std::memory_order_relaxed), rd = recR.load(std::memory_order_acquire);
        const uint32_t cap = (uint32_t)(recBuf.size() / 2);
        for (int i = 0; i < n; ++i) {
            if (w - rd >= cap) { recDrops.fetch_add(1); break; }
            recBuf[(w & (cap - 1)) * 2] = l[i];
            recBuf[(w & (cap - 1)) * 2 + 1] = r[i];
            ++w;
        }
        recW.store(w, std::memory_order_release);
    }
    frames += n;
}

void Instrument::process(float *outL, float *outR, int nframes) {
    std::lock_guard<std::mutex> lk(mx);
    int done = 0;
    while (done < nframes) {
        int n = std::min(kBlock, nframes - done);
        block(outL + done, outR + done, n);
        done += n;
    }
    publish();
}

int Instrument::readRecorded(float *dst, int maxFrames) {
    uint32_t rd = recR.load(std::memory_order_relaxed), w = recW.load(std::memory_order_acquire);
    const uint32_t cap = (uint32_t)(recBuf.size() / 2);
    int n = 0;
    while (rd != w && n < maxFrames) {
        dst[n * 2] = recBuf[(rd & (cap - 1)) * 2];
        dst[n * 2 + 1] = recBuf[(rd & (cap - 1)) * 2 + 1];
        ++rd; ++n;
    }
    recR.store(rd, std::memory_order_release);
    return n;
}

void Instrument::readScope(float *dst, int n) const {
    if (n > kScopeSize) n = kScopeSize;
    uint32_t w = scopeW.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i) dst[i] = scope[(w - n + i) & (kScopeSize - 1)];
}

void Instrument::publish() {
    uiSeq.fetch_add(1, std::memory_order_acq_rel); // odd: write in progress
    UiState &u = ui;
    int nv = 0;
    for (auto &v : voices) if (v.active()) ++nv;
    for (int s = 0; s < kStrings; ++s) {
        int v = slotVoice[0][s];
        u.str[s].gate = v >= 0;
        u.str[s].note = v >= 0 ? voices[v].note : 0;
        u.str[s].target = v >= 0 ? voices[v].target : 0;
        u.str[s].held = strings[s].n;
        u.str[s].topCol = strings[s].n > 0 ? (strings[s].col[strings[s].n - 1] & 0xff) : -1;
    }
    u.set = st;
    u.beat = beat;
    u.backPlaying = backPlaying;
    for (int i = 0; i < kProgMax; ++i) u.prog[i] = prog[i];
    u.progLen = progLen; u.progPos = backPlaying ? progPos : -1; u.selSlot = selSlot;
    u.holdChord = holdChord; u.droneMask = droneMask; u.curChord = backPlaying ? curChord : -1;
    std::memcpy(u.backName, parts[1].patch.name, sizeof u.backName);
    std::memcpy(u.leadName, parts[0].patch.name, sizeof u.leadName);
    u.loopState = loopState;
    u.loopBars = (int)(loopLen / 4.0 + 0.5);
    if (loopState == L_REC) { u.loopPos = (float)((beat - loopStart) / 4.0); u.loopBars = 0; }
    else if (loopLen > 0 && (loopState == L_PLAY || loopState == L_OVERDUB)) {
        double p = std::fmod(beat - loopStart, loopLen); if (p < 0) p = 0;
        u.loopPos = (float)(p / loopLen);
    } else u.loopPos = 0;
    u.layers = nLayers; u.countBeat = countBeat;
    u.loopFull = loopFull; u.loopClosing = closing;
    u.peak = peakHold; u.limiter = limiter.reduction; u.voices = nv;
    u.sustain = sustain; u.bend = parts[0].bend;
    u.xyX = xyX; u.xyY = xyY; u.xyTouch = xyTouch;
    u.frames = frames;
    u.droppedEvents = dropped.load(std::memory_order_relaxed);
    uiSeq.fetch_add(1, std::memory_order_acq_rel);
}

UiState Instrument::uiState() const {
    UiState out;
    for (int tries = 0; tries < 1000; ++tries) {
        uint32_t s1 = uiSeq.load(std::memory_order_acquire);
        if (s1 & 1) continue;
        out = ui;
        if (uiSeq.load(std::memory_order_acquire) == s1) break;
    }
    return out;
}

// ------------------------------------------------------------ bulk state

void Instrument::snapshot(SessionData &out) {
    std::lock_guard<std::mutex> lk(mx);
    out.settings = st;
    out.lead = parts[0].patch;
    out.backing = parts[1].patch;
    for (int i = 0; i < kProgMax; ++i) out.prog[i] = prog[i];
    out.progLen = progLen; out.holdChord = holdChord; out.droneMask = droneMask;
    out.loopBeats = nLayers > 0 ? loopLen : 0;
    out.layers.clear();
    for (int i = 0; i < nLayers; ++i) {
        LayerData ld;
        ld.ev.assign(layers[i].ev.begin(), layers[i].ev.begin() + layers[i].n);
        ld.patches.assign(layers[i].patches, layers[i].patches + layers[i].nPatches);
        out.layers.push_back(std::move(ld));
    }
}

void Instrument::restore(const SessionData &in) {
    std::lock_guard<std::mutex> lk(mx);
    panic();
    loopClear();
    st = in.settings;
    st.tuning.root = ((st.tuning.root % 12) + 12) % 12;
    st.tuning.scale = std::clamp(st.tuning.scale, 0, SC_COUNT - 1);
    parts[0].patch = in.lead; parts[0].patch.clamp();
    parts[1].patch = in.backing; parts[1].patch.clamp();
    progLen = std::clamp(in.progLen, 0, kProgMax);
    for (int i = 0; i < kProgMax; ++i) prog[i] = ((in.prog[i] % 7) + 7) % 7;
    holdChord = in.holdChord; droneMask = in.droneMask & 0x7f; selSlot = -1;
    backPlaying = false;
    nLayers = 0;
    if (in.loopBeats >= 4 && !in.layers.empty()) {
        loopLen = std::min(in.loopBeats, kMaxLoopBeats);
        for (size_t i = 0; i < in.layers.size() && i < (size_t)kLayers; ++i) {
            Layer &ly = layers[i];
            const LayerData &ld = in.layers[i];
            ly.n = (int)std::min(ld.ev.size(), (size_t)kLayerEvents);
            std::copy(ld.ev.begin(), ld.ev.begin() + ly.n, ly.ev.begin());
            ly.nPatches = (int)std::min(ld.patches.size(), (size_t)kLayerPatches);
            if (ly.nPatches == 0) { ly.patches[0] = in.lead; ly.nPatches = 1; }
            else for (int k = 0; k < ly.nPatches; ++k) { ly.patches[k] = ld.patches[k]; ly.patches[k].clamp(); }
            ly.cursor = 0;
            ++nLayers;
        }
        loopState = L_STOPPED;
    }
    publish();
}

} // namespace td
