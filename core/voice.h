// venv-vrequency — one synthesiser voice and the Part (sound + live modulation) it plays.
#pragma once
#include "dsp.h"
#include "params.h"

namespace td {

// A Part is one independent instrument layer: the live lead, the backing, or a
// loop layer. Each owns its own patch so backing/loops keep their sound when
// the lead changes.
struct Part {
    Patch patch;
    float bend = 0;     // semitones
    float modCut = 0;   // octaves added to cutoff
    float modRes = 0;   // added to resonance
    float modVib = 0;   // cents added to vibrato depth
    float modMix = 0;   // added to osc B mix (timbre morph)
    float modDrive = 0; // added to drive
    float modDly = 0;   // added to delay send
    float modRev = 0;   // added to reverb send
    float gain = 1.0f;
    void clearMod() { modCut = modRes = modVib = modMix = modDrive = modDly = modRev = 0; }
};

struct Voice {
    // identity
    int part = -1;
    int slot = -1;        // string / chord-voice index within the part
    bool gate = false;
    uint32_t age = 0;     // blocks since note-on
    uint64_t serial = 0;  // allocation order, for stealing

    // pitch (MIDI note numbers, fractional)
    float note = 60, target = 60, glideStep = 0;
    int glideBlocks = 0;

    float vel = 1, panL = 0.7f, panR = 0.7f;

    // oscillator state
    float p1 = 0, p2 = 0, pu = 0, ps = 0;
    float inc1 = 0, inc2 = 0, incU = 0;
    float vibPh = 0, lfoPh = 0;
    Env amp, fenv;
    Svf filt;
    Rng rng;
    // smoothed controls
    float cutSm = 1000, levelSm = 0, mixSm = 0, subSm = 0, noiseSm = 0, driveSm = 0, uniSm = 0, fmSm = 0;
    bool fresh = true;
    bool relPending = false; // released before it ever sounded: let the attack speak first
    float lvlPrev = 0;   // per-sample level ramp (no zipper noise on patch changes)
    float fade = 1.0f, fadeStep = 0; // fast fade-out used by panic / stealing

    bool active() const { return part >= 0; }
    bool sounding() const { return part >= 0 && amp.active(); }

    void start(int part_, int slot_, float midiNote, float velocity, float pan, uint64_t serial_) {
        part = part_; slot = slot_; serial = serial_;
        note = target = midiNote;
        glideBlocks = 0; glideStep = 0;
        vel = velocity;
        float a = (pan * 0.5f + 0.5f) * (kPi * 0.5f);
        panL = std::cos(a); panR = std::sin(a);
        rng.s = (uint32_t)(serial_ * 2654435761u) | 1u;
        p1 = 0; ps = 0;
        p2 = rng.uni(); pu = rng.uni();
        vibPh = 0; lfoPh = rng.uni();
        amp.reset(); fenv.reset(); filt.reset();
        amp.gateOn(); fenv.gateOn();
        gate = true; age = 0; fresh = true; relPending = false;
        fade = 1.0f; fadeStep = 0;
    }

    // Continuous slide to a new pitch: no envelope retrigger.
    void glideTo(float midiNote, float glideMs, float sr) {
        target = midiNote;
        float blocks = glideMs * 0.001f * sr / kBlock;
        if (blocks < 1.0f) { note = target; glideBlocks = 0; glideStep = 0; return; }
        glideBlocks = (int)(blocks + 0.5f);
        glideStep = (target - note) / glideBlocks;
    }

    void release() {
        gate = false;
        if (age < 2) { relPending = true; return; } // a tap shorter than one block still sounds
        amp.gateOff(); fenv.gateOff();
    }
    void kill(float sr, float ms = 8.0f) { // click-free fast stop
        gate = false;
        fadeStep = 1.0f / (ms * 0.001f * sr);
    }
    void free() { part = -1; gate = false; amp.reset(); fenv.reset(); }

