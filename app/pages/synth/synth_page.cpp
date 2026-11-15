// venv-vrequency — the Synth page (see synth_page.h): the layout, the scope's X-Y pad and the chord pads.
#include "app/pages/synth/synth_page.h"
#include "app/sounds/sounds.h"
#include "app/ui/palette.h"

using namespace td;
using namespace pal;

SynthPage::SynthPage(Shell &s, ChordPads &c) : Page(s), sounds(s.soundShelf()), pads(c), scope(s) {}

// A finger on a string is let go, as if it had lifted.
void SynthPage::releaseTouches() {
    for (int s = 0; s < kStrings; ++s)
        if (touchCol[s] >= 0) { inst.key(s, touchCol[s] | 0x100, false); touchCol[s] = -1; }
}

void SynthPage::paint() {
    paintHeader();

    // ---------------- scope + X-Y pad ----------------
    QRectF xy(16, 92, 688, 226);
    scope.draw(xy);
    ui.zone(xy, "xy", [this, xy](QPointF p, int ph) {
        Event e; e.type = EV_XY; e.a = ph != PH_RELEASE;
        e.f = (float)qBound(0.0, (p.x() - xy.x()) / xy.width(), 1.0);
        e.g = (float)qBound(0.0, (xy.bottom() - p.y()) / xy.height(), 1.0);
        inst.push(e);
    });
    ui.button(QRectF(xy.right() - 266, xy.y() + 8, 64, 40), "scopeview", Scope::modeShort(scope.mode), scope.mode != Scope::M_LINE, [this] { sh.perform(A_SCOPE_VIEW); });
    ui.button(QRectF(xy.right() - 196, xy.y() + 8, 118, 40), "xymap", nice(xyMapName(u.set.xyMap)), false, [this] { sh.perform(A_XYMAP); });
    ui.button(QRectF(xy.right() - 72, xy.y() + 8, 64, 40), "xyhold", "Hold", u.set.xyHold, [this] { sh.set(S_XYHOLD, u.set.xyHold ? 0 : 1); });

    paintStrings(QRectF(16, 328, 688, 176));
    pads.paint(QRectF(16, 512, 688, 106));
    paintSoftKeys();
}
