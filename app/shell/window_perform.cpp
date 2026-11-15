// venv-vrequency — the actions of the key map: what each A_... does, whether it came from a key,
// a button or the control socket.
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include <cmath>

using namespace td;

void MainWindow::perform(int act, bool shift) {
    switch (act) {
    case A_PANIC:
        inst.cmd(EV_PANIC);
        keys.held.clear(); keys.susLatch = false;
        toast("All notes off", 900);
        break;
    case A_LOOP: inst.cmd(EV_LOOP, LC_PEDAL); break;
    case A_OCT_DOWN: inst.cmd(EV_OCTAVE, -1); break;
    case A_OCT_UP: inst.cmd(EV_OCTAVE, 1); break;
    case A_CHORD_ROW:
        keys.chordRow = !keys.chordRow;
        toast(keys.chordRow ? "Bottom row plays chords" : "Bottom row is a string again");
        break;
    case A_PAGE_NEXT: goPage((pg + 1) % PG_COUNT); break;
    case A_HOME: goPage(PG_PLAY); keys.testOpen = false; break;
    case A_KEY_UP: set(S_ROOT, (float)((u.set.tuning.root + (shift ? 11 : 1)) % 12)); break;
    case A_SCALE_NEXT: set(S_SCALE, (float)((u.set.tuning.scale + (shift ? SC_COUNT - 1 : 1)) % SC_COUNT)); break;
    case A_RANDOM: sounds.randomise(); break;
    case A_MUTATE: sounds.mutate(); break;
    case A_UNDO_SOUND: sounds.undoSound(); break;
    case A_FAVOURITE: sounds.saveFavourite(); break;
    case A_METRO: set(S_METRO, u.set.metro ? 0 : 1); toast(u.set.metro ? "Metronome off" : "Metronome on", 900); break;
    case A_TAP: tapTempo(); break;
    case A_LOOP_UNDO: inst.cmd(EV_LOOP, LC_UNDO); break;
    case A_LOOP_PLAY: inst.cmd(EV_LOOP, LC_PLAYSTOP); break;
    case A_LOOP_CLEAR: inst.cmd(EV_LOOP, LC_CLEAR); toast("Loop cleared", 900); break;
    case A_RECORD: recorder.toggle(); break;
    case A_BEAT_PLAY: inst.beatDo(BT_PLAYSTOP); toast(u.beatPlaying ? "Beat stopped" : "Beat playing", 900); break;
    case A_BEAT_REC: inst.beatDo(BT_REC); toast(u.beatRec ? "Beat recording off" : "Beat recording: pads write hits", 1200); break;
    case A_PLAY_LAST: recorder.togglePlayback(); break;
    case A_CHAT: // Ctrl+Y: the chat dock on this page; with Shift (or on the Chat page itself) the full Chat page
        if (shift || pg == PG_CHAT) chat.toggle();
        else if (!guide.dockOpen) guide.dockOpenFocus();
        else guide.dockFocus = !guide.dockFocus;
        break;
    case A_HELP: goPage(pg == PG_HELP ? PG_PLAY : PG_HELP); break;
    case A_TOUR: if (tourView) { if (tourView->isOpen()) tourView->close(); else tourView->open(QString(), QString(), false, nullptr, true); } break;
    case A_XYMAP: set(S_XYMAP, (float)((u.set.xyMap + 1) % XY_COUNT)); break;
    case A_QUIT: close(); break;
    case A_SCOPE_VIEW: toast(QString("Scope: ") + synthPage.cycleScopeView(), 900); break;
    case A_POLY:
        releaseAllKeys(); // the held keys belong to the old way of playing
        set(S_POLY, u.set.poly ? 0 : 1);
        toast(u.set.poly ? "Keys: strings (one note per row, slides)" : "Keys: poly (every key its own note)", 1400);
        break;
    case A_FILTER_OPEN: nudgeCutoff(0.5); break;   // from a button or the socket: one step
    case A_FILTER_CLOSE: nudgeCutoff(-0.5); break;
    default:
        if (act >= A_SOUND_1 && act <= A_SOUND_10) sounds.selectSlot(act - A_SOUND_1);
        break;
    }
}

// Moves the lead sound's cutoff by `octaves`, and says where it is.
void MainWindow::nudgeCutoff(double octaves) {
    const float c = std::clamp((float)(sounds.lead.p[P_CUTOFF] * std::pow(2.0, octaves)), 40.0f, 16000.0f);
    sounds.setParam(P_CUTOFF, c);
    toast(c >= 1000 ? QString("Cutoff %1 kHz").arg(c / 1000, 0, 'f', 1) : QString("Cutoff %1 Hz").arg((int)c), 700);
}

// Every frame: while Ctrl+C or Ctrl+X is held the cutoff glides, two and a half octaves a second.
void MainWindow::sweepFilter() {
    static qint64 last = 0;
    const qint64 now = clock.elapsed();
    const double dt = std::min(0.1, (now - last) / 1000.0);
    last = now;
    if (keys.filterDir) nudgeCutoff(keys.filterDir * 2.5 * dt);
}
