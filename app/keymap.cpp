#include "keymap.h"
#include "store.h"
#include <QFile>
#include <QJsonObject>
#include <Qt>

const char *actName(int act) {
    static const char *N[A_COUNT] = {
        "none", "sustain", "chromatic", "chord_layer", "command", "panic", "loop_pedal",
        "bend_down", "bend_up", "octave_down", "octave_up", "chord_row_toggle",
        "next_page", "play_page",
        "sound_1", "sound_2", "sound_3", "sound_4", "sound_5", "sound_6", "sound_7", "sound_8", "sound_9", "sound_10",
        "key_up", "next_scale", "new_sound", "mutate_sound", "undo_sound", "save_favourite", "metronome", "tap_tempo",
        "backing_style", "backing_mode", "backing_clear", "backing_start_stop", "backing_remove_last",
        "loop_undo", "loop_start_stop", "loop_clear", "record_wav", "help", "xy_mapping", "quit",
        "beat_start_stop", "beat_record", "play_last_recording", "track_start_stop", "chat", "tour"};
    return (act >= 0 && act < A_COUNT) ? N[act] : "none";
}

namespace {
struct Named { int key; const char *name; };
const Named kNamed[] = {
    {Qt::Key_Space, "Space"}, {Qt::Key_Backspace, "Backspace"}, {Qt::Key_Return, "Return"}, {Qt::Key_Enter, "Enter"},
    {Qt::Key_Tab, "Tab"}, {Qt::Key_Escape, "Escape"}, {Qt::Key_Shift, "Shift"}, {Qt::Key_Alt, "Alt"},
    {Qt::Key_Control, "Ctrl"}, {Qt::Key_Meta, "Meta"}, {Qt::Key_Left, "Left"}, {Qt::Key_Right, "Right"},
    {Qt::Key_Up, "Up"}, {Qt::Key_Down, "Down"}, {Qt::Key_Delete, "Delete"}, {Qt::Key_CapsLock, "CapsLock"},
    {Qt::Key_PageUp, "PageUp"}, {Qt::Key_PageDown, "PageDown"}, {Qt::Key_F1, "F1"}, {Qt::Key_F4, "F4"},
    {Qt::Key_F5, "F5"}, {Qt::Key_F11, "F11"}, {Qt::Key_F12, "F12"},
};
}

QString keyName(int k) {
    for (const Named &n : kNamed) if (n.key == k) return n.name;
    if (k >= 0x21 && k <= 0x7e) return QString(QChar(k));
    return QString("0x%1").arg(k, 0, 16);
}

int keyFromName(const QString &name) {
    for (const Named &n : kNamed) if (name.compare(n.name, Qt::CaseInsensitive) == 0) return n.key;
    if (name.size() == 1) return name[0].toUpper().unicode();
    if (name.startsWith("0x")) return name.mid(2).toInt(nullptr, 16);
    return 0;
}

KeyMap KeyMap::defaults() {
    KeyMap k;
    k.rows[0] = "ZXCVBNM";
    k.rows[1] = "ASDFGHJKL";
    k.rows[2] = "QWERTYUIOP";
    auto P = [&](int key, int act) { k.plain[key] = act; };
    auto C = [&](int key, int act) { k.command[key] = act; };
    P(Qt::Key_Space, A_SUSTAIN); P(Qt::Key_Shift, A_CHROMATIC); P(Qt::Key_Alt, A_CHORD_LAYER);
    P(Qt::Key_Control, A_COMMAND);
    P(Qt::Key_Backspace, A_PANIC); P(Qt::Key_Return, A_LOOP); P(Qt::Key_Enter, A_LOOP);
    P(Qt::Key_Left, A_BEND_DOWN); P(Qt::Key_Right, A_BEND_UP); P(Qt::Key_BracketLeft, A_BEND_DOWN); P(Qt::Key_BracketRight, A_BEND_UP);
    P(Qt::Key_Down, A_OCT_DOWN); P(Qt::Key_Up, A_OCT_UP); P(Qt::Key_Minus, A_OCT_DOWN); P(Qt::Key_Equal, A_OCT_UP);
    P(Qt::Key_Dollar, A_CHORD_ROW);
    P(Qt::Key_Tab, A_PAGE_NEXT); P(Qt::Key_Escape, A_HOME);
    for (int i = 0; i < 9; ++i) P(Qt::Key_1 + i, A_SOUND_1 + i);
    P(Qt::Key_0, A_SOUND_10);
    C(Qt::Key_K, A_KEY_UP); C(Qt::Key_S, A_SCALE_NEXT); C(Qt::Key_R, A_RANDOM); C(Qt::Key_N, A_MUTATE);
    C(Qt::Key_Z, A_UNDO_SOUND); C(Qt::Key_F, A_FAVOURITE); C(Qt::Key_M, A_METRO); C(Qt::Key_T, A_TAP);
    C(Qt::Key_A, A_BACK_STYLE); C(Qt::Key_B, A_BACK_MODE); C(Qt::Key_X, A_BACK_CLEAR); C(Qt::Key_P, A_BACK_PLAY);
    C(Qt::Key_Backspace, A_BACK_REMOVE);
    C(Qt::Key_U, A_LOOP_UNDO); C(Qt::Key_L, A_LOOP_PLAY); C(Qt::Key_D, A_LOOP_CLEAR);
    C(Qt::Key_E, A_RECORD); C(Qt::Key_H, A_HELP); C(Qt::Key_G, A_XYMAP); C(Qt::Key_Q, A_QUIT);
    C(Qt::Key_J, A_BEAT_PLAY); C(Qt::Key_I, A_BEAT_REC); C(Qt::Key_W, A_PLAY_LAST);
    C(Qt::Key_Space, A_TRACK_PLAY); C(Qt::Key_Y, A_CHAT); C(Qt::Key_O, A_TOUR);
    return k;
}

