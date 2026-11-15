// venv-vrequency — the Synth page's three strings: notes, the key letters, the sounding pitch and touch.
#include "app/pages/synth/synth_page.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <QPainterPath>
#include <cmath>

using namespace td;
using namespace pal;

void SynthPage::paintStrings(QRectF sp) {
    const Tuning &t = u.set.tuning;
    const KeyMap &km = sh.km;
    const KeyState &keys = sh.keys;
    const int L = Qt::AlignLeft | Qt::AlignVCenter;
    ui.frame(sp, LOW, BG);
    const qreal x0 = sp.x() + 40, dx = (sp.width() - 80) / 9.0;
    qint64 now = sh.clock.elapsed();
    for (int row = 2; row >= 0; --row) {
        qreal y = sp.y() + 36 + (2 - row) * 54;
        int count = km.rows[row].size();
        int shift = row == 0 ? 1 : 0; // the Q20's bottom letter row starts one key in
        auto X = [&](qreal col) { return x0 + (col + shift) * dx; };
        const StringUi &su = u.str[row];
        auto bracket = [this](qreal cx, qreal y, int wd) { // [ ] round a sounding pitch
            ui.g->fillRect(QRectF(cx - 21, y - 15, wd, 31), INK);
            ui.g->fillRect(QRectF(cx + 21 - wd + 1, y - 15, wd, 31), INK);
            ui.g->fillRect(QRectF(cx - 21, y - 15, 7, wd), INK);
            ui.g->fillRect(QRectF(cx - 21, y + 16 - wd, 7, wd), INK);
            ui.g->fillRect(QRectF(cx + 15, y - 15, 7, wd), INK);
            ui.g->fillRect(QRectF(cx + 15, y + 16 - wd, 7, wd), INK);
        };
        // the string is a trace: flat at rest, standing wave while it sounds
        if (su.gate) {
            QPainterPath line;
            for (int i = 0; i <= 80; ++i) {
                qreal x = sp.x() + 8 + (sp.width() - 16) * i / 80.0;
                qreal yy = y + 3.5 * std::sin(i * 0.7 + now * 0.03) * std::sin(3.14159 * i / 80.0);
                if (i == 0) line.moveTo(x, yy); else line.lineTo(x, yy);
            }
            ui.g->setRenderHint(QPainter::Antialiasing, true);
            ui.g->setBrush(Qt::NoBrush);
            ui.g->setPen(QPen(QColor(255, 255, 255, 60), 5));
            ui.g->drawPath(line);
            ui.g->setPen(QPen(INK, 1.6));
            ui.g->drawPath(line);
            ui.g->setRenderHint(QPainter::Antialiasing, false);
        } else {
            ui.g->fillRect(QRectF(sp.x() + 8, y, sp.width() - 16, 1), LOW);
        }
        ui.txt(QRectF(sp.x() + 4, y - 26, 20, 12), QString("S%1").arg(row + 1), 10, su.gate ? INK : LOW, L);
        // pitch -> x (piecewise linear across the row's notes)
        auto pitchX = [&](float pitch) {
            float pbase = pitch - u.bend;
            int prev = t.noteFor(row, 0, false);
            if (pbase <= prev) return X(0) - qMin(0.6, (double)(prev - pbase) / 6.0) * dx;
            for (int c = 1; c < count; ++c) {
                int n = t.noteFor(row, c, false);
                if (pbase <= n) return X(c - 1) + dx * (pbase - prev) / qMax(1, n - prev);
                prev = n;
            }
            return X(count - 1) + qMin(0.6, (double)(pbase - prev) / 6.0) * dx;
        };
        bool chordKeys = row == 0 && (keys.chordRow || keys.chord);
        for (int c = 0; c < count; ++c) {
            int n = t.noteFor(row, c, false);
            bool isHeldTop = u.set.poly ? (su.heldMask >> c & 1) : (su.held > 0 && su.topCol == c);
            bool isRoot = ((n - t.root) % 12 + 12) % 12 == 0;
            QRectF box(std::floor(X(c)) - 17, y - 11, 35, 23);
            ui.frame(box, isHeldTop || isRoot ? INK : LOW, isHeldTop ? INK : BG);
            QString nn = QString("%1%2").arg(noteName(n)).arg(n / 12 - 1);
            ui.txt(box, nn, 12, isHeldTop ? BG : (isRoot ? INK : MID), Qt::AlignCenter, isRoot || isHeldTop);
            QRectF kr(box.center().x() - 8, y - 27, 16, 13);
            if (chordKeys) ui.tag(kr.adjusted(1, 0, -1, 0), QString(km.rows[row][c]), 10, Qt::AlignCenter);
            else ui.txt(kr, QString(km.rows[row][c]), 11, LOW);
        }
        // where the pitch really is: a cursor that travels during a slide, leaving a fading trail
        std::deque<float> &tr = trail[row];
        if (u.set.poly) { // poly: brackets round every note still sounding, held or sustained
            tr.clear();
            for (int c = 0; c < count; ++c)
                if (su.soundMask >> c & 1) bracket(std::floor(X(c)), y, 2);
        } else if (su.gate) {
            float px = (float)pitchX(su.note + u.bend);
            tr.push_back(px);
            if (tr.size() > 10) tr.pop_front();
            bool sliding = std::fabs(su.note - su.target) > 0.02f;
            for (size_t i = 0; i + 1 < tr.size(); ++i) {
                if (std::fabs(tr[i] - px) < 1.0f) continue;
                int v = 40 + 14 * (int)i;
                ui.g->fillRect(QRectF(std::floor(tr[i]), y + 13, 1, 9), QColor(v, v, v));
            }
            qreal cx = std::floor(px);
            bracket(cx, y, sliding ? 3 : 2);
            QPainterPath m; // trigger-style marker under the string
            m.moveTo(cx - 4, y + 23); m.lineTo(cx + 5, y + 23); m.lineTo(cx + 0.5, y + 17); m.closeSubpath();
            ui.g->setRenderHint(QPainter::Antialiasing, true);
            ui.g->setPen(Qt::NoPen);
            ui.g->setBrush(INK);
            ui.g->drawPath(m);
            ui.g->setRenderHint(QPainter::Antialiasing, false);
        } else if (!tr.empty()) tr.clear();
        // touch: press plays, dragging along the string slides
        QRectF zr(sp.x(), y - 27, sp.width(), 54);
        ui.zone(zr, QString("string%1").arg(row), [this, row, count, x0, dx, shift](QPointF p, int ph) {
            int col = (int)std::floor((p.x() - x0) / dx - shift + 0.5);
            col = qBound(0, col, count - 1);
            int &cur = touchCol[row];
            if (ph == PH_RELEASE) { if (cur >= 0) inst.key(row, cur | 0x100, false); cur = -1; return; }
            if (col == cur) return;
            inst.key(row, col | 0x100, true, sh.keys.chromatic);
            if (cur >= 0) inst.key(row, cur | 0x100, false);
            cur = col;
        });
    }
}
