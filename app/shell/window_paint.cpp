// venv-vrequency — drawing the window: a frame is the page (or the tour's picture), then the guide,
// the key test, the toast and the tube face over everything.
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <QRadialGradient>
#include <cmath>

using namespace td;
using namespace pal;

namespace {
const qreal CW = 720;
} // namespace

QImage MainWindow::shot() {
    QImage img(720, 720, QImage::Format_RGB32); // the instrument's native canvas, whatever the window size
    QPainter p(&img);
    drawAll(p, 720, 720);
    return img;
}

// ------------------------------------------------------------------ paint

extern qint64 msSinceStart();

void MainWindow::paintEvent(QPaintEvent *) {
    static bool first = true;
    if (first) {
        first = false;
        if (qEnvironmentVariableIsSet("VREQUENCY_TIMING")) fprintf(stderr, "vrequency: first frame %lld ms after launch\n", (long long)msSinceStart());
    }
    QElapsedTimer pt; pt.start();
    {
        QPainter painter(this);
        drawAll(painter, width(), height());
    }
    // VREQUENCY_TIMING=1 prints how often the window is drawn and how long a frame takes
    static const bool timing = qEnvironmentVariableIsSet("VREQUENCY_TIMING");
    static int frames = 0; static qint64 spent = 0, since = 0;
    ++frames; spent += pt.nsecsElapsed();
    if (timing && clock.elapsed() - since >= 2000) {
        fprintf(stderr, "vrequency: page %d, %.1f frames/s, %.1f ms each\n", pg, frames * 1000.0 / (clock.elapsed() - since), spent / 1e6 / frames);
        frames = 0; spent = 0; since = clock.elapsed();
    }
}

void MainWindow::drawAll(QPainter &painter, qreal W, qreal H) {
    u = inst.uiState();
    if (sink.generation != seenSinkGeneration) { // output device changed: pick a sensible loop timing offset
        seenSinkGeneration = sink.generation;
        set(S_LATENCY, sink.bluetooth ? 200.0f : 0.0f);
    }
    ui.beginFrame(painter);
    painter.fillRect(QRectF(0, 0, W, H), BG);
    qreal s = qMin(W, H) / CW;
    painter.translate((W - CW * s) / 2, (H - CW * s) / 2);
    painter.scale(s, s);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    ui.recordTexts(true);
    guide.beginFrame();

    for (Page *p : pageOf) if (p) p->tick();

    if (tourOpen()) tourView->paint(); // the tour draws a picture instead of the page: nothing on it is live
    else if (Page *p = pageOf[pg]) { if (p->tabs()) paintTabs(); p->paint(); }
    ui.recordTexts(false);
    if (!tourOpen()) {
        guide.layoutDock();
        guide.paintHighlights();
        chat.paintTicker();
        guide.paintDock();
        if (keys.testOpen) systemPage.paintKeyTest();
        if (tourView) tourView->paintPrompt();
    }

    paintToast();

    // tube face: faint scan lines and darker corners over everything
    if (facePix.isNull()) {
        QImage im(720, 720, QImage::Format_ARGB32_Premultiplied);
        im.fill(Qt::transparent);
        QPainter q(&im);
        for (int y = 1; y < 720; y += 2) q.fillRect(0, y, 720, 1, QColor(0, 0, 0, 34));
        QRadialGradient v(QPointF(360, 360), 520);
        v.setColorAt(0.0, QColor(0, 0, 0, 0));
        v.setColorAt(0.80, QColor(0, 0, 0, 0));
        v.setColorAt(1.0, QColor(0, 0, 0, 70));
        q.fillRect(0, 0, 720, 720, v);
        facePix = QPixmap::fromImage(im);
    }
    painter.drawPixmap(0, 0, facePix);
    ui.endFrame();
}

// A message box for a moment, as the scope reports "Auto-scale".
void MainWindow::paintToast() {
    if (clock.elapsed() < toastUntil && !toastText.isEmpty()) {
        QString t = ui.fit(toastText, 14, 640);
        int w = t.size() * ui.chW(14) + 28;
        QRectF r(std::floor((CW - w) / 2), pg == PG_TRACK ? 398 : 86, w, 30); // on the Track page, clear of the bars
        ui.frame(r.adjusted(-4, -4, 4, 4), INK, BG);
        ui.tag(r, t, 14);
    }
}

void MainWindow::paintTabs() {
    qreal w = 688.0 / PG_COUNT;
    for (int i = 0; i < PG_COUNT; ++i) // one joined menu strip: neighbours share their edge
        ui.button(QRectF(std::floor(16 + i * w), 8, std::floor(16 + (i + 1) * w) - std::floor(16 + i * w) + 1, 46), QString("tab_") + pageName(i), (i == PG_CHAT && chat.unread > 0 ? QString("Chat %1").arg(qMin(chat.unread, 9)) : QString(pageTitle(i))), pg == i,
               [this, i] { goPage(i); });
}
