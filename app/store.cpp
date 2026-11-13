#include "store.h"
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QtEndian>
#include <cmath>

namespace store {

QString baseDir() {
    QByteArray env = qgetenv("TENDRIL_HOME");
    if (!env.isEmpty()) return QString::fromLocal8Bit(env);
    return QDir::homePath() + "/Tendril";
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

QJsonObject patchToJson(const td::Patch &p) {
    QJsonObject o, params;
    o["name"] = QString::fromUtf8(p.name);
    for (int i = 0; i < td::P_COUNT; ++i) params[td::paramInfo(i).key] = (double)p.p[i];
    o["params"] = params;
    return o;
}

td::Patch patchFromJson(const QJsonObject &o) {
    td::Patch p;
    p.setName(o["name"].toString("Untitled").toUtf8().constData());
    QJsonObject params = o["params"].toObject();
    for (int i = 0; i < td::P_COUNT; ++i) {
        const td::ParamInfo &m = td::paramInfo(i);
        if (params.contains(m.key)) p.p[i] = (float)params[m.key].toDouble(m.def);
    }
    p.clamp();
    return p;
}

QJsonObject settingsToJson(const td::Settings &s) {
    QJsonObject o;
    o["root"] = s.tuning.root; o["scale"] = s.tuning.scale; o["octave"] = s.tuning.octave;
    o["rowInterval"] = s.tuning.rowInterval;
    o["bpm"] = s.bpm; o["volume"] = s.volume; o["xyMap"] = s.xyMap; o["xyHold"] = s.xyHold;
    o["backMode"] = s.backMode; o["backStyle"] = s.backStyle; o["chordBeats"] = s.chordBeats;
    o["arpRate"] = s.arpRate; o["seventh"] = s.seventh; o["metro"] = s.metro;
    o["bendRange"] = s.bendRange; o["bendTime"] = s.bendTime; o["glideScale"] = s.glideScale;
    o["backVol"] = s.backVol; o["loopVol"] = s.loopVol; o["latencyMs"] = s.latencyMs;
    return o;
}

td::Settings settingsFromJson(const QJsonObject &o) {
    td::Settings s;
    s.tuning.root = qBound(0, o["root"].toInt(s.tuning.root), 11);
    s.tuning.scale = qBound(0, o["scale"].toInt(s.tuning.scale), td::SC_COUNT - 1);
    s.tuning.octave = qBound(-3, o["octave"].toInt(0), 3);
    s.tuning.rowInterval = qBound(1, o["rowInterval"].toInt(5), 12);
    s.bpm = (float)qBound(40.0, o["bpm"].toDouble(s.bpm), 240.0);
    s.volume = (float)qBound(0.0, o["volume"].toDouble(s.volume), 1.0);
    s.xyMap = qBound(0, o["xyMap"].toInt(0), td::XY_COUNT - 1);
    s.xyHold = o["xyHold"].toBool(false);
    s.backMode = qBound(0, o["backMode"].toInt(0), td::BM_COUNT - 1);
    s.backStyle = qBound(0, o["backStyle"].toInt(0), td::BS_COUNT - 1);
    s.chordBeats = qBound(1, o["chordBeats"].toInt(4), 8);
    s.arpRate = qBound(1, o["arpRate"].toInt(2), 4);
    s.seventh = o["seventh"].toBool(false);
    s.metro = o["metro"].toBool(false);
    s.bendRange = (float)qBound(1.0, o["bendRange"].toDouble(2), 12.0);
    s.bendTime = (float)qBound(10.0, o["bendTime"].toDouble(120), 1000.0);
    s.glideScale = (float)qBound(0.0, o["glideScale"].toDouble(1), 4.0);
    s.backVol = (float)qBound(0.0, o["backVol"].toDouble(0.7), 1.0);
    s.loopVol = (float)qBound(0.0, o["loopVol"].toDouble(0.9), 1.0);
    s.latencyMs = (float)qBound(0.0, o["latencyMs"].toDouble(0), 500.0);
    return s;
}

QJsonObject sessionToJson(const td::SessionData &s) {
    QJsonObject o;
    o["format"] = "tendril-session-1";
    o["settings"] = settingsToJson(s.settings);
    o["lead"] = patchToJson(s.lead);
    o["backing"] = patchToJson(s.backing);
    QJsonArray prog;
    for (int i = 0; i < s.progLen; ++i) prog.append(s.prog[i]);
    o["progression"] = prog;
    o["holdChord"] = s.holdChord;
    o["droneMask"] = s.droneMask;
    o["loopBeats"] = s.loopBeats;
    QJsonArray layers;
    for (const td::LayerData &ld : s.layers) {
        QJsonObject lo;
        QJsonArray pats, evs;
        for (const td::Patch &p : ld.patches) pats.append(patchToJson(p));
        for (const td::LoopEv &e : ld.ev) {
            QJsonArray a; // compact: [beat, op, slot, a, b]
            a.append(e.beat); a.append((int)e.op); a.append((int)e.slot); a.append((double)e.a); a.append((double)e.b);
            evs.append(a);
        }
        lo["patches"] = pats; lo["events"] = evs;
        layers.append(lo);
    }
    o["layers"] = layers;
    return o;
}

bool sessionFromJson(const QJsonObject &o, td::SessionData &s) {
    if (o["format"].toString() != "tendril-session-1") return false;
    s.settings = settingsFromJson(o["settings"].toObject());
    s.lead = patchFromJson(o["lead"].toObject());
    s.backing = patchFromJson(o["backing"].toObject());
    QJsonArray prog = o["progression"].toArray();
    s.progLen = qMin((int)prog.size(), td::kProgMax);
    for (int i = 0; i < s.progLen; ++i) s.prog[i] = prog[i].toInt();
    s.holdChord = o["holdChord"].toInt(-1);
    s.droneMask = o["droneMask"].toInt(0);
    s.loopBeats = o["loopBeats"].toDouble(0);
    s.layers.clear();
    uint32_t seq = 0;
    for (const QJsonValue &lv : o["layers"].toArray()) {
        QJsonObject lo = lv.toObject();
        td::LayerData ld;
        for (const QJsonValue &pv : lo["patches"].toArray()) ld.patches.push_back(patchFromJson(pv.toObject()));
        for (const QJsonValue &ev : lo["events"].toArray()) {
            QJsonArray a = ev.toArray();
            if (a.size() < 5) continue;
            td::LoopEv e;
            e.beat = a[0].toDouble(); e.op = (uint8_t)a[1].toInt(); e.slot = (uint8_t)a[2].toInt();
            e.a = (float)a[3].toDouble(); e.b = (float)a[4].toDouble(); e.seq = seq++;
            if (e.beat < 0 || e.beat > td::kMaxLoopBeats || e.op > td::OP_PARAM) continue;
            ld.ev.push_back(e);
        }
        s.layers.push_back(std::move(ld));
    }
    return true;
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

QString savePatch(const QString &subdir, const td::Patch &p) {
    ensureDirs();
    QString base = QString::fromUtf8(p.name).toLower();
    QString safe;
    for (QChar c : base) safe += (c.isLetterOrNumber() ? c : QChar('-'));
    if (safe.isEmpty()) safe = "sound";
    QString dir = baseDir() + "/" + subdir + "/";
    QString path = dir + safe + ".json";
    for (int n = 2; QFile::exists(path); ++n) path = dir + safe + "-" + QString::number(n) + ".json";
    return writeJson(path, patchToJson(p)) ? path : QString();
}

} // namespace store

// ---------------------------------------------------------------- WAV

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
