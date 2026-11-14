// venv-vrequency — making clips from what is live, comparing them, and naming their sounds.
#include "core/track/clip.h"
#include <algorithm>
#include <cstdio>

namespace td {

static void setName(Clip &c, const char *s) { std::snprintf(c.name, sizeof c.name, "%s", s); }

bool beatClip(const SessionData &s, Clip &out) {
    bool any = false;
    for (int d = 0; d < kDrums; ++d) any = any || s.beat[d] != 0;
    if (!any) return false;
    out = Clip();
    out.kind = CK_BEAT; out.bars = 1;
    for (int d = 0; d < kDrums; ++d) out.beat[d] = s.beat[d];
    out.kit = std::clamp(s.settings.kit, 0, kKits - 1);
    setName(out, "BEAT");
    return true;
}

bool chordsClip(const SessionData &s, Clip &out) {
    if (s.progLen <= 0 && s.holdChord < 0 && s.droneMask == 0) return false;
    out = Clip();
    out.kind = CK_CHORDS;
    out.progLen = std::clamp(s.progLen, 0, kProgMax);
    for (int i = 0; i < kProgMax; ++i) out.prog[i] = s.prog[i];
    out.holdChord = s.holdChord; out.droneMask = s.droneMask;
    out.backStyle = s.settings.backStyle; out.chordBeats = std::max(1, s.settings.chordBeats);
    out.arpRate = s.settings.arpRate; out.seventh = s.settings.seventh;
    out.patch = s.backing;
    // a progression lasts its chords, rounded up to whole bars; a held chord or drone is one bar
    int beats = (out.progLen > 0 && out.holdChord < 0) ? out.progLen * out.chordBeats : 4;
    out.bars = std::clamp((beats + 3) / 4, 1, kTrackMaxBars);
    setName(out, "CHORDS");
    return true;
}

bool loopClip(const SessionData &s, Clip &out) {
    if (s.loopBeats < 4 || s.layers.empty()) return false;
    out = Clip();
    out.kind = CK_LOOP;
    out.bars = std::clamp((int)(s.loopBeats / 4.0 + 0.5), 1, kTrackMaxBars);
    out.layers = s.layers;
    if ((int)out.layers.size() > kLayers) out.layers.resize(kLayers);
    for (auto &ld : out.layers) {
        if (ld.patches.empty()) ld.patches.push_back(s.lead);
        std::stable_sort(ld.ev.begin(), ld.ev.end(), [](const LoopEv &a, const LoopEv &b) { return a.beat < b.beat; });
    }
    setName(out, "LOOP");
    return true;
}

static bool samePatch(const Patch &a, const Patch &b) {
    for (int i = 0; i < P_COUNT; ++i) if (a.p[i] != b.p[i]) return false;
    return true;
}

bool sameClip(const Clip &a, const Clip &b) {
    if (a.kind != b.kind || a.bars != b.bars) return false;
    switch (a.kind) {
    case CK_BEAT:
        if (a.kit != b.kit) return false;
        for (int d = 0; d < kDrums; ++d) if (a.beat[d] != b.beat[d]) return false;
        return true;
    case CK_CHORDS:
        if (a.progLen != b.progLen || a.holdChord != b.holdChord || a.droneMask != b.droneMask) return false;
        if (a.backStyle != b.backStyle || a.chordBeats != b.chordBeats || a.arpRate != b.arpRate || a.seventh != b.seventh) return false;
        for (int i = 0; i < a.progLen; ++i) if (a.prog[i] != b.prog[i]) return false;
        return samePatch(a.patch, b.patch);
    default:
        if (a.layers.size() != b.layers.size()) return false;
        for (size_t i = 0; i < a.layers.size(); ++i) {
            const LayerData &x = a.layers[i], &y = b.layers[i];
            if (x.ev.size() != y.ev.size() || x.patches.size() != y.patches.size()) return false;
            for (size_t k = 0; k < x.ev.size(); ++k)
                if (x.ev[k].beat != y.ev[k].beat || x.ev[k].op != y.ev[k].op || x.ev[k].slot != y.ev[k].slot || x.ev[k].a != y.ev[k].a) return false;
            for (size_t k = 0; k < x.patches.size(); ++k) if (!samePatch(x.patches[k], y.patches[k])) return false;
        }
        return true;
    }
}

bool samePick(const SoundPick &a, const SoundPick &b) {
    if (a.on != b.on || a.kit != b.kit || a.octave != b.octave || a.own != b.own) return false;
    return !a.on || samePatch(a.patch, b.patch);
}

const char *recordedSoundName(const Clip &c) {
    if (c.kind == CK_BEAT) return kitName(c.kit);
    if (c.kind == CK_CHORDS) return c.patch.name;
    return (!c.layers.empty() && !c.layers[0].patches.empty()) ? c.layers[0].patches[0].name : "";
}

const char *soundName(const Clip &c, const SoundPick &pick) {
    if (c.kind == CK_BEAT) return pick.kit >= 0 ? kitName(std::clamp(pick.kit, 0, kKits - 1)) : kitName(c.kit);
    return pick.on ? pick.patch.name : recordedSoundName(c);
}

} // namespace td
