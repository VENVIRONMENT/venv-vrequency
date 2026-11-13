// venv vrequency — key bindings (editable in ~/vrequency/keymap.json).
#pragma once
#include <QHash>
#include <QString>

enum Act {
    A_NONE,
    // held / immediate performance keys
    A_SUSTAIN, A_CHROMATIC, A_CHORD_LAYER, A_COMMAND, A_PANIC, A_LOOP,
    A_BEND_DOWN, A_BEND_UP, A_OCT_DOWN, A_OCT_UP, A_CHORD_ROW,
    A_PAGE_NEXT, A_HOME,
    A_SOUND_1, A_SOUND_2, A_SOUND_3, A_SOUND_4, A_SOUND_5, A_SOUND_6, A_SOUND_7, A_SOUND_8, A_SOUND_9, A_SOUND_10,
    // command-layer actions (hold the command key, default Ctrl)
    A_KEY_UP, A_SCALE_NEXT, A_RANDOM, A_MUTATE, A_UNDO_SOUND, A_FAVOURITE, A_METRO, A_TAP,
    A_BACK_STYLE, A_BACK_MODE, A_BACK_CLEAR, A_BACK_PLAY, A_BACK_REMOVE,
    A_LOOP_UNDO, A_LOOP_PLAY, A_LOOP_CLEAR, A_RECORD, A_HELP, A_XYMAP, A_QUIT,
    A_COUNT
};

const char *actName(int act);  // stable name used in keymap.json
QString keyName(int qtKey);    // "Z", "Space", "Shift", ...
int keyFromName(const QString &name);

struct KeyMap {
    QString rows[3];            // [0] lowest string .. [2] highest; one character per key, left to right
    QHash<int, int> plain;      // Qt::Key -> Act
    QHash<int, int> command;    // Qt::Key -> Act while the command key is held

    static KeyMap defaults();
    bool stringKey(int qtKey, int &row, int &col) const;
    QString keyFor(int act, bool commandLayer) const; // for the on-screen guide

    // Loads `path` over the defaults; writes a fresh file when none exists.
    static KeyMap loadOrCreate(const QString &path);
    bool save(const QString &path) const;
};
