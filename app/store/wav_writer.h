// venv-vrequency — a WAV file written as the audio arrives: recordings.
#pragma once
#include <QFile>
#include <QString>

// Streaming 16-bit stereo WAV writer.
class WavWriter {
public:
    bool open(const QString &path, int rate);
    void write(const float *interleaved, int frames);
    void close();
    bool isOpen() const { return f.isOpen(); }
    qint64 frames() const { return nFrames; }
    QString path() const { return f.fileName(); }
private:
    QFile f;
    qint64 nFrames = 0;
    int rate = 48000;
    void header();
};
