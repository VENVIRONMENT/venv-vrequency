// venv-vrequency — the first-run offer: "Take the 2-minute tour?", its timing and its answer.
#include "app/tour/tour.h"
#include "app/tour/tourpriv.h"
#include "app/shell/mainwindow.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <QTimer>
#include <algorithm>
#include <cmath>
using namespace pal;

// ---------------------------------------------------------------------------------- the first-run offer

void TourView::armFirstRunPrompt(int delayMs, int lifeMs) {
    if (w.tourSeen()) return;
    promptDone = false;
    promptAt = w.clock.elapsed() + delayMs;
    promptUntil = promptAt + lifeMs;
}

bool TourView::promptShowing() const {
    if (promptDone || opened || w.tourSeen()) return false;
    if (promptAt < 0) return false;
    const qint64 now = w.clock.elapsed();
    return now >= promptAt && now < promptUntil;
}

void TourView::answerPrompt(bool yes) {
    markSeen();
    if (yes) open("synth", QString(), true, nullptr, true);
    w.update();
}

bool TourView::promptKey(int k) {
    if (!promptShowing()) {
        // an offer nobody answered goes away quietly, once, and is not made again
        if (!promptDone && !w.tourSeen() && promptAt >= 0 && w.clock.elapsed() >= promptUntil) markSeen();
        return false;
    }
    if (k == Qt::Key_Return || k == Qt::Key_Enter) { answerPrompt(true); return true; }
    if (k == Qt::Key_Escape) { answerPrompt(false); return true; }
    return false;
}

void TourView::paintPrompt() {
    if (!promptShowing()) {
        if (!promptDone && !w.tourSeen() && promptAt >= 0 && w.clock.elapsed() >= promptUntil) markSeen(); // unanswered: it does not come back
        return;
    }
    const QRectF r(12, 6, 696, 48);
    w.ui.frame(grow(r, 2), INK, BG);
    w.ui.frame(r, LOW);
    w.ui.zone(r, "tour_prompt", [](QPointF, int) {}); // the offer takes touches meant for what is under it
    w.ui.tag(QRectF(r.x() + 6, r.y() + 6, 56, 18), "TOUR", 12);
    w.ui.txt(QRectF(r.x() + 70, r.y() + 4, 330, 22), "Take the 2-minute tour?", 15, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    w.ui.txt(QRectF(r.x() + 8, r.y() + 27, 380, 18), "Enter = yes    Esc = later    keep playing meanwhile", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    w.ui.button(QRectF(r.right() - 270, r.y() + 6, 150, 36), "tour_prompt_yes", "Take the tour", true, [this] { answerPrompt(true); });
    w.ui.button(QRectF(r.right() - 114, r.y() + 6, 106, 36), "tour_prompt_later", "Later", false, [this] { answerPrompt(false); });
}
