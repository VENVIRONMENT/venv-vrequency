#include "store.h"
#include <algorithm>
#include <cmath>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QtEndian>
#include <cmath>

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
    o["kit"] = s.kit; o["beatVol"] = s.beatVol;
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
    s.kit = qBound(0, o["kit"].toInt(0), td::kKits - 1);
    s.beatVol = (float)qBound(0.0, o["beatVol"].toDouble(0.7), 1.0);
    return s;
}

// The beat: one string of sixteen characters per drum, "x" for a hit and "." for a rest.
QJsonArray beatToJson(const uint16_t *beat) {
    QJsonArray a;
    for (int d = 0; d < td::kDrums; ++d) {
        QString row;
        for (int s = 0; s < td::kDrumSteps; ++s) row += (beat[d] >> s) & 1 ? 'x' : '.';
        a.append(row);
    }
    return a;
}

void beatFromJson(const QJsonArray &a, uint16_t *beat) {
    for (int d = 0; d < td::kDrums; ++d) {
        beat[d] = 0;
        const QString row = d < a.size() ? a[d].toString() : QString();
        for (int s = 0; s < td::kDrumSteps && s < row.size(); ++s)
            if (row[s] == 'x') beat[d] |= (uint16_t)(1u << s);
    }
}

static QJsonArray layersToJson(const std::vector<td::LayerData> &src) {
    QJsonArray layers;
    for (const td::LayerData &ld : src) {
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
    return layers;
}

static std::vector<td::LayerData> layersFromJson(const QJsonArray &src) {
    std::vector<td::LayerData> out;
    uint32_t seq = 0;
    for (const QJsonValue &lv : src) {
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
        out.push_back(std::move(ld));
    }
    return out;
}

QJsonObject sessionToJson(const td::SessionData &s) {
    QJsonObject o;
    o["format"] = "vrequency-session-1";
    o["settings"] = settingsToJson(s.settings);
    o["lead"] = patchToJson(s.lead);
    o["backing"] = patchToJson(s.backing);
    QJsonArray prog;
    for (int i = 0; i < s.progLen; ++i) prog.append(s.prog[i]);
    o["progression"] = prog;
    o["holdChord"] = s.holdChord;
    o["droneMask"] = s.droneMask;
    o["loopBeats"] = s.loopBeats;
    o["layers"] = layersToJson(s.layers);
    o["beat"] = beatToJson(s.beat);
    return o;
}

bool sessionFromJson(const QJsonObject &o, td::SessionData &s) {
    if (o["format"].toString() != "vrequency-session-1") return false;
    s.settings = settingsFromJson(o["settings"].toObject());
    s.lead = patchFromJson(o["lead"].toObject());
    s.backing = patchFromJson(o["backing"].toObject());
    QJsonArray prog = o["progression"].toArray();
    s.progLen = qMin((int)prog.size(), td::kProgMax);
    for (int i = 0; i < s.progLen; ++i) s.prog[i] = prog[i].toInt();
    s.holdChord = o["holdChord"].toInt(-1);
    s.droneMask = o["droneMask"].toInt(0);
    s.loopBeats = o["loopBeats"].toDouble(0);
    beatFromJson(o["beat"].toArray(), s.beat);
    s.layers = layersFromJson(o["layers"].toArray());
    return true;
}

// ---- the track ----

// A picked sound is written only when there is one, so older files read the same as before.
static void pickToJson(const td::SoundPick &k, QJsonObject &into) {
    if (!k.any() && !k.own) return;
    QJsonObject o;
    if (k.own) o["own"] = true;
    if (k.on) o["sound"] = patchToJson(k.patch);
    if (k.kit >= 0) o["kit"] = k.kit;
    if (k.octave != 0) o["octave"] = k.octave;
    into["pick"] = o;
}

static td::SoundPick pickFromJson(const QJsonObject &from, int clipKind) {
    td::SoundPick k;
    const QJsonObject o = from["pick"].toObject();
    if (o.isEmpty()) return k;
    k.own = o["own"].toBool(false);
    if (clipKind == td::CK_BEAT) { // a beat takes a kit and nothing else
        const int kit = o["kit"].toInt(-1);
        if (kit >= 0 && kit < td::kKits) k.kit = kit;
        return k;
    }
    if (o["sound"].isObject()) { k.on = true; k.patch = patchFromJson(o["sound"].toObject()); }
    k.octave = qBound(-2, o["octave"].toInt(0), 2);
    return k;
}

QJsonObject trackToJson(const td::TrackData &t) {
    static const char *K[] = {"beat", "chords", "loop"};
    QJsonObject o;
    o["bars"] = t.bars; o["cycleFrom"] = t.cycleFrom; o["cycleTo"] = t.cycleTo;
    QJsonArray clips;
    for (const auto &cp : t.clips) {
        const td::Clip &c = *cp;
        QJsonObject co;
        co["kind"] = K[qBound(0, c.kind, 2)];
        co["name"] = QString::fromUtf8(c.name);
        co["bars"] = c.bars;
        if (c.kind == td::CK_BEAT) {
            co["kit"] = c.kit; co["beat"] = beatToJson(c.beat);
        } else if (c.kind == td::CK_CHORDS) {
            QJsonArray prog;
            for (int i = 0; i < c.progLen; ++i) prog.append(c.prog[i]);
            co["progression"] = prog; co["holdChord"] = c.holdChord; co["droneMask"] = c.droneMask;
            co["style"] = c.backStyle; co["chordBeats"] = c.chordBeats; co["arpRate"] = c.arpRate; co["seventh"] = c.seventh;
            co["sound"] = patchToJson(c.patch);
        } else {
            co["layers"] = layersToJson(c.layers);
        }
        pickToJson(c.pick, co);
        clips.append(co);
    }
    o["clips"] = clips;
    QJsonArray lanes;
    for (int ln = 0; ln < td::kLanes; ++ln) {
        QJsonArray la;
        for (const td::Placement &p : t.lane[ln]) {
            QJsonObject po;
            po["clip"] = p.clip; po["bar"] = p.bar; po["bars"] = p.bars;
            if (p.mute) po["mute"] = true;
            QJsonArray pcs;
            for (const td::Piece &x : p.pieces) {
                QJsonArray a; a.append(x.src); a.append(x.len); a.append(x.mute ? 1 : 0);
                pcs.append(a);
            }
            po["pieces"] = pcs;
            pickToJson(p.pick, po);
            la.append(po);
        }
        lanes.append(la);
    }
    o["lanes"] = lanes;
    QJsonArray mix; // level, mute and solo of each lane, in lane order
    for (int ln = 0; ln < td::kLanes; ++ln) {
        QJsonObject mo;
        mo["level"] = t.mix[ln].level;
        if (t.mix[ln].mute) mo["mute"] = true;
        if (t.mix[ln].solo) mo["solo"] = true;
        mix.append(mo);
    }
    o["mix"] = mix;
    return o;
}

void trackFromJson(const QJsonObject &o, td::TrackData &t) {
    t = td::TrackData();
    t.bars = qBound(1, o["bars"].toInt(8), td::kTrackMaxBars);
    t.cycleFrom = qBound(0, o["cycleFrom"].toInt(0), td::kTrackMaxBars);
    t.cycleTo = qBound(0, o["cycleTo"].toInt(0), td::kTrackMaxBars);
    for (const QJsonValue &cv : o["clips"].toArray()) {
        const QJsonObject co = cv.toObject();
        td::Clip c;
        const QString kind = co["kind"].toString();
        c.kind = kind == "chords" ? td::CK_CHORDS : (kind == "loop" ? td::CK_LOOP : td::CK_BEAT);
        qstrncpy(c.name, co["name"].toString().toUtf8().constData(), sizeof c.name);
        c.bars = qBound(1, co["bars"].toInt(1), td::kTrackMaxBars);
        if (c.kind == td::CK_BEAT) {
            c.kit = qBound(0, co["kit"].toInt(0), td::kKits - 1);
            beatFromJson(co["beat"].toArray(), c.beat);
            c.bars = 1;
        } else if (c.kind == td::CK_CHORDS) {
            const QJsonArray prog = co["progression"].toArray();
            c.progLen = qMin((int)prog.size(), td::kProgMax);
            for (int i = 0; i < c.progLen; ++i) c.prog[i] = ((prog[i].toInt() % 7) + 7) % 7;
            c.holdChord = qBound(-1, co["holdChord"].toInt(-1), 6);
            c.droneMask = co["droneMask"].toInt(0) & 0x7f;
            c.backStyle = qBound(0, co["style"].toInt(0), td::BS_COUNT - 1);
            c.chordBeats = qBound(1, co["chordBeats"].toInt(4), 8);
            c.arpRate = qBound(1, co["arpRate"].toInt(2), 4);
            c.seventh = co["seventh"].toBool(false);
            c.patch = patchFromJson(co["sound"].toObject());
        } else {
            c.layers = layersFromJson(co["layers"].toArray());
            if ((int)c.layers.size() > td::kLayers) c.layers.resize(td::kLayers);
            for (td::LayerData &ld : c.layers) {
                if (ld.patches.empty()) ld.patches.push_back(td::Patch());
                std::stable_sort(ld.ev.begin(), ld.ev.end(), [](const td::LoopEv &a, const td::LoopEv &b) { return a.beat < b.beat; });
            }
        }
        c.pick = pickFromJson(co, c.kind);
        td::addClip(t, c);
    }
    const QJsonArray lanes = o["lanes"].toArray();
    for (int ln = 0; ln < td::kLanes && ln < lanes.size(); ++ln) {
        for (const QJsonValue &pv : lanes[ln].toArray()) {
            const QJsonObject po = pv.toObject();
            td::Placement p;
            p.clip = po["clip"].toInt(-1); p.bar = po["bar"].toInt(-1); p.bars = po["bars"].toInt(0);
            p.mute = po["mute"].toBool(false);
            if (p.clip < 0 || p.clip >= (int)t.clips.size()) continue;
            const td::Clip &c = *t.clips[p.clip];
            if (!td::laneTakes(ln, c.kind) || !td::laneFree(t, ln, p.bar, p.bars)) continue;
            double total = 0; bool ok = true;
            for (const QJsonValue &xv : po["pieces"].toArray()) {
                const QJsonArray a = xv.toArray();
                if (a.size() < 2) { ok = false; break; }
                td::Piece x; x.src = a[0].toDouble(); x.len = a[1].toDouble(); x.mute = a.size() > 2 && a[2].toInt() != 0;
                if (x.len < 0.124 || x.src < 0 || x.src + x.len > c.beats() + 1e-6) { ok = false; break; }
                total += x.len;
                p.pieces.push_back(x);
            }
            if (!ok || std::fabs(total - c.beats()) > 1e-6) td::resetPieces(p, c); // unreadable cuts: the clip whole
            p.pick = pickFromJson(po, c.kind);
            t.lane[ln].push_back(p);
        }
        td::sortLane(t, ln);
    }
    const QJsonArray mix = o["mix"].toArray(); // older files have none: every lane as recorded
    for (int ln = 0; ln < td::kLanes && ln < mix.size(); ++ln) {
        const QJsonObject mo = mix[ln].toObject();
        t.mix[ln].level = qBound(0, mo["level"].toInt(td::kLaneLevelNormal), 100);
        t.mix[ln].mute = mo["mute"].toBool(false);
        t.mix[ln].solo = mo["solo"].toBool(false);
    }
    for (int ln = 0; ln < td::kLanes; ++ln)
        for (const td::Placement &p : t.lane[ln]) t.bars = qMax(t.bars, p.bar + p.bars);
    if (t.cycleTo > t.bars || t.cycleTo <= t.cycleFrom) t.cycleFrom = t.cycleTo = 0;
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
