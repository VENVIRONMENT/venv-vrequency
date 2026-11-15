// venv-vrequency — events: what any thread may ask of the instrument through its queue.
#pragma once
#include <cstdint>

namespace td {

enum EvType : uint8_t {
    EV_KEY,        // a = string (0 = lowest), b = column, c = down, d = chromatic
    EV_CHORD,      // a = degree 0..6 (tap)
    EV_SUSTAIN,    // a = on/off
    EV_BEND,       // f = -1..+1
    EV_XY,         // f = x, g = y (0..1), a = touching
    EV_PANIC,
    EV_SET,        // a = Setting, f = value
    EV_PATCHPARAM, // a = Param, f = value (lead part)
    EV_LOOP,       // a = LoopCmd
    EV_PADS,       // the chord pads: stop them, or bring back what they held
    EV_OCTAVE,     // a = delta (+1 / -1): held notes sweep with it
    EV_DRUM,       // a = Drum, f = velocity (0 = full): a pad hit; written into the beat while recording
    EV_BEAT,       // a = BeatCmd, b = drum, c = step
    EV_NOTE,       // a = string, b = id, c = down, f = MIDI pitch (any, not scale-locked), g = velocity 0..1: a note from outside the keyboard
};

enum LoopCmd { LC_PEDAL, LC_UNDO, LC_CLEAR, LC_PLAYSTOP };
// BT_ERASE: b = drum (-1 = every drum). BT_STEP: b = drum, c = step, d = 0 off / 1 on / 2 toggle.
// Switching to another beat: BT_NEXTROW (b = drum, c = its sixteen steps as bits) for each drum,
// then BT_SWITCH (d = kit). Stopped, it switches at once; playing, at the start of the next bar.
enum BeatCmd { BT_PLAYSTOP, BT_REC, BT_ERASE, BT_STEP, BT_NEXTROW, BT_SWITCH };

struct Event {
    uint8_t type = 0;
    int16_t a = 0, b = 0, c = 0, d = 0;
    float f = 0, g = 0;
};

} // namespace td
