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
    EV_BACK,       // a = BackCmd, b = argument
    EV_OCTAVE,     // a = delta (+1 / -1): held notes sweep with it
    EV_PULSE,      // a = pulse pad, f = velocity (0 = full): a tap; written into the loop while it records
    EV_PULSESET,   // a = pulse pad, b = PulseField, f = value
    EV_PULSELATCH, // a = pulse pad, b = on / off
    EV_PULSEHOLD,  // a = pulse pad, b = key down / up: the pad goes while held (and sustained); recorded
    EV_NOTE,       // a = string, b = id, c = down, f = MIDI pitch (any, not scale-locked), g = velocity 0..1: a note from outside the keyboard
};

enum LoopCmd { LC_PEDAL, LC_UNDO, LC_CLEAR, LC_PLAYSTOP };
enum BackCmd { BC_PLAYSTOP, BC_CLEAR, BC_REMOVE_LAST, BC_RESYNC_SOUND, BC_SELECT_SLOT };

struct Event {
    uint8_t type = 0;
    int16_t a = 0, b = 0, c = 0, d = 0;
    float f = 0, g = 0;
};

} // namespace td
