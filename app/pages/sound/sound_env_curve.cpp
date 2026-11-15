// venv-vrequency — the amp-envelope curve of the Sound page's picture, run through the engine's own Env.
#include "app/pages/sound/sound_curves.h"
#include "core/dsp.h"
#include <cmath>

namespace td {

float EnvCurve::xOf(float t) const { return std::sqrt(clampf(t / total, 0.0f, 1.0f)); }

float EnvCurve::yOf(float x) const {
    float f = clampf(x, 0.0f, 1.0f) * N;
    int i = (int)f;
    if (i >= N) return y[N];
    return y[i] + (y[i + 1] - y[i]) * (f - i);
}

void envCurve(const float *p, EnvCurve &c) {
    const float atk = p[P_ATK], dec = p[P_DEC], sus = p[P_SUS], rel = p[P_REL];
    // The attack aims at 1.3 and ends on crossing 1.0, which takes exactly `atk` seconds with the
    // engine's rate; the decay and release rates reach 99.3 % of their way in `dec` / `rel` seconds.
    c.tA = atk; c.tD = dec; c.tH = 0.5f; c.tR = rel;
    c.total = c.tA + c.tD + c.tH + c.tR;
    const float sr = clampf(16.0f / atk, 1500.0f, 24000.0f);
    const float aC = Env::attackCoef(atk, sr), dC = Env::decayCoef(dec, sr), rC = Env::decayCoef(rel, sr);
    const float tOff = c.tA + c.tD + c.tH;
    Env e; e.gateOn();
    bool off = false;
    long n = 0;
    float v = 0;
    for (int i = 0; i <= EnvCurve::N; ++i) {
        float x = (float)i / EnvCurve::N, t = c.total * x * x;
        while ((float)n / sr < t) {
            if (!off && (float)n / sr >= tOff) { e.gateOff(); off = true; }
            v = e.tick(aC, dC, sus, rC);
            ++n;
        }
        c.y[i] = i == 0 ? 0.0f : clampf(v, 0.0f, 1.0f);
    }
}

} // namespace td
