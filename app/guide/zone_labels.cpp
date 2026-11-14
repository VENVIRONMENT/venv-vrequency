// venv-vrequency — guided help: a zone's name in words (see guide.h).
#include "app/guide/guide.h"
#include "app/guide/zone_names.h"
#include <QRegularExpression>

QStringList aliasZones(const QString &n) {
    if (n == "octave") return {"octave_down", "octave_up"};
    if (n == "strings") return {"string0", "string1", "string2"};
    if (n == "tabs") return {"tab_*"};
    if (n == "chords") return {"chord*"};
    if (n == "pads") return {"pad*"};
    return {};
}

QString Guide::zoneLabel(const QString &name) const {
    static const QHash<QString, QString> fixed = {
        {"xy", "X-Y pad (scope)"}, {"hdr_sound", "Sound name"}, {"hdr_tune", "Key, tempo and octave"},
        {"key_readout", "Key"}, {"tempo_readout", "Tempo"}, {"octave_readout", "Octave readout"},
        {"octave_down", "Octave down"}, {"octave_up", "Octave up"}, {"loop_pedal", "Loop pedal"},
        {"loop_undo", "Undo loop layer"}, {"loop_playstop", "Loop play / stop"}, {"sustain", "Sustain"},
        {"new_sound", "New sound"}, {"menu", "Menu"}, {"string0", "Low string"}, {"string1", "Middle string"},
        {"string2", "High string"}, {"track_lanes", "Track lanes"}, {"track_ruler", "Track ruler (bars)"},
        {"track_keep", "Keep"}, {"chop_strip", "Chop strip"}, {"chat_history", "Chat history"},
        {"chat_input", "Chat input"}, {"chat_send", "Send"}, {"help_section", "Help section title"},
        {"help_next", "Next help page"}, {"help_ask", "Ask Rook"}, {"help_back", "Back to playing"},
        {"dock", "Chat dock"}, {"dock_history", "Dock messages"}, {"dock_input", "Dock input"},
        {"dock_send", "Dock Send"}, {"dock_full", "Open the Chat page"}, {"dock_close", "Hide the dock"},
        {"ticker", "Message ticker"}, {"backmode", "Chord tap mode"}, {"backplay", "Backing play / stop"},
        {"sound_octave-", "Sound octave down"}, {"sound_octave+", "Sound octave up"},
    };
    if (fixed.contains(name)) return fixed[name];
    static const QRegularExpression lane("^mixer_(lane|fader|mute|solo)(\\d)$");
    const auto m = lane.match(name);
    if (m.hasMatch()) {
        const QString k = m.captured(1), ln = QString("Lane %1").arg(m.captured(2).toInt() + 1);
        return k == "lane" ? ln : (k == "fader" ? ln + " fader" : (k == "mute" ? ln + " mute" : ln + " solo"));
    }
    if (name.startsWith("tab_")) return QString("%1 tab").arg(name.mid(4));
    if (name.startsWith("help_row_")) { const QString d = rowLabels ? rowLabels->value(name) : QString(); if (!d.isEmpty()) return d.left(60); }
    for (const Ui::Zone &z : ui.zones()) {
        if (z.name != name) continue;
        if (z.r.width() * z.r.height() > 45000) break;       // a big area has no single caption
        QStringList parts;
        for (const auto &t : ui.texts())
            if (z.r.contains(t.first.center())) parts << t.second.trimmed();
        parts.removeAll(QString());
        if (parts.isEmpty()) break;
        if (parts.size() > 3) parts = parts.mid(0, 3);
        QString s = parts.join(" ");
        if (s.size() > 40) s = s.left(39) + QString::fromUtf8("…");
        return s;
    }
    QString h = name;
    h.replace('_', ' ');
    return h;
}
