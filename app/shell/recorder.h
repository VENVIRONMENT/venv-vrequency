// venv-vrequency — recording what you hear to a WAV file, and playing the newest recording back.
#pragma once
#include "app/shell/part.h"
#include "app/store/store.h"
#include <QTimer>
#include <array>
#include <memory>

class QProcess;

class Recorder : public Part {
public:
    explicit Recorder(Shell &s);
    ~Recorder();

    QString start();                         // the file being written, or empty when it could not be made
    QString stop();                          // the file written
    void toggle();
    bool recording() const { return wav.isOpen(); }
    int seconds() const;                     // whole seconds recorded so far
    bool togglePlayback();                   // play the newest recording back, or stop it; false = nothing to play
    bool playingBack() const;
    QString lastPath() const { return lastRecording; }
    void setLastPath(const QString &path) { lastRecording = path; }   // an exported track is the newest recording
    void shutdown();                         // the window is closing: finish the file, stop the player

private:
    WavWriter wav;
    QString lastRecording;
    QTimer drainTimer;
    std::unique_ptr<QProcess> player;        // plays a recording back
    std::array<float, 4096 * 2> buf;

    void drain();
};
