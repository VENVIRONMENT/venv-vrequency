// venv-vrequency — the tour's keys, Ask Rook, and the state report for Rook and the tests.
#include "app/tour/tour.h"
#include "app/tour/tourpriv.h"
#include "app/shell/mainwindow.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <QTimer>
#include <algorithm>
#include <cmath>
using namespace pal;

void TourView::ask() {
    const HelpCard *c = nullptr;
    QString zone;
    QString page = pageId();
    if (conceptOn) {
        c = cards().conceptCard(conceptId);
        if (c) zone = "concepts." + c->key;
        else { c = nullptr; }
    } else if (selIdx >= 0 && selIdx < (int)pics[picIndex()].hot.size()) {
        c = cardFor(pics[picIndex()].hot[selIdx]);
        zone = pics[picIndex()].hot[selIdx].name;
    }
    QString text;
    if (c) text = c->ask.isEmpty() ? QString("What does %1 do?").arg(c->title) : c->ask;
    else text = QString("Tell me about the %1 page.").arg(pageInfo(pageIdx).label);
    w.tourAsk(page, zone, text);
}

// Ask Rook leaves the tour: the answer is about the real control, so the player is put on the real page
// it belongs to with the chat dock open and the question typed in (not sent: Enter sends it).
void MainWindow::tourAsk(const QString &tourPage, const QString &zone, const QString &text) {
    if (!tourView) return;
    tourView->close();
    const int idx = TourView::pageIndex(tourPage);
    if (idx >= 0) {
        const int real = TourView::pageInfo(idx).realPage;
        if (real == PG_CHAT && pg != PG_CHAT) chat.returnPage = pg;
        goPage(real);
    }
    chat.draft = text.left(380);
    guide.askFromTour(tourPage, zone);
    update();
}

// ---------------------------------------------------------------------------------- input

void TourView::key(int k, const QString &text, bool shift) {
    if (!opened) return;
    if (!built) { if (k == Qt::Key_Escape) close(); return; }
    const bool pageKey = k == Qt::Key_Comma || k == Qt::Key_Period || k == Qt::Key_Less || k == Qt::Key_Greater;
    switch (k) {
    case Qt::Key_Escape:
        if (conceptOn && !conceptId.isEmpty()) { conceptId.clear(); scrollA = scrollB = 0; }
        else if (conceptOn) conceptOn = false;
        else if (selIdx >= 0) choose(-1);
        else close();
        break;
    case Qt::Key_Left: if (!conceptOn) move(-1, 0); break;
    case Qt::Key_Right: if (!conceptOn) move(1, 0); break;
    case Qt::Key_Up: if (conceptOn) cycle(-1); else move(0, -1); break;
    case Qt::Key_Down: if (conceptOn) cycle(1); else move(0, 1); break;
    case Qt::Key_Tab: cycle(shift ? -1 : 1); break;
    case Qt::Key_Backtab: cycle(-1); break;
    case Qt::Key_Return: case Qt::Key_Enter: ask(); break;
    case Qt::Key_PageDown: case Qt::Key_Space: scrollA += 120; scrollB += 120; break;
    case Qt::Key_PageUp: scrollA = qMax(0, scrollA - 120); scrollB = qMax(0, scrollB - 120); break;
    case Qt::Key_Home: choose(-1); conceptId.clear(); break;
    case Qt::Key_V: if (!conceptOn) setVariant(variant + 1); break;
    case Qt::Key_C: conceptOn = !conceptOn; if (conceptOn) { conceptId.clear(); scrollA = scrollB = 0; } break;
    default:
        if (pageKey) {
            const int d = (k == Qt::Key_Comma || k == Qt::Key_Less) ? -1 : 1;
            if (guided) nextStep(d);
            else setPage((pageIdx + d + pageCount()) % pageCount());
        }
        break;
    }
    (void)text;
    w.update();
}

// ---------------------------------------------------------------------------------- state for Rook and the tests

QJsonObject TourView::stateJson() const {
    QJsonObject o;
    o["open"] = opened;
    o["building"] = opened && building;
    o["page"] = pageId().isEmpty() ? QJsonValue() : QJsonValue(pageId());
    const QString s = opened ? selectedName() : QString();
    o["selected"] = s.isEmpty() ? QJsonValue() : QJsonValue(s);
    o["concept"] = (opened && conceptOn && !conceptId.isEmpty()) ? QJsonValue(conceptId) : QJsonValue();
    o["concepts_open"] = opened && conceptOn;
    o["guided"] = opened && guided;
    o["step"] = guided ? step + 1 : 0;
    return o;
}
