// venv-vrequency — the window's mouse and touch: events become pointer presses, moves and releases
// on the 720x720 canvas.
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include <QMouseEvent>
#include <QTouchEvent>

namespace {
const qreal CW = 720;
} // namespace

QPointF MainWindow::toCanvas(QPointF p) const {
    qreal s = qMin(width(), height()) / CW;
    qreal ox = (width() - CW * s) / 2, oy = (height() - CW * s) / 2;
    return QPointF((p.x() - ox) / s, (p.y() - oy) / s);
}

void MainWindow::pointer(int id, QPointF c, int phase) {
    const bool taken = ui.touch(id, c, phase, [this](const QString &name) {
        if (!tourOpen()) guide.noteTap(name);
    });
    if (taken) update();
}

void MainWindow::mousePressEvent(QMouseEvent *e) { pointer(-1, toCanvas(e->position()), PH_PRESS); }
void MainWindow::mouseMoveEvent(QMouseEvent *e) { if (e->buttons()) pointer(-1, toCanvas(e->position()), PH_MOVE); }
void MainWindow::mouseReleaseEvent(QMouseEvent *e) { pointer(-1, toCanvas(e->position()), PH_RELEASE); }

bool MainWindow::tourOpen() const { return tourView && tourView->isOpen() && !tourSuspended; }

bool MainWindow::event(QEvent *ev) {
    switch (ev->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel: {
        auto *te = static_cast<QTouchEvent *>(ev);
        for (const QEventPoint &pt : te->points()) {
            QPointF c = toCanvas(pt.position());
            if (ev->type() == QEvent::TouchCancel || pt.state() == QEventPoint::Released) pointer(pt.id(), c, PH_RELEASE);
            else if (pt.state() == QEventPoint::Pressed) pointer(pt.id(), c, PH_PRESS);
            else if (pt.state() == QEventPoint::Updated) pointer(pt.id(), c, PH_MOVE);
        }
        ev->accept();
        return true;
    }
    default: return QWidget::event(ev);
    }
}
