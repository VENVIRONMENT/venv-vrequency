// venv-vrequency — the actions of the key map: what each A_... does, whether it came from a key,
// a button or the control socket.
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"

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
    case A_BACK_STYLE: set(S_BACKSTYLE, (float)((u.set.backStyle + 1) % BS_COUNT)); toast(QString("Backing: %1").arg(backStyleName((u.set.backStyle + 1) % BS_COUNT)), 1000); break;
    case A_BACK_MODE: set(S_BACKMODE, (float)((u.set.backMode + 1) % BM_COUNT)); toast(QString("Chord taps: %1").arg(backModeName((u.set.backMode + 1) % BM_COUNT)), 1000); break;
    case A_BACK_CLEAR: inst.cmd(EV_BACK, BC_CLEAR); toast("Backing cleared", 900); break;
    case A_BACK_PLAY: inst.cmd(EV_BACK, BC_PLAYSTOP); break;
    case A_BACK_REMOVE: inst.cmd(EV_BACK, BC_REMOVE_LAST); break;
    case A_LOOP_UNDO: inst.cmd(EV_LOOP, LC_UNDO); break;
    case A_LOOP_PLAY: inst.cmd(EV_LOOP, LC_PLAYSTOP); break;
    case A_LOOP_CLEAR: inst.cmd(EV_LOOP, LC_CLEAR); toast("Loop cleared", 900); break;
    case A_RECORD: recorder.toggle(); break;
    case A_BEAT_PLAY: inst.beatDo(BT_PLAYSTOP); toast(u.beatPlaying ? "Beat stopped" : "Beat playing", 900); break;
    case A_BEAT_REC: inst.beatDo(BT_REC); toast(u.beatRec ? "Beat recording off" : "Beat recording: pads write hits", 1200); break;
    case A_PLAY_LAST: recorder.togglePlayback(); break;
    case A_TRACK_PLAY: trackPage.trackPlayStop(); break;
    case A_CHAT: // Ctrl+Y: the chat dock on this page; with Shift (or on the Chat page itself) the full Chat page
        if (shift || pg == PG_CHAT) chat.toggle();
        else if (!guide.dockOpen) guide.dockOpenFocus();
        else guide.dockFocus = !guide.dockFocus;
        break;
    case A_HELP: goPage(pg == PG_HELP ? PG_PLAY : PG_HELP); break;
    case A_TOUR: if (tourView) { if (tourView->isOpen()) tourView->close(); else tourView->open(QString(), QString(), false, nullptr, true); } break;
    case A_XYMAP: set(S_XYMAP, (float)((u.set.xyMap + 1) % XY_COUNT)); break;
    case A_QUIT: close(); break;
    default:
        if (act >= A_SOUND_1 && act <= A_SOUND_10) sounds.selectSlot(act - A_SOUND_1);
        break;
    }
}
