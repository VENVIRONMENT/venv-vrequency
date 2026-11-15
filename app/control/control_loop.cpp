// venv-vrequency — the control socket: loop and beat commands.
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
