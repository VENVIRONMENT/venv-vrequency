// venv-vrequency — the control socket's small parsers: key names, chord degrees, chat lines.
#include "app/control/control_internal.h"

using namespace td;

namespace ctl {

const char *loopNames[] = {"empty", "count-in", "recording", "playing", "overdubbing", "stopped"};

int parseRoot(const QJsonValue &v) {
    if (v.isDouble()) { const double d = v.toDouble(); return (d >= 0 && d < 12 && d == std::floor(d)) ? (int)d : -1; }
    if (!v.isString()) return -1;
    QString s = v.toString().trimmed().toUpper();
    static const char *sharp[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char *flat[12] = {"C", "DB", "D", "EB", "E", "F", "GB", "G", "AB", "A", "BB", "B"};
    for (int i = 0; i < 12; ++i) if (s == sharp[i] || s == flat[i]) return i;
    return -1;
}

int parseDegree(const QJsonValue &v) { // a chord: 1..7 or a roman numeral
    if (v.isDouble()) { const double d = v.toDouble(); return (d >= 1 && d <= 7 && d == std::floor(d)) ? (int)d - 1 : -1; }
    if (!v.isString()) return -1;
    static const char *R[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    const QString s = v.toString().trimmed().toUpper();
    for (int i = 0; i < 7; ++i) if (s == R[i]) return i;
    return -1;
}
QJsonObject msgJson(const ChatMsg &m) {
    QJsonObject o;
    o["seq"] = m.seq; o["from"] = m.from; o["text"] = m.text; o["kind"] = m.kind;
    o["ts"] = (double)m.ts; o["user"] = m.user;
    if (m.user) { // what the player was looking at (see highlight)
        if (!m.page.isEmpty()) o["page"] = m.page;
        o["focus"] = QJsonArray::fromStringList(m.focus);
        if (!m.via.isEmpty()) o["via"] = m.via;
    }
    return o;
}

} // namespace ctl
