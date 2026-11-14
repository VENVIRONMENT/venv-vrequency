// venv-vrequency — the control socket: notes, chords, slides and release.
#include "app/control/control.h"
#include "app/control/control_internal.h"
#include "app/shell/mainwindow.h"

using namespace td;
using namespace ctl;

// The pitch a request asks for: `degKey` is a scale degree on the string (0 = its first key,
// negative or past an octave allowed), `midiKey` an exact MIDI note.
bool ControlServer::notePitch(const QJsonObject &a, int string, const char *degKey, const char *midiKey, float &midi, QString &e) const {
    Args A{a, e};
    const bool hasDeg = A.has(degKey), hasMidi = A.has(midiKey);
    if (hasDeg == hasMidi) { A.fail(QString("give either '%1' or '%2'").arg(degKey, midiKey)); return false; }
    if (hasDeg) {
        const int d = A.integer(degKey, -48, 48, 0);
        if (!e.isEmpty()) return false;
        midi = (float)inst.uiState().set.tuning.noteFor(string, d, false);
    } else {
        midi = (float)A.num(midiKey, 12, 120, 60);
    }
    return e.isEmpty();
}

int ControlServer::startNote(int string, float midi, float vel, int durMs) {
    int id = nextNoteId++;
    if (nextNoteId > 0x7ff) nextNoteId = 0x400;
    liveNotes[id] = string;
    inst.note(string, id, midi, true, vel);
    QTimer::singleShot(durMs, this, [this, id] { releaseNote(id); });
    return id;
}

void ControlServer::releaseNote(int id) {
    auto it = liveNotes.find(id);
    if (it == liveNotes.end()) return;
    inst.note(it.value(), id, 0, false);
    liveNotes.erase(it);
}

QJsonObject ControlServer::cmdNote(const QJsonObject &a, QString &e, bool chord) {
    Args A{a, e};
    struct N { int string; float midi, vel; int dur; };
    std::vector<N> list;
    const int dur0 = A.integer("dur_ms", 20, 30000, 500);
    const float vel0 = (float)A.num("velocity", 0, 1, 1);
    if (!e.isEmpty()) return {};
    auto one = [&](const QJsonObject &o, int forcedString) {
        Args B{o, e};
        N n;
        n.string = forcedString >= 0 ? forcedString : B.integer("string", 0, kStrings - 1, 0, true);
        n.dur = B.integer("dur_ms", 20, 30000, dur0);
        n.vel = (float)B.num("velocity", 0, 1, vel0);
        if (!e.isEmpty()) return;
        if (!notePitch(o, n.string, "degree", "midi", n.midi, e)) return;
        list.push_back(n);
    };
    if (!chord && !a.contains("notes")) {
        one(a, -1);
    } else if (a.contains("notes") && a["notes"].isArray()) {
        const QJsonArray arr = a["notes"].toArray();
        if (arr.isEmpty() || arr.size() > kStrings * 4) A.fail("'notes' must hold 1 to 12 notes");
        for (const QJsonValue &v : arr) {
            if (!e.isEmpty()) break;
            if (!v.isObject()) { A.fail("every note must be an object"); break; }
            one(v.toObject(), -1);
        }
    } else if (a.contains("degrees") || a.contains("midis")) { // a chord, one note on each string from the lowest
        const bool deg = a.contains("degrees");
        const QJsonValue v = a[deg ? "degrees" : "midis"];
        if (!v.isArray() || v.toArray().isEmpty() || v.toArray().size() > kStrings) A.fail("give 1 to 3 values for the strings, lowest first");
        const QJsonArray arr = v.toArray();
        for (int i = 0; i < arr.size() && e.isEmpty(); ++i) {
            if (!arr[i].isDouble()) { A.fail("every value must be a number"); break; }
            QJsonObject o; o[deg ? "degree" : "midi"] = arr[i];
            one(o, i);
        }
    } else {
        A.fail("give 'notes', 'degrees' or 'midis'");
    }
    if (!e.isEmpty()) return {};
    QJsonArray played;
    for (const N &n : list) {
        const int id = startNote(n.string, n.midi, n.vel, n.dur);
        QJsonObject p; p["string"] = n.string; p["midi"] = (double)n.midi; p["hz"] = midiToHz(n.midi); p["id"] = id;
        played.append(p);
    }
    QJsonObject r;
    r["played"] = played;
    return r;
}

QJsonObject ControlServer::cmdSlide(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const int s = A.integer("string", 0, kStrings - 1, 0, true);
    const int ms = A.integer("ms", 50, 30000, 600, false);
    const double vel = A.num("velocity", 0, 1, 1);
    const bool midi = A.str("unit", "degree", 8) == "midi";
    if (!e.isEmpty()) return {};
    if (!a.contains("from") || !a.contains("to")) { e = "slide needs 'from' and 'to'"; return {}; }
    QJsonObject f, t;
    f[midi ? "midi" : "degree"] = a["from"]; t[midi ? "midi" : "degree"] = a["to"];
    float m0, m1;
    if (!a["from"].isDouble() || !a["to"].isDouble()) { e = "'from' and 'to' must be numbers"; return {}; }
    if (!notePitch(f, s, "degree", "midi", m0, e) || !notePitch(t, s, "degree", "midi", m1, e)) return {};
    // the first note sounds, then the second is pressed while it is held: the string glides at the sound's glide time
    const int id = startNote(s, m0, (float)vel, ms);
    const int hold = qMin(ms / 3, 800);
    QTimer::singleShot(hold, this, [this, s, m1, ms, hold, id, vel] {
        if (!liveNotes.contains(id)) return;
        const int id2 = startNote(s, m1, (float)vel, qMax(20, ms - hold));
        (void)id2;
        releaseNote(id); // the first key lifts: the string stays on the second
    });
    QJsonObject r;
    r["from_midi"] = (double)m0; r["to_midi"] = (double)m1;
    return r;
}

QJsonObject ControlServer::cmdRelease(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const int s = A.integer("string", -1, kStrings - 1, -1);
    if (!e.isEmpty()) return {};
    const QList<int> ids = liveNotes.keys();
    int n = 0;
    for (int id : ids) if (s < 0 || liveNotes[id] == s) { releaseNote(id); ++n; }
    QJsonObject r; r["released"] = n;
    return r;
}