    // Render one control block (n <= kBlock) into `out` (mono). Returns false
    // when the voice has finished and was freed.
    bool render(const Part &pt, float sr, float *out, int n) {
        const float *P = pt.patch.p;
        // ---- pitch ----
        if (glideBlocks > 0) {
            note += glideStep;
            if (--glideBlocks == 0) note = target;
        }
        float t = age * (float)kBlock / sr;
        float vibEnv = clampf((t - P[P_VIBDELAY]) * 4.0f, 0.0f, 1.0f);
        vibPh += P[P_VIBRATE] * n / sr;
        vibPh -= (int)vibPh;
        float vibCents = (P[P_VIBDEPTH] * vibEnv + pt.modVib) * fastSin(vibPh);
        lfoPh += P[P_LFORATE] * n / sr;
        lfoPh -= (int)lfoPh;
        float lfo = fastSin(lfoPh);

        float semis = note + pt.bend + vibCents * 0.01f;
        float f1 = 440.0f * std::exp2((semis - 69.0f) * (1.0f / 12.0f));
        if (f1 > sr * 0.45f) f1 = sr * 0.45f;
        float tInc1 = f1 / sr;
        float tInc2 = tInc1 * std::exp2((P[P_SEMI2] + P[P_DETUNE] * 0.01f) * (1.0f / 12.0f));
        if (tInc2 > 0.45f) tInc2 = 0.45f;
        float tIncU = tInc1 * std::exp2(P[P_SPREAD] * (1.0f / 1200.0f));
        if (fresh) { inc1 = tInc1; inc2 = tInc2; incU = tIncU; }
        float d1 = (tInc1 - inc1) / n, d2 = (tInc2 - inc2) / n, dU = (tIncU - incU) / n;

        // ---- envelopes / filter (control rate) ----
        float aC = Env::attackCoef(P[P_ATK], sr), dC = Env::decayCoef(P[P_DEC], sr);
        float rC = Env::decayCoef(P[P_REL], sr), sus = P[P_SUS];
        float faC = Env::attackCoef(P[P_FATK], sr * (1.0f / n));
        float fdC = Env::decayCoef(P[P_FDEC], sr * (1.0f / n));
        float frC = Env::decayCoef(P[P_FREL], sr * (1.0f / n));
        float fe = fenv.tick(faC, fdC, P[P_FSUS], frC);

        float cutOct = P[P_FENV] * fe + P[P_LFOCUT] * lfo + P[P_KTRK] * (note - 60.0f) * (1.0f / 12.0f) + pt.modCut;
        float cutT = P[P_CUTOFF] * std::exp2(clampf(cutOct, -8.0f, 8.0f));
        cutT = clampf(cutT, 30.0f, 18000.0f);
        const float sm = 0.3f;
        float mixT = clampf(P[P_MIX2] + pt.modMix, 0.0f, 1.0f);
        float driveT = clampf(P[P_DRIVE] + pt.modDrive, 0.0f, 1.0f);
        float levelT = P[P_LEVEL] * vel * pt.gain;
        bool wasFresh = fresh;
        if (fresh) {
            cutSm = cutT; mixSm = mixT; subSm = P[P_SUB]; noiseSm = P[P_NOISE];
            driveSm = driveT; uniSm = P[P_UNISON]; fmSm = P[P_FM]; levelSm = levelT;
            fresh = false;
        } else {
            cutSm += sm * (cutT - cutSm); mixSm += sm * (mixT - mixSm);
            subSm += sm * (P[P_SUB] - subSm); noiseSm += sm * (P[P_NOISE] - noiseSm);
            driveSm += sm * (driveT - driveSm); uniSm += sm * (P[P_UNISON] - uniSm);
            fmSm += sm * (P[P_FM] - fmSm); levelSm += sm * (levelT - levelSm);
        }
        filt.set(cutSm, clampf(P[P_RESO] + pt.modRes, 0.0f, 0.97f), sr);

        int w1 = (int)P[P_WAVE1], w2 = (int)P[P_WAVE2], fmode = (int)P[P_FMODE];
        float pw = clampf(P[P_PW] + P[P_LFOPW] * lfo, 0.08f, 0.92f);
        float mix2 = mixSm, mix1 = 1.0f - mixSm;
        float uni = uniSm, sub = subSm, noise = noiseSm, fm = fmSm;
        bool useFm = fm > 0.001f, useUni = uni > 0.001f, useSub = sub > 0.001f, useNoise = noise > 0.001f;
        bool useO2 = mix2 > 0.001f;
        float pre = 1.0f + driveSm * 9.0f;
        float post = 1.0f / (1.0f + driveSm * 2.2f);
        bool useDrive = driveSm > 0.001f;
        float trem = 1.0f - P[P_LFOAMP] * (0.5f - 0.5f * lfo);
        float lvlT = levelSm * trem;
        if (wasFresh) lvlPrev = lvlT;
        float lvl = lvlPrev, dLvl = (lvlT - lvlPrev) / n;
        lvlPrev = lvlT;
        float incS = inc1 * 0.5f;

        for (int i = 0; i < n; ++i) {
            inc1 += d1; inc2 += d2; incU += dU;
            p2 += inc2; if (p2 >= 1.0f) p2 -= 1.0f;
            p1 += inc1; if (p1 >= 1.0f) p1 -= 1.0f;
            float o1;
            if (useFm) {
                float pm = p1 + fm * 0.25f * fastSin(p2);
                pm -= std::floor(pm);
                o1 = oscSample(w1, pm, inc1, pw);
            } else {
                o1 = oscSample(w1, p1, inc1, pw);
            }
            if (useUni) {
                pu += incU; if (pu >= 1.0f) pu -= 1.0f;
                o1 = o1 * (1.0f - 0.4f * uni) + oscSample(w1, pu, incU, pw) * uni * 0.8f;
            }
            float x = o1 * mix1;
            if (useO2) x += oscSample(w2, p2, inc2, pw) * mix2;
            if (useSub) { ps += incS; if (ps >= 1.0f) ps -= 1.0f; x += fastSin(ps) * sub; }
            if (useNoise) x += rng.bipolar() * noise;
            float y = filt.tick(x * 0.5f, fmode);
            if (useDrive) y = fastTanh(y * pre) * post;
            float e = amp.tick(aC, dC, sus, rC);
            if (fadeStep > 0) {
                fade -= fadeStep;
                if (fade <= 0) { fade = 0; }
            }
            lvl += dLvl;
            out[i] = y * e * lvl * fade;
        }
        ++age;
        if (relPending && age >= 2) { relPending = false; amp.gateOff(); fenv.gateOff(); }
        if (!amp.active() || (fadeStep > 0 && fade <= 0)) { free(); return false; }
        return true;
    }
};

} // namespace td
