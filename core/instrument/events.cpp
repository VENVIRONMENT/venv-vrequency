// venv-vrequency — the audio thread's side of the event queue: settings and commands.
#include "core/instrument/instrument.h"
#include "core/drums/drums.h"
#include "core/dsp.h"
#include "core/synth/params.h"
#include "core/synth/scale.h"
#include <algorithm>
#include <cmath>

namespace td {

void Instrument::applySetting(int s, float v) {
    switch (s) {
    case S_ROOT: st.tuning.root = ((int)v % 12 + 12) % 12; break;
    case S_SCALE: st.tuning.scale = std::clamp((int)v, 0, SC_COUNT - 1); break;
    case S_OCTAVE: st.tuning.octave = std::clamp((int)v, -3, 3); break;
    case S_ROWINT: st.tuning.rowInterval = std::clamp((int)v, 1, 12); break;
    case S_BPM: st.bpm = clampf(v, 40, 240); break;
    case S_VOLUME: st.volume = clampf(v, 0, 1); break;
    case S_XYMAP: st.xyMap = std::clamp((int)v, 0, XY_COUNT - 1); applyXY(0, st.xyMap, xyX, xyY, false, false); break;
    case S_XYHOLD: st.xyHold = v > 0.5f; if (!st.xyHold && !xyTouch) applyXY(0, st.xyMap, xyX, xyY, false, false); break;
    case S_PADMODE: st.padMode = std::clamp((int)v, 0, PM_COUNT - 1); break;
    case S_METRO: st.metro = v > 0.5f; break;
    case S_BENDRANGE: st.bendRange = clampf(v, 1, 12); break;
    case S_BENDTIME: st.bendTime = clampf(v, 10, 1000); break;
    case S_GLIDESCALE: st.glideScale = clampf(v, 0, 4); break;
    case S_LOOPVOL: st.loopVol = clampf(v, 0, 1); break;
    case S_LATENCY: st.latencyMs = clampf(v, 0, 500); break;
    case S_KIT: st.kit = std::clamp((int)v, 0, kKits - 1); break;
    case S_BEATVOL: st.beatVol = clampf(v, 0, 1); break;
    default: break;
    }
}

void Instrument::handle(const Event &e) {
    switch (e.type) {
    case EV_KEY:
        if (e.c) keyDown(e.a, e.b, e.d != 0); else keyUp(e.a, e.b);
        break;
    case EV_NOTE:
        if (e.c) noteDown(e.a, e.b, e.f, e.g); else keyUp(e.a, e.b);
        break;
    case EV_CHORD: chordTap(e.a); break;
    case EV_SUSTAIN: setSustain(e.a != 0); break;
    case EV_BEND:
        bendTarget[0] = clampf(e.f, -1, 1) * st.bendRange;
        loopRecord(OP_BEND, 0, bendTarget[0], 0);
        break;
    case EV_XY: {
        xyX = e.f; xyY = e.g; xyTouch = e.a != 0;
        applyXY(0, st.xyMap, xyX, xyY, xyTouch, st.xyHold);
        int flags = st.xyMap | (xyTouch ? 0x80 : 0) | (st.xyHold ? 0x40 : 0);
        loopRecord(OP_XY, flags, xyX, xyY);
        break;
    }
    case EV_PANIC: panic(); break;
    case EV_SET: applySetting(e.a, e.f); break;
    case EV_PATCHPARAM:
        if (e.a >= 0 && e.a < P_COUNT) {
            const ParamInfo &m = paramInfo(e.a);
            float v = clampf(e.f, m.min, m.max);
            parts[0].patch.p[e.a] = v;
            loopRecord(OP_PARAM, e.a, v, 0);
        }
        break;
    case EV_LOOP:
        switch (e.a) {
        case LC_PEDAL: loopPedal(); break;
        case LC_UNDO: loopUndo(); break;
        case LC_CLEAR: loopClear(); break;
        case LC_PLAYSTOP: loopPlayStop(); break;
        }
        break;
    case EV_PADS: padsPlaying = !padsPlaying; break;
    case EV_OCTAVE: shiftOctave(e.a); break;
    case EV_DRUM: drumHit(e.a, e.f > 0 ? e.f : 1.0f); break;
    case EV_BEAT: beatCmd(e.a, e.b, e.c, e.d); break;
    }
}

} // namespace td
