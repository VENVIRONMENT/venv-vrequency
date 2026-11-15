// venv-vrequency — the synth's per-voice warp: FOLD, CRUSH, DOWNSAMPLE, RING MOD and BEND IN.
//
// Each note is warped on its own, after the filter and DRIVE and before its envelope and panning,
// so a fold or a crush keeps its character as the note fades. In order: DRIVE (the synth's own
// saturation, unchanged), FOLD, CRUSH, DOWNSAMPLE (the shared stages in core/fx/warp.h), then RING
// MOD (a sine at 1.41 times the note, multiplied in and blended). BEND IN starts each note up to
// two octaves off and slides it onto its pitch in 70 ms. With every warp parameter at 0 the voice
// skips all of it, so older sounds come out sample for sample as before.
// warpCurve() runs a ramp through the same code: the Sound page draws what the voice does.
#pragma once
#include "core/fx/warp.h"
#include "core/synth/params.h"

namespace td {

constexpr float kRingRatio = 1.41f;   // the ring sine, as a multiple of the note: inharmonic
constexpr float kBendInMs = 70;       // how long BEND IN takes to land

inline float voiceDrive(float y, float pre, float post) { return fastTanh(y * pre) * post; }
inline float drivePre(float d) { return 1.0f + d * 9.0f; }
inline float drivePost(float d) { return 1.0f / (1.0f + d * 2.2f); }

struct VoiceWarp {
    WarpShape shape;
    WarpGrid grid;
    float foldSm = 0, ringSm = 0, ringPh = 0, ringInc = 0, bendFrom = 0;
    int bendBlock = 0, bendBlocks = 1;
    bool on = false, gridOn = false, ringOn = false;

    // The pitch offset for this block, in semitones: from BEND IN at the start, easing to 0.
    float bendIn(float semis, bool fresh, float sr) {
        if (fresh) {
            bendFrom = semis; bendBlock = 0;
            bendBlocks = (int)(kBendInMs * 0.001f * sr / kBlock + 0.5f);
            if (bendBlocks < 1) bendBlocks = 1;
        }
        if (bendFrom == 0 || bendBlock >= bendBlocks) return 0;
        const float w = 1.0f - (float)bendBlock++ / bendBlocks;
        return bendFrom * w * w;
    }
    // The settings for this block. inc: the note's phase step per sample (for the ring sine).
    void block(const float *P, float inc, bool fresh) {
        const float foldT = P[P_FOLD], ringT = P[P_RING];
        if (fresh) { foldSm = foldT; ringSm = ringT; ringPh = 0; grid.reset(); }
        else { foldSm += 0.3f * (foldT - foldSm); ringSm += 0.3f * (ringT - ringSm); }
        shape.set(0, foldSm);
        grid.set(crushBits(P[P_CRUSH]), dsampKeep(P[P_DSAMP]));
        ringInc = inc * kRingRatio;
        if (ringInc > 0.49f) ringInc = 0.49f;
        gridOn = grid.on(); ringOn = ringSm > 1e-4f;
        on = shape.on() || gridOn || ringOn;
    }
    float operator()(float y) {
        y = shape(y);
        if (gridOn) y = grid(y);
        if (ringOn) {
            ringPh += ringInc; if (ringPh >= 1.0f) ringPh -= 1.0f;
            y += ringSm * (y * fastSin(ringPh) - y);
        }
        return y;
    }
};

// The voice's DRIVE, FOLD, CRUSH and DOWNSAMPLE applied to a ramp from -1 to 1 (n samples), as the
// voice would apply them. RING MOD is left out: it moves with time and the note, not with the input.
inline void warpCurve(const float *P, float *out, int n) {
    float Q[P_COUNT];
    for (int i = 0; i < P_COUNT; ++i) Q[i] = P[i];
    Q[P_RING] = 0;
    VoiceWarp w;
    w.block(Q, 0, true);
    const float d = P[P_DRIVE], pre = drivePre(d), post = drivePost(d);
    for (int i = 0; i < n; ++i) {
        float y = n > 1 ? -1.0f + 2.0f * i / (n - 1) : 0;
        if (d > 0.001f) y = voiceDrive(y, pre, post);
        out[i] = w.on ? w(y) : y;
    }
}

} // namespace td
