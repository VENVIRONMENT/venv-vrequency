// venv-vrequency — files on disk: patches, favourites, sessions, recordings.
//
//   ~/vrequency/                 (or $VREQUENCY_HOME)
//     state.json               last settings, the ten sound slots, current sound
//     keymap.json              editable key bindings
//     favourites/*.json        saved sounds
//     sessions/*.json          saved sessions (settings, sounds, backing, loops)
//     recordings/*.wav         exported audio
#pragma once
#include "instrument.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace store {

QString baseDir();
void ensureDirs();
QString timestampName(const QString &prefix, const QString &ext);

QJsonObject patchToJson(const td::Patch &p);
td::Patch patchFromJson(const QJsonObject &o);
QJsonObject settingsToJson(const td::Settings &s);
td::Settings settingsFromJson(const QJsonObject &o);
QJsonObject sessionToJson(const td::SessionData &s);
bool sessionFromJson(const QJsonObject &o, td::SessionData &s);
QJsonArray beatToJson(const uint16_t *beat);
void beatFromJson(const QJsonArray &a, uint16_t *beat);
QJsonObject trackToJson(const td::TrackData &t);
void trackFromJson(const QJsonObject &o, td::TrackData &t); // anything damaged is left out

bool writeJson(const QString &path, const QJsonObject &o);
bool readJson(const QString &path, QJsonObject &o);

struct NamedPatch { QString file; td::Patch patch; };
QVector<NamedPatch> listPatches(const QString &subdir);
// Returns the file path ("" on failure). A name already taken gets "-2", "-3"...; with `overwrite` a file
// holding a sound of exactly the same name is replaced instead, and `*replaced` says so.
QString savePatch(const QString &subdir, const td::Patch &p, bool overwrite = false, bool *replaced = nullptr);
QStringList listFiles(const QString &subdir, const QString &ext); // newest first

} // namespace store

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