bool KeyMap::stringKey(int key, int &row, int &col) const {
    if (key < 0x20 || key > 0x7e) return false;
    QChar c = QChar(key).toUpper();
    for (int r = 0; r < 3; ++r) {
        int i = rows[r].indexOf(c);
        if (i >= 0) { row = r; col = i; return true; }
    }
    return false;
}

// The pads sit in two rows of four, so the two upper key rows play them: the first four keys of
// each row for the left hand, and a second set further right for the other hand.
bool KeyMap::drumKey(int key, int &drum) const {
    int row, col;
    if (!stringKey(key, row, col) || row == 0) return false;
    int second = row == 2 ? 6 : 5;   // U I O P on the top row, H J K L on the middle row
    int c = col < 4 ? col : (col >= second && col < second + 4 ? col - second : -1);
    if (c < 0) return false;
    drum = (row == 2 ? 0 : 4) + c;
    return true;
}

QString KeyMap::drumKeys(int drum) const {
    const QString &r = rows[drum < 4 ? 2 : 1];
    int c = drum % 4, second = (drum < 4 ? 6 : 5) + c;
    QString s = c < r.size() ? QString(r[c]) : QString();
    if (second < r.size()) s += QString(" ") + r[second];
    return s;
}

QString KeyMap::keyFor(int act, bool commandLayer) const {
    const QHash<int, int> &h = commandLayer ? command : plain;
    QStringList names;
    for (auto it = h.begin(); it != h.end(); ++it)
        if (it.value() == act) names << keyName(it.key());
    names.sort();
    return names.isEmpty() ? QString("-") : names.first();
}

bool KeyMap::save(const QString &path) const {
    QJsonObject o, strings, keys, cmd;
    o["_help"] = "strings: one character per key, left to right. keys / command: action -> key name "
                 "(a letter, digit, symbol, or Space Backspace Return Tab Escape Shift Alt Ctrl Left Right Up Down). "
                 "'command' bindings fire while the key bound to 'command' is held. Delete this file to restore defaults.";
    strings["low"] = rows[0]; strings["middle"] = rows[1]; strings["high"] = rows[2];
    o["strings"] = strings;
    auto dump = [](const QHash<int, int> &h, QJsonObject &dst) {
        for (int act = 1; act < A_COUNT; ++act) {
            QStringList names;
            for (auto it = h.begin(); it != h.end(); ++it) if (it.value() == act) names << keyName(it.key());
            names.sort();
            if (!names.isEmpty()) dst[actName(act)] = names.join(" ");
        }
    };
    dump(plain, keys); dump(command, cmd);
    o["keys"] = keys; o["command"] = cmd;
    return store::writeJson(path, o);
}

KeyMap KeyMap::loadOrCreate(const QString &path) {
    KeyMap k = defaults();
    QJsonObject o;
    if (!QFile::exists(path)) { k.save(path); return k; }
    if (!store::readJson(path, o)) return k;
    QJsonObject s = o["strings"].toObject();
    if (!s["low"].toString().isEmpty()) k.rows[0] = s["low"].toString().toUpper();
    if (!s["middle"].toString().isEmpty()) k.rows[1] = s["middle"].toString().toUpper();
    if (!s["high"].toString().isEmpty()) k.rows[2] = s["high"].toString().toUpper();
    // An action the file names takes the file's keys; one it does not mention (added in a later
    // version than the file) keeps its default, unless the file gave that key to something else.
    auto apply = [](const QJsonObject &src, QHash<int, int> &dst) {
        if (src.isEmpty()) return;
        const QHash<int, int> defaults = dst;
        dst.clear();
        for (int act = 1; act < A_COUNT; ++act) {
            if (!src.contains(actName(act))) continue;
            const QString v = src[actName(act)].toString();
            for (const QString &nm : v.split(' ', Qt::SkipEmptyParts)) {
                int key = keyFromName(nm);
                if (key) dst[key] = act;
            }
        }
        for (auto it = defaults.begin(); it != defaults.end(); ++it)
            if (!src.contains(actName(it.value())) && !dst.contains(it.key())) dst[it.key()] = it.value();
    };
    apply(o["keys"].toObject(), k.plain);
    apply(o["command"].toObject(), k.command);
    return k;
}
