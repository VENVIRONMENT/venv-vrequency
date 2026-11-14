// venv-vrequency — the tour's demo: reading the controls off the scratch window, the demo melody, the extra hotspots.
#include "app/tour/tour.h"
#include "app/tour/tourdemo.h"
#include "audio/audio.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "core/presets/presets.h"
#include "core/presets/sound_shelf.h"
#include <QThread>
#include <array>
#include <set>

using namespace td;

namespace {
bool hiddenZone(const QString &n) { return n.startsWith("dock") || n == "ticker" || n == "keytestblock" || n.startsWith("help_row_"); }
} // namespace

void TourDemo::capture(Rig &rig, int page, TourView::Pic &pic) {
    rig.w.chat.hideTicker(); rig.w.toastUntil = 0;
    rig.settle(3);
    pic.img = rig.w.shot();
    pic.small = pic.img.scaled(380, 380, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    std::set<QString> seen;
    size_t from = 0;
    if (page == 11) // the Key test panel swallows every touch but its own Close button
        for (size_t i = 0; i < rig.w.ui.zones().size(); ++i) if (rig.w.ui.zones()[i].name == "keytestblock") from = i + 1;
    for (size_t i = from; i < rig.w.ui.zones().size(); ++i) {
        const auto &z = rig.w.ui.zones()[i];
        if (hiddenZone(z.name) || seen.count(z.name) || z.r.width() < 4 || z.r.height() < 4) continue;
        seen.insert(z.name);
        TourView::Hot h;
        h.name = z.name; h.r = z.r;
        pic.hot.push_back(h);
    }
}

LayerData TourDemo::melody(const Patch &sound, std::initializer_list<std::array<double, 3>> notes) {
    LayerData ld; ld.patches.push_back(sound);
    uint32_t seq = 0;
    for (const auto &n : notes) {
        LoopEv a; a.beat = n[0]; a.op = OP_ON; a.slot = 0; a.a = (float)n[2]; a.b = 1; a.seq = seq++;
        LoopEv b; b.beat = n[0] + n[1]; b.op = OP_OFF; b.slot = 0; b.a = b.b = 0; b.seq = seq++;
        ld.ev.push_back(a); ld.ev.push_back(b);
    }
    return ld;
}

void TourDemo::extras(int page, TourView::Pic &pic) {
    auto add = [&](const char *name, QRectF r, const char *ref) { TourView::Hot h; h.name = name; h.r = r; h.ref = ref; h.real = false; pic.hot.push_back(h); };
    switch (page) {
    case 4: // synth
        add("level_strip", QRectF(16, 76, 688, 12), "concepts.level-strip");
        add("scope_readouts_top", QRectF(18, 93, 470, 18), "concepts.scope-readouts");
        add("scope_readouts_bottom", QRectF(18, 298, 480, 18), "concepts.scope-readouts");
        add("string_brackets", QRectF(235, 400, 48, 40), "concepts.string-brackets");
        break;
    case 7: add("step_grid", QRectF(16, 80, 688, 130), "concepts.step-grid"); break;
    case 9: add("listen_result", QRectF(240, 494, 456, 72), "concepts.listen-result"); break;
    case 10: add("status_box", QRectF(16, 424, 688, 206), "concepts.status-box"); break;
    case 12: add("key_list_rows", QRectF(16, 100, 688, 474), "concepts.help-rows"); break;
    default: break;
    }
}
