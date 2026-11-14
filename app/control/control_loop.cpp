// venv-vrequency — the control socket: loop, backing chords, beat and track commands.
#include "app/control/control.h"
#include "app/control/control_internal.h"
#include "app/shell/mainwindow.h"

using namespace td;
using namespace ctl;

QJsonObject ControlServer::cmdLoop(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const QString act = A.str("action", "", 12, true);
    if (!e.isEmpty()) return {};
    const int st = inst.uiState().loopState;
    if (act == "record") {
        if (st == L_COUNTIN || st == L_REC) { e = "already recording"; return {}; }
        inst.cmd(EV_LOOP, LC_PEDAL);
    } else if (act == "play") {
        if (st == L_EMPTY) { e = "nothing recorded"; return {}; }
        if (st == L_STOPPED) inst.cmd(EV_LOOP, LC_PLAYSTOP);
        else if (st == L_REC || st == L_COUNTIN) inst.cmd(EV_LOOP, LC_PEDAL); // closes the recording and plays
    } else if (act == "stop") {
        if (st == L_PLAY || st == L_OVERDUB) inst.cmd(EV_LOOP, LC_PLAYSTOP);
        else { e = "the loop is not playing"; return {}; }
    } else if (act == "clear") inst.cmd(EV_LOOP, LC_CLEAR);
    else if (act == "undo") inst.cmd(EV_LOOP, LC_UNDO);
    else { e = "action must be record, play, stop, clear or undo"; return {}; }
    QJsonObject r; r["action"] = act;
    return r;
}

QJsonObject ControlServer::cmdBacking(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const QString act = A.str("action", "", 12);
    const bool seventh = A.has("seventh") ? A.boolean("seventh", false) : inst.uiState().set.seventh;
    const int beats = A.integer("beats", 1, 8, 0);
    const QString style = A.str("style", "", 12);
    if (!e.isEmpty()) return {};
    std::vector<int> chords;
    if (a.contains("chords")) {
        if (!a["chords"].isArray() || a["chords"].toArray().isEmpty() || a["chords"].toArray().size() > kProgMax) {
            e = "'chords' must be a list of 1 to 8 chords (1 to 7 or I to VII)"; return {};
        }
        for (const QJsonValue &v : a["chords"].toArray()) {
            const int d = parseDegree(v);
            if (d < 0) { e = "a chord must be 1 to 7 or a numeral I to VII"; return {}; }
            chords.push_back(d);
        }
    } else if (act.isEmpty() && !a.contains("seventh") && !a.contains("beats") && style.isEmpty()) {
        e = "give 'chords', 'action' (play, stop, clear, remove_last), 'style', 'seventh' or 'beats'"; return {};
    }
    int styleIdx = -1;
    if (!style.isEmpty()) {
        const QString s = style.toLower().remove(' ').remove('-');
        static const char *N[] = {"sustain", "pulse", "arpup", "arpdown", "arpupdn"};
        for (int i = 0; i < BS_COUNT; ++i) if (s == N[i]) styleIdx = i;
        if (styleIdx < 0) { e = "style must be sustain, pulse, arp up, arp down or arp up-dn"; return {}; }
    }
    if (!act.isEmpty() && act != "play" && act != "stop" && act != "clear" && act != "remove_last") { e = "action must be play, stop, clear or remove_last"; return {}; }
    const UiState u = inst.uiState();
    if (a.contains("seventh")) inst.set(S_SEVENTH, seventh ? 1.0f : 0.0f);
    if (beats > 0) inst.set(S_CHORDBEATS, (float)beats);
    if (styleIdx >= 0) inst.set(S_BACKSTYLE, (float)styleIdx);
    if (!chords.empty()) {
        inst.cmd(EV_BACK, BC_CLEAR);
        inst.set(S_BACKMODE, (float)BM_PROG);
        for (int d : chords) inst.cmd(EV_CHORD, d);
    }
    if (act == "clear") inst.cmd(EV_BACK, BC_CLEAR);
    else if (act == "remove_last") inst.cmd(EV_BACK, BC_REMOVE_LAST);
    else if (act == "play" && !u.backPlaying) inst.cmd(EV_BACK, BC_PLAYSTOP);
    else if (act == "stop" && u.backPlaying) inst.cmd(EV_BACK, BC_PLAYSTOP);
    QJsonObject r; r["chords"] = (int)chords.size();
    return r;
}

