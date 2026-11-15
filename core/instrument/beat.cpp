// venv-vrequency — the beat.
//
// One bar of sixteen steps for eight drums, locked to the beat clock: bars start on
// multiples of four beats, the same grid the looper uses.
#include "core/instrument/instrument.h"
#include "core/drums/drums.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace td {

void Instrument::drumHit(int drum, float vel) {
    if (drum < 0 || drum >= kDrums) return;
    drums.trigger(st.kit, drum, vel);
    ++drumHits[drum];
    if (!(beatPlaying && beatRec)) return;
    // The player hits in time with what they hear, which left the engine latencyMs ago.
    double pos = beat - st.latencyMs * 0.001 * st.bpm / 60.0;
    long q = std::lround(pos * 4.0);
    int step = (int)(((q % kDrumSteps) + kDrumSteps) % kDrumSteps);
    beatPat[drum] |= (uint16_t)(1u << step);
    if (q > lastDrumStep) skipStep[drum] = q; // that step is still to come: it has just been heard
}

void Instrument::beatCmd(int cmd, int drum, int step, int arg) {
    switch (cmd) {
    case BT_PLAYSTOP:
        beatPlaying = !beatPlaying;
        if (beatPlaying) {
            if (transportIdle()) { beat = 0; lastDrumStep = -1; }   // start on step 1, now
            else lastDrumStep = (long)std::floor(beat * 4.0);                       // join the bar in progress
        } else {
            beatRec = false;
            if (beatSwitching) beatSwitchNow();
        }
        for (long &s : skipStep) s = -1;
        break;
    case BT_REC:
        beatRec = !beatRec;
        if (beatRec && !beatPlaying) beatCmd(BT_PLAYSTOP, 0, 0, 0);
        break;
    case BT_ERASE:
        for (int d = 0; d < kDrums; ++d) if (drum < 0 || drum == d) beatPat[d] = 0;
        break;
    case BT_NEXTROW:
        if (drum >= 0 && drum < kDrums) beatNext[drum] = (uint16_t)step;
        break;
    case BT_SWITCH:
        beatNextKit = std::clamp(arg, 0, kKits - 1);
        beatSwitching = true;
        if (!beatPlaying) beatSwitchNow();
        break;
    case BT_STEP:
        if (drum >= 0 && drum < kDrums && step >= 0 && step < kDrumSteps) {
            uint16_t bit = (uint16_t)(1u << step);
            if (arg == 0) beatPat[drum] &= (uint16_t)~bit;
            else if (arg == 1) beatPat[drum] |= bit;
            else beatPat[drum] ^= bit;
        }
        break;
    }
}

void Instrument::beatSwitchNow() {
    for (int d = 0; d < kDrums; ++d) { beatPat[d] = beatNext[d]; skipStep[d] = -1; }
    st.kit = beatNextKit;
    beatSwitching = false;
}

void Instrument::tickBeat(double, double b1) {
    if (!beatPlaying) return;
    long step = (long)std::floor(b1 * 4.0);
    if (step == lastDrumStep) return;
    lastDrumStep = step;
    int s = (int)(((step % kDrumSteps) + kDrumSteps) % kDrumSteps);
    if (beatSwitching && s == 0) beatSwitchNow(); // the new beat starts on the bar line
    for (int d = 0; d < kDrums; ++d) {
        if (!(beatPat[d] & (1u << s))) continue;
        if (skipStep[d] == step) { skipStep[d] = -1; continue; }
        drums.trigger(st.kit, d, stepVelocity(d, s));
        ++drumHits[d];
    }
}

} // namespace td
