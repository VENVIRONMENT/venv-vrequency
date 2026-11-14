// venv-vrequency — bulk state: copying a session out of the instrument and putting one back.
#include "core/instrument/instrument.h"
#include "core/drums/drums.h"
#include "core/instrument/session.h"
#include "core/synth/scale.h"
#include <algorithm>
#include <mutex>

namespace td {

void Instrument::snapshot(SessionData &out) {
    std::lock_guard<std::mutex> lk(mx);
    out.settings = st;
    out.lead = parts[0].patch;
    out.backing = parts[1].patch;
    for (int i = 0; i < kProgMax; ++i) out.prog[i] = prog[i];
    out.progLen = progLen; out.holdChord = holdChord; out.droneMask = droneMask;
    if (liveSaved) { // a track is playing on the backing: report the live backing, not the clip
        out.settings.backStyle = live.backStyle; out.settings.chordBeats = live.chordBeats;
        out.settings.arpRate = live.arpRate; out.settings.seventh = live.seventh;
        out.backing = live.patch;
        for (int i = 0; i < kProgMax; ++i) out.prog[i] = live.prog[i];
        out.progLen = live.progLen; out.holdChord = live.holdChord; out.droneMask = live.droneMask;
    }
    out.loopBeats = nLayers > 0 ? loopLen : 0;
    for (int d = 0; d < kDrums; ++d) out.beat[d] = beatPat[d];
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
    st.kit = std::clamp(st.kit, 0, kKits - 1);
    for (int d = 0; d < kDrums; ++d) beatPat[d] = in.beat[d];
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
