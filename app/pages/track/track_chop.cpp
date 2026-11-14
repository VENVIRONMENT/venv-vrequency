// venv-vrequency — the Track page: the Chop strip, where a placed clip is cut into pieces and reordered.
#include "app/pages/track/track_page.h"
#include "app/pages/track/track_layout.h"
#include "app/ui/palette.h"
#include <QPainter>

using namespace td;
using namespace pal;

void TrackPage::trackChop() {
    if (chopOpen) { chopOpen = false; return; }
    if (!chosen()) { toast("Choose a placed clip first", 1400); return; }
    chopOpen = true; chopPiece = 0;
    soundOpen = false; mixOpen = false;
}

void TrackPage::chopEdit(const std::function<bool(Placement &)> &fn) {
    if (!chosen()) return;
    const int ln = selLane, i = selIdx, bar = chosen()->bar, pc = chopPiece;
    trackApply([&](TrackData &t) { return fn(t.lane[ln][i]); });
    chooseAt(ln, bar);
    chopPiece = pc;
    chopOpen = chosen() != nullptr;
}

void TrackPage::chopTap(double frac, double stripWidth) {
    Placement *p = chosen();
    if (!p) return;
    const Clip &c = *track.clips[p->clip];
    const double gr = tl::chopGrid(c), total = std::max(4.0, c.beats()), pxPerBeat = stripWidth / total;
    const double beat = qBound(0.0, frac * total, total - 1e-3), q = std::round(beat / gr) * gr;
    if (q > 1e-6 && q < total - 1e-6 && std::fabs(beat - q) * pxPerBeat <= 14) {
        double acc = 0; int edge = -1;
        for (int i = 0; i + 1 < (int)p->pieces.size(); ++i) { acc += p->pieces[i].len; if (std::fabs(acc - q) < 1e-6) edge = i; }
        if (edge >= 0) {
            Placement probe = *p;
            if (!joinPieces(probe, edge)) { toast("Those two were moved apart: Reset undoes every cut", 2400); return; }
            chopEdit([edge](Placement &x) { return joinPieces(x, edge); });
        } else {
            chopEdit([q](Placement &x) { return cutPiece(x, q); });
        }
        if (Placement *n = chosen()) chopPiece = qMax(0, pieceAt(*n, beat));
        return;
    }
    chopPiece = qMax(0, pieceAt(*p, beat));
}

