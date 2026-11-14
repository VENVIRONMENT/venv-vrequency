// venv-vrequency self-test — touch: the XY pad, the strings under a second finger, the chord pads.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/ui/zones.h"
#include <cmath>
#include <cstdio>

using namespace td;

namespace selftest {
void testTouch(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    std::printf("- touch\n");
    QRectF xy = w.zoneRect(zn::xy);
    w.pointer(1, xy.center(), PH_PRESS); wait(60);
    w.pointer(1, QPointF(xy.left() + xy.width() * 0.9, xy.top() + xy.height() * 0.2), PH_MOVE); wait(80);
    const UiState u = inst.uiState();
    check(u.xyTouch && u.xyX > 0.85f && u.xyY > 0.75f, "dragging the pad moves the XY control");
    check(inst.part(0).modCut > 2.0f, "which modulates the filter live");
    // second finger plays a string while the first stays on the pad
    QRectF s1 = w.zoneRect(zn::at(zn::string, 1));
    w.pointer(2, QPointF(s1.left() + s1.width() * 0.3, s1.center().y()), PH_PRESS); wait(80);
    check(inst.uiState().str[1].gate, "a second touch plays a string at the same time");
    float n0 = inst.uiState().str[1].target;
    w.pointer(2, QPointF(s1.left() + s1.width() * 0.6, s1.center().y()), PH_MOVE); wait(80);
    check(inst.uiState().str[1].target > n0 && inst.uiState().str[1].gate, "dragging along the string slides the pitch");
    w.pointer(2, s1.center(), PH_RELEASE);
    w.pointer(1, xy.center(), PH_RELEASE); wait(80);
    check(!inst.uiState().xyTouch && std::fabs(inst.part(0).modCut) < 0.01f, "lifting the finger returns the sound to normal");
    inst.set(S_XYMAP, XY_GLITCH); wait(60);
    w.pointer(1, xy.center(), PH_PRESS); wait(80);
    check(inst.uiState().glitch, "the Glitch mapping crushes the sound while the pad is touched");
    w.pointer(1, xy.center(), PH_RELEASE); wait(80);
    check(!inst.uiState().glitch, "and stops when the finger lifts");
    inst.set(S_XYMAP, XY_FILTER); wait(60);
    tap(w, zn::at(zn::chord, 4)); wait(60);
    check(inst.uiState().progLen == 5, "touch chord pads work");
}
} // namespace selftest
