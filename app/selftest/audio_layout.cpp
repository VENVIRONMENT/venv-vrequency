// venv-vrequency self-test — the audio output runs, and every page fits the screen and redraws in time.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "audio/audio.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QScreen>
#include <cstdio>

using namespace td;

namespace selftest {
void testAudio(Context &ctx) {
    Instrument &inst = ctx.inst; AudioOut &audio = ctx.audio;
    std::printf("- audio\n");
    check(audio.backend() != "none (silent)", QString("audio backend opened: %1").arg(QString::fromStdString(audio.backend())));
    uint64_t f0 = inst.uiState().frames; wait(500); uint64_t f1 = inst.uiState().frames;
    double rate = (f1 - f0) / 0.5;
    check(rate > 40000 && rate < 56000, QString("audio callback runs in real time (%1 frames/s)").arg((int)rate));
}

void testLayout(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst; const QString &shotsDir = ctx.shotsDir;
    std::printf("- layout\n");
    {
        QScreen *sc = w.screen();
        std::printf("  info  screen %dx%d logical, device pixel ratio %.2f, window %dx%d, platform %s\n", sc->geometry().width(), sc->geometry().height(),
                    sc->devicePixelRatio(), w.width(), w.height(), QGuiApplication::platformName().toUtf8().constData());
    }
    QImage img = w.shot();
    check(img.width() == 720 && img.height() == 720, "interface renders at 720x720");
    static const char *files[PG_COUNT] = {"0-play", "1-sound", "9-pulse", "3-tune", "4-system", "5-help", "14-chat"};
    if (!shotsDir.isEmpty()) QDir().mkpath(shotsDir);
    for (int p = 0; p < PG_COUNT; ++p) {
        w.setPage(p);
        wait(120);
        QImage im = w.shot();
        bool inside = true; int count = 0;
        for (const QString &z : w.zoneNames()) { // every control on the page, not a sample
            QRectF r = w.zoneRect(z);
            ++count;
            if (r.left() < 0 || r.top() < 0 || r.right() > 720 || r.bottom() > 720) inside = false;
        }
        check(inside && count > 5, QString("%1 page: all %2 controls fit inside 720x720 (no scrolling)").arg(pageName(p)).arg(count));
        if (!shotsDir.isEmpty()) im.save(QString("%1/%2.png").arg(shotsDir).arg(files[p]));
    }
    w.setPage(PG_PLAY);
    wait(100);
    {
        QElapsedTimer pt; pt.start();
        for (int i = 0; i < 40; ++i) w.shot();
        std::printf("  info  play page redraw: %.1f ms per frame\n", pt.elapsed() / 40.0);
    }
    { // a busy wave on the scope once cost 40 to 60 ms a frame: a whole processor core at 25 frames a second
        key(w, Qt::Key_Z, true); key(w, Qt::Key_S, true); key(w, Qt::Key_E, true); wait(300);
        double worst = 0; int worstPage = 0;
        for (int p = 0; p < PG_COUNT; ++p) {
            w.setPage(p); w.shot();
            QElapsedTimer t; t.start();
            for (int i = 0; i < 20; ++i) w.shot();
            const double ms = t.nsecsElapsed() / 20e6;
            std::printf("  info  %s page redraw with sound: %.1f ms\n", pageName(p), ms);
            if (ms > worst) { worst = ms; worstPage = p; }
        }
        key(w, Qt::Key_E, false); key(w, Qt::Key_S, false); key(w, Qt::Key_Z, false);
        inst.cmd(EV_PANIC); // no release tails left for the checks that follow
        w.setPage(PG_PLAY); wait(400);
        check(worst < 25.0, QString("with three notes sounding every page redraws well inside a 40 ms frame (slowest: %1, %2 ms)").arg(pageTitle(worstPage)).arg(worst, 0, 'f', 1));
    }
}
} // namespace selftest
