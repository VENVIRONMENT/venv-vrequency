// venv-vrequency — a WAV file written as the audio arrives (see wav_writer.h).
#include "app/store/wav_writer.h"
#include <QtEndian>
#include <cmath>

void WavWriter::header() {
    quint32 dataBytes = (quint32)(nFrames * 4);
    char h[44];
    memcpy(h, "RIFF", 4); qToLittleEndian<quint32>(36 + dataBytes, h + 4);
    memcpy(h + 8, "WAVEfmt ", 8); qToLittleEndian<quint32>(16, h + 16);
    qToLittleEndian<quint16>(1, h + 20); qToLittleEndian<quint16>(2, h + 22);
    qToLittleEndian<quint32>((quint32)rate, h + 24); qToLittleEndian<quint32>((quint32)rate * 4, h + 28);
    qToLittleEndian<quint16>(4, h + 32); qToLittleEndian<quint16>(16, h + 34);
    memcpy(h + 36, "data", 4); qToLittleEndian<quint32>(dataBytes, h + 40);
    f.seek(0);
    f.write(h, 44);
}

bool WavWriter::open(const QString &path, int rate_) {
    close();
    rate = rate_; nFrames = 0;
    f.setFileName(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    header();
    return true;
}

void WavWriter::write(const float *x, int frames) {
    if (!f.isOpen() || frames <= 0) return;
    QByteArray buf(frames * 4, Qt::Uninitialized);
    qint16 *d = (qint16 *)buf.data();
    for (int i = 0; i < frames * 2; ++i) {
        float v = x[i] < -1 ? -1 : (x[i] > 1 ? 1 : x[i]);
        d[i] = qToLittleEndian<qint16>((qint16)std::lrint(v * 32767.0f));
    }
    f.write(buf);
    nFrames += frames;
}

void WavWriter::close() {
    if (!f.isOpen()) return;
    header();
    f.close();
}
