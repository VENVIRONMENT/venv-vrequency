// venv-vrequency — what the audio thread publishes: the state for the interface, the scope and the recording tap.
#include "core/instrument/instrument.h"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace td {

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
        int v = st.poly ? -1 : slotVoice[0][s]; // poly: slots are per key, not per row
        const StringState &ss = strings[s];
        const float top = ss.n > 0 ? ss.pitch[ss.n - 1] : 0;
        u.str[s].gate = st.poly ? ss.n > 0 : v >= 0;
        u.str[s].note = v >= 0 ? voices[v].note : top;
        u.str[s].target = v >= 0 ? voices[v].target : top;
        u.str[s].held = strings[s].n;
        u.str[s].topCol = strings[s].n > 0 ? (strings[s].col[strings[s].n - 1] & 0xff) : -1;
    }
    u.set = st;
    u.beat = beat;
    u.padsPlaying = padsPlaying;
    u.holdChord = holdChord; u.droneMask = droneMask; u.curChord = padsPlaying ? curChord : -1;
    std::memcpy(u.padsName, parts[1].patch.name, sizeof u.padsName);
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
    for (int p = 0; p < kPulsePads; ++p) {
        u.pulse[p] = pulses.pad[p]; u.pulseLatched[p] = pulses.latched[p]; u.pulseHits[p] = pulses.hits[p];
    }
    u.pulsing = pulses.active();
    u.peak = peakHold; u.limiter = limiter.reduction; u.voices = nv;
    u.sustain = sustain; u.bend = parts[0].bend;
    u.xyX = xyX; u.xyY = xyY; u.xyTouch = xyTouch;
    u.glitch = crush.on(); u.glitchX = crush.coarse; u.glitchY = crush.grit;
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

} // namespace td
