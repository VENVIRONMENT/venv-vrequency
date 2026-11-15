// venv-vrequency — the words of the Help page: one list of rows (keys, what they do) for each of
// the three parts, built from the key map so a remapped key shows up here.
#include "app/pages/help/help_page.h"

std::vector<HelpRow> helpRows(const KeyMap &km, int helpPage) {
    using Row = HelpRow;
    auto K = [&km](int act, bool cmd = false) { return (cmd ? km.keyFor(A_COMMAND, false) + "+" : QString()) + km.keyFor(act, cmd); };
    std::vector<Row> rows;
    if (helpPage == 0) {
        rows = {
            {km.rows[2].left(1) + QString::fromUtf8(" ‥ ") + km.rows[2].right(1), "high string: left to right goes up the scale"},
            {km.rows[1].left(1) + QString::fromUtf8(" ‥ ") + km.rows[1].right(1), "middle string"},
            {km.rows[0].left(1) + QString::fromUtf8(" ‥ ") + km.rows[0].right(1), "low string (or chords, see below)"},
            {"hold + press", "hold a key, press another on its row: it SLIDES"},
            {"let go", "release the newer key: it slides back"},
            {K(A_CHROMATIC) + " (hold)", "chromatic: keys step in semitones"},
            {K(A_SUSTAIN) + " (hold)", "sustain: released notes keep ringing"},
            {K(A_PANIC), "panic: silence everything"},
            {K(A_LOOP), "loop pedal: count-in, record, play, overdub"},
            {"Left / Right", "pitch bend down / up (hold)"},
            {"Up / Down", "octave up / down, held notes sweep"},
            {QString::fromUtf8("1 ‥ 9, 0"), "sounds 1 to 10"},
            {K(A_CHORD_LAYER) + "+" + km.rows[0].left(1) + QString::fromUtf8("‥") + km.rows[0].right(1), QString::fromUtf8("backing chord I ‥ VII")},
            {K(A_CHORD_ROW), "toggle: bottom row plays chords"},
            {"Tab / Esc", "next page / back to the Synth page"},
            {"screen", "drag the scope = X-Y control, drag a string = slide"},
        };
    } else if (helpPage == 2) {
        rows = {
            {km.rows[2].left(4).split("", Qt::SkipEmptyParts).join(" "), "Pulse page: pads 1-4 (click, metal, snap, scratch)"},
            {km.rows[1].left(4).split("", Qt::SkipEmptyParts).join(" "), "Pulse page: pads 5-8 (clap, tom, thump, 808)"},
            {km.rows[2].mid(6, 4).split("", Qt::SkipEmptyParts).join(" ") + " / " + km.rows[1].mid(5, 4).split("", Qt::SkipEmptyParts).join(" "), "the same pads again, for the other hand"},
            {"Left / Right", "Pulse page: select the pad before / after"},
            {km.rows[0].left(1) + QString::fromUtf8(" ‥ ") + km.rows[0].right(1), "Pulse page: the low string still plays"},
            {K(A_LOOP), "loop pedal: pulse taps are recorded too"},
            {"Loop", "Pulse page soft key: the same loop pedal"},
            {"Latch", "the selected pad keeps going until tapped again"},
            {"Sync", "every pad's RATE snaps to a division of the tempo"},
            {"Type", "the selected pad's kind: click, metal, snap..."},
            {"Tone Length Rate", "the selected pad's sliders (and Level)"},
            {K(A_PLAY_LAST, true), "play the last recording back / stop it"},
        };
    } else {
        rows = {
            {K(A_RANDOM, true), "new sound: invent an instrument"},
            {K(A_MUTATE, true), "mutate the current sound a little"},
            {K(A_UNDO_SOUND, true), "undo: back to the previous sound"},
            {K(A_FAVOURITE, true), "save the sound as a favourite"},
            {K(A_KEY_UP, true), "key up a semitone (with " + K(A_CHROMATIC) + ": down)"},
            {K(A_SCALE_NEXT, true), "next scale (with " + K(A_CHROMATIC) + ": previous)"},
            {K(A_TAP, true) + " " + K(A_METRO, true), "tap tempo / metronome"},
            {K(A_BACK_MODE, true), "chord taps: progression / hold / drone"},
            {K(A_BACK_STYLE, true), "backing style: sustain / pulse / arpeggio"},
            {K(A_BACK_PLAY, true) + " " + K(A_BACK_CLEAR, true), "backing start-stop / clear backing"},
            {K(A_BACK_REMOVE, true), "remove the last chord of the progression"},
            {K(A_LOOP_UNDO, true), "undo the last loop layer"},
            {K(A_LOOP_PLAY, true) + " " + K(A_LOOP_CLEAR, true), "loop start-stop / clear the loop"},
            {K(A_XYMAP, true), "next touch-pad mapping"},
            {K(A_RECORD, true), "record what you hear to a WAV file"},
            {K(A_HELP, true) + " " + K(A_CHAT, true) + " " + K(A_QUIT, true), "this guide / chat dock (with Shift: Chat page) / quit"},
        };
    }
    return rows;
}
