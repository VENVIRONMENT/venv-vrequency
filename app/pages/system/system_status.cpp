// venv-vrequency — the System page's saved sessions and its status box.
#include "app/pages/system/system_page.h"
#include "app/shell/recorder.h"
#include "app/shell/sink_probe.h"
#include "app/store/store.h"
#include "app/ui/palette.h"
#include "audio/audio.h"
#include <QFileInfo>

using namespace td;
using namespace pal;

void SystemPage::paintSessions() {
    ui.section(284, QString("Saved sessions  %1  tap to load").arg(sessions.size()));
    for (int k = 0; k < 4; ++k) {
        QRectF r(16 + (k % 2) * 348, 308 + (k / 2) * 54, 340, 48);
        if (k >= sessions.size()) { ui.frame(r, GRAT); continue; }
        QString path = sessions[k];
        ui.button(r, QString("session%1").arg(k), QFileInfo(path).completeBaseName(), false, [this, path] {
            sh.toast(sh.loadSession(path) ? QString("Loaded %1: press play on the loop / backing").arg(QFileInfo(path).completeBaseName()) : QString("Could not read that session"), 2600);
        });
    }
}

void SystemPage::paintStatus() {
    const SinkProbe &sink = sh.sinkNow();
    AudioOut &audio = sh.audio;
    QRectF ip(16, 424, 688, 206);
    ui.frame(ip, LOW, BG);
    ui.tag(QRectF(ip.x() + 1, ip.y() + 1, 62, 18), "Status", 12);
    QStringList L;
    const QString output = sink.description.isEmpty() ? QString("unknown") : sink.description;
    L << QString("Audio    %1, %2 Hz, %3-frame blocks (%4 ms)").arg(QString::fromStdString(audio.backend())).arg(audio.sampleRate()).arg(audio.quantum()).arg(1000.0 * audio.quantum() / audio.sampleRate(), 0, 'f', 1);
    L << QString("Output   %1%2").arg(output.left(52)).arg(audio.streaming() ? "" : " (idle)");
    L << (sink.bluetooth ? QString("         Bluetooth: about 0.2 s behind your fingers") : QString());
    L << QString("Engine   load %1%, peak %2%, %3 late blocks, %4 voices").arg((int)(audio.load() * 100)).arg((int)(audio.peakLoad() * 100)).arg(audio.overruns()).arg(u.voices);
    L << QString("Files    %1").arg(store::baseDir());
    const QString last = sh.recorderNow().lastPath();
    L << QString("Last WAV %1").arg(last.isEmpty() ? QString("-") : QFileInfo(last).fileName());
    L << QString("Keys     edit keymap.json in the folder above to remap");
    for (int i = 0; i < L.size(); ++i)
        ui.txt(QRectF(32, 448 + i * 22, 656, 20), ui.fit(L[i], 13, 656), 13, i == 2 ? MID : INK, Qt::AlignLeft | Qt::AlignVCenter);
    if (sink.dummy)
        ui.tag(QRectF(32, 448 + 7 * 22 + 2, 560, 20), "No real audio device: connect headphones or a speaker.", 13, Qt::AlignLeft | Qt::AlignVCenter);
}
