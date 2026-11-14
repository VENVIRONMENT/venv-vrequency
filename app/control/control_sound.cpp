// venv-vrequency — the control socket: sound parameters, sounds, favourites, and scale/tempo/octave/bend/sustain.
#include "app/control/control.h"
#include "app/control/control_internal.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"

using namespace td;
using namespace ctl;

QJsonObject ControlServer::cmdParams(const QJsonObject &a, QString &e, bool set) {
    Args A{a, e};
    QJsonObject r;
    if (set) {
        const QString name = A.str("name", "", 24, true);
        const double v = A.num("value", -1e9, 1e9, 0, true);
        if (!e.isEmpty()) return {};
        int p = -1;
        for (int i = 0; i < P_COUNT; ++i) if (name == paramInfo(i).key) p = i;
        if (p < 0) { e = "unknown parameter '" + name + "' (get_params lists them)"; return {}; }
        const ParamInfo &m = paramInfo(p);
        float x = (float)qBound((double)m.min, v, (double)m.max);
        if (m.integer) x = std::round(x);
        w.sounds.setParam(p, x);
        w.sounds.curSlot = -1;
        r["name"] = name; r["value"] = (double)x; r["clamped"] = (double)x != v;
        return r;
    }
    QJsonArray arr;
    for (int p = 0; p < P_COUNT; ++p) {
        const ParamInfo &m = paramInfo(p);
        QJsonObject o;
        o["name"] = m.key; o["label"] = m.label; o["value"] = (double)w.sounds.lead.p[p];
        o["min"] = (double)m.min; o["max"] = (double)m.max; o["default"] = (double)m.def; o["integer"] = m.integer;
        arr.append(o);
    }
    r["sound"] = QString::fromUtf8(w.sounds.lead.name);
    r["params"] = arr;
    return r;
}

QJsonObject ControlServer::cmdSound(const QJsonObject &a, QString &e) {
    QJsonObject r;
    QJsonValue v = a.contains("slot") ? a["slot"] : a["name"];
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (!(d >= 1 && d <= 10) || d != std::floor(d)) { e = "'slot' must be a whole number from 1 to 10"; return {}; }
        w.sounds.slotSave = false;
        w.sounds.selectSlot((int)d - 1);
    } else if (v.isString()) {
        const QString s = v.toString().trimmed();
        if (s.size() > 40) { e = "name too long"; return {}; }
        if (s.compare("random", Qt::CaseInsensitive) == 0) w.sounds.randomise();
        else if (s.compare("mutate", Qt::CaseInsensitive) == 0) w.sounds.mutate();
        else if (s.compare("undo", Qt::CaseInsensitive) == 0) {
            if (w.sounds.undo.empty()) { e = "nothing to undo"; return {}; }
            w.sounds.undoSound();
        } else {
            bool found = false;
            for (const store::NamedPatch &f : w.sounds.favs)
                if (!found && QString::fromUtf8(f.patch.name).compare(s, Qt::CaseInsensitive) == 0) {
                    found = true;
                    w.sounds.recall(f.patch);
                }
            if (!found) { e = "no favourite called '" + s + "'"; return {}; }
        }
    } else {
        e = "give 'slot' (1 to 10) or 'name' (a favourite, random, mutate or undo)";
        return {};
    }
    r["name"] = QString::fromUtf8(w.sounds.lead.name);
    r["slot"] = w.sounds.curSlot >= 0 ? QJsonValue(w.sounds.curSlot + 1) : QJsonValue();
    return r;
}

QJsonObject ControlServer::cmdFavourite(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const QString name = A.str("name", "", 24, true).trimmed();
    if (e.isEmpty() && (name.isEmpty() || name.contains(QRegularExpression("[\\x00-\\x1f]")))) e = "'name' must be 1 to 24 printable characters";
    if (!e.isEmpty()) return {};
    Patch p = w.sounds.lead;
    p.setName(name.toUtf8().constData());
    bool updated = false;
    const QString path = store::savePatch("favourites", p, true, &updated);
    w.refreshLists();
    if (path.isEmpty()) { e = "could not write the favourite"; return {}; }
    w.toast((updated ? "Updated " : "Favourite saved: ") + name);
    QJsonObject r; r["path"] = path; r["name"] = name; r["updated"] = updated;
    return r;
}

QJsonObject ControlServer::cmdPlayControls(const QString &cmd, const QJsonObject &a, QString &e) {
    Args A{a, e};
    QJsonObject r;
    if (cmd == "scale") {
        if (!A.has("name") && !A.has("root")) { e = "give 'name' and/or 'root'"; return {}; }
        int sc = -1, root = -1;
        if (A.has("name")) {
            const QJsonValue v = a["name"];
            if (v.isDouble() && v.toDouble() >= 0 && v.toDouble() < SC_COUNT && v.toDouble() == std::floor(v.toDouble())) sc = (int)v.toDouble();
            else if (v.isString()) {
                const QString s = v.toString().toLower().remove(' ').remove('-').remove('_');
                for (int i = 0; i < SC_COUNT; ++i)
                    if (s == QString(scaleDef(i).name).toLower().remove(' ') || s == QString(scaleDef(i).shortName).toLower().remove(' '))
                        sc = i;
                if (s == "minorpentatonic") sc = SC_MINPENT;
                if (s == "majorpentatonic") sc = SC_MAJPENT;
            }
            if (sc < 0) { e = "unknown scale (major, minor, majpent, minpent, blues, dorian, mixolydian, chromatic)"; return {}; }
        }
        if (A.has("root")) {
            root = parseRoot(a["root"]);
            if (root < 0) { e = "'root' must be a note name like A, C# or Bb, or 0 to 11"; return {}; }
        }
        if (sc >= 0) inst.set(S_SCALE, (float)sc);
        if (root >= 0) inst.set(S_ROOT, (float)root);
        if (sc >= 0) r["scale"] = scaleDef(sc).name;
        if (root >= 0) r["root"] = noteName(root);
    } else if (cmd == "tempo") {
        const double bpm = A.num("bpm", 40, 240, 100, true);
        if (!e.isEmpty()) return {};
        inst.set(S_BPM, (float)bpm);
        r["bpm"] = bpm;
    } else if (cmd == "octave") {
        const int cur = inst.uiState().set.tuning.octave;
        int target = cur;
        if (A.has("value")) target = A.integer("value", -3, 3, cur);
        else if (A.has("delta")) target = qBound(-3, cur + A.integer("delta", -6, 6, 0), 3);
        else A.fail("give 'value' (-3 to 3) or 'delta'");
        if (!e.isEmpty()) return {};
        for (int i = 0; i < qAbs(target - cur); ++i) inst.cmd(EV_OCTAVE, target > cur ? 1 : -1);
        r["octave"] = target;
    } else if (cmd == "bend") {
        const double v = A.num("value", -1, 1, 0, true);
        const int ms = A.integer("ms", 0, 10000, 0);
        if (!e.isEmpty()) return {};
        Event ev; ev.type = EV_BEND; ev.f = (float)v; inst.push(ev);
        if (ms > 0) QTimer::singleShot(ms, this, [this] { Event z; z.type = EV_BEND; z.f = 0; inst.push(z); });
        r["value"] = v;
    } else { // sustain
        const bool on = A.boolean("on", true);
        if (!e.isEmpty()) return {};
        inst.cmd(EV_SUSTAIN, on ? 1 : 0);
        r["on"] = on;
    }
    return r;
}

