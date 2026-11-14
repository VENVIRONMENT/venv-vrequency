// venv-vrequency — recording and listening back (see recorder.h).
#include "app/shell/recorder.h"
#include "app/store/store.h"
#include "audio/audio.h"
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

Recorder::Recorder(Shell &s) : Part(s) {
    QObject::connect(&drainTimer, &QTimer::timeout, &drainTimer, [this] { drain(); });
}

Recorder::~Recorder() = default;

void Recorder::shutdown() {
    if (wav.isOpen()) stop();
    if (playingBack()) { player->kill(); player->waitForFinished(500); }
}

int Recorder::seconds() const { return wav.frames() / sh.audio.sampleRate(); }

QString Recorder::start() {
    store::ensureDirs();
    QString path = store::baseDir() + "/recordings/" + store::timestampName("vrequency", "wav");
    if (!wav.open(path, sh.audio.sampleRate())) { sh.toast("Could not create WAV file"); return QString(); }
    float junk[1024];
    while (inst.readRecorded(junk, 512) > 0) {} // discard stale frames
    inst.setRecording(true);
    drainTimer.start(40);
    return path;
}

QString Recorder::stop() {
    inst.setRecording(false);
    drainTimer.stop();
    drain();
    lastRecording = wav.path();
    double secs = wav.frames() / (double)sh.audio.sampleRate();
    wav.close();
    sh.toast(QString("Saved %1 (%2 s)").arg(QFileInfo(lastRecording).fileName()).arg(secs, 0, 'f', 1), 2600);
    return lastRecording;
}

// Listening back: the newest recording is played by PipeWire's own player (or ALSA's), so it
// comes out of the same speaker as the synth.
bool Recorder::playingBack() const { return player && player->state() != QProcess::NotRunning; }

bool Recorder::togglePlayback() {
    if (playingBack()) { player->kill(); sh.toast("Playback stopped", 900); return true; }
    if (wav.isOpen()) { sh.toast("Stop recording first"); return false; }
    QString path = lastRecording;
    if (path.isEmpty() || !QFile::exists(path)) {
        const QStringList all = store::listFiles("recordings", "wav");
        path = all.isEmpty() ? QString() : all.first();
    }
    if (path.isEmpty()) { sh.toast("Nothing recorded yet"); return false; }
    if (!player) {
        player = std::make_unique<QProcess>();
        QObject::connect(player.get(), &QProcess::finished, player.get(), [this](int, QProcess::ExitStatus) { sh.redraw(); });
        QObject::connect(player.get(), &QProcess::errorOccurred, player.get(), [this](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) sh.toast("No audio player found (pw-play or aplay)", 2600);
            sh.redraw();
        });
    }
    lastRecording = path;
    const bool pw = !QStandardPaths::findExecutable("pw-play").isEmpty();
    player->start(pw ? "pw-play" : "aplay", pw ? QStringList{path} : QStringList{"-q", path});
    sh.toast(QString("Playing %1").arg(QFileInfo(path).fileName()), 1600);
    return true;
}

void Recorder::toggle() {
    if (wav.isOpen()) stop();
    else if (!start().isEmpty()) sh.toast("Recording audio…", 1200);
}

void Recorder::drain() {
    int n;
    while ((n = inst.readRecorded(buf.data(), 4096)) > 0) wav.write(buf.data(), n);
}
