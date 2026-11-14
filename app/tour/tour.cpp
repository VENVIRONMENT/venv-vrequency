// venv-vrequency — the interactive tour (see tour.h): opening and closing, the page, the card database, the pictures.
#include "app/tour/tour.h"
#include "app/tour/tourpriv.h"
#include "app/shell/mainwindow.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <QTimer>
#include <algorithm>
#include <cmath>
using namespace pal;

// ---------------------------------------------------------------------------------- lifecycle

TourView::TourView(MainWindow &win) : w(win) { pageIdx = 4; }
TourView::~TourView() = default;

const CardDb &TourView::cards() const { return useOverride ? override_ : helpCards(); }
void TourView::setCards(const CardDb &db) { override_ = db; useOverride = true; }
void TourView::resetCards() { useOverride = false; override_ = CardDb(); }

void TourView::ensureBuilt() {
    if (built) return;
    tourBuildPictures(pics, &w.sink);
    built = true;
}

const TourView::Pic &TourView::picture(int page) {
    ensureBuilt();
    return pics[qBound(0, page, picCount() - 1)];
}

bool TourView::seen() const { return w.tourSeenFlag; }
void TourView::forgetSeen() { w.tourSeenFlag = false; promptDone = false; promptOn = false; }

void TourView::markSeen() {
    promptOn = false; promptDone = true;
    if (!w.tourSeenFlag) { w.tourSeenFlag = true; }
    w.saveState();
}

bool TourView::open(const QString &page, const QString &element, bool guide, QString *err, bool defer) {
    int idx = pageIdx;
    if (!page.isEmpty()) {
        idx = pageIndex(page);
        if (idx < 0) { if (err) *err = "unknown tour page '" + page.left(40) + "'"; return false; }
    } else if (!opened) {
        const int last = pageIndex(w.tourLastPage);
        idx = last >= 0 ? last : pageIndex("synth");
    }
    if (!opened) w.guide.dockClose();
    opened = true;
    promptOn = false; promptDone = true;
    if (!built && defer) { // drawing the pictures takes a moment: say so, then do it
        pendingPage = idx; pendingElement = element; pendingGuided = guide; building = true;
        QTimer::singleShot(30, [this] { if (opened && building) { ensureBuilt(); building = false; finishOpen(pendingPage, pendingElement, pendingGuided); } });
        w.update();
        return true;
    }
    ensureBuilt();
    building = false;
    finishOpen(idx, element, guide);
    return true;
}

void TourView::finishOpen(int idx, const QString &element, bool guide) {
    conceptOn = false; conceptId.clear();
    guided = guide;
    if (guided) { step = 0; idx = pageIndex(tourPathAt(0)); }
    setPage(idx);
    if (!element.isEmpty() && !select(element)) {
        QString id = element.startsWith("concepts.") ? element.mid(9) : element;
        if (cards().conceptCard(id)) selectConcept(id);
    }
    markSeen();
    w.update();
}

void TourView::close() {
    if (!opened) return;
    opened = false; building = false; conceptOn = false; conceptId.clear(); guided = false;
    w.tourLastPage = pageId();
    w.saveState();
    w.update();
}

void TourView::setPage(int i, bool keep) {
    pageIdx = qBound(0, i, pageCount() - 1);
    variant = 0;
    if (!keep) selIdx = -1;
    scrollA = scrollB = 0;
    w.tourLastPage = pageId();
    conceptOn = false;
}
