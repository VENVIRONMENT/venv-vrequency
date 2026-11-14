// venv-vrequency — playing the drum sounds: hits, and the drum bus that mixes them (audio thread).
#include "core/drums/drum_kit.h"
#include "core/dsp.h"
#include <cmath>

namespace td {

void DrumKit::trigger(int kit, int drum, float vel) {
    if (kit < 0 || kit >= kKits || drum < 0 || drum >= kDrums) return;
    // the same drum again (or a closed hat over an open one) fades the old hit out instead of piling up
    for (V &x : v)
        if (x.s && (x.drum == drum || (x.drum == D_OPENHAT && drum == D_HAT))) x.fading = true;
    V *slot = nullptr;
    for (V &x : v) if (!x.s) { slot = &x; break; }
    if (!slot) { // all busy: take the one furthest through its sound
        slot = &v[0];
        for (V &x : v) if (x.pos > slot->pos) slot = &x;
    }
    static const float pan[kDrums] = {0.0f, 0.0f, 0.25f, 0.25f, -0.15f, -0.3f, -0.35f, 0.35f};
    slot->s = &smp[kit][drum];
    slot->pos = 0; slot->drum = drum; slot->gain = clampf(vel, 0, 1.5f);
    slot->gl = std::sqrt(1.0f - pan[drum]); slot->gr = std::sqrt(1.0f + pan[drum]);
    slot->fade = 1; slot->fading = false;
    nActive = 0;
    for (V &x : v) if (x.s) ++nActive;
}

void DrumKit::silence() {
    for (V &x : v) if (x.s) x.fading = true;
}

void DrumKit::render(int kit, float *l, float *r, int n) {
    if (nActive == 0) return;
    float bl[kBlock], br[kBlock];
    if (n > kBlock) n = kBlock;
    for (int i = 0; i < n; ++i) bl[i] = br[i] = 0;
    nActive = 0;
    for (V &x : v) {
        if (!x.s) continue;
        const float *s = x.s->data();
        int size = (int)x.s->size();
        for (int i = 0; i < n && x.pos < size; ++i) {
            float y = s[x.pos++] * x.gain * x.fade;
            if (x.fading) x.fade *= fadeCoef;
            bl[i] += y * x.gl; br[i] += y * x.gr;
        }
        if (x.pos >= size || x.fade < 1e-3f) x.s = nullptr; else ++nActive;
    }
    if (kit == KIT_CRUNCHY) {
        for (int i = 0; i < n; ++i) { bl[i] = fastTanh(bl[i] * 1.6f) * (1.0f / 1.3f); br[i] = fastTanh(br[i] * 1.6f) * (1.0f / 1.3f); }
    } else if (kit == KIT_GLITCHY) {
        float inc = 22000.0f / sr;
        for (int i = 0; i < n; ++i) {
            holdPh += inc;
            if (holdPh >= 1.0f) { holdPh -= 1.0f; holdL = std::round(bl[i] * 64.0f) * (1.0f / 64.0f); holdR = std::round(br[i] * 64.0f) * (1.0f / 64.0f); }
            bl[i] = holdL; br[i] = holdR;
        }
        if (nActive == 0) holdL = holdR = 0;
    }
    for (int i = 0; i < n; ++i) { l[i] += bl[i]; r[i] += br[i]; }
}

} // namespace td
