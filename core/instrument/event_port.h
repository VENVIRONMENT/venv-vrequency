// venv-vrequency — the way in to the engine: the control thread's calls that queue an event or a
// patch for the audio thread. One writer only (see the top of instrument.h).
#pragma once
#include "core/instrument/events.h"
#include "core/instrument/spsc.h"
#include "core/synth/params.h"
#include <atomic>
#include <cstdint>

namespace td {

struct PatchMsg { int part; Patch patch; };

class EventPort {
public:
    bool push(const Event &e) { if (evq.push(e)) return true; ++dropped; return false; }
    void key(int string, int col, bool down, bool chromatic = false) {
        Event e; e.type = EV_KEY; e.a = (int16_t)string; e.b = (int16_t)col; e.c = down; e.d = chromatic; push(e);
    }
    void set(int setting, float v) { Event e; e.type = EV_SET; e.a = (int16_t)setting; e.f = v; push(e); }
    void cmd(uint8_t type, int a = 0, int b = 0) { Event e; e.type = type; e.a = (int16_t)a; e.b = (int16_t)b; push(e); }
    // a note at an exact pitch on a string, from outside the keyboard (id: any number 0x400 up, to release it again)
    void note(int string, int id, float midi, bool down, float vel = 1.0f) {
        Event e; e.type = EV_NOTE; e.a = (int16_t)string; e.b = (int16_t)id; e.c = down; e.f = midi; e.g = vel; push(e);
    }
    // the pulse pads: a tap, one setting (PulseField), latch on or off
    void hit(int pad, float vel = 1.0f) { Event e; e.type = EV_PULSE; e.a = (int16_t)pad; e.f = vel; push(e); }
    void pulseSet(int pad, int field, float v) { Event e; e.type = EV_PULSESET; e.a = (int16_t)pad; e.b = (int16_t)field; e.f = v; push(e); }
    void pulseLatch(int pad, bool on) { Event e; e.type = EV_PULSELATCH; e.a = (int16_t)pad; e.b = on; push(e); }

protected:
    Spsc<Event, 1024> evq;
    Spsc<PatchMsg, 16> patchq;
    std::atomic<uint32_t> dropped{0};
};

} // namespace td