QJsonObject ControlServer::cmdBeat(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const QString act = A.str("action", "", 8);
    if (!e.isEmpty()) return {};
    if (!a.contains("action") && !a.contains("kit") && !a.contains("set_step") && !a.contains("hit")) {
        e = "give 'action' (play, stop, clear), 'kit', 'set_step' or 'hit'"; return {};
    }
    if (!act.isEmpty() && act != "play" && act != "stop" && act != "clear") { e = "action must be play, stop or clear"; return {}; }
    int kit = -1;
    if (a.contains("kit")) {
        const QJsonValue v = a["kit"];
        if (v.isDouble() && v.toDouble() >= 0 && v.toDouble() < kKits && v.toDouble() == std::floor(v.toDouble())) kit = (int)v.toDouble();
        else if (v.isString()) for (int i = 0; i < kKits; ++i) if (v.toString().compare(kitName(i), Qt::CaseInsensitive) == 0) kit = i;
        if (kit < 0) { e = "kit must be soft, crunchy or glitchy"; return {}; }
    }
    struct Step { int drum, step; bool on; };
    std::vector<Step> steps;
    if (a.contains("set_step")) {
        QJsonArray arr = a["set_step"].isArray() ? a["set_step"].toArray() : QJsonArray{a["set_step"]};
        if (arr.isEmpty() || arr.size() > kDrums * kDrumSteps) { e = "'set_step' must hold 1 to 128 steps"; return {}; }
        for (const QJsonValue &v : arr) {
            if (!v.isObject()) { e = "every set_step must be an object {drum, step, on}"; return {}; }
            Args B{v.toObject(), e};
            Step s;
            s.drum = parseDrum(v.toObject()["drum"]);
            if (s.drum < 0) { e = "'drum' must be 0 to 7 or a name (kick, snare, hat, openhat, clap, rim, tom, cowbell)"; return {}; }
            s.step = B.integer("step", 0, kDrumSteps - 1, 0, true);
            s.on = B.boolean("on", true);
            if (!e.isEmpty()) return {};
            steps.push_back(s);
        }
    }
    int hit = -1;
    if (a.contains("hit")) {
        const QJsonValue v = a["hit"].isObject() ? a["hit"].toObject()["drum"] : a["hit"];
        hit = parseDrum(v);
        if (hit < 0) { e = "'hit' needs a drum: 0 to 7 or a name"; return {}; }
    }
    const UiState u = inst.uiState();
    if (kit >= 0) inst.set(S_KIT, (float)kit);
    for (const Step &s : steps) inst.beatDo(BT_STEP, s.drum, s.step, s.on ? 1 : 0);
    if (act == "clear") inst.beatDo(BT_ERASE, -1);
    else if (act == "play" && !u.beatPlaying) inst.beatDo(BT_PLAYSTOP);
    else if (act == "stop" && u.beatPlaying) inst.beatDo(BT_PLAYSTOP);
    if (hit >= 0) inst.hit(hit);
    QJsonObject r; r["steps_set"] = (int)steps.size();
    return r;
}

QJsonObject ControlServer::cmdTrack(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const QString act = A.str("action", "", 8, true);
    if (!e.isEmpty()) return {};
    QJsonObject r;
    bool any = false;
    for (int ln = 0; ln < kLanes; ++ln) any = any || !w.trackPage.track.lane[ln].empty();
    if (act == "keep") {
        w.trackPage.trackKeep();
        r["clips"] = (int)w.trackPage.track.clips.size();
    } else if (act == "clear") {
        inst.trackDo(TC_STOP);
        w.trackPage.trackApply([](TrackData &t) { const bool had = !t.clips.empty(); t = TrackData(); return had; });
        w.trackPage.handOn = false;
        r["clips"] = 0;
    } else if (act == "place") {
        const int clip = A.integer("clip", 0, 1000, 0, true);
        const int bar = A.integer("bar", 0, kTrackMaxBars - 1, 0, true);
        const int lane = A.integer("lane", 0, kLanes - 1, -1);
        if (!e.isEmpty()) return {};
        if (clip >= (int)w.trackPage.track.clips.size()) { e = "no such clip: keep first (see state.track.clips)"; return {}; }
        w.trackPage.handOn = false;
        w.trackPage.trackTake(clip);
        const int l = lane >= 0 ? lane : laneForKind(w.trackPage.track.clips[clip]->kind);
        const bool ok = w.trackPage.trackPlace(l, bar);
        w.trackPage.handOn = false;
        if (!ok) { e = "no room there"; return {}; }
        r["placed"] = true;
    } else if (act == "play") {
        if (!any) { e = "nothing on the track"; return {}; }
        if (!inst.uiState().trackPlaying) w.trackPage.trackPlayStop();
    } else if (act == "stop") {
        if (inst.uiState().trackPlaying) w.trackPage.trackPlayStop();
    } else if (act == "export") {
        if (!any) { e = "nothing on the track to export"; return {}; }
        if (w.trackPage.exportStatus() == 1) { e = "an export is already running"; return {}; }
        w.trackPage.trackExport();
        r["path"] = w.trackPage.exportFile; r["async"] = true; // the file is complete when state.track.exporting is false
    } else {
        e = "action must be keep, place, play, stop, export or clear";
        return {};
    }
    return r;
}

