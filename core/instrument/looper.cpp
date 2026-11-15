// venv-vrequency — the looper: the pedal and its commands, and playing the layers back.
#include "core/instrument/instrument.h"
#include "core/synth/params.h"
#include <algorithm>
#include <cmath>

namespace td {

void Instrument::trigClick(bool accent) {
    clickEnv = 1.0f; clickPh = 0;
    clickFreq = accent ? 2200.0f : 1500.0f;
}

void Instrument::quietPart(int p) {
    if (glitchOwner == p) { crush.set(false, 0, 0); glitchOwner = -1; }
    partOff(p, false);
    parts[p].clearMod();
    bendTarget[p] = 0;
}

void Instrument::resetLayerPart(int li) {
    int p = 2 + li;
    quietPart(p);
    parts[p].patch = layers[li].patches[0];
    layers[li].cursor = 0;
}

void Instrument::loopPedal() {
    switch (loopState) {
    case L_EMPTY:
        if (padsActive() || beatPlaying) recStartBeat = std::ceil((beat + 2.0) / 4.0) * 4.0;
        else { beat = 0; recStartBeat = 4; }
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
        loopStart = (padsActive() || beatPlaying) ? std::ceil(beat / 4.0) * 4.0 : beat;
        for (int i = 0; i < nLayers; ++i) resetLayerPart(i);
        loopState = L_PLAY;
    }
}

void Instrument::applyOp(int part, const LoopEv &e, const Patch *patches, size_t nPatches) {
    switch (e.op) {
    case OP_ON: opOn(part, e.slot, e.a, e.b); break;
    case OP_GLIDE: opGlide(part, e.slot, e.a); break;
    case OP_OFF: opOff(part, e.slot); break;
    case OP_BEND: bendTarget[part] = e.a; break;
    case OP_XY: applyXY(part, e.slot & 0x0f, e.a, e.b, (e.slot & 0x80) != 0, (e.slot & 0x40) != 0); break;
    case OP_PATCH: if (e.slot < nPatches) parts[part].patch = patches[e.slot]; break;
    case OP_PARAM: if (e.slot < P_COUNT) parts[part].patch.p[e.slot] = e.a; break;
    }
}

void Instrument::playLayers(double p0, double p1) {
    for (int li = 0; li < nLayers; ++li) {
        Layer &ly = layers[li];
        parts[2 + li].gain = st.loopVol;
        while (ly.cursor < ly.n && ly.ev[ly.cursor].beat < p1) {
            if (ly.ev[ly.cursor].beat >= p0 - 1e-9) applyOp(2 + li, ly.ev[ly.cursor], ly.patches, (size_t)ly.nPatches);
            ++ly.cursor;
        }
    }
}

void Instrument::tickLoop(double b0, double b1) {
    // beat clicks (count-in always, metronome when enabled)
    double m = std::ceil(b0);
    bool onBeat = m < b1;
    if (onBeat && (loopState == L_COUNTIN || st.metro || (beatPlaying && beatRec))) trigClick(((long)m) % 4 == 0);

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

} // namespace td
