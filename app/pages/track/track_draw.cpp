// venv-vrequency — the Track page: drawing a placed clip, its bars, notes and chords.
#include "app/pages/track/track_page.h"
#include "app/pages/track/track_layout.h"
#include "app/ui/palette.h"
#include <QPainter>

using namespace td;
using namespace pal;

void TrackPage::hatch(QRectF r, QColor c) {
    ui.g->save();
    ui.g->setClipRect(r, Qt::IntersectClip);
    ui.g->setPen(QPen(c, 1));
    for (qreal x = r.x() - r.height(); x < r.right(); x += 7) ui.g->drawLine(QPointF(x, r.bottom()), QPointF(x + r.height(), r.y()));
    ui.g->restore();
}

const TrackPage::ClipDash &TrackPage::dashes(const Clip &c) {
    auto it = dashCache.constFind(&c);
    if (it != dashCache.constEnd()) return it.value();
    ClipDash cd;
    for (const LayerData &ld : c.layers) {
        float on[kSlots] = {0}, pitch[kSlots] = {0};
        bool open[kSlots] = {false};
        for (const LoopEv &e : ld.ev) {
            if (e.op != OP_ON && e.op != OP_GLIDE && e.op != OP_OFF) continue;
            if (e.slot >= kSlots) continue;
            if (open[e.slot]) { cd.d.push_back({on[e.slot], (float)e.beat, pitch[e.slot]}); open[e.slot] = false; }
            if (e.op != OP_OFF) { open[e.slot] = true; on[e.slot] = (float)e.beat; pitch[e.slot] = e.a; }
        }
        for (int s = 0; s < kSlots; ++s)
            if (open[s]) cd.d.push_back({on[s], (float)c.beats(), pitch[s]});
    }
    if (!cd.d.empty()) {
        cd.lo = 1e9f; cd.hi = -1e9f;
        for (const Dash &d : cd.d) { cd.lo = std::min(cd.lo, d.pitch); cd.hi = std::max(cd.hi, d.pitch); }
    }
    return *dashCache.insert(&c, cd);
}

// Draws the part of a clip between two points of its own time into a rectangle.
void TrackPage::drawClipBody(const Clip &c, double srcA, double srcB, QRectF r, QColor ink) {
    const double span = srcB - srcA;
    if (span <= 1e-9 || r.width() < 2) return;
    auto X = [&](double b) { return r.x() + (b - srcA) / span * r.width(); };
    if (c.kind == CK_BEAT) {
        const qreal sw = 0.25 / span * r.width(), rh = (r.height() - 2) / kDrums;
        for (long s = (long)std::ceil(srcA * 4.0 - 1e-6); s * 0.25 < srcB - 1e-6; ++s) {
            const int step = (int)(((s % kDrumSteps) + kDrumSteps) % kDrumSteps);
            const qreal x = std::floor(X(s * 0.25));
            if (step % 4 == 0) ui.g->fillRect(QRectF(x, r.bottom() - 2, 1, 2), LOW);
            for (int d = 0; d < kDrums; ++d)
                if (c.beat[d] & (1u << step))
                    ui.g->fillRect(QRectF(x + 1, std::floor(r.y() + 1 + d * rh), qMax(2.0, std::floor(sw) - 1), qMax(2.0, std::floor(rh) - 1)), ink);
        }
    } else if (c.kind == CK_CHORDS) {
        const bool held = c.holdChord >= 0 || c.progLen <= 0;
        const double cb = held ? c.beats() : qMax(1, c.chordBeats);
        for (long k = (long)std::floor(srcA / cb + 1e-9); k * cb < srcB - 1e-6; ++k) {
            const qreal x0 = qMax(r.x(), std::floor(X(k * cb))), x1 = qMin(r.right(), std::floor(X((k + 1) * cb)));
            if (k * cb > srcA + 1e-6) ui.g->fillRect(QRectF(x0, r.y() + 2, 1, r.height() - 4), GRAT);
            QString name;
            if (c.holdChord >= 0) name = buildChord(u.set.tuning, c.holdChord, c.seventh).name;
            else if (c.progLen > 0) name = buildChord(u.set.tuning, c.prog[((k % c.progLen) + c.progLen) % c.progLen], c.seventh).name;
            else name = "drone";
            const int px = x1 - x0 >= 44 ? 13 : 11;
            if (x1 - x0 >= name.size() * ui.chW(px) + 2) ui.txt(QRectF(x0, r.y(), x1 - x0, r.height()), name, px, ink, Qt::AlignCenter, px == 13);
            else ui.g->fillRect(QRectF(x0 + 3, r.center().y(), qMax(1.0, x1 - x0 - 6), 2), ink == INK ? MID : ink);
        }
    } else {
        const ClipDash &cd = dashes(c);
        const float range = cd.hi - cd.lo + 1.0f;
        for (const Dash &d : cd.d) {
            if (d.b <= srcA || d.a >= srcB) continue;
            const qreal x0 = std::floor(X(std::max((double)d.a, srcA))), x1 = std::floor(X(std::min((double)d.b, srcB)));
            const qreal y = std::floor(r.y() + 2 + (cd.hi - d.pitch) / range * (r.height() - 7));
            ui.g->fillRect(QRectF(x0, y, qMax(2.0, x1 - x0 - 1), 3), ink);
        }
    }
}

