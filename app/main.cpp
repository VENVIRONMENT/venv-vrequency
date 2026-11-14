// venv-vrequency — pocket string synthesiser for the Hackberry Pi.
#include "audio/audio.h"
#include "core/instrument/instrument.h"
#include "app/control/control.h"
#include "app/selftest/selftest.h"
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QTemporaryDir>
#include <chrono>
#include <cstdio>
#include <cstring>

static std::chrono::steady_clock::time_point g_start = std::chrono::steady_clock::now();
qint64 msSinceStart() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - g_start).count();
}

int tourDumpZones(const QString &pictureDir);
int renderTourShots(MainWindow &w, const QString &dir);

int main(int argc, char **argv) {
    bool windowed = false, selftest = false, keytest = false, control = true, dumpZones = false;
    QString shots, render, dumpDir, prefer = "auto";
    for (int i = 1; i < argc; ++i) {
        QString a = argv[i];
        if (a == "--windowed") windowed = true;
        else if (a == "--selftest") selftest = true;
        else if (a == "--keytest") keytest = true;
        else if (a == "--no-control") control = false;
        else if (a == "--dump-zones") { dumpZones = true; if (i + 1 < argc && argv[i + 1][0] != '-') dumpDir = argv[++i]; }
        else if (a == "--shots" && i + 1 < argc) shots = argv[++i];
        else if (a == "--render" && i + 1 < argc) render = argv[++i];
        else if (a == "--audio" && i + 1 < argc) prefer = argv[++i];
        else if (a == "--version") { std::printf("vrequency 0.13.0\n"); return 0; }
        else if (a == "--help" || a == "-h") {
            std::printf("venv-vrequency - pocket string synthesiser\n\n"
                        "  vrequency               start full screen\n"
                        "  vrequency --windowed    start in a 720x720 window\n"
                        "  vrequency --keytest     open on the keyboard test screen\n"
                        "  vrequency --no-control  do not open the control socket (copilot / MCP access)\n"
                        "  vrequency --audio X     force a backend: pipewire | alsa | null\n"
                        "  vrequency --selftest [--shots DIR]   run the built-in checks (uses a temporary data folder)\n"
                        "  vrequency --render DIR  write pictures of every page, then exit\n"
                        "  vrequency --dump-zones [DIR]  print every touch zone of every tour page; with DIR also the pictures (developer; QT_QPA_PLATFORM=offscreen)\n");
            return 0;
        }
    }
    QApplication app(argc, argv);
    // the bundled pixel fonts (assets/fonts, CC BY-SA 4.0, Andrew Gleeson)
    QFontDatabase::addApplicationFont(":/fonts/MGS1 Codec.ttf");
    QFontDatabase::addApplicationFont(":/fonts/MGS1 HUD.ttf");
    if (dumpZones) return tourDumpZones(dumpDir);   // draws off-screen in a scratch copy: no audio, no files
    app.setApplicationName("venv-vrequency");
    app.setDesktopFileName("vrequency");

    QTemporaryDir tmp; // self-test never touches the player's real files
    if (selftest || !render.isEmpty()) qputenv("VREQUENCY_HOME", tmp.path().toLocal8Bit());

    td::Instrument inst(48000.0f);
    AudioOut audio;
    audio.start([&inst](float *l, float *r, int n) { inst.process(l, r, n); }, 48000, 256, prefer.toStdString());
    if (!audio.lastError().empty()) std::fprintf(stderr, "vrequency: audio: %s\n", audio.lastError().c_str());

    int rc;
    {
        MainWindow w(inst, audio);
        if (keytest) w.setKeyTest(true);
        ControlServer ctl(w, inst); // the copilot's socket; the self-test starts its own
        if (control && !selftest && render.isEmpty() && !ctl.start())
            std::fprintf(stderr, "vrequency: control socket: %s\n", ctl.lastError().toUtf8().constData());
        if (selftest) {
            w.show();
            rc = runSelfTest(w, inst, audio, shots);
        } else if (!render.isEmpty()) {
            w.show();
            if (qEnvironmentVariableIsSet("VREQUENCY_TOUR_SHOTS_ONLY")) { QDir().mkpath(render); rc = renderTourShots(w, render); } // developer shortcut
            else { rc = renderShots(w, inst, render); renderTourShots(w, render); }
        } else {
            w.tour().armFirstRunPrompt(1500);   // first run only: an offer, never a block
            if (windowed) w.show(); else w.showFullScreen();
            rc = app.exec();
        }
    }
    audio.stop();
    return rc;
}
