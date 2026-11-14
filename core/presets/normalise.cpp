// venv-vrequency — loudness normalisation: every sound is measured and set to the same level.
#include "core/presets/presets.h"
#include "core/dsp.h"
#include "core/synth/params.h"
#include "core/synth/part.h"
#include "core/synth/voice.h"
#include <cmath>

namespace td {

float normalisePatch(Patch &p, float sr) {
    Part pt;
    pt.patch = p;
    pt.patch.p[P_LEVEL] = 1.0f;
    // measure the sound at its loudest: don't wait for multi-second attacks
    if (pt.patch.p[P_ATK] > 0.03f) pt.patch.p[P_ATK] = 0.03f;
    if (pt.patch.p[P_FATK] > 0.25f) pt.patch.p[P_FATK] = 0.25f;
    pt.patch.p[P_VIBDELAY] = 0;
    Voice v[2];
    v[0].start(0, 0, 57, 1.0f, 0, 1);
    v[1].start(0, 1, 69, 1.0f, 0, 2);
    const int total = (int)(sr * 1.5f);
    const int win = (int)(sr * 0.05f);
    float tmp[kBlock], mix[kBlock];
    double acc = 0; int accN = 0;
    float maxRms = 0, peak = 0;
    for (int done = 0; done < total; done += kBlock) {
        for (int i = 0; i < kBlock; ++i) mix[i] = 0;
        for (auto &vv : v) {
            if (!vv.active()) continue;
            vv.render(pt, sr, tmp, kBlock);
            for (int i = 0; i < kBlock; ++i) mix[i] += tmp[i];
        }
        for (int i = 0; i < kBlock; ++i) {
            float a = std::fabs(mix[i]);
            if (a > peak) peak = a;
            acc += (double)mix[i] * mix[i];
            if (++accN >= win) {
                float r = (float)std::sqrt(acc / accN);
                if (r > maxRms) maxRms = r;
                acc = 0; accN = 0;
            }
        }
    }
    const float targetRms = 0.11f, peakCap = 0.42f;
    float g = maxRms > 1e-6f ? targetRms / maxRms : 1.0f;
    if (peak * g > peakCap) g = peakCap / peak;
    const ParamInfo &m = paramInfo(P_LEVEL);
    p.p[P_LEVEL] = clampf(g, m.min, m.max);
    return peak * p.p[P_LEVEL];
}

} // namespace td
