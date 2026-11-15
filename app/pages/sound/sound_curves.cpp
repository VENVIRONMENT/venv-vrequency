// venv-vrequency — the oscillator and filter curves of the Sound page's pictures (see sound_curves.h).
#include "app/pages/sound/sound_curves.h"
#include "core/dsp.h"
#include <cmath>

namespace td {

void oscCurve(const float *p, OscCurve &c) {
    const int w1 = (int)p[P_WAVE1], w2 = (int)p[P_WAVE2];
    const float pw = p[P_PW], mix2 = p[P_MIX2], mix1 = 1.0f - mix2, fm = p[P_FM], sub = p[P_SUB];
    const float r = std::exp2((p[P_SEMI2] + p[P_DETUNE] * 0.01f) * (1.0f / 12.0f)); // B's cycles per A cycle
    const float dt = 1.5f / OscCurve::N;                                            // a polyBLEP edge about one point wide
    c.bOn = mix2 > 0.001f;
    c.bSemis = p[P_SEMI2] + p[P_DETUNE] * 0.01f;
    float peak = 0.05f;
    for (int i = 0; i <= OscCurve::N; ++i) {
        float ph = i == OscCurve::N ? 1.0f - 1e-4f : (float)i / OscCurve::N;
        float ph2 = ph * r; ph2 -= std::floor(ph2);                 // B starts in step with A
        float pm = ph;
        if (fm > 0.001f) { pm = ph + fm * 0.25f * fastSin(ph2); pm -= std::floor(pm); }
        float s = oscSample(w1, pm, dt, pw) * mix1;
        if (sub > 0.001f) s += fastSin(ph * 0.5f) * sub;            // the sub runs at half A's rate
        c.a[i] = s;
        c.b[i] = c.bOn ? oscSample(w2, ph2, dt * r, pw) * mix2 : 0.0f;
        peak = std::fmax(peak, std::fmax(std::fabs(c.a[i]), std::fabs(c.b[i])));
    }
    c.scale = peak;   // the picture is fitted to the taller of the two; A and B keep their size against each other
}

float freqAt(int i, int n) { return kFreqLo * std::pow(kFreqHi / kFreqLo, (float)i / n); }

// The zero-delay-feedback SVF is the bilinear transform of the analogue one: with W the frequency
// in units of the (pre-warped) cutoff, the three outputs of Svf::tick are
//   low = 1/D, high = -W^2/D, band = jW (0.6 + 0.4 k)/D, with D = 1 - W^2 + jkW.
float filterDb(int mode, float cutoff, float reso, float hz) {
    const float fc = clampf(cutoff, 20.0f, kFilterSr * 0.45f);
    const float k = 2.0f - 2.0f * clampf(reso, 0.0f, 0.97f);
    const float W = std::tan(kPi * clampf(hz, 1.0f, kFilterSr * 0.49f) / kFilterSr) / std::tan(kPi * fc / kFilterSr);
    const float re = 1.0f - W * W, im = k * W;
    const float den = std::sqrt(re * re + im * im);
    const float num = mode == 0 ? 1.0f : (mode == 1 ? W * W : W * (0.6f + 0.4f * k));
    return 20.0f * std::log10(num / (den + 1e-9f) + 1e-9f);
}

void filterCurve(const float *p, FilterCurve &c) {
    const int mode = (int)p[P_FMODE];
    const float reso = p[P_RESO];
    c.cutoff = p[P_CUTOFF];
    c.cutDb = filterDb(mode, c.cutoff, reso, c.cutoff);
    // the engine's cutoff at the top of the filter envelope (key tracking is zero at middle C)
    c.envCutoff = clampf(c.cutoff * std::exp2(clampf(p[P_FENV], -8.0f, 8.0f)), 30.0f, 18000.0f);
    c.envOn = std::fabs(p[P_FENV]) > 0.05f;
    for (int i = 0; i <= FilterCurve::N; ++i) {
        float f = freqAt(i, FilterCurve::N);
        c.db[i] = filterDb(mode, c.cutoff, reso, f);
        c.dbEnv[i] = filterDb(mode, c.envCutoff, reso, f);
    }
}

} // namespace td
