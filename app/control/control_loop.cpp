// venv-vrequency — the control socket: loop, backing chords and pulse pad commands.
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

// {"cmd":"pulse","pad":5,"type":"train","tone":1000,"length":250,"rate":40,"level":0.8,"latch":true,"hit":true,"sync":false}
// Every field is optional, but there must be one; all but sync need a pad (0 to 7).
QJsonObject ControlServer::cmdPulse(const QJsonObject &a, QString &e) {
    Args A{a, e};
    static const char *const fields[] = {"hit", "latch", "type", "tone", "length", "rate", "level", "sync"};
    bool any = false, needPad = false;
    for (const char *f : fields) if (A.has(f)) { any = true; if (QString(f) != "sync") needPad = true; }
    if (!any) { e = "give 'hit', 'latch', 'type', 'tone', 'length', 'rate', 'level' or 'sync'"; return {}; }
    const int pad = A.integer("pad", 0, kPulsePads - 1, -1, needPad);
    const bool hit = A.boolean("hit", false), latch = A.boolean("latch", false), sync = A.boolean("sync", false);
    const QString type = A.str("type", "", 12);
    const double tone = A.num("tone", kPulseToneMin, kPulseToneMax, -1), length = A.num("length", kPulseLenMin, kPulseLenMax, -1);
    const double rate = A.num("rate", kPulseRateMin, kPulseRateMax, -1), level = A.num("level", 0, 1, -1);
    if (!e.isEmpty()) return {};
    const int t = type.isEmpty() ? -1 : pulseTypeFromKey(type.toLower().toUtf8().constData());
    if (!type.isEmpty() && t < 0) { e = "type must be click, tick, noise, blip, gated, train, thump or hiss"; return {}; }
    if (A.has("sync")) inst.set(S_PULSESYNC, sync ? 1 : 0);
    if (t >= 0) inst.pulseSet(pad, PF_TYPE, (float)t);
    if (tone >= 0) inst.pulseSet(pad, PF_TONE, (float)tone);
    if (length >= 0) inst.pulseSet(pad, PF_LENGTH, (float)length);
    if (rate >= 0) inst.pulseSet(pad, PF_RATE, (float)rate);
    if (level >= 0) inst.pulseSet(pad, PF_LEVEL, (float)level);
    if (A.has("latch")) w.pulsePage.setLatch(pad, latch);
    if (hit) w.pulsePage.hitPad(pad);
    w.saveSoon();
    QJsonObject r;
    if (pad >= 0) r["pad"] = pad;
    return r;
}
