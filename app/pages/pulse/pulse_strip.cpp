// venv-vrequency — the Pulse page's "last 4 s" strip: one lane per pad, a tick for every hit, click
// or gate the engine logged (PulseBank's record), at the sample it began; a long one is a thin bar.
#include "app/pages/pulse/pulse_page.h"
#include "app/ui/palette.h"
#include "app/ui/zones.h"
#include <QPainter>
#include <cmath>

using namespace td;
using namespace pal;

namespace {
constexpr double kSpan = 4.0;      // seconds shown
constexpr size_t kKeep = 6000;     // hits kept at most (a latched 200 Hz train makes 800 in 4 s)
} // namespace

// Every frame, on any page: what the engine logged since the last frame.
void PulsePage::drainTicks() {
    PulseTick buf[256];
    for (int guard = 0; guard < 8; ++guard) {
        const int n = inst.readPulseTicks(tickCursor, buf, 256);
        for (int i = 0; i < n; ++i) hist.push_back(buf[i]);
        if (n < 256) break;
    }
    const double sr = inst.sampleRate();
    const uint64_t now = u.frames, span = (uint64_t)(kSpan * sr);
    while (!hist.empty() && (hist.size() > kKeep || hist.front().frame + hist.front().dur + span < now)) hist.pop_front();
}

void PulsePage::paintStrip(QRectF r) {
    ui.frame(r, GRAT, BG);
    const double sr = inst.sampleRate(), now = (double)u.frames, perPx = kSpan * sr / (r.width() - 2);
    const qreal laneH = (r.height() - 4) / kPulsePads, x1 = r.right() - 1;
    for (int p = 0; p < kPulsePads; ++p) // faint lanes; the selected pad's is a little brighter
        ui.g->fillRect(QRectF(r.x() + 1, std::floor(r.y() + 2 + (p + 0.5) * laneH), r.width() - 2, 1), p == sel ? LOW : GRAT);
    for (int s = 1; s < 4; ++s) // a mark every second
        ui.g->fillRect(QRectF(std::floor(x1 - s * sr / perPx), r.y() + 1, 1, 3), LOW);
    ui.g->save();
    ui.g->setRenderHint(QPainter::Antialiasing, true);
    ui.g->setClipRect(r.adjusted(1, 1, -1, -1));
    qreal lastX[kPulsePads];
    for (qreal &x : lastX) x = -1e9;
    for (const PulseTick &t : hist) {
        const qreal x = x1 - (now - (double)t.frame) / perPx, w = t.dur / perPx;
        if (x + w < r.x() || x > x1 + 1 || t.pad >= kPulsePads) continue;
        const qreal top = r.y() + 2 + t.pad * laneH;
        if (w >= 2.0) { // something long: a bar for its length, the tick at its start
            ui.g->fillRect(QRectF(x, top + laneH * 0.5 - 1, w, 2), MID);
            ui.g->fillRect(QRectF(x, top + 1, 1, laneH - 2), INK);
        } else if (std::fabs(x - lastX[t.pad]) >= 0.75) { // ticks closer than a pixel are one tick
            ui.g->fillRect(QRectF(x, top + 1, 1, laneH - 2), INK);
            lastX[t.pad] = x;
        }
    }
    ui.g->restore();
    ui.zone(r, zn::pulselast, [](QPointF, int) {});
}
