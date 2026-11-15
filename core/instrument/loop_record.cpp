// venv-vrequency — the looper: writing what is played into a layer, and closing the layer.
#include "core/instrument/instrument.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace td {

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
    for (int p = 0; p < kPulsePads; ++p) // and so do the pads whose keys are down
        if (pulses.held[p]) loopRecord(OP_PULSE, p, 1.0f, 1.0f);
}

void Instrument::loopCommit(bool atLoopEnd) {
    if (recLayer < 0) return;
    Layer &ly = layers[recLayer];
    double pos = 0;
    if (!atLoopEnd && loopLen > 0) { pos = std::fmod(beat - loopStart, loopLen); if (pos < 0) pos += loopLen; }
    // close any note still open in the recorded stream
    bool on[kStrings] = {false}, pad[kPulsePads] = {false};
    std::sort(ly.ev.begin(), ly.ev.begin() + ly.n, [](const LoopEv &x, const LoopEv &y) { return x.seq < y.seq; });
    for (int i = 0; i < ly.n; ++i) {
        const LoopEv &e = ly.ev[i];
        if (e.op == OP_PULSE && e.slot < kPulsePads && e.b > 0.5f) { pad[e.slot] = e.b < 1.5f; continue; }
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
    for (int p = 0; p < kPulsePads; ++p)
        if (pad[p] && ly.n < kLayerEvents) {
            LoopEv &e = ly.ev[ly.n++];
            e.beat = atLoopEnd ? loopLen - 1e-6 : pos;
            e.op = OP_PULSE; e.slot = (uint8_t)p; e.a = 1; e.b = 2; e.seq = evSeq++;
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

} // namespace td
