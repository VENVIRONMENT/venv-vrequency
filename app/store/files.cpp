// venv-vrequency — the folder the app keeps its files in, and reading and writing them (see store.h).
#include "app/store/store.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace store {

QString baseDir() {
    QByteArray env = qgetenv("VREQUENCY_HOME");
    if (!env.isEmpty()) return QString::fromLocal8Bit(env);
    return QDir::homePath() + "/vrequency";
}

void ensureDirs() {
    QDir d(baseDir());
    d.mkpath("favourites");
    d.mkpath("sessions");
    d.mkpath("recordings");
}

QString timestampName(const QString &prefix, const QString &ext) {
    return prefix + "-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + "." + ext;
}

bool writeJson(const QString &path, const QJsonObject &o) {
    QSaveFile f(path); // atomic: never leaves a half-written file
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return f.commit();
}

bool readJson(const QString &path, QJsonObject &o) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonParseError err;
    QJsonDocument d = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !d.isObject()) return false;
    o = d.object();
    return true;
}

QStringList listFiles(const QString &subdir, const QString &ext) {
    QDir d(baseDir() + "/" + subdir);
    QStringList out;
    for (const QFileInfo &fi : d.entryInfoList({"*." + ext}, QDir::Files, QDir::Time)) out << fi.absoluteFilePath();
    return out;
}

QVector<NamedPatch> listPatches(const QString &subdir) {
    QVector<NamedPatch> out;
    for (const QString &path : listFiles(subdir, "json")) {
        QJsonObject o;
        if (readJson(path, o)) out.push_back({path, patchFromJson(o)});
    }
    return out;
}

QString savePatch(const QString &subdir, const td::Patch &p, bool overwrite, bool *replaced) {
    ensureDirs();
    QString base = QString::fromUtf8(p.name).toLower();
    QString safe;
    for (QChar c : base) safe += (c.isLetterOrNumber() ? c : QChar('-'));
    if (safe.isEmpty()) safe = "sound";
    QString dir = baseDir() + "/" + subdir + "/";
    QString path = dir + safe + ".json";
    if (replaced) *replaced = false;
    for (int n = 2; QFile::exists(path); ++n) {
        if (overwrite) { // the same name as the sound already in that file: update it in place
            QJsonObject o;
            if (readJson(path, o) && QString::fromUtf8(patchFromJson(o).name) == QString::fromUtf8(p.name)) { if (replaced) *replaced = true; break; }
        }
        path = dir + safe + "-" + QString::number(n) + ".json";
    }
    return writeJson(path, patchToJson(p)) ? path : QString();
}

} // namespace store
