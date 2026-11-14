// venv-vrequency — the Tune page (see tune_page.h): the controls above the Listen panel.
#include "app/pages/tune/tune_page.h"
#include "app/ui/palette.h"
#include <cmath>

using namespace td;
using namespace pal;

void TunePage::paint() {
    const Tuning &t = u.set.tuning;
    ui.section(60, "Key");
    qreal w6 = (688 - 5 * 6) / 6.0;
    for (int i = 0; i < 12; ++i)
        ui.button(QRectF(std::floor(16 + (i % 6) * (w6 + 6)), 84 + (i / 6) * 52, std::floor(w6), 46), QString("root%1").arg(i), noteName(i), t.root == i, [this, i] { sh.set(S_ROOT, (float)i); });
    ui.section(190, "Scale");
    qreal w4 = (688 - 3 * 6) / 4.0;
    for (int i = 0; i < SC_COUNT; ++i)
        ui.button(QRectF(std::floor(16 + (i % 4) * (w4 + 6)), 214 + (i / 4) * 52, std::floor(w4), 46), QString("scale%1").arg(i), scaleDef(i).name, t.scale == i, [this, i] { sh.set(S_SCALE, (float)i); });

    qreal w3 = (688 - 2 * 8) / 3.0;
    auto c3 = [w3](int i, qreal y) { return QRectF(std::floor(16 + i * (w3 + 8)), y, std::floor(w3), 60); };
    ui.stepper(c3(0, 326), "oct", "Octave", QString("%1%2").arg(t.octave > 0 ? "+" : "").arg(t.octave),
            [this] { inst.cmd(EV_OCTAVE, -1); }, [this] { inst.cmd(EV_OCTAVE, 1); });
    ui.stepper(c3(1, 326), "rowint", "String gap", QString("%1 st").arg(t.rowInterval),
            [this] { sh.set(S_ROWINT, (float)(u.set.tuning.rowInterval - 1)); }, [this] { sh.set(S_ROWINT, (float)(u.set.tuning.rowInterval + 1)); });
    ui.stepper(c3(2, 326), "bendr", "Bend range", QString("%1 st").arg((int)u.set.bendRange),
            [this] { sh.set(S_BENDRANGE, u.set.bendRange - 1); }, [this] { sh.set(S_BENDRANGE, u.set.bendRange + 1); });

    ui.stepper(c3(0, 394), "bpm", "Tempo", QString("%1 bpm").arg((int)std::round(u.set.bpm)),
            [this] { sh.set(S_BPM, std::round(u.set.bpm) - 1); }, [this] { sh.set(S_BPM, std::round(u.set.bpm) + 1); });
    QRectF mid = c3(1, 394);
    ui.button(QRectF(mid.x(), 394, std::floor(mid.width() / 2) - 4, 60), "tap", "Tap", false, [this] { sh.perform(A_TAP); }, "tempo");
    ui.button(QRectF(mid.x() + std::floor(mid.width() / 2) + 4, 394, mid.right() - (mid.x() + std::floor(mid.width() / 2) + 4), 60), "metro", "Click", u.set.metro, [this] { sh.set(S_METRO, u.set.metro ? 0 : 1); }, "metronome");
    ui.slider(c3(2, 394), "glidescale", "Glide speed", u.set.glideScale / 4.0f,
           u.set.glideScale < 0.02f ? QString("instant") : QString::fromUtf8("×%1").arg(u.set.glideScale, 0, 'f', 2), [this](float n) { sh.set(S_GLIDESCALE, n * 4.0f); });

    paintListen();
}
