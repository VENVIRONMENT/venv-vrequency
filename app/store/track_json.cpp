// venv-vrequency — the track as JSON (see store.h).
#include "app/store/store.h"
#include <QJsonArray>
#include <algorithm>
#include <cmath>

namespace store {

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

} // namespace store
