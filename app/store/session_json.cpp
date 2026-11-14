// venv-vrequency — sounds, settings, the beat and a whole session as JSON (see store.h).
#include "app/store/store.h"
#include <QJsonArray>
#include <algorithm>
#include <cmath>

namespace store {

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

QJsonArray layersToJson(const std::vector<td::LayerData> &src) {
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

std::vector<td::LayerData> layersFromJson(const QJsonArray &src) {
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
} // namespace store
