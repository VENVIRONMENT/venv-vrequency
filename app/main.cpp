// venv-vrequency — pocket string synthesiser for the Hackberry Pi.
#include "audio.h"
#include "instrument.h"
#include "control.h"
#include "mainwindow.h"
#include <QApplication>
#include <QTemporaryDir>
#include <chrono>
#include <cstdio>
#include <cstring>

static std::chrono::steady_clock::time_point g_start = std::chrono::steady_clock::now();
qint64 msSinceStart() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - g_start).count();
}

int runSelfTest(MainWindow &w, td::Instrument &inst, AudioOut &audio, const QString &shotsDir);
int renderShots(MainWindow &w, td::Instrument &inst, const QString &dir);

int main(int argc, char **argv) {
    bool windowed = false, selftest = false, keytest = false, control = true;
    QString shots, render, prefer = "auto";
    for (int i = 1; i < argc; ++i) {
        QString a = argv[i];
        if (a == "--windowed") windowed = true;
        else if (a == "--selftest") selftest = true;
        else if (a == "--keytest") keytest = true;
        else if (a == "--no-control") control = false;
        else if (a == "--shots" && i + 1 < argc) shots = argv[++i];
        else if (a == "--render" && i + 1 < argc) render = argv[++i];
        else if (a == "--audio" && i + 1 < argc) prefer = argv[++i];
        else if (a == "--version") { std::printf("vrequency 0.12.0\n"); return 0; }
        else if (a == "--help" || a == "-h") {
            std::printf("venv-vrequency - pocket string synthesiser\n\n"
                        "  vrequency               start full screen\n"
                        "  vrequency --windowed    start in a 720x720 window\n"
                        "  vrequency --keytest     open on the keyboard test screen\n"
                        "  vrequency --no-control  do not open the control socket (copilot / MCP access)\n"
                        "  vrequency --audio X     force a backend: pipewire | alsa | null\n"
                        "  vrequency --selftest [--shots DIR]   run the built-in checks (uses a temporary data folder)\n"
                        "  vrequency --render DIR  write pictures of every page, then exit\n");
            return 0;
        }
    }
    QApplication app(argc, argv);
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
            rc = renderShots(w, inst, render);
        } else {
            if (windowed) w.show(); else w.showFullScreen();
            rc = app.exec();
        }
    }
    audio.stop();
    return rc;
}