void TrackPage::drawPlacement(int ln, int idx) {
    const Placement &pl = track.lane[ln][idx];
    if (pl.bar + pl.bars <= viewBar || pl.bar >= viewBar + tl::VIEW) return;
    if (pl.clip < 0 || pl.clip >= (int)track.clips.size() || pl.pieces.empty()) return;
    const Clip &c = *track.clips[pl.clip];
    const QRectF lr = tl::laneRect(ln);
    auto X = [&](double bars) { return std::floor(tl::GX + (bars - viewBar) * tl::BW); };
    const qreal x0 = X(pl.bar), x1 = X(pl.bar + pl.bars);
    const bool sel = ln == selLane && idx == selIdx;
    const QColor ink = pl.mute ? LOW : INK;
    const QRectF r(x0, lr.y(), x1 - x0 + 1, lr.height());
    ui.g->fillRect(r, BG);
    const QRectF hd(r.x() + 1, r.y() + 1, r.width() - 2, 14);
    ui.g->fillRect(hd, sel ? INK : GRAT);
    const SoundPick &pk = pickFor(c, pl);
    QString title = QString::fromUtf8(c.name) + (pl.mute ? " muted" : "");
    if (pk.any()) { // a picked sound is named beside the clip
        QString snd = c.kind == CK_BEAT ? nice(soundName(c, pk)) : QString::fromUtf8(soundName(c, pk));
        if (pk.octave) snd += QString(" %1%2").arg(pk.octave > 0 ? "+" : "").arg(pk.octave);
        title += "  " + snd;
    }
    ui.txt(hd.adjusted(4, 0, -2, 0), ui.fit(title, 11, hd.width() - 6), 11,
        sel ? BG : (pl.mute ? MID : INK), Qt::AlignLeft | Qt::AlignVCenter, true);
    const QRectF body(r.x() + 2, r.y() + 16, r.width() - 4, r.height() - 19);
    const double Lc = std::max(4.0, c.beats()), total = pl.bars * 4.0;
    double t = 0;
    for (int guard = 0; t < total - 1e-9 && guard < 4096; ++guard) {
        const double lp = std::fmod(t, Lc);
        double ps = 0;
        const int pc = pieceAt(pl, lp, &ps);
        const Piece &pcs = pl.pieces[pc];
        const double into = lp - ps, len = std::min(pcs.len - into, total - t);
        if (len < 1e-6) break;
        const qreal a = X(pl.bar + t / 4.0), b = X(pl.bar + (t + len) / 4.0);
        const QRectF pr(qMax(a, body.x()), body.y(), qMin(b, body.right()) - qMax(a, body.x()), body.height());
        if (pr.right() > tl::GX && pr.x() < tl::GX + tl::GW) {
            if (pcs.mute) hatch(pr, LOW);
            else drawClipBody(c, pcs.src + into, pcs.src + into + len, pr, ink);
        }
        t += len;
        if (t < total - 1e-9) ui.g->fillRect(QRectF(b, body.y() - 1, 1, body.height() + 2), std::fmod(t, Lc) < 1e-6 ? MID : LOW);
    }
    ui.frame(r, sel ? INK : MID);
    if (sel) ui.frame(r.adjusted(1, 1, -1, -1), INK);
}
