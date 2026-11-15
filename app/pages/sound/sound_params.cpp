// venv-vrequency — the Sound page's parameter sliders: the quick eight, the All parameters view,
// and how a parameter's value is shown and mapped to the slider.
#include "app/pages/sound/sound_page.h"
#include "app/sounds/sounds.h"
#include "app/ui/palette.h"
#include <cmath>

using namespace td;
using namespace pal;

float paramToNorm(int p, float v) {
    const ParamInfo &m = paramInfo(p);
    if (m.logScale) return std::log(v / m.min) / std::log(m.max / m.min);
    return (v - m.min) / (m.max - m.min);
}

float normToParam(int p, float n) {
    const ParamInfo &m = paramInfo(p);
    n = n < 0 ? 0 : (n > 1 ? 1 : n);
    float v = m.logScale ? m.min * std::pow(m.max / m.min, n) : m.min + n * (m.max - m.min);
    if (m.integer) v = std::round(v);
    return v;
}

QString paramText(int p, float v) {
    switch (p) {
    case P_WAVE1: case P_WAVE2: return waveName((int)v);
    case P_FMODE: return filterName((int)v);
    case P_DLYDIV: return delayDivName((int)v);
    case P_SEMI2: return QString("%1%2 st").arg(v > 0 ? "+" : "").arg((int)v);
    case P_CUTOFF: return v >= 1000 ? QString("%1 kHz").arg(v / 1000, 0, 'f', 1) : QString("%1 Hz").arg((int)v);
    case P_DETUNE: case P_SPREAD: case P_VIBDEPTH: return QString("%1 ct").arg((int)v);
    case P_GLIDE: return QString("%1 ms").arg((int)v);
    case P_ATK: case P_DEC: case P_REL: case P_FATK: case P_FDEC: case P_FREL: case P_VIBDELAY:
        return v < 1 ? QString("%1 ms").arg((int)(v * 1000)) : QString("%1 s").arg(v, 0, 'f', 1);
    case P_VIBRATE: case P_LFORATE: return QString("%1 Hz").arg(v, 0, 'f', v < 1 ? 2 : 1);
    case P_FENV: case P_LFOCUT: return QString("%1 oct").arg(v, 0, 'f', 1);
    case P_FM: return QString::number(v, 'f', 1);
    case P_LEVEL: return QString("%1 dB").arg(20 * std::log10(v), 0, 'f', 1);
    default: {
        const ParamInfo &m = paramInfo(p);
        return QString("%1%").arg((int)std::round((v - m.min) / (m.max - m.min) * 100));
    }
    }
}

void SoundPage::paramSlider(QRectF r, int p) {
    ui.slider(r, QString("param%1").arg(p), nice(paramInfo(p).label), paramToNorm(p, sounds.lead.p[p]), paramText(p, sounds.lead.p[p]),
           [this, p](float n) { sounds.setParam(p, normToParam(p, n)); });
}

void SoundPage::paintQuick() {
    static const int macro[8] = {P_CUTOFF, P_RESO, P_ATK, P_REL, P_GLIDE, P_DRIVE, P_DLYMIX, P_REVMIX};
    for (int k = 0; k < 8; ++k) paramSlider(QRectF(16 + (k % 2) * 348, 296 + (k / 2) * 58, 340, 52), macro[k]);
}
