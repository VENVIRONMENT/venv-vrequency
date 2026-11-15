// venv-vrequency — files on disk: patches, favourites, sessions, recordings (written by wav_writer.h).
//
//   ~/vrequency/                 (or $VREQUENCY_HOME)
//     state.json               last settings, the ten sound slots, current sound
//     keymap.json              editable key bindings
//     favourites/*.json        saved sounds
//     sessions/*.json          saved sessions (settings, sounds, chord pads, loops, the beat)
//     recordings/*.wav         exported audio
#pragma once
#include "core/instrument/instrument.h"
#include "core/instrument/session.h"
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
QJsonArray layersToJson(const std::vector<td::LayerData> &src);          // a loop's recorded layers
std::vector<td::LayerData> layersFromJson(const QJsonArray &src);

bool writeJson(const QString &path, const QJsonObject &o);
bool readJson(const QString &path, QJsonObject &o);

struct NamedPatch { QString file; td::Patch patch; };
QVector<NamedPatch> listPatches(const QString &subdir);
// Returns the file path ("" on failure). A name already taken gets "-2", "-3"...; with `overwrite` a file
// holding a sound of exactly the same name is replaced instead, and `*replaced` says so.
QString savePatch(const QString &subdir, const td::Patch &p, bool overwrite = false, bool *replaced = nullptr);
QStringList listFiles(const QString &subdir, const QString &ext); // newest first

} // namespace store
