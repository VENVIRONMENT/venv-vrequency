// venv-vrequency — the output device probe (see sink_probe.h).
#include "app/shell/sink_probe.h"
#include <QProcess>
#include <QTimer>

void SinkProbe::start() {
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] { probe(); });
    timer->start(4000);
    probe();
}

void SinkProbe::probe() {
    auto *pr = new QProcess(this);
    connect(pr, &QProcess::finished, this, [this, pr](int, QProcess::ExitStatus) {
        QString out = QString::fromUtf8(pr->readAllStandardOutput());
        QString desc; bool dummy = false, bt = false;
        for (const QString &ln : out.split('\n')) {
            if (ln.contains("node.description")) desc = ln.section('"', 1, 1);
            if (ln.contains("node.name") && ln.contains("auto_null")) dummy = true;
            if (ln.contains("node.name") && ln.contains("bluez_output")) bt = true;
        }
        if (!out.isEmpty()) {
            if (bt != bluetooth || generation == 0) ++generation;
            description = desc; this->dummy = dummy; bluetooth = bt;
        }
        pr->deleteLater();
    });
    connect(pr, &QProcess::errorOccurred, this, [pr](QProcess::ProcessError) { pr->deleteLater(); });
    pr->start("wpctl", {"inspect", "@DEFAULT_AUDIO_SINK@"});
}
