// venv-vrequency — the control socket: state, session save/load and screenshot commands.
#include "app/control/control.h"
#include "app/control/control_internal.h"
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include "app/store/store.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

using namespace td;
using namespace ctl;

QJsonObject ControlServer::cmdState() {
    const UiState u = inst.uiState();
    QJsonObject o;
    o["page"] = pageName(w.page());
    QJsonObject snd;
    snd["name"] = QString::fromUtf8(w.sounds.lead.name);
    snd["slot"] = w.sounds.curSlot >= 0 ? QJsonValue(w.sounds.curSlot + 1) : QJsonValue();
    snd["slots"] = [&] { QJsonArray s; for (int k = 0; k < 10; ++k) s.append(QString::fromUtf8(w.sounds.bank[k].name)); return s; }();
    QJsonArray fav;
    for (const store::NamedPatch &f : w.sounds.favs) fav.append(QString::fromUtf8(f.patch.name));
    snd["favourites"] = fav;
    QJsonObject pr;
    for (int p = 0; p < P_COUNT; ++p) pr[paramInfo(p).key] = (double)w.sounds.lead.p[p];
    snd["params"] = pr;
    o["sound"] = snd;
    QJsonObject sc;
    sc["name"] = scaleDef(u.set.tuning.scale).name;
    sc["index"] = u.set.tuning.scale;
    sc["root"] = noteName(u.set.tuning.root);
    sc["octave"] = u.set.tuning.octave;
    o["scale"] = sc;
    o["tempo"] = (double)u.set.bpm;
    o["volume"] = (double)u.set.volume;
    QJsonObject lp;
    lp["state"] = loopNames[qBound(0, u.loopState, 5)];
    lp["layers"] = u.layers; lp["bars"] = u.loopBars;
    o["looper"] = lp;
    QJsonObject bt;
    bt["playing"] = u.beatPlaying; bt["rec"] = u.beatRec; bt["kit"] = QString(kitName(u.set.kit)).toLower();
    QJsonObject steps;
    for (int d = 0; d < kDrums; ++d) {
        QString s;
        for (int k = 0; k < kDrumSteps; ++k) s += (u.beatPat[d] >> k) & 1 ? 'x' : '.';
        steps[QString(drumName(d)).toLower().remove(' ')] = s;
    }
    bt["steps"] = steps;
    o["beat"] = bt;
    QJsonObject bk;
    bk["playing"] = u.backPlaying;
    bk["mode"] = QString(backModeName(u.set.backMode)).toLower();
    bk["style"] = QString(backStyleName(u.set.backStyle)).toLower();
    bk["seventh"] = u.set.seventh;
    QJsonArray ch, deg;
    for (int i = 0; i < u.progLen && i < kProgMax; ++i) {
        ch.append(QString::fromUtf8(buildChord(u.set.tuning, u.prog[i], u.set.seventh).name));
        deg.append(u.prog[i] + 1);
    }
    bk["chords"] = ch; bk["degrees"] = deg;
    bk["position"] = u.progPos;
    o["backing"] = bk;
    QJsonObject tr;
    tr["playing"] = u.trackPlaying; tr["bars"] = w.trackPage.track.bars;
    QJsonArray clips, lanes;
    static const char *kinds[] = {"beat", "chords", "loop"};
    for (size_t i = 0; i < w.trackPage.track.clips.size(); ++i) {
        QJsonObject c;
        c["index"] = (int)i; c["name"] = QString::fromUtf8(w.trackPage.track.clips[i]->name);
        c["kind"] = kinds[qBound(0, w.trackPage.track.clips[i]->kind, 2)]; c["bars"] = w.trackPage.track.clips[i]->bars;
        clips.append(c);
    }
    static const char *laneN[kLanes] = {"beat", "chords", "loop_a", "loop_b"};
    for (int l = 0; l < kLanes; ++l) {
        QJsonArray pl;
        for (const Placement &p : w.trackPage.track.lane[l]) { QJsonObject x; x["clip"] = p.clip; x["bar"] = p.bar; x["bars"] = p.bars; x["mute"] = p.mute; pl.append(x); }
        QJsonObject ln; ln["lane"] = laneN[l]; ln["placements"] = pl; lanes.append(ln);
    }
    tr["clips"] = clips; tr["lanes"] = lanes;
    tr["exporting"] = w.trackPage.exportStatus() == 1;
    o["track"] = tr;
    o["playing"] = u.voices > 0 || u.backPlaying || u.beatPlaying || u.trackPlaying || u.loopState == L_PLAY || u.loopState == L_OVERDUB;
    o["voices"] = u.voices;
    o["toast"] = w.lastToast();
    QJsonObject cp;
    cp["listening"] = w.chat.listening();
    cp["last_seq"] = w.chat.seq;
    cp["unread"] = w.chat.unread;
    o["chat"] = cp;
    o["chat_tail"] = chatJson(qMax(0, w.chat.seq - 10), 10);
    QJsonObject hf;
    hf["page"] = pageName(w.page());
    hf["highlighted"] = QJsonArray::fromStringList(w.guide.highlightedZones());
    hf["dock_open"] = w.guide.dockIsOpen() && w.page() != PG_CHAT;
    hf["dock_focus"] = w.guide.dockHasFocus();
    hf["last_question_zone"] = w.guide.lastQuestionZone();
    hf["topic"] = w.guide.dockTopic();
    o["help_focus"] = hf;
    o["tour"] = w.tour().stateJson();
    return o;
}

QJsonObject ControlServer::cmdSession(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const QString act = A.str("action", "", 6, true);
    const QString name = A.str("name", "", 40, true);
    if (e.isEmpty() && !QRegularExpression("^[A-Za-z0-9_-]{1,40}$").match(name).hasMatch()) e = "'name' may hold letters, digits, - and _";
    if (e.isEmpty() && act != "save" && act != "load") e = "action must be save or load";
    if (!e.isEmpty()) return {};
    store::ensureDirs();
    const QString path = store::baseDir() + "/sessions/" + name + ".json";
    if (act == "save") {
        if (!w.saveSession(path)) { e = "could not write the session"; return {}; }
    } else {
        if (!QFile::exists(path)) { e = "no session called '" + name + "'"; return {}; }
        if (!w.loadSession(path)) { e = "that session file is damaged"; return {}; }
    }
    QJsonObject r; r["path"] = path;
    return r;
}

QJsonObject ControlServer::cmdScreenshot(QString &e) {
    const QString dir = QDir::tempPath();
    const QString path = QString("%1/vrequency-shot-%2.png").arg(dir).arg(QDateTime::currentMSecsSinceEpoch());
    if (!w.shot().save(path)) { e = "could not write " + path; return {}; }
    shots << path;
    while (shots.size() > 6) QFile::remove(shots.takeFirst()); // only the newest few are kept
    QJsonObject r; r["path"] = path; r["width"] = 720; r["height"] = 720;
    return r;
}

