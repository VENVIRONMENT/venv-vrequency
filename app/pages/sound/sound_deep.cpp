// venv-vrequency — the Sound page's All parameters view: five pages of sliders. The first holds the
// oscillator parameters under the OSCILLATORS picture; the second the amp envelope and the filter
// under theirs; the next two the rest; the fifth is the Warp view (sound_warp.cpp). The pictures
// follow the sliders as they move.
#include "app/pages/sound/sound_page.h"
#include "app/sounds/sounds.h"
#include "app/ui/palette.h"

using namespace td;
using namespace pal;

namespace {
const int kOsc[8] = {P_WAVE1, P_WAVE2, P_MIX2, P_SEMI2, P_DETUNE, P_SUB, P_PW, P_FM};
const int kEnvFilter[8] = {P_ATK, P_DEC, P_SUS, P_REL, P_FMODE, P_CUTOFF, P_RESO, P_FENV};
const int kMore1[12] = {P_UNISON, P_SPREAD, P_NOISE, P_FATK, P_FDEC, P_FSUS, P_FREL, P_KTRK, P_VIBRATE, P_VIBDEPTH, P_VIBDELAY, P_LFORATE};
const int kMore2[12] = {P_LFOCUT, P_LFOAMP, P_LFOPW, P_GLIDE, P_DRIVE, P_CHORUS, P_DLYMIX, P_DLYDIV, P_DLYFB, P_REVMIX, P_REVSIZE, P_LEVEL};
}

const int *deepParams(int page, int &count) {
    switch (page) {
    case 0: count = 8; return kOsc;
    case 1: count = 8; return kEnvFilter;
    case 2: count = 12; return kMore1;
    case 3: count = 12; return kMore2;
    default: count = 6; return kWarpParams;
    }
}

int deepPageOf(int param) {
    for (int pg = 0; pg < kDeepPages; ++pg) {
        int n; const int *ids = deepParams(pg, n);
        for (int i = 0; i < n; ++i) if (ids[i] == param) return pg;
    }
    return 0;
}

void SoundPage::paintDeep() {
    deepPage = qBound(0, deepPage, kDeepPages - 1);
    int n; const int *ids = deepParams(deepPage, n);
    const float *p = sounds.lead.p;
    pics.note(u.str[0].gate || u.str[1].gate || u.str[2].gate, sh.clock.elapsed() / 1000.0);
    qreal y0 = 296;
    if (deepPage == kDeepPages - 1) { // the Warp view, as WARP shows it
        paintWarp(296);
        ui.txt(QRectF(16, 540, 688, 20), "Each note is warped on its own, after the filter, before its envelope.", 13, MID, Qt::AlignLeft | Qt::AlignVCenter);
    } else if (deepPage == 0) { pics.oscillators(QRectF(16, 296, 688, 104), p); y0 = 408; }
    if (deepPage == 1) { // the amp envelope and the filter side by side
        pics.envelope(QRectF(16, 296, 340, 104), p);
        pics.filter(QRectF(364, 296, 340, 104), p);
        y0 = 408;
    }
    if (deepPage < kDeepPages - 1)
        for (int k = 0; k < n; ++k) paramSlider(QRectF(16 + (k % 2) * 348, y0 + (k / 2) * 58, 340, 52), ids[k]);
    ui.button(QRectF(16, 650, 160, 54), "deepprev", QString::fromUtf8("‹ Prev"), false, [this] { deepPage = (deepPage + kDeepPages - 1) % kDeepPages; });
    ui.txt(QRectF(176, 650, 368, 54), QString("Page %1 / %2").arg(deepPage + 1).arg(kDeepPages), 14, MID);
    ui.button(QRectF(544, 650, 160, 54), "deepnext", QString::fromUtf8("Next ›"), false, [this] { deepPage = (deepPage + 1) % kDeepPages; });
}
