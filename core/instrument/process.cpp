// venv-vrequency — the audio block: events in, the clocks ticked, voices, pulses and effects mixed out.
#include "core/instrument/instrument.h"
#include "core/dsp.h"
#include "core/synth/params.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>

namespace td {

void Instrument::block(float *l, float *r, int n) {
    PatchMsg pm;
    while (patchq.pop(pm)) {
        if (pm.part == 0) setLeadPatch(pm.patch);
        else if (pm.part > 0 && pm.part < kParts) parts[pm.part].patch = pm.patch;
    }
    Event e;
    while (evq.pop(e)) handle(e);

    // a loop layer that switched the crusher on must not leave it on once the loop stops or goes
    if (glitchOwner >= 2 && ((loopState != L_PLAY && loopState != L_OVERDUB) || glitchOwner - 2 >= nLayers)) {
        crush.set(false, 0, 0); glitchOwner = -1;
    }
    double b0 = beat, b1 = beat + (double)st.bpm / 60.0 / sr * n;
    tickLoop(b0, b1);
    tickBacking(b0, b1);
    beat = b1;

    parts[0].gain = 1.0f;
    parts[1].gain = st.backVol;
    float bc = 1.0f - std::exp(-(float)n / (st.bendTime * 0.001f * sr * 0.33f));
    for (int p = 0; p < kParts; ++p) {
        parts[p].bend += bc * (bendTarget[p] - parts[p].bend);
        const float *P = parts[p].patch.p;
        float tc = P[P_CHORUS], tdl = clampf(P[P_DLYMIX] + parts[p].modDly, 0, 1), trv = clampf(P[P_REVMIX] + parts[p].modRev, 0, 1);
        sendSm[p][0] += 0.2f * (tc - sendSm[p][0]);
        sendSm[p][1] += 0.2f * (tdl - sendSm[p][1]);
        sendSm[p][2] += 0.2f * (trv - sendSm[p][2]);
    }

    float dryL[kBlock], dryR[kBlock], sC[kBlock], sD[kBlock], sR[kBlock], tmp[kBlock];
    for (int i = 0; i < n; ++i) dryL[i] = dryR[i] = sC[i] = sD[i] = sR[i] = 0;

    int nActive = 0;
    for (int vi = 0; vi < kVoices; ++vi) {
        Voice &v = voices[vi];
        if (!v.active()) continue;
        ++nActive;
        int p = v.part;
        v.render(parts[p], sr, tmp, n);
        float gl = v.panL, gr = v.panR;
        float c = sendSm[p][0], d = sendSm[p][1], rv = sendSm[p][2];
        for (int i = 0; i < n; ++i) {
            float x = tmp[i];
            dryL[i] += x * gl; dryR[i] += x * gr;
            sC[i] += x * c; sD[i] += x * d; sR[i] += x * rv;
        }
    }

    if (pulses.active()) {
        renderPulses(dryL, dryR, n, b0, b1);
        ++nActive; // keeps the effects running and the idle shortcut off
    }
    if (freq.active()) { renderFreq(dryL, dryR, sD, n, b0, b1); ++nActive; }
    else freq.idle(n);

    // Nothing sounding and every effect tail has died away: skip the effects
    // entirely (saves battery while the instrument sits idle).
    bool recording = recOn.load(std::memory_order_relaxed);
    if (nActive == 0 && clickEnv <= 1e-4f && silentBlocks > 600 && !recording) {
        for (int i = 0; i < n; ++i) l[i] = r[i] = 0;
        uint32_t sw0 = scopeW.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) scope[(sw0 + i) & (kScopeSize - 1)] = 0;
        scopeW.store(sw0 + n, std::memory_order_release);
        peakHold *= 0.98f;
        frames += n;
        return;
    }

    const float *LP = parts[0].patch.p;
    float wetL[kBlock], wetR[kBlock];
    for (int i = 0; i < n; ++i) wetL[i] = wetR[i] = 0;
    chorus.process(sC, wetL, wetR, n);
    for (int i = 0; i < n; ++i) { wetL[i] *= 0.7f; wetR[i] *= 0.7f; }
    float dsec = delayBeats((int)LP[P_DLYDIV]) * 60.0f / st.bpm;
    delay.process(sD, wetL, wetR, n, dsec, LP[P_DLYFB]);
    reverb.process(sR, wetL, wetR, n, LP[P_REVSIZE]);

    float clickDec = std::exp(-1.0f / (0.012f * sr));
    float pk = 0;
    for (int i = 0; i < n; ++i) {
        volSm += 0.002f * (st.volume - volSm);
        float ol = (dryL[i] + wetL[i]) * volSm, orr = (dryR[i] + wetR[i]) * volSm;
        if (clickEnv > 1e-4f) {
            clickPh += clickFreq / sr; if (clickPh >= 1.0f) clickPh -= 1.0f;
            float c = fastSin(clickPh) * clickEnv * 0.22f;
            clickEnv *= clickDec;
            ol += c; orr += c;
        }
        l[i] = ol; r[i] = orr;
    }
    crush.process(l, r, n);
    limiter.process(l, r, n);

    uint32_t sw = scopeW.load(std::memory_order_relaxed);
    for (int i = 0; i < n; ++i) {
        float mono = 0.5f * (l[i] + r[i]);
        scope[(sw + i) & (kScopeSize - 1)] = mono;
        float a = std::fabs(l[i]) > std::fabs(r[i]) ? std::fabs(l[i]) : std::fabs(r[i]);
        if (a > pk) pk = a;
    }
    scopeW.store(sw + n, std::memory_order_release);
    if (pk > peakHold) peakHold = pk; else peakHold *= 0.98f;
    if (nActive == 0 && pk < 1e-6f) ++silentBlocks; else silentBlocks = 0;

    if (recording) {
        uint32_t w = recW.load(std::memory_order_relaxed), rd = recR.load(std::memory_order_acquire);
        const uint32_t cap = (uint32_t)(recBuf.size() / 2);
        for (int i = 0; i < n; ++i) {
            if (w - rd >= cap) { recDrops.fetch_add(1); break; }
            recBuf[(w & (cap - 1)) * 2] = l[i];
            recBuf[(w & (cap - 1)) * 2 + 1] = r[i];
            ++w;
        }
        recW.store(w, std::memory_order_release);
    }
    frames += n;
}

void Instrument::process(float *outL, float *outR, int nframes) {
    std::lock_guard<std::mutex> lk(mx);
    int done = 0;
    while (done < nframes) {
        int n = std::min(kBlock, nframes - done);
        block(outL + done, outR + done, n);
        done += n;
    }
    publish();
}

} // namespace td