void TrackPage::paintChop() {
    const int L = Qt::AlignLeft | Qt::AlignVCenter;
    Placement &pl = *chosen();
    const Clip &c = *track.clips[pl.clip];
    const double Lc = std::max(4.0, c.beats()), gr = tl::chopGrid(c);
    const int n = (int)pl.pieces.size();
    chopPiece = qBound(0, chopPiece, n - 1);
    ui.section(414, QString("Chop  %1 on %2, %3").arg(QString::fromUtf8(c.name)).arg(tl::laneLabel(selLane)).arg(tl::barsText(pl.bar, pl.bars)));

    const QRectF sr(16, 438, 688, 124);
    ui.frame(sr, LOW, BG);
    auto X = [&](double b) { return std::floor(sr.x() + b / Lc * sr.width()); };
    const qreal ppb = sr.width() / Lc;
    double acc = 0;
    for (int i = 0; i < n; ++i) {
        const Piece &pc = pl.pieces[i];
        const qreal x0 = X(acc), x1 = X(acc + pc.len);
        const QRectF pr(x0, sr.y(), x1 - x0 + (i == n - 1 ? 0 : 1), sr.height());
        const bool on = i == chopPiece;
        const QRectF hd(pr.x() + 1, pr.y() + 1, pr.width() - 2, 16);
        ui.g->fillRect(hd, on ? INK : GRAT);
        const QString letter = i < 26 ? QString(QChar('A' + i)) : QString::number(i + 1);
        ui.txt(hd.adjusted(5, 0, -2, 0), ui.fit(pc.mute ? letter + " muted" : letter, 11, hd.width() - 6), 11, on ? BG : INK, L, true);
        const QRectF body(pr.x() + 2, pr.y() + 19, pr.width() - 4, pr.height() - 38);
        for (double b = std::ceil((acc + 1e-6) / gr) * gr; b < acc + pc.len - 1e-6; b += gr) // where a cut may fall
            for (qreal y = body.y(); y < body.bottom(); y += 4) ui.g->fillRect(QRectF(X(b), y, 1, 2), GRAT);
        if (pc.mute) hatch(body, LOW);
        else drawClipBody(c, pc.src, pc.src + pc.len, body, INK);
        ui.frame(pr, on ? INK : MID);
        if (on) ui.frame(pr.adjusted(1, 1, -1, -1), INK);
        acc += pc.len;
    }
    // where each moment falls on the track
    const double step = ppb >= 44 ? 1.0 : (ppb * 2 >= 44 ? 2.0 : 4.0);
    for (double b = 0; b < Lc - 1e-6; b += step)
        ui.txt(QRectF(X(b) + 5, sr.bottom() - 18, 60, 15), QString("%1.%2").arg(pl.bar + 1 + (int)(b / 4)).arg((int)std::fmod(b, 4.0) + 1), 11, LOW, L);
    if (pl.bars * 4.0 < Lc - 1e-6) { // split off: the rest of the clip is not heard here
        const QRectF un(X(pl.bars * 4.0) + 1, sr.y() + 18, sr.right() - X(pl.bars * 4.0) - 2, sr.height() - 19);
        ui.g->fillRect(un, QColor(4, 4, 4, 190));
        ui.txt(un, "not played here", 11, LOW);
    }
    if (u.trackPlaying) {
        const double rel = u.trackPos - pl.bar * 4.0;
        if (rel >= 0 && rel < pl.bars * 4.0) ui.g->fillRect(QRectF(X(std::fmod(rel, Lc)), sr.y() + 1, 2, sr.height() - 2), INK);
    }
    ui.zone(sr, "chop_strip", [this, sr](QPointF p, int ph) {
        if (ph == PH_PRESS) chopTap((p.x() - sr.x()) / sr.width(), sr.width());
    });

    const qreal pw = (688 - 3 * 8) / 4.0;
    auto c4 = [pw](int i, qreal y) { return QRectF(std::floor(16 + i * (pw + 8)), y, std::floor(pw), 54); };
    const double every = std::max(1.0, gr);
    ui.button(c4(0, 570), "chop_halves", "Halves", false, [this] { chopEdit([](Placement &x) { return cutPiece(x, piecesLength(x) / 2); }); }, "cut in two");
    ui.button(c4(1, 570), "chop_beats", every >= 4 ? "Bars" : "Beats", false, [this] {
        chopEdit([this](Placement &x) { size_t b = x.pieces.size(); cutEvery(x, std::max(1.0, tl::chopGrid(*track.clips[x.clip]))); return x.pieces.size() != b; });
    },
           every == 1.0 ? "cut every beat" : (every == 2.0 ? "cut every 2 beats" : "cut every bar"));
    ui.button(c4(2, 570), "chop_reset", "Reset", false, [this] {
        chopEdit([this](Placement &x) { if (x.pieces.size() == 1 && !x.pieces[0].mute) return false; resetPieces(x, *track.clips[x.clip]); return true; });
        chopPiece = 0;
    }, "undo every cut");
    ui.button(c4(3, 570), "chop_done", "Done", false, [this] { chopOpen = false; }, "back to the track");
    const QString letter = chopPiece < 26 ? QString(QChar('A' + chopPiece)) : QString::number(chopPiece + 1);
    auto move = [this](int d) { // reads the pieces when tapped, not when drawn
        Placement *p = chosen();
        const int pc = chopPiece, to = pc + d;
        if (!p || to < 0 || to >= (int)p->pieces.size()) return;
        chopEdit([pc, d](Placement &x) { return movePiece(x, pc, d); });
        chopPiece = to;
    };
    ui.button(c4(0, 630), "chop_move_left", QString::fromUtf8("‹ Move"), false, [move] { move(-1); }, QString("piece %1 earlier").arg(letter));
    ui.button(c4(1, 630), "chop_move_right", QString::fromUtf8("Move ›"), false, [move] { move(1); }, QString("piece %1 later").arg(letter));
    ui.button(c4(2, 630), "chop_mute_piece", "Mute piece", pl.pieces[chopPiece].mute, [this] {
        chopEdit([this](Placement &x) { if (chopPiece >= (int)x.pieces.size()) return false; x.pieces[chopPiece].mute = !x.pieces[chopPiece].mute; return true; });
    }, "leave a gap");
    ui.button(c4(3, 630), "chop_repeat_piece", "Repeat piece", false, [this] {
        const int pc = chopPiece;
        chopEdit([pc](Placement &x) { return repeatPiece(x, pc); });
        if (Placement *p = chosen()) chopPiece = qMin(pc + 1, (int)p->pieces.size() - 1);
    }, "the next plays it too");
    ui.txt(QRectF(18, 690, 684, 20), "Tap a dotted line to cut there, a cut to mend it, a piece to choose it. Pieces stay in time.", 12, MID, L);
}
