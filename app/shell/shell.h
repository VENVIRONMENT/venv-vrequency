// venv-vrequency — what a page may use of the app around it: the engine, the audio device, the
// drawing toolkit, the key map and the keys held, a clock, and a few services the window provides.
// Pages and helpers are given a Shell; none of them sees the window itself.
#pragma once
#include "app/shell/key_state.h"
#include "app/shell/keymap.h"
#include "app/ui/ui.h"
#include "core/instrument/instrument.h"
#include <QElapsedTimer>

class AudioOut;
class Sounds;

// PG_PLAY is the instrument itself; its tab is called Synth. The app opens on it.
enum PageId { PG_TRACK, PG_PLAY, PG_SOUND, PG_BEAT, PG_BACK, PG_TUNE, PG_SYS, PG_HELP, PG_CHAT, PG_COUNT };
const char *pageName(int p);                 // "track", "synth"...: the name the control socket and the zones use
const char *pageTitle(int p);                // "Track", "Synth"...: what the tab says
int pageFromName(const QString &n);          // -1 when unknown; "play" is the Synth page

class Shell {
public:
    Shell(td::Instrument &i, AudioOut &a) : inst(i), audio(a) {}

    td::Instrument &inst;
    AudioOut &audio;
    Ui ui;
    KeyMap km;
    KeyState keys;
    QElapsedTimer clock;                     // started with the window
    td::UiState u;                           // the engine's state as of this frame

    virtual int page() const = 0;
    // The one way to change page: every armed mode is dropped whenever the page really changes.
    virtual void goPage(int p) = 0;
    virtual void perform(int act, bool shift = false) = 0;    // an action of the key map (A_...)
    virtual void toast(const QString &s, int ms = 1800) = 0;  // a message box for a moment
    virtual void redraw() = 0;
    virtual void saveState() = 0;
    virtual void saveSoon() = 0;             // an edit is on disk a moment later
    virtual void refreshLists() = 0;         // read the favourites and sessions on disk again
    virtual const Sounds &soundShelf() const = 0;              // the ten slots and the favourites
    virtual void setLastRecording(const QString &path) = 0;    // an exported track is the newest recording
    void set(int setting, float v) { inst.set(setting, v); }

protected:
    ~Shell() = default;
};
