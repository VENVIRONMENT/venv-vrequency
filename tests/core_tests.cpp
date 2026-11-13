// venv vrequency — engine tests. Renders the instrument offline and measures what
// actually comes out (frequencies, envelopes, voices), rather than trusting
// internal state alone.
//
//   vrequency_tests          run all checks
//   vrequency_tests --bench  CPU benchmark at maximum polyphony
#include "analysis.h"
#include "instrument.h"
#include "presets.h"
#include "sounds.h"
#include <algorithm>
#include <string>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

using namespace td;

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        if (cond) { ++g_pass; }                                               \
        else { ++g_fail; std::printf("  FAIL %s:%d  ", __FILE__, __LINE__);   \
               std::printf(__VA_ARGS__); std::printf("\n"); }                 \
    } while (0)
#define SECTION(name) std::printf("- %s\n", name)

static const float SR = 48000.0f;

struct Rig {
    Instrument inst{SR};
    std::vector<float> L, R;
    Rig() { enableFlushToZero(); }
    void run(double seconds, const std::function<void()> &watch = nullptr) {
        int total = (int)(seconds * SR);
        float l[256], r[256];
        while (total > 0) {
            int n = total < 256 ? total : 256;
            inst.process(l, r, n);
            L.insert(L.end(), l, l + n);
            R.insert(R.end(), r, r + n);
            total -= n;
            if (watch) watch();
        }
    }
    void clear() { L.clear(); R.clear(); }
    // last `seconds` of rendered audio
    const float *tail(double seconds, int &n) const {
        n = (int)(seconds * SR);
        if (n > (int)L.size()) n = (int)L.size();
        return L.data() + L.size() - n;
    }
    int gated(int part) const {
        int c = 0;
        for (int i = 0; i < kVoices; ++i)
            if (inst.voice(i).active() && inst.voice(i).part == part && inst.voice(i).gate) ++c;
        return c;
    }
    int active(int part) const {
        int c = 0;
        for (int i = 0; i < kVoices; ++i)
            if (inst.voice(i).active() && inst.voice(i).part == part) ++c;
        return c;
    }
};

static Patch purePatch() {
    Patch p;
    p.setName("Pure");
    p.p[P_WAVE1] = W_SINE; p.p[P_MIX2] = 0; p.p[P_UNISON] = 0; p.p[P_SUB] = 0; p.p[P_NOISE] = 0;
    p.p[P_ATK] = 0.002f; p.p[P_DEC] = 0.1f; p.p[P_SUS] = 1.0f; p.p[P_REL] = 0.05f;
    p.p[P_FMODE] = F_LP; p.p[P_CUTOFF] = 16000; p.p[P_RESO] = 0; p.p[P_FENV] = 0; p.p[P_KTRK] = 0;
    p.p[P_VIBDEPTH] = 0; p.p[P_LFOCUT] = 0; p.p[P_LFOAMP] = 0; p.p[P_GLIDE] = 200;
    p.p[P_DRIVE] = 0; p.p[P_CHORUS] = 0; p.p[P_DLYMIX] = 0; p.p[P_REVMIX] = 0; p.p[P_LEVEL] = 0.5f;
    return p;
}

static float rms(const float *x, int n) {
    double e = 0;
    for (int i = 0; i < n; ++i) e += (double)x[i] * x[i];
    return n > 0 ? (float)std::sqrt(e / n) : 0;
}
static float peak(const std::vector<float> &v) {
    float p = 0;
    for (float x : v) if (std::fabs(x) > p) p = std::fabs(x);
    return p;
}
// frequency from interpolated positive-going zero crossings
static float measureHz(const float *x, int n) {
    double first = -1, last = -1; int count = 0;
    for (int i = 1; i < n; ++i)
        if (x[i - 1] < 0 && x[i] >= 0) {
            double t = (i - 1) + (-x[i - 1]) / (double)(x[i] - x[i - 1]);
            if (first < 0) first = t;
            last = t; ++count;
        }
    if (count < 2) return 0;
    return (float)((count - 1) * SR / (last - first));
}
static float goertzel(const float *x, int n, float f) {
    float c = 2.0f * std::cos(2.0f * kPi * f / SR), s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { // Hann window keeps neighbouring notes from leaking in
        float w = 0.5f - 0.5f * std::cos(2.0f * kPi * i / (n - 1));
        float s0 = x[i] * w + c * s1 - s2; s2 = s1; s1 = s0;
    }
    return std::sqrt(std::fabs(s1 * s1 + s2 * s2 - c * s1 * s2)) / n * 4;
}
// brightness proxy: energy of the first difference relative to the signal
static float brightness(const float *x, int n) {
    double d = 0, e = 0;
    for (int i = 1; i < n; ++i) { d += (double)(x[i] - x[i - 1]) * (x[i] - x[i - 1]); e += (double)x[i] * x[i]; }
    return e > 0 ? (float)std::sqrt(d / e) : 0;
}
static bool near(float a, float b, float relTol) { return std::fabs(a - b) <= relTol * std::fabs(b); }

// ---------------------------------------------------------------------------

static void testScales() {
    SECTION("scale lock: note tables");
    Tuning t; // defaults: A minor pentatonic
    int expect0[7] = {45, 48, 50, 52, 55, 57, 60}; // A2 C3 D3 E3 G3 A3 C4
    for (int c = 0; c < 7; ++c) CHECK(t.noteFor(0, c, false) == expect0[c], "bottom row col %d = %d", c, t.noteFor(0, c, false));
    CHECK(t.noteFor(1, 0, false) == 50, "middle string starts on D3, got %d", t.noteFor(1, 0, false));
    CHECK(t.noteFor(2, 0, false) == 55, "top string starts on G3, got %d", t.noteFor(2, 0, false));
    CHECK(t.noteFor(0, 3, true) == 48, "chromatic override gives semitone steps");
    for (int sc = 0; sc < SC_COUNT; ++sc)
        for (int root = 0; root < 12; ++root) {
            Tuning u; u.scale = sc; u.root = root;
            const ScaleDef &d = scaleDef(sc);
            bool allIn = true, rising = true;
            for (int row = 0; row < 3; ++row) {
                int prev = -1;
                for (int col = 0; col < 10; ++col) {
                    int n = u.noteFor(row, col, false);
                    int pc = ((n - root) % 12 + 12) % 12;
                    bool in = false;
                    for (int i = 0; i < d.n; ++i) if (d.steps[i] == pc) in = true;
                    if (!in) allIn = false;
                    if (n <= prev) rising = false;
                    prev = n;
                }
            }
            CHECK(allIn, "%s root %d: every key is in the scale", d.name, root);
            CHECK(rising, "%s root %d: pitch rises left to right", d.name, root);
        }
    SECTION("chords: diatonic triads");
    Tuning am; // A minor family
    Chord i = buildChord(am, 0, false), iv = buildChord(am, 3, false), v7 = buildChord(am, 4, true), III = buildChord(am, 2, false);
    CHECK(std::strcmp(i.name, "Am") == 0, "i = %s", i.name);
    CHECK(std::strcmp(iv.name, "Dm") == 0, "iv = %s", iv.name);
    CHECK(std::strcmp(v7.name, "Em7") == 0, "v7 = %s", v7.name);
    CHECK(std::strcmp(III.name, "C") == 0, "III = %s", III.name);
    CHECK(i.count == 4 && v7.count == 5, "triad has bass+3, seventh has bass+4");
    Tuning cm; cm.root = 0; cm.scale = SC_MAJOR;
    CHECK(std::strcmp(buildChord(cm, 4, true).name, "G7") == 0, "V7 in C = %s", buildChord(cm, 4, true).name);
    CHECK(std::strcmp(buildChord(cm, 6, false).name, "Bdim") == 0, "vii in C = %s", buildChord(cm, 6, false).name);
}

static void testFrequenciesAndGlide() {
    SECTION("a key press sounds the correct frequency");
    Rig r;
    r.inst.setPatch(0, purePatch());
    r.inst.key(0, 0, true);
    r.run(0.5);
    int n; const float *x = r.tail(0.3, n);
    float hz = measureHz(x, n);
    CHECK(near(hz, 110.0f, 0.002f), "A2 expected 110 Hz, measured %.3f", hz);
    CHECK(rms(x, n) > 0.05f, "note is audible (rms %.4f)", rms(x, n));
    CHECK(r.gated(0) == 1, "one voice sounding");
    uint64_t serial0 = 0; int vi = -1;
    for (int i = 0; i < kVoices; ++i) if (r.inst.voice(i).gate) { serial0 = r.inst.voice(i).serial; vi = i; }

    SECTION("second key on the same string slides (no re-attack)");
    r.inst.key(0, 2, true); // D3 = 146.83 Hz, glide 200 ms
    std::vector<float> track; bool reAttack = false; float minEnv = 1;
    for (int step = 0; step < 15; ++step) {
        r.run(0.02);
        const float *w = r.tail(0.02, n);
        track.push_back(measureHz(w, n));
        const Voice &v = r.inst.voice(vi);
        if (v.amp.st == Env::ATTACK || v.serial != serial0) reAttack = true;
        if (v.amp.v < minEnv) minEnv = v.amp.v;
    }
    CHECK(!reAttack, "same voice keeps sounding, envelope never re-enters attack");
    CHECK(minEnv > 0.95f, "amplitude envelope stays up through the slide (min %.3f)", minEnv);
    CHECK(r.active(0) == 1, "still exactly one voice (no second note was triggered)");
    int between = 0; bool monotonic = true;
    for (size_t i = 0; i < track.size(); ++i) {
        if (track[i] > 113 && track[i] < 143) ++between;
        if (i > 0 && track[i] < track[i - 1] - 0.8f) monotonic = false;
    }
    CHECK(between >= 5, "pitch passes continuously through intermediate frequencies (%d windows between)", between);
    CHECK(monotonic, "pitch rises monotonically during the slide");
    r.run(0.2);
    x = r.tail(0.15, n);
    hz = measureHz(x, n);
    CHECK(near(hz, 146.832f, 0.002f), "slide lands on D3 146.83 Hz, measured %.3f", hz);

    SECTION("releasing the newer key slides back to the held key");
    r.inst.key(0, 2, false);
    r.run(0.1);
    x = r.tail(0.02, n);
    float mid = measureHz(x, n);
    CHECK(mid > 111 && mid < 146, "mid-return pitch is in between (%.2f Hz)", mid);
    r.run(0.4);
    x = r.tail(0.2, n);
    hz = measureHz(x, n);
    CHECK(near(hz, 110.0f, 0.002f), "returns to 110 Hz, measured %.3f", hz);
    CHECK(r.inst.voice(vi).serial == serial0 && r.inst.voice(vi).gate, "still the original voice");

    SECTION("releasing the last key releases the voice");
    r.inst.key(0, 0, false);
    r.run(0.5);
    CHECK(r.active(0) == 0, "voice freed after release (%d active)", r.active(0));
    x = r.tail(0.1, n);
    CHECK(rms(x, n) < 1e-4f, "silence after release (rms %.6f)", rms(x, n));

    SECTION("glide time is configurable");
    r.inst.set(S_GLIDESCALE, 0.0f);
    r.inst.key(0, 0, true); r.run(0.2);
    r.inst.key(0, 2, true); r.run(0.06);
    x = r.tail(0.04, n);
    CHECK(near(measureHz(x, n), 146.832f, 0.004f), "glide scale 0 jumps at once (%.2f Hz)", measureHz(x, n));
    r.inst.cmd(EV_PANIC); r.run(0.1);
}

static void testPolyphony() {
    SECTION("strings sound simultaneously; held chord shapes glide together");
    Rig r;
    r.inst.setPatch(0, purePatch());
    r.inst.key(0, 0, true); r.inst.key(1, 0, true); r.inst.key(2, 0, true); // A2 D3 G3
    r.run(0.6);
    int n; const float *x = r.tail(0.4, n);
    CHECK(r.gated(0) == 3, "three voices gated (%d)", r.gated(0));
    float a = goertzel(x, n, 110.0f), d = goertzel(x, n, 146.832f), g = goertzel(x, n, 195.998f), off = goertzel(x, n, 170.0f);
    CHECK(a > 0.03f && d > 0.03f && g > 0.03f, "A2, D3, G3 all present (%.3f %.3f %.3f)", a, d, g);
    CHECK(off < a * 0.05f, "no energy between the notes");
    // re-finger the whole shape one key to the right while the old keys are held
    r.inst.key(0, 1, true); r.inst.key(1, 1, true); r.inst.key(2, 1, true);
    r.run(0.1);
    CHECK(r.active(0) == 3, "chord slide reuses the three voices (%d active)", r.active(0));
    r.run(0.6);
    x = r.tail(0.4, n);
    float c3 = goertzel(x, n, 130.813f), e3 = goertzel(x, n, 164.814f), a3 = goertzel(x, n, 220.0f);
    CHECK(c3 > 0.03f && e3 > 0.03f && a3 > 0.03f, "shape arrived on C3, E3, A3 (%.3f %.3f %.3f)", c3, e3, a3);
    CHECK(goertzel(x, n, 110.0f) < 0.005f, "old pitches are gone (%.4f)", goertzel(x, n, 110.0f));

    SECTION("sustain holds released notes; panic silences everything");
    for (int s = 0; s < 3; ++s) { r.inst.key(s, 0, false); }
    r.inst.cmd(EV_SUSTAIN, 1);
    for (int s = 0; s < 3; ++s) { r.inst.key(s, 1, false); }
    r.run(0.3);
    CHECK(r.gated(0) == 3, "sustain keeps strings ringing after key release (%d)", r.gated(0));
    r.inst.key(0, 4, true); r.run(0.05);
    CHECK(r.active(0) == 3, "a key on a sustained string slides it instead of adding a voice");
    r.inst.key(0, 4, false);
    r.inst.cmd(EV_SUSTAIN, 0);
    r.run(0.4);
    CHECK(r.active(0) == 0, "releasing sustain releases the strings (%d active)", r.active(0));
    r.inst.key(2, 2, true); r.inst.key(2, 2, false); // press+release inside one audio block
    r.run(0.02);
    int nn; CHECK(rms(r.tail(0.015, nn), (int)(0.015 * SR)) > 0.01f, "a very short tap still makes a sound");
    r.run(0.3);
    r.inst.key(0, 0, true); r.inst.key(1, 3, true); r.run(0.1);
    r.inst.cmd(EV_PANIC); r.run(0.2);
    CHECK(r.inst.uiState().voices == 0, "panic frees all voices");

    SECTION("octave shift sweeps held notes");
    r.inst.key(0, 0, true); r.run(0.3);
    r.inst.cmd(EV_OCTAVE, 1); r.run(0.6);
    x = r.tail(0.2, n);
    CHECK(near(measureHz(x, n), 220.0f, 0.003f), "held A2 swept up to A3 (%.2f Hz)", measureHz(x, n));
    CHECK(r.active(0) == 1, "without retriggering");
}

static void testPresetsAndRandomiser() {
    SECTION("factory sounds are distinct");
    struct Feat { float bright, early, late, tail; };
    std::vector<Feat> feats; std::vector<Patch> pats;
    for (int i = 0; i < kFactoryCount; ++i) {
        Rig r;
        Patch p = factoryPatch(i);
        pats.push_back(p);
        r.inst.setPatch(0, p);
        r.inst.key(1, 3, true);
        r.run(1.2);
        r.inst.key(1, 3, false);
        r.run(0.6);
        Feat f;
        f.bright = brightness(r.L.data() + 4800, 24000);
        f.early = rms(r.L.data(), 4800);
        f.late = rms(r.L.data() + 48000, 9600);
        f.tail = rms(r.L.data() + (int)(1.5 * SR), 9600);
        feats.push_back(f);
        float pk = peak(r.L);
        CHECK(pk > 0.02f && pk < 0.8f, "%s: sensible level (peak %.3f)", p.name, pk);
        bool finite = true;
        for (float v : r.L) if (!(v == v)) finite = false;
        CHECK(finite, "%s: output is finite", p.name);
    }
    int similar = 0;
    for (int i = 0; i < kFactoryCount; ++i)
        for (int j = i + 1; j < kFactoryCount; ++j) {
            auto rel = [](float a, float b) { return std::fabs(a - b) / (std::fabs(a) + std::fabs(b) + 1e-6f); };
            float d = rel(feats[i].bright, feats[j].bright) + rel(feats[i].early, feats[j].early) +
                      rel(feats[i].late, feats[j].late) + rel(feats[i].tail, feats[j].tail);
            if (d < 0.25f) { ++similar; std::printf("    similar: %s / %s (d=%.3f)\n", pats[i].name, pats[j].name, d); }
        }
    CHECK(similar == 0, "all %d presets differ in measured brightness/envelope (%d similar pairs)", kFactoryCount, similar);

    SECTION("switching preset changes the sound of the next note");
    {
        Rig r;
        r.inst.setPatch(0, factoryPatch(3)); r.inst.key(1, 2, true); r.run(0.5);
        int n; float b1 = brightness(r.tail(0.3, n), (int)(0.3 * SR));
        r.inst.key(1, 2, false); r.run(0.3);
        r.inst.setPatch(0, factoryPatch(5)); r.inst.key(1, 2, true); r.run(0.5);
        float b2 = brightness(r.tail(0.3, n), (int)(0.3 * SR));
        CHECK(std::fabs(b1 - b2) > 0.3f * std::min(b1, b2), "brightness changed (%.4f -> %.4f)", b1, b2);
        CHECK(std::strcmp(r.inst.uiState().leadName, "Deep Bass") == 0, "engine reports the new patch name");
    }

    SECTION("randomiser: different every time, always safe");
    Rng rng; rng.s = 0xC0FFEE;
    Patch prev = randomPatch(rng);
    int identical = 0, unsafe = 0, silent = 0; float worstPeak = 0, minR = 1e9f, maxR = 0;
    int kinds[64] = {0};
    for (int k = 0; k < 120; ++k) {
        Patch p = randomPatch(rng);
        float dist = 0;
        for (int i = 0; i < P_COUNT; ++i) {
            const ParamInfo &m = paramInfo(i);
            dist += std::fabs(p.p[i] - prev.p[i]) / (m.max - m.min);
        }
        if (dist < 0.5f) ++identical;
        kinds[(int)p.p[P_WAVE1] * 3 + (int)p.p[P_FMODE]]++;
        prev = p;
        Rig r;
        r.inst.setPatch(0, p);
        // worst case: six notes at once, bottom/middle/top of each string
        r.inst.key(0, 0, true); r.inst.key(1, 2, true); r.inst.key(2, 4, true);
        r.run(0.8);
        r.inst.cmd(EV_SUSTAIN, 1);
        r.inst.key(0, 0, false); r.inst.key(1, 2, false); r.inst.key(2, 4, false);
        r.run(1.2);
        float pk = peak(r.L); float pr = peak(r.R); if (pr > pk) pk = pr;
        if (pk > worstPeak) worstPeak = pk;
        bool finite = true;
        for (float v : r.L) if (!(v == v)) finite = false;
        if (!finite || pk > 1.0f) ++unsafe;
        float lv = rms(r.L.data(), (int)r.L.size());
        if (lv < 0.004f) { ++silent; std::printf("    quiet: %s rms %.5f\n", p.name, lv); }
        if (lv < minR) minR = lv;
        if (lv > maxR) maxR = lv;
    }
    int used = 0;
    for (int k : kinds) if (k) ++used;
    CHECK(identical == 0, "consecutive random patches differ (%d near-identical)", identical);
    CHECK(unsafe == 0, "no patch clips or blows up (worst peak %.3f)", worstPeak);
    CHECK(worstPeak <= 1.0f, "master output bounded to +-1.0 (%.3f)", worstPeak);
    CHECK(silent == 0, "no silent patches (%d)", silent);
    CHECK(used >= 8, "covers many oscillator/filter combinations (%d)", used);
    std::printf("    random patch 3-note rms range: %.4f .. %.4f, worst peak %.3f\n", minR, maxR, worstPeak);

    SECTION("mutate stays close and safe");
    Patch base = factoryPatch(0);
    Patch mu = mutatePatch(base, rng, 0.3f);
    float d = 0;
    for (int i = 0; i < P_COUNT; ++i) d += std::fabs(mu.p[i] - base.p[i]) / (paramInfo(i).max - paramInfo(i).min);
    CHECK(d > 0.05f && d < 6.0f, "mutation moved parameters moderately (%.3f)", d);
}

// Share of a signal's energy that sits at one frequency (projection onto a sine/cosine pair).
static float shareAt(const float *x, int n, double hz) {
    double c = 0, q = 0, e = 0;
    for (int i = 0; i < n; ++i) {
        double ph = 2.0 * M_PI * hz * i / SR;
        c += x[i] * std::cos(ph); q += x[i] * std::sin(ph); e += (double)x[i] * x[i];
    }
    return e > 0 ? (float)(2.0 * (c * c + q * q) / (n * e)) : 0;
}

static void testExtraSounds() {
    SECTION("extra sounds: five dream sounds built around a sine");
    // Dry (no chorus, echo, reverb, wobble or detune beating) so the wave shape itself is measured:
    // held A3 = 220 Hz.
    auto dryShare = [](Patch p) {
        p.p[P_DETUNE] = 0; p.p[P_CHORUS] = 0; p.p[P_DLYMIX] = 0; p.p[P_REVMIX] = 0; p.p[P_VIBDEPTH] = 0; p.p[P_LFOAMP] = 0;
        Rig r;
        r.inst.setPatch(0, p);
        r.inst.key(1, 3, true);
        r.run(2.0);
        int n; const float *x = r.tail(0.5, n);   // 110 whole cycles
        return shareAt(x, n, 220.0);
    };
    float saw = dryShare(factoryPatch(0));
    CHECK(saw < 0.8f, "Warm Lead (saws) keeps %.0f%% of its energy at the fundamental", saw * 100);
    std::vector<std::string> names;
    for (int i = 0; i < kExtraCount; ++i) {
        Patch p = extraPatch(i);
        names.push_back(p.name);
        float sh = dryShare(p);
        CHECK(sh > 0.85f, "%s: %.0f%% of the energy is the fundamental sine", p.name, sh * 100);
        Rig r;
        r.inst.setPatch(0, p);
        r.inst.key(0, 0, true); r.inst.key(1, 4, true); r.inst.key(2, 4, true);
        r.run(3.0);
        r.inst.key(0, 0, false); r.inst.key(1, 4, false); r.inst.key(2, 4, false);
        r.run(1.0);
        bool finite = true;
        for (float v : r.L) if (!(v == v) || std::fabs(v) > 4.0f) finite = false;
        float lvl = rms(r.L.data() + (int)SR, 2 * (int)SR);
        CHECK(finite && peak(r.L) <= 1.0f, "%s: three held notes stay finite and below full scale (peak %.2f)", p.name, peak(r.L));
        CHECK(lvl > 0.02f && lvl < 0.4f, "%s: playing level is sensible (rms %.3f)", p.name, lvl);
        CHECK(rms(r.L.data() + (int)(3.6 * SR), (int)(0.4 * SR)) > 0.002f, "%s: rings on after release", p.name);
    }
    std::sort(names.begin(), names.end());
    CHECK(std::adjacent_find(names.begin(), names.end()) == names.end(), "the %d names are all different", kExtraCount);
    for (int i = 0; i < kExtraCount; ++i)
        for (int k = 0; k < kFactoryCount; ++k)
            if (std::strcmp(extraPatch(i).name, factoryPatch(k).name) == 0) names.clear();
    CHECK(!names.empty(), "no extra sound shares a name with a factory sound");
}

static void testDrums() {
    SECTION("drums: three kits of eight sounds");
    {
        Rig r;
        const DrumKit &dk = r.inst.drumKit();
        int bad = 0, loud = 0, tails = 0, dull = 0;
        for (int k = 0; k < kKits; ++k) {
            for (int d = 0; d < kDrums; ++d) {
                const std::vector<float> &s = dk.sample(k, d);
                float pk = peak(s);
                bool finite = !s.empty();
                for (float v : s) if (!(v == v)) finite = false;
                if (!finite) ++bad;
                if (pk < 0.08f || pk > 1.3f) { ++loud; std::printf("    %s %s peak %.2f\n", kitName(k), drumName(d), pk); }
                if (!s.empty() && std::fabs(s.back()) > 1e-3f) ++tails;
            }
            float bk = brightness(dk.sample(k, D_KICK).data(), (int)dk.sample(k, D_KICK).size());
            float bh = brightness(dk.sample(k, D_HAT).data(), (int)dk.sample(k, D_HAT).size());
            if (!(bh > 4.0f * bk)) { ++dull; std::printf("    %s kick %.3f hat %.3f\n", kitName(k), bk, bh); }
        }
        CHECK(bad == 0, "all %d sounds exist and are finite", kKits * kDrums);
        CHECK(loud == 0, "every sound peaks between 0.08 and 1.3 (%d outside)", loud);
        CHECK(tails == 0, "every sound ends in silence (%d do not)", tails);
        CHECK(dull == 0, "in every kit the hat is much brighter than the kick");
        int same = 0;
        for (int d = 0; d < kDrums; ++d)
            for (int a = 0; a < kKits; ++a)
                for (int b = a + 1; b < kKits; ++b) {
                    const std::vector<float> &x = dk.sample(a, d), &y = dk.sample(b, d);
                    double diff = 0, e = 0; size_t n = std::min(x.size(), y.size());
                    for (size_t i = 0; i < n; ++i) { diff += (double)(x[i] - y[i]) * (x[i] - y[i]); e += (double)x[i] * x[i]; }
                    if (diff < 0.05 * e) ++same;
                }
        CHECK(same == 0, "the same drum differs between kits (%d alike)", same);
    }

    SECTION("drums: a pad hit sounds at once");
    {
        Rig r;
        r.inst.set(S_KIT, KIT_CRUNCHY);
        r.run(0.05);
        CHECK(peak(r.L) == 0.0f, "silent before the hit");
        r.clear();
        r.inst.hit(D_KICK);
        r.run(0.03);
        CHECK(peak(r.L) > 0.1f, "kick is audible inside 30 ms (peak %.2f)", peak(r.L));
        CHECK(r.inst.uiState().drumHits[D_KICK] == 1 && !r.inst.uiState().beatPlaying, "counted, and the beat did not start by itself");
        r.run(2.0); r.clear(); r.run(0.2);
        CHECK(peak(r.L) < 1e-3f, "and it dies away (peak %.5f two seconds later)", peak(r.L));
        float pk[kKits];
        for (int k = 0; k < kKits; ++k) {
            Rig q; q.inst.set(S_KIT, (float)k); q.inst.hit(D_SNARE); q.run(0.15);
            pk[k] = brightness(q.L.data(), (int)q.L.size());
        }
        CHECK(!near(pk[0], pk[1], 0.1f) && !near(pk[1], pk[2], 0.1f), "changing kit changes the snare (%.3f %.3f %.3f)", pk[0], pk[1], pk[2]);
    }

    SECTION("beat: sixteen steps in time with the tempo");
    {
        Rig r;
        r.inst.set(S_BPM, 120); // one step = 125 ms, one bar = 2 s
        for (int s : {0, 4, 8, 12}) r.inst.beatDo(BT_STEP, D_KICK, s, 1);
        r.inst.beatDo(BT_STEP, D_HAT, 2, 1);
        r.inst.beatDo(BT_PLAYSTOP);
        auto kicks = [&] { return (int)r.inst.uiState().drumHits[D_KICK]; };
        r.run(0.02);  CHECK(kicks() == 1 && r.inst.uiState().beatPlaying, "step 1 sounds as soon as the beat starts");
        CHECK(r.inst.uiState().beatStep == 0, "the display is on step 1");
        r.run(0.46);  CHECK(kicks() == 1, "nothing extra before beat 2 (0.48 s)");
        r.run(0.04);  CHECK(kicks() == 2, "second kick on beat 2 (0.50 s)");
        CHECK(r.inst.uiState().drumHits[D_HAT] == 1, "the hat on step 3 has played once");
        r.run(1.0);   CHECK(kicks() == 4, "four kicks in the first bar");
        r.run(0.5);   CHECK(kicks() == 5 && r.inst.uiState().beatStep == 0, "the bar repeats (2.02 s)");
        r.inst.set(S_BPM, 240);
        r.run(1.0);   CHECK(kicks() == 9, "doubling the tempo doubles the rate (%d kicks)", kicks());
        r.inst.beatDo(BT_PLAYSTOP);
        r.run(1.0);   CHECK(kicks() == 9 && r.inst.uiState().beatStep == -1, "stop stops it");
        r.inst.beatDo(BT_STEP, D_KICK, 4, 2);
        r.run(0.01);  CHECK(r.inst.uiState().beatPat[D_KICK] == 0x1101, "toggling a step off (pattern %04x)", r.inst.uiState().beatPat[D_KICK]);
    }

    SECTION("beat: recording what the pads play");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        r.inst.beatDo(BT_REC);
        r.run(0.37);
        UiState u = r.inst.uiState();
        CHECK(u.beatPlaying && u.beatRec, "Rec starts the beat");
        r.inst.hit(D_SNARE);      // 0.37 s = step 3.96 -> step 4 (index 3)... nearest sixteenth
        r.run(0.12);
        r.inst.hit(D_CLAP);       // 0.49 s: just before step 5 (0.50 s)
        r.run(0.2);
        u = r.inst.uiState();
        CHECK(u.beatPat[D_SNARE] == (1u << 3), "a hit at 0.37 s lands on step 4 (pattern %04x)", u.beatPat[D_SNARE]);
        CHECK(u.beatPat[D_CLAP] == (1u << 4), "a hit just before step 5 lands on step 5 (pattern %04x)", u.beatPat[D_CLAP]);
        CHECK(u.drumHits[D_CLAP] == 1, "and is not sounded a second time when the step arrives (%u)", u.drumHits[D_CLAP]);
        r.run(2.0);
        u = r.inst.uiState();
        CHECK(u.drumHits[D_SNARE] == 2 && u.drumHits[D_CLAP] == 2, "both play back in the next bar");
        r.inst.beatDo(BT_REC);
        r.run(0.3);
        r.inst.hit(D_TOM);
        r.run(0.1);
        u = r.inst.uiState();
        CHECK(!u.beatRec && u.beatPlaying && u.beatPat[D_TOM] == 0, "with Rec off a pad plays without being written");
        r.inst.beatDo(BT_ERASE, D_SNARE);
        r.run(0.01);
        u = r.inst.uiState();
        CHECK(u.beatPat[D_SNARE] == 0 && u.beatPat[D_CLAP] != 0, "erase clears one drum only");
        r.inst.beatDo(BT_ERASE, -1);
        r.inst.cmd(EV_PANIC);
        r.run(0.01);
        u = r.inst.uiState();
        CHECK(u.beatPat[D_CLAP] == 0 && !u.beatPlaying, "erase all, and panic stops the beat");
    }
    {
        Rig r;
        r.inst.set(S_BPM, 120); r.inst.set(S_LATENCY, 200); // Bluetooth: the player hears everything 200 ms late
        r.inst.beatDo(BT_REC);
        r.run(0.70);              // the player hits on what they hear as beat 2 (0.5 s), 200 ms later
        r.inst.hit(D_SNARE);
        r.run(0.05);
        CHECK(r.inst.uiState().beatPat[D_SNARE] == (1u << 4), "with a 200 ms output delay a late hit is written where it was heard (pattern %04x)", r.inst.uiState().beatPat[D_SNARE]);
    }

    SECTION("beat: with backing, saved in a session, and never over full scale");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        r.inst.beatDo(BT_STEP, D_KICK, 0, 1);
        r.inst.beatDo(BT_PLAYSTOP);
        r.run(1.3);
        r.inst.cmd(EV_CHORD, 0);  // a chord tap starts the backing while the beat runs
        r.run(0.5);
        CHECK(r.inst.uiState().drumHits[D_KICK] == 1 && r.inst.uiState().backPlaying, "starting the backing does not restart the bar");
        r.run(0.3);
        CHECK(r.inst.uiState().drumHits[D_KICK] == 2, "the next bar still starts at 2.0 s");
    }
    {
        Rig r;
        r.inst.set(S_KIT, KIT_GLITCHY); r.inst.set(S_BEATVOL, 0.4f);
        r.inst.beatDo(BT_STEP, D_COWBELL, 15, 1); r.inst.beatDo(BT_STEP, D_KICK, 0, 1);
        r.run(0.01);
        SessionData sd; r.inst.snapshot(sd);
        Rig q; q.inst.restore(sd);
        UiState u = q.inst.uiState();
        CHECK(u.beatPat[D_COWBELL] == 0x8000 && u.beatPat[D_KICK] == 1 && u.set.kit == KIT_GLITCHY && near(u.set.beatVol, 0.4f, 0.01f),
              "pattern, kit and beat volume survive a session save and load");
        CHECK(!u.beatPlaying, "a loaded session does not start the beat");
    }
    for (int k = 0; k < kKits; ++k) {
        Rig r;
        r.inst.set(S_KIT, (float)k); r.inst.set(S_BEATVOL, 1); r.inst.set(S_VOLUME, 1); r.inst.set(S_BPM, 200);
        for (int d = 0; d < kDrums; ++d) for (int s = 0; s < kDrumSteps; ++s) r.inst.beatDo(BT_STEP, d, s, 1);
        r.inst.beatDo(BT_PLAYSTOP);
        r.run(2.5);
        bool finite = true;
        for (float v : r.L) if (!(v == v)) finite = false;
        CHECK(finite && peak(r.L) <= 1.0f && peak(r.R) <= 1.0f, "%s kit: every drum on every step stays inside full scale (peak %.2f)", kitName(k), peak(r.L));
    }
}

// ---- the track ----

static std::shared_ptr<const TrackData> share(const TrackData &t) { return std::make_shared<const TrackData>(t); }

static Clip kickSnareClip() { // kick on step 1, snare on step 9
    Clip c; c.kind = CK_BEAT; c.bars = 1; c.kit = KIT_SOFT;
    c.beat[D_KICK] = 1u << 0; c.beat[D_SNARE] = 1u << 8;
    return c;
}

static Clip noteClip(float note, double on, double off, int bars = 1) {
    Clip c; c.kind = CK_LOOP; c.bars = bars;
    LayerData ld; ld.patches.push_back(purePatch());
    LoopEv a; a.beat = on; a.op = OP_ON; a.slot = 0; a.a = note; a.b = 1; a.seq = 0;
    LoopEv b; b.beat = off; b.op = OP_OFF; b.slot = 0; b.a = 0; b.b = 0; b.seq = 1;
    ld.ev.push_back(a); ld.ev.push_back(b);
    c.layers.push_back(ld);
    return c;
}

static void put(TrackData &t, int lane, int clip, int bar, int bars = 0) {
    Placement p = placeClip(*t.clips[clip], bar);
    p.clip = clip;
    if (bars > 0) p.bars = bars;
    t.lane[lane].push_back(p);
    sortLane(t, lane);
}

// seconds at which each new hit of `drum` was counted, to the nearest block of 256 frames
static std::vector<double> hitTimes(Rig &r, int drum, double seconds) {
    std::vector<double> out;
    uint32_t seen = r.inst.uiState().drumHits[drum];
    double t = 0;
    r.run(seconds, [&] {
        t += 256.0 / SR;
        uint32_t now = r.inst.uiState().drumHits[drum];
        for (; seen < now; ++seen) out.push_back(t);
    });
    return out;
}

static void testTrack() {
    SECTION("track: chopping keeps a clip's length");
    {
        Clip c = kickSnareClip();
        Placement p = placeClip(c, 0);
        CHECK(p.pieces.size() == 1 && p.pieces[0].len == 4.0 && p.bars == 1, "a placed clip is one piece of four beats");
        CHECK(cutPiece(p, 2.0) && p.pieces.size() == 2, "cut at beat 3 makes two pieces");
        CHECK(!cutPiece(p, 2.0) && !cutPiece(p, 0.0) && !cutPiece(p, 4.0), "cutting on an existing edge does nothing");
        CHECK(p.pieces[1].src == 2.0 && p.pieces[1].len == 2.0, "the second piece starts where the cut was");
        CHECK(movePiece(p, 0, 1) && p.pieces[0].src == 2.0 && p.pieces[1].src == 0.0, "pieces swap places");
        CHECK(!joinPieces(p, 0), "pieces out of order cannot be mended");
        movePiece(p, 0, 1);
        CHECK(joinPieces(p, 0) && p.pieces.size() == 1 && p.pieces[0].len == 4.0, "pieces in order mend back into one");
        cutEvery(p, 1.0);
        CHECK(p.pieces.size() == 4 && near((float)piecesLength(p), 4.0f, 1e-6f), "cut on every beat: four pieces, still four beats");
        CHECK(repeatPiece(p, 0) && p.pieces[1].src == 0.0 && near((float)piecesLength(p), 4.0f, 1e-6f), "repeat: the next piece plays the same material");
        double st = -1;
        CHECK(pieceAt(p, 2.5, &st) == 2 && st == 2.0, "beat 2.5 falls in the third piece");
        resetPieces(p, c);
        CHECK(p.pieces.size() == 1 && p.pieces[0].src == 0.0, "reset undoes every cut");
        TrackData t; addClip(t, c);
        put(t, LN_BEAT, 0, 2, 2);
        CHECK(!laneFree(t, LN_BEAT, 3, 1) && !laneFree(t, LN_BEAT, 1, 2) && laneFree(t, LN_BEAT, 4, 1) && laneFree(t, LN_BEAT, 0, 2), "placements may not overlap");
        CHECK(laneFree(t, LN_BEAT, 3, 1, 0) && placementAt(t, LN_BEAT, 3) == 0 && placementAt(t, LN_BEAT, 4) == -1, "a placement can be moved over itself");
        // split at a bar line and join again
        Clip two = noteClip(60, 0.0, 7.0, 2);
        TrackData s; int ci = addClip(s, two);
        put(s, LN_LOOPA, ci, 0, 4); // two passes
        cutPiece(s.lane[LN_LOOPA][0], 4.0);
        movePiece(s.lane[LN_LOOPA][0], 0, 1); // heard: bar 2 of the clip, then bar 1
        const Placement whole = s.lane[LN_LOOPA][0];
        CHECK(splitPlacement(s, LN_LOOPA, 0, 1) && s.lane[LN_LOOPA].size() == 2, "a placed clip splits at a bar line");
        CHECK(!splitPlacement(s, LN_LOOPA, 0, 0) && !splitPlacement(s, LN_LOOPA, 1, 4), "but not at its own edges");
        const Placement &lft = s.lane[LN_LOOPA][0], &rgt = s.lane[LN_LOOPA][1];
        CHECK(lft.bar == 0 && lft.bars == 1 && rgt.bar == 1 && rgt.bars == 3, "into bars 1 and 2 to 4");
        bool sameSound = true;
        for (double x = 0.1; x < 12.0; x += 0.25) // bars 2 to 4 as heard before and after
            if (std::fabs(sourceAt(whole, std::fmod(4.0 + x, 8.0)) - sourceAt(rgt, std::fmod(x, 8.0))) > 1e-6) sameSound = false;
        CHECK(sameSound && near((float)piecesLength(rgt), 8.0f, 1e-6f), "the right half carries on from where the left stops");
        CHECK(joinPlacements(s, LN_LOOPA, 0) && s.lane[LN_LOOPA].size() == 1 && s.lane[LN_LOOPA][0].bars == 4, "and the two join back into one");
        put(s, LN_LOOPA, ci, 4, 2);
        CHECK(!joinPlacements(s, LN_LOOPA, 0), "a fresh copy does not continue a reordered clip, so they do not join");
        TrackData s2; ci = addClip(s2, two);
        put(s2, LN_LOOPA, ci, 0); put(s2, LN_LOOPA, ci, 2);
        CHECK(joinPlacements(s2, LN_LOOPA, 0) && s2.lane[LN_LOOPA][0].bars == 4, "a clip and its repeat join into one long one");
        CHECK(laneTakes(LN_LOOPB, CK_LOOP) && laneTakes(LN_LOOPA, CK_LOOP) && !laneTakes(LN_BEAT, CK_LOOP) && !laneTakes(LN_LOOPA, CK_CHORDS), "each kind of clip has its lanes");
    }

    SECTION("track: keeping what is live as clips");
    {
        Rig r;
        SessionData s; r.inst.snapshot(s);
        Clip c;
        CHECK(!beatClip(s, c) && !chordsClip(s, c) && !loopClip(s, c), "nothing to keep from an empty instrument");
        r.inst.set(S_KIT, KIT_GLITCHY);
        r.inst.beatDo(BT_STEP, D_KICK, 0, 1);
        r.inst.set(S_BACKMODE, BM_PROG); r.inst.set(S_CHORDBEATS, 4);
        r.inst.cmd(EV_CHORD, 0); r.inst.cmd(EV_CHORD, 3); r.inst.cmd(EV_CHORD, 4);
        r.run(0.1);
        r.inst.cmd(EV_BACK, BC_PLAYSTOP);
        r.run(0.1);
        r.inst.snapshot(s);
        Clip b, ch;
        CHECK(beatClip(s, b) && b.kind == CK_BEAT && b.kit == KIT_GLITCHY && b.beat[D_KICK] == 1 && b.bars == 1, "the beat becomes a one-bar clip with its kit");
        CHECK(chordsClip(s, ch) && ch.kind == CK_CHORDS && ch.progLen == 3 && ch.bars == 3, "three chords of a bar each become a three-bar clip");
        Clip b2 = b;
        CHECK(sameClip(b, b2), "a clip equals its copy");
        b2.beat[D_HAT] = 5;
        CHECK(!sameClip(b, b2) && !sameClip(b, ch), "and differs from a changed one");
    }

    SECTION("track: a beat clip plays in time, a gap is silent, the track goes round");
    {
        Rig r;
        r.inst.set(S_BPM, 120); // one bar = 2 s
        TrackData t; t.bars = 2;
        addClip(t, kickSnareClip());
        put(t, LN_BEAT, 0, 0);
        r.inst.setTrack(share(t));
        r.run(0.2);
        CHECK(!r.inst.uiState().trackPlaying && r.inst.uiState().drumHits[D_KICK] == 0, "setting a track does not start it");
        r.inst.trackDo(TC_PLAY, 0);
        std::vector<double> k = hitTimes(r, D_KICK, 8.1);
        CHECK(k.size() == 3, "kick once per pass of a two-bar track: 3 in 8.1 s (got %zu)", k.size());
        if (k.size() == 3) CHECK(k[0] < 0.02 && near((float)k[1], 4.0f, 0.02f) && near((float)k[2], 8.0f, 0.02f), "at 0, 4 and 8 s (%.3f %.3f %.3f)", k[0], k[1], k[2]);
        CHECK(r.inst.uiState().drumHits[D_SNARE] == 2, "snare twice (%u)", r.inst.uiState().drumHits[D_SNARE]);
        UiState u = r.inst.uiState();
        CHECK(u.trackPlaying && near((float)u.trackPos, 0.2f, 0.15f), "the position wrapped to the start (%.3f beats)", u.trackPos);
        r.inst.trackDo(TC_STOP);
        r.run(0.1);
        uint32_t n = r.inst.uiState().drumHits[D_KICK];
        r.run(4.5);
        CHECK(!r.inst.uiState().trackPlaying && r.inst.uiState().drumHits[D_KICK] == n, "stop stops it");
    }

    SECTION("track: a clip stretched over more bars repeats");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        TrackData t; t.bars = 4;
        addClip(t, kickSnareClip());
        put(t, LN_BEAT, 0, 1, 3);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        std::vector<double> k = hitTimes(r, D_KICK, 7.9);
        CHECK(k.size() == 3, "placed on bars 2 to 4: three kicks (got %zu)", k.size());
        if (k.size() == 3) CHECK(near((float)k[0], 2.0f, 0.02f) && near((float)k[1], 4.0f, 0.02f) && near((float)k[2], 6.0f, 0.02f), "at 2, 4 and 6 s (%.3f %.3f %.3f)", k[0], k[1], k[2]);
    }

    SECTION("track: chopped pieces play in their new order; a muted piece is silent");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        TrackData t; t.bars = 1;
        addClip(t, kickSnareClip());
        put(t, LN_BEAT, 0, 0);
        Placement &p = t.lane[LN_BEAT][0];
        cutPiece(p, 2.0);
        movePiece(p, 0, 1); // second half first: snare on beat 1, kick on beat 3
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_ONCE, 0);
        std::vector<double> sn, k;
        {
            uint32_t ss = 0, kk = 0; double tm = 0;
            r.run(2.5, [&] {
                tm += 256.0 / SR;
                UiState u = r.inst.uiState();
                for (; ss < u.drumHits[D_SNARE]; ++ss) sn.push_back(tm);
                for (; kk < u.drumHits[D_KICK]; ++kk) k.push_back(tm);
            });
        }
        CHECK(sn.size() == 1 && k.size() == 1, "one snare and one kick (%zu, %zu)", sn.size(), k.size());
        if (sn.size() == 1 && k.size() == 1) CHECK(sn[0] < 0.02 && near((float)k[0], 1.0f, 0.02f), "swapped halves: snare at 0 s, kick at 1 s (%.3f, %.3f)", sn[0], k[0]);
        UiState u = r.inst.uiState();
        CHECK(!u.trackPlaying && u.trackPasses == 1, "play once stops at the end and says so");

        t.lane[LN_BEAT][0].pieces[0].mute = true; // mute the snare half
        r.inst.setTrack(share(t));
        uint32_t s0 = u.drumHits[D_SNARE], k0 = u.drumHits[D_KICK];
        r.inst.trackDo(TC_ONCE, 0);
        r.run(2.5);
        u = r.inst.uiState();
        CHECK(u.drumHits[D_SNARE] == s0 && u.drumHits[D_KICK] == k0 + 1, "muted piece: no snare, the kick still plays");

        // repeat: both halves play the kick half
        Placement &q = t.lane[LN_BEAT][0];
        resetPieces(q, *t.clips[0]); cutPiece(q, 2.0); repeatPiece(q, 0);
        r.inst.setTrack(share(t));
        s0 = u.drumHits[D_SNARE]; k0 = u.drumHits[D_KICK];
        r.inst.trackDo(TC_ONCE, 0);
        r.run(2.5);
        u = r.inst.uiState();
        CHECK(u.drumHits[D_SNARE] == s0 && u.drumHits[D_KICK] == k0 + 2, "repeated piece: the kick twice, no snare (%u, %u)", u.drumHits[D_KICK] - k0, u.drumHits[D_SNARE] - s0);
    }

    SECTION("track: a loop clip plays its notes on its own parts");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        TrackData t; t.bars = 2;
        addClip(t, noteClip(69, 0.0, 2.0));
        addClip(t, noteClip(57, 1.0, 3.0));
        put(t, LN_LOOPA, 0, 1);
        put(t, LN_LOOPB, 1, 1);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        r.run(1.5);
        CHECK(r.active(kTrackPart0) == 0 && r.active(kTrackPart0 + kLayers) == 0, "nothing before the clips start");
        r.run(0.75); // 2.25 s: bar 2, beat 1.5
        CHECK(r.gated(kTrackPart0) == 1 && r.gated(kTrackPart0 + kLayers) == 0, "lane A's note is on, lane B's has not started");
        r.clear(); r.run(0.2);
        int n; const float *x = r.tail(0.15, n);
        CHECK(near(measureHz(x, n), 440.0f, 0.01f), "and it is the A that was recorded (%.1f Hz)", measureHz(x, n));
        r.run(0.3); // 2.75 s: beat 2.5 of the bar
        CHECK(r.gated(kTrackPart0) == 1 && r.gated(kTrackPart0 + kLayers) == 1, "both lanes sound together");
        r.run(0.5); // 3.25 s
        CHECK(r.gated(kTrackPart0) == 0 && r.gated(kTrackPart0 + kLayers) == 1, "lane A's note has ended");
        r.run(1.0); // 4.25 s: wrapped, bar 1 again
        CHECK(r.gated(kTrackPart0) == 0 && r.gated(kTrackPart0 + kLayers) == 0, "after the wrap the lanes are silent again");
        CHECK(r.gated(0) == 0, "the lead part was never used");
    }

    SECTION("track: a note still sounding at the end of a clip is let go");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        TrackData t; t.bars = 2;
        Clip c = noteClip(69, 0.0, 3.9);
        c.layers[0].ev.pop_back(); // never released
        addClip(t, c);
        put(t, LN_LOOPA, 0, 0);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        r.run(1.0);
        CHECK(r.gated(kTrackPart0) == 1, "held during the clip");
        r.run(1.5);
        CHECK(r.gated(kTrackPart0) == 0, "released when the clip ends");
    }

    SECTION("track: a chords clip drives the backing and gives it back");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        r.inst.set(S_BACKSTYLE, BS_PULSE); // the live backing: pulse, one chord, not playing
        r.inst.set(S_CHORDBEATS, 2);
        r.inst.cmd(EV_CHORD, 5);
        r.run(0.1);
        r.inst.cmd(EV_BACK, BC_PLAYSTOP);
        r.run(0.5);
        CHECK(r.gated(1) == 0 && r.inst.uiState().progLen == 1, "live backing: one chord, stopped");

        Clip c; c.kind = CK_CHORDS; c.bars = 2;
        c.prog[0] = 0; c.prog[1] = 3; c.progLen = 2; c.chordBeats = 4; c.backStyle = BS_SUSTAIN; c.patch = purePatch();
        TrackData t; t.bars = 3;
        addClip(t, c);
        put(t, LN_CHORDS, 0, 0);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        r.run(1.0);
        UiState u = r.inst.uiState();
        CHECK(u.backPlaying && u.curChord == 0 && r.gated(1) >= 3, "bar 1: chord I sounds (%d voices)", r.gated(1));
        SessionData s; r.inst.snapshot(s);
        CHECK(s.progLen == 1 && s.prog[0] == 5 && s.settings.backStyle == BS_PULSE && s.settings.chordBeats == 2, "a snapshot taken meanwhile still holds the live backing");
        r.inst.cmd(EV_CHORD, 2);
        r.run(1.5); // 2.5 s
        u = r.inst.uiState();
        CHECK(u.curChord == 3 && u.progLen == 2, "bar 2: chord IV; a chord tap during the track is ignored");
        r.run(2.0); // 4.5 s: bar 3 is empty
        CHECK(r.gated(1) == 0, "bar 3: no clip, the chord is let go");
        r.run(2.0); // 6.5 s: wrapped into bar 1
        CHECK(r.inst.uiState().curChord == 0 && r.gated(1) >= 3, "round again: chord I");
        r.inst.trackDo(TC_STOP);
        r.run(0.2);
        u = r.inst.uiState();
        CHECK(!u.backPlaying && r.gated(1) == 0, "stop lets the chord go");
        CHECK(u.progLen == 1 && u.prog[0] == 5 && u.set.backStyle == BS_PULSE && u.set.chordBeats == 2, "and the live backing is back as it was");
    }

    SECTION("track: a chords clip cut in two and swapped starts on its second chord");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        Clip c; c.kind = CK_CHORDS; c.bars = 2;
        c.prog[0] = 0; c.prog[1] = 4; c.progLen = 2; c.chordBeats = 4; c.patch = purePatch();
        TrackData t; t.bars = 2;
        addClip(t, c);
        put(t, LN_CHORDS, 0, 0);
        cutPiece(t.lane[LN_CHORDS][0], 4.0);
        movePiece(t.lane[LN_CHORDS][0], 0, 1);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        int first = -2;
        r.run(0.05, [&] { if (first == -2 && r.inst.uiState().curChord >= 0) first = r.inst.uiState().curChord; });
        CHECK(first == 4, "the very first chord heard is V, not I (%d)", first);
        r.run(2.45);
        CHECK(r.inst.uiState().curChord == 0, "then I in the second bar");
    }

    SECTION("track: cycle, play from a bar, and the live transport");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        r.inst.beatDo(BT_STEP, D_COWBELL, 0, 1);
        r.inst.beatDo(BT_PLAYSTOP);
        r.run(0.5);
        CHECK(r.inst.uiState().beatPlaying, "the live beat is running");
        TrackData t; t.bars = 4; t.cycleFrom = 2; t.cycleTo = 3;
        addClip(t, kickSnareClip());
        put(t, LN_BEAT, 0, 0, 4);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 2);
        r.run(0.1);
        UiState u = r.inst.uiState();
        CHECK(u.trackPlaying && !u.beatPlaying, "starting the track stops the live beat");
        CHECK(u.trackPos >= 8.0 && u.trackPos < 8.5, "it starts from bar 3 (%.2f beats)", u.trackPos);
        double lo = 1e9, hi = -1;
        r.run(5.0, [&] { double p = r.inst.uiState().trackPos; lo = std::min(lo, p); hi = std::max(hi, p); });
        CHECK(lo >= 8.0 && hi < 12.0, "with a cycle on bar 3 the position stays inside it (%.2f to %.2f)", lo, hi);
        uint32_t cb = r.inst.uiState().drumHits[D_COWBELL];
        r.inst.beatDo(BT_PLAYSTOP); // the live beat joins in
        double before = r.inst.uiState().trackPos;
        r.run(0.1);
        double after = r.inst.uiState().trackPos;
        CHECK(after > before && after - before < 0.3, "starting the live beat does not restart the track (%.2f to %.2f)", before, after);
        r.run(4.0);
        CHECK(r.inst.uiState().drumHits[D_COWBELL] >= cb + 2, "and the live beat plays over it");
        t.cycleFrom = t.cycleTo = 0;
        t.lane[LN_BEAT].clear();
        r.inst.setTrack(share(t));
        r.run(0.5);
        CHECK(r.inst.uiState().trackPlaying, "changing the track while it plays keeps it playing");
        r.inst.cmd(EV_PANIC);
        r.run(0.1);
        CHECK(!r.inst.uiState().trackPlaying && !r.inst.uiState().beatPlaying, "panic stops everything");
        r.inst.setTrack(nullptr);
        r.inst.trackDo(TC_PLAY, 0);
        r.run(0.1);
        CHECK(!r.inst.uiState().trackPlaying, "with no track there is nothing to play");
    }

    SECTION("track: recording a loop over a playing track stays on its bar lines");
    {
        Rig r;
        r.inst.set(S_BPM, 120);
        TrackData t; t.bars = 4;
        addClip(t, kickSnareClip());
        put(t, LN_BEAT, 0, 0, 4);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        r.run(1.0);
        r.inst.cmd(EV_LOOP, LC_PEDAL);
        r.run(0.2);
        UiState u = r.inst.uiState();
        CHECK(u.trackPlaying && u.loopState == L_COUNTIN && u.trackPos > 2.0, "the count-in does not restart the track (%.2f beats)", u.trackPos);
        r.run(3.0); // 4.2 s: recording began at the bar line at 4 s
        CHECK(r.inst.uiState().loopState == L_REC, "recording starts on the next bar line but one");
    }

    SECTION("track: everything at once stays inside full scale");
    {
        Rig r;
        r.inst.set(S_BPM, 150); r.inst.set(S_VOLUME, 1.0f); r.inst.set(S_BEATVOL, 1.0f); r.inst.set(S_LOOPVOL, 1.0f); r.inst.set(S_BACKVOL, 1.0f);
        TrackData t; t.bars = 2;
        Clip b; b.kind = CK_BEAT; b.kit = KIT_CRUNCHY;
        for (int d = 0; d < kDrums; ++d) b.beat[d] = 0xffff;
        Clip ch; ch.kind = CK_CHORDS; ch.bars = 1; ch.prog[0] = 0; ch.progLen = 1; ch.seventh = true; ch.patch = factoryPatch(0);
        addClip(t, b); addClip(t, ch);
        addClip(t, noteClip(45, 0.0, 3.9)); addClip(t, noteClip(64, 0.0, 3.9));
        put(t, LN_BEAT, 0, 0, 2); put(t, LN_CHORDS, 1, 0, 2); put(t, LN_LOOPA, 2, 0, 2); put(t, LN_LOOPB, 3, 0, 2);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        r.run(4.0);
        bool finite = true;
        for (float v : r.L) if (!(v == v)) finite = false;
        CHECK(finite && peak(r.L) <= 1.0f && peak(r.R) <= 1.0f && peak(r.L) > 0.3f, "peak %.3f / %.3f", peak(r.L), peak(r.R));
    }

    SECTION("track: a picked sound stands in for the recorded one");
    {
        Patch saw = purePatch(); saw.setName("Sawish"); saw.p[P_WAVE1] = W_SAW;
        auto second = [&](const SoundPick &clipPick, const SoundPick &placePick, float *hz) { // share of the 2nd harmonic, and the pitch
            Rig r;
            r.inst.set(S_BPM, 120);
            TrackData t; t.bars = 1;
            Clip c = noteClip(69, 0.0, 3.9);
            c.pick = clipPick;
            addClip(t, c);
            put(t, LN_LOOPA, 0, 0);
            t.lane[LN_LOOPA][0].pick = placePick;
            r.inst.setTrack(share(t));
            r.inst.trackDo(TC_PLAY, 0);
            r.run(0.6); r.clear(); r.run(0.3);
            int n; const float *x = r.tail(0.25, n);
            if (hz) *hz = measureHz(x, n);
            return shareAt(x, n, 880.0 * (hz && *hz < 300 ? 0.5 : 1.0));
        };
        SoundPick none, sawPick, down;
        sawPick.on = true; sawPick.patch = saw;
        down.octave = -1;
        float hz = 0;
        const float plain = second(none, none, &hz);
        CHECK(plain < 0.02f && near(hz, 440.0f, 0.01f), "as recorded: a sine at 440 Hz (2nd harmonic %.3f)", plain);
        const float onPlace = second(none, sawPick, &hz);
        CHECK(onPlace > 0.1f && near(hz, 440.0f, 0.01f), "picked for the placement: the same note, now a saw (2nd harmonic %.3f)", onPlace);
        const float onClip = second(sawPick, none, &hz);
        CHECK(onClip > 0.1f, "picked for the clip: every placement without its own pick plays it (%.3f)", onClip);
        second(sawPick, down, &hz);
        CHECK(near(hz, 220.0f, 0.01f), "a placement's own pick wins over the clip's: one octave down, %.1f Hz", hz);
        SoundPick up2; up2.octave = 2;
        second(none, up2, &hz);
        CHECK(near(hz, 1760.0f, 0.01f), "two octaves up: %.1f Hz", hz);

        Clip c = noteClip(69, 0.0, 3.9);
        Placement pl = placeClip(c, 0);
        CHECK(!pickFor(c, pl).any() && std::strcmp(soundName(c, pickFor(c, pl)), "Pure") == 0, "nothing picked: the clip names its recorded sound");
        c.pick = sawPick;
        CHECK(std::strcmp(soundName(c, pickFor(c, pl)), "Sawish") == 0 && std::strcmp(recordedSoundName(c), "Pure") == 0, "picked: the pick's name, and the recorded one is still known");
        CHECK(samePick(sawPick, sawPick) && !samePick(sawPick, none) && !samePick(down, none), "picks compare by sound, kit and octave");
    }

    SECTION("track: sound changes recorded in a loop are passed over once a sound is picked");
    {
        auto second = [&](bool picked) {
            Rig r;
            r.inst.set(S_BPM, 120);
            TrackData t; t.bars = 1;
            Clip c = noteClip(69, 0.0, 3.9);
            Patch saw = purePatch(); saw.p[P_WAVE1] = W_SAW;
            c.layers[0].patches.push_back(saw);
            LoopEv e; e.beat = 0.5; e.op = OP_PATCH; e.slot = 1; e.a = 0; e.b = 0; e.seq = 2; // the player changed to the saw half a beat in
            c.layers[0].ev.insert(c.layers[0].ev.begin() + 1, e);
            if (picked) { c.pick.on = true; c.pick.patch = purePatch(); }
            addClip(t, c);
            put(t, LN_LOOPA, 0, 0);
            r.inst.setTrack(share(t));
            r.inst.trackDo(TC_PLAY, 0);
            r.run(0.8); r.clear(); r.run(0.3);
            int n; const float *x = r.tail(0.25, n);
            return shareAt(x, n, 880.0);
        };
        const float rec = second(false), pk = second(true);
        CHECK(rec > 0.1f, "as recorded the change to a saw is heard (2nd harmonic %.3f)", rec);
        CHECK(pk < 0.02f, "with a sine picked it stays a sine (%.3f)", pk);
    }

    SECTION("track: a picked kit and a picked backing sound");
    {
        auto beatAudio = [&](int clipKit, int pickKit) {
            Rig r;
            r.inst.set(S_BPM, 120);
            TrackData t; t.bars = 1;
            Clip c = kickSnareClip(); c.kit = clipKit;
            addClip(t, c);
            put(t, LN_BEAT, 0, 0);
            t.lane[LN_BEAT][0].pick.kit = pickKit;
            r.inst.setTrack(share(t));
            r.inst.trackDo(TC_PLAY, 0);
            r.run(1.5);
            return r.L;
        };
        auto diff = [](const std::vector<float> &a, const std::vector<float> &b) {
            float d = 0;
            for (size_t i = 0; i < a.size() && i < b.size(); ++i) d = std::max(d, std::fabs(a[i] - b[i]));
            return d;
        };
        const std::vector<float> soft = beatAudio(KIT_SOFT, -1), crunchy = beatAudio(KIT_CRUNCHY, -1), picked = beatAudio(KIT_SOFT, KIT_CRUNCHY);
        CHECK(diff(soft, crunchy) > 0.01f, "the two kits do sound different (%.3f)", diff(soft, crunchy));
        CHECK(diff(picked, crunchy) < 1e-6f, "a soft beat with the crunchy kit picked is the crunchy beat (difference %.6f)", diff(picked, crunchy));

        auto droneHz = [&](int octave, bool saw, float *second) {
            Rig r;
            r.inst.set(S_BPM, 120);
            Clip c; c.kind = CK_CHORDS; c.bars = 1; c.droneMask = 1; c.patch = purePatch();
            if (saw) { c.pick.on = true; c.pick.patch = purePatch(); c.pick.patch.p[P_WAVE1] = W_SAW; }
            c.pick.octave = octave;
            TrackData t; t.bars = 1;
            addClip(t, c);
            put(t, LN_CHORDS, 0, 0);
            r.inst.setTrack(share(t));
            r.inst.trackDo(TC_PLAY, 0);
            r.run(0.6); r.clear(); r.run(0.3);
            int n; const float *x = r.tail(0.25, n);
            const float hz = measureHz(x, n);
            if (second) *second = shareAt(x, n, hz * 2.0);
            return hz;
        };
        float h0 = 0, h1 = 0;
        const float base = droneHz(0, false, &h0), low = droneHz(-1, false, nullptr), sawHz = droneHz(0, true, &h1);
        CHECK(base > 60 && near(low, base / 2, 0.01f), "a chords clip an octave down: %.1f Hz against %.1f", low, base);
        CHECK(near(sawHz, base, 0.01f) && h0 < 0.02f && h1 > 0.1f, "a chords clip with a saw picked: same note, new sound (2nd harmonic %.3f, was %.3f)", h1, h0);

        Rig r; // the octave does not leak into the live backing once the track stops
        r.inst.set(S_BPM, 120);
        r.inst.setPatch(0, purePatch());
        Clip c; c.kind = CK_CHORDS; c.bars = 1; c.droneMask = 1; c.patch = purePatch(); c.pick.octave = -2;
        TrackData t; t.bars = 1;
        addClip(t, c); put(t, LN_CHORDS, 0, 0);
        r.inst.setTrack(share(t));
        r.inst.trackDo(TC_PLAY, 0);
        r.run(0.5);
        r.inst.trackDo(TC_STOP);
        r.run(0.5);
        r.inst.set(S_BACKMODE, BM_DRONE);
        r.inst.cmd(EV_CHORD, 0);
        r.run(0.4); r.clear(); r.run(0.3);
        int n; const float *x = r.tail(0.25, n);
        CHECK(near(measureHz(x, n), base, 0.01f), "after the track the live drone is at its own pitch again (%.1f Hz)", measureHz(x, n));
    }

    SECTION("track: clips with different picks do not join");
    {
        TrackData t; t.bars = 4;
        addClip(t, kickSnareClip());
        put(t, LN_BEAT, 0, 0, 4);
        CHECK(splitPlacement(t, LN_BEAT, 0, 2), "split a four-bar placement");
        t.lane[LN_BEAT][1].pick.kit = KIT_GLITCHY;
        CHECK(!joinPlacements(t, LN_BEAT, 0), "the halves no longer join once one has another kit");
        t.lane[LN_BEAT][0].pick.kit = KIT_GLITCHY;
        CHECK(joinPlacements(t, LN_BEAT, 0) && t.lane[LN_BEAT][0].pick.kit == KIT_GLITCHY, "with the same kit they do, and keep it");
    }
}

static void testSoundShelf() {
    SECTION("sound shelf: Keys, Guitar and Bass");
    std::vector<std::string> names;
    float lo = 1e9f, hi = 0;
    bool unique = true, found = true, hints = true;
    for (int k = 0; k < kLibKinds; ++k)
        for (int i = 0; i < kLibSounds; ++i) {
            const Patch &p = librarySound(k, i);
            for (const std::string &n : names) unique = unique && n != p.name;
            names.push_back(p.name);
            int at = -1;
            found = found && libraryFind(p.name, &at) == k && at == i;
            hints = hints && std::strlen(libraryHint(k, i)) > 3;
            Rig r;
            r.inst.setPatch(0, p);
            r.inst.set(S_VOLUME, 1.0f);
            r.inst.key(1, 2, true);
            r.run(0.25);
            bool finite = true;
            for (float v : r.L) if (!(v == v)) finite = false;
            const float pk = std::max(peak(r.L), peak(r.R));
            CHECK(finite && pk > 0.02f && pk <= 1.0f, "%s %s sounds and stays inside full scale (peak %.3f)", libraryKindName(k), p.name, pk);
            lo = std::min(lo, pk); hi = std::max(hi, pk);
        }
    CHECK(unique && found && hints, "eighteen sounds, each with its own name, findable by it, with a hint");
    for (int i = 0; i < kExtraCount; ++i) unique = unique && libraryFind(extraPatch(i).name) < 0;
    for (int i = 0; i < kFactoryCount; ++i) unique = unique && libraryFind(factoryPatch(i).name) < 0;
    CHECK(unique, "no shelf sound shares a name with a factory or dream sound");
    CHECK(libraryOctave(SK_BASS) == -1 && libraryOctave(SK_KEYS) == 0 && libraryOctave(SK_GUITAR) == 0, "bass sits an octave down, the others where they were played");
    CHECK(&librarySound(SK_KEYS, 0) == &librarySound(SK_KEYS, 0), "a sound is built once and kept");
}

static void testLimiter() {
    SECTION("limiter: loud input cannot exceed full scale");
    Rig r;
    Patch p = factoryPatch(8);
    p.p[P_LEVEL] = 4.0f; p.p[P_RESO] = 0.95f; p.p[P_DRIVE] = 1.0f;
    r.inst.setPatch(0, p);
    r.inst.set(S_VOLUME, 1.0f);
    r.inst.set(S_BACKVOL, 1.0f);
    r.inst.set(S_SEVENTH, 1);
    r.inst.cmd(EV_CHORD, 0);
    for (int s = 0; s < 3; ++s) r.inst.key(s, s + 1, true);
    r.run(2.0);
    float pk = std::max(peak(r.L), peak(r.R));
    CHECK(pk <= 1.0f, "peak %.4f <= 1.0 with a deliberately abusive patch", pk);
    CHECK(pk > 0.5f, "and it is actually loud (%.3f)", pk);
}

static void testXY() {
    SECTION("XY pad modulates the sound while a note is held");
    Rig r;
    Patch p = purePatch();
    p.p[P_WAVE1] = W_SAW; p.p[P_CUTOFF] = 1200; p.p[P_RESO] = 0.2f;
    r.inst.setPatch(0, p);
    r.inst.key(1, 2, true);
    Event e; e.type = EV_XY; e.a = 1; e.f = 0.05f; e.g = 0.0f; r.inst.push(e);
    r.run(0.5);
    int n; float dark = brightness(r.tail(0.2, n), (int)(0.2 * SR));
    e.f = 0.95f; r.inst.push(e);
    r.run(0.5);
    float bright = brightness(r.tail(0.2, n), (int)(0.2 * SR));
    CHECK(bright > dark * 2.0f, "x sweeps the filter (%.4f -> %.4f)", dark, bright);
    CHECK(r.active(0) == 1, "note kept sounding throughout");
    e.a = 0; r.inst.push(e); r.run(0.3);
    float back = brightness(r.tail(0.2, n), (int)(0.2 * SR));
    CHECK(back < bright * 0.8f && back > dark * 1.2f, "releasing the pad returns to the patch setting (%.4f)", back);
    // vibrato mapping
    r.inst.set(S_XYMAP, XY_WOBBLE);
    e.a = 1; e.f = 0.5f; e.g = 1.0f; r.inst.push(e);
    float lo = 1e9f, hi = 0;
    for (int i = 0; i < 20; ++i) { r.run(0.03); float hz = measureHz(r.tail(0.03, n), (int)(0.03 * SR)); if (hz < lo) lo = hz; if (hz > hi) hi = hz; }
    CHECK(hi / lo > 1.03f, "y adds vibrato in WOBBLE mapping (%.2f..%.2f Hz)", lo, hi);
    // pitch bend
    e.a = 0; r.inst.push(e);
    Event b; b.type = EV_BEND; b.f = 1.0f; r.inst.push(b);
    r.run(0.8);
    float hz = measureHz(r.tail(0.2, n), (int)(0.2 * SR));
    float expect = (float)midiToHz(55 + 2); // middle string col 2 = G3
    CHECK(near(hz, expect, 0.01f), "bend up reaches +2 semitones (%.2f vs %.2f Hz)", hz, expect);
}

static float heldFraction(const float *x, int n) { // how much of the wave is flat steps
    int same = 0;
    for (int i = 1; i < n; ++i) if (std::fabs(x[i] - x[i - 1]) < 2e-5f) ++same; // the limiter after it may drift by a hair
    return n > 1 ? (float)same / (n - 1) : 0;
}
static int distinctLevels(const float *x, int n) {
    std::vector<long> v(n);
    for (int i = 0; i < n; ++i) v[i] = std::lround(x[i] * 2000);
    std::sort(v.begin(), v.end());
    return (int)(std::unique(v.begin(), v.end()) - v.begin());
}

static void testGlitch() {
    SECTION("GLITCH pad mapping crushes the whole output and lets go cleanly");
    Rig r;
    r.inst.setPatch(0, purePatch());
    r.inst.key(1, 2, true); // G3, 196 Hz
    r.run(0.5);
    int n; const float *t = r.tail(0.2, n);
    const float hz = (float)midiToHz(55);
    float cleanHeld = heldFraction(t, n), cleanRms = rms(t, n), cleanTone = goertzel(t, n, hz);
    int cleanLevels = distinctLevels(t, n);
    CHECK(cleanHeld < 0.05f && cleanLevels > 150, "clean sine is smooth (%.3f flat, %d levels)", cleanHeld, cleanLevels);

    r.inst.set(S_XYMAP, XY_GLITCH);
    Event e; e.type = EV_XY; e.a = 1; e.f = 0.5f; e.g = 0.8f; r.inst.push(e);
    r.run(0.5);
    t = r.tail(0.2, n);
    float held = heldFraction(t, n), tone = goertzel(t, n, hz);
    int levels = distinctLevels(t, n);
    CHECK(r.inst.uiState().glitch, "pad down switches the crusher on");
    CHECK(held > 0.8f, "wave becomes flat steps (%.3f flat; expect about %.3f)", held, 1.0f - 1.0f / r.inst.crusher().holdSamples());
    CHECK(levels >= 4 && levels <= 64, "only a few levels are left (%d)", levels);
    CHECK(tone > 0.5f * cleanTone, "the note is still there underneath (%.4f vs %.4f)", tone, cleanTone);
    CHECK(rms(t, n) > 0.5f * cleanRms && rms(t, n) < 1.7f * cleanRms, "loudness stays in the same range (%.4f vs %.4f)", rms(t, n), cleanRms);
    CHECK(peak(r.L) <= 1.0f, "output stays inside full scale (%.3f)", peak(r.L));

    // moving on the pad changes the character: right = coarser, down = more levels
    e.f = 0.95f; e.g = 0.1f; r.inst.push(e); r.run(0.3);
    t = r.tail(0.2, n);
    CHECK(heldFraction(t, n) > held && distinctLevels(t, n) > levels, "x coarsens, low y restores levels (%.3f flat, %d levels)", heldFraction(t, n), distinctLevels(t, n));

    // quieter playing gets the same grit, not silence
    r.inst.set(S_VOLUME, 0.15f); e.f = 0.5f; e.g = 0.8f; r.inst.push(e); r.run(0.8);
    t = r.tail(0.2, n);
    int quietLevels = distinctLevels(t, n);
    CHECK(quietLevels >= 4 && quietLevels <= 64 && rms(t, n) > 0.001f, "same grit at low volume (%d levels, rms %.4f)", quietLevels, rms(t, n));
    r.inst.set(S_VOLUME, 0.7f);

    e.a = 0; r.inst.push(e); r.run(0.4);
    t = r.tail(0.2, n);
    CHECK(!r.inst.uiState().glitch && heldFraction(t, n) < 0.05f, "lifting the finger gives the clean sound back (%.3f flat)", heldFraction(t, n));

    // Hold latches it; changing the mapping or panic always releases it
    r.inst.set(S_XYHOLD, 1); e.a = 1; r.inst.push(e); r.run(0.1); e.a = 0; r.inst.push(e); r.run(0.2);
    CHECK(r.inst.uiState().glitch, "Hold keeps it on after the finger lifts");
    r.inst.set(S_XYMAP, XY_FILTER); r.run(0.2);
    CHECK(!r.inst.uiState().glitch, "choosing another mapping switches it off");
    r.inst.set(S_XYMAP, XY_GLITCH); e.a = 1; r.inst.push(e); r.run(0.1);
    r.inst.cmd(EV_PANIC); r.run(0.1);
    CHECK(!r.inst.uiState().glitch, "panic switches it off");
}

static void testBacking() {
    SECTION("backing: tapped progression loops at tempo and keeps its sound");
    Rig r;
    Patch lead = factoryPatch(0);
    r.inst.setPatch(0, lead);
    r.inst.set(S_BPM, 120); // 4-beat chords = 2 s each
    r.run(0.05);
    for (int d : {0, 3, 4, 3}) r.inst.cmd(EV_CHORD, d);
    std::vector<int> seq; int minBack = 99; bool leadHeard = false;
    double t = 0;
    auto watch = [&]() {
        UiState u = r.inst.uiState();
        if (seq.empty() || seq.back() != u.curChord) seq.push_back(u.curChord);
        if (t > 0.3 && r.gated(1) < minBack) minBack = r.gated(1);
        if (r.gated(0) > 0) leadHeard = true;
        t += 256 / SR;
    };
    r.run(1.0, watch);
    CHECK(std::strcmp(r.inst.uiState().backName, "Warm Lead") == 0, "backing captured the live sound");
    r.inst.setPatch(0, factoryPatch(1)); // change the lead instrument
    r.inst.key(2, 1, true);
    r.run(4.0, watch);
    r.inst.key(2, 3, true);
    r.run(4.5, watch);
    UiState u = r.inst.uiState();
    CHECK(std::strcmp(u.backName, "Warm Lead") == 0, "backing still uses its original sound (%s)", u.backName);
    CHECK(std::strcmp(u.leadName, "Glass Bells") == 0, "lead uses the new sound (%s)", u.leadName);
    int expect[5] = {0, 3, 4, 3, 0};
    bool ok = seq.size() >= 5;
    for (int i = 0; ok && i < 5; ++i) if (seq[i] != expect[i]) ok = false;
    CHECK(ok, "chords stepped I-IV-V-IV-I (got %zu steps: %d %d %d %d %d)", seq.size(), seq.size() > 0 ? seq[0] : -9,
          seq.size() > 1 ? seq[1] : -9, seq.size() > 2 ? seq[2] : -9, seq.size() > 3 ? seq[3] : -9, seq.size() > 4 ? seq[4] : -9);
    CHECK(minBack >= 4, "bass + triad sounded continuously under the melody (min %d voices)", minBack);
    CHECK(leadHeard, "melody played over the backing");
    CHECK(u.progLen == 4 && u.backPlaying, "progression has 4 chords and is playing");

    SECTION("backing: remove, replace, stop, drone, arp");
    r.inst.cmd(EV_BACK, BC_REMOVE_LAST); r.run(0.05);
    CHECK(r.inst.uiState().progLen == 3, "remove last chord");
    r.inst.cmd(EV_BACK, BC_SELECT_SLOT, 1); r.inst.cmd(EV_CHORD, 5); r.run(0.05);
    u = r.inst.uiState();
    CHECK(u.progLen == 3 && u.prog[1] == 5, "selected slot replaced (slot1=%d)", u.prog[1]);
    r.inst.cmd(EV_BACK, BC_PLAYSTOP); r.run(0.3);
    CHECK(r.gated(1) == 0, "stop releases the backing");
    r.inst.cmd(EV_BACK, BC_PLAYSTOP); r.run(0.3);
    CHECK(r.gated(1) >= 4, "start resumes it");
    r.inst.set(S_BACKSTYLE, BS_ARP_UP); r.inst.set(S_ARPRATE, 4);
    int trig = 0; uint64_t lastSerial = 0;
    r.run(2.0, [&]() {
        for (int i = 0; i < kVoices; ++i) {
            const Voice &v = r.inst.voice(i);
            if (v.active() && v.part == 1 && v.slot == 1 && v.serial > lastSerial) { lastSerial = v.serial; ++trig; }
        }
    });
    CHECK(trig >= 12 && trig <= 18, "arpeggio retriggers 4 steps per beat (%d in 4 beats)", trig);
    r.inst.cmd(EV_BACK, BC_CLEAR); r.run(0.2);
    u = r.inst.uiState();
    CHECK(u.progLen == 0 && !u.backPlaying && r.gated(1) == 0, "clear empties the backing");
    r.inst.set(S_BACKSTYLE, BS_SUSTAIN);
    r.inst.set(S_BACKMODE, BM_DRONE);
    r.inst.cmd(EV_CHORD, 0); r.inst.cmd(EV_CHORD, 4); r.run(0.3);
    CHECK(r.gated(1) == 2, "two latched drone notes (%d)", r.gated(1));
    r.inst.cmd(EV_CHORD, 4); r.run(0.2);
    CHECK(r.gated(1) == 1, "tapping a drone again removes it (%d)", r.gated(1));
    r.inst.set(S_BACKMODE, BM_HOLD);
    r.inst.cmd(EV_BACK, BC_CLEAR); r.run(0.1);
    r.inst.cmd(EV_CHORD, 2); r.run(0.3);
    CHECK(r.gated(1) == 4 && r.inst.uiState().curChord == 2, "hold mode latches one chord");
    uint64_t before = 0, after = 0;
    for (int i = 0; i < kVoices; ++i) if (r.inst.voice(i).part == 1 && r.inst.voice(i).gate) before += r.inst.voice(i).serial;
    r.inst.cmd(EV_CHORD, 5); r.run(0.05);
    for (int i = 0; i < kVoices; ++i) if (r.inst.voice(i).part == 1 && r.inst.voice(i).gate) after += r.inst.voice(i).serial;
    CHECK(r.gated(1) == 4 && before == after, "changing a held chord glides the same four voices");
}

static void testLooper() {
    SECTION("looper: count-in, record, loop playback");
    Rig r;
    r.inst.setPatch(0, purePatch());
    r.inst.set(S_BPM, 120); // beat 0.5 s, bar 2 s
    r.run(0.05);
    r.inst.cmd(EV_LOOP, LC_PEDAL);
    r.run(0.1);
    CHECK(r.inst.uiState().loopState == L_COUNTIN, "pedal starts a count-in");
    r.clear();
    r.run(1.85);
    // four clicks: count onsets in the rendered audio
    int clicks = 0; bool in = false;
    for (size_t i = 0; i < r.L.size(); ++i) {
        float a = std::fabs(r.L[i]);
        if (!in && a > 0.05f) { ++clicks; in = true; }
        if (in && i % 4800 == 0) in = false;
    }
    r.run(0.1);
    CHECK(clicks >= 3 && clicks <= 4, "count-in clicks heard (%d after the first)", clicks);
    CHECK(r.inst.uiState().loopState == L_REC, "recording starts after the count-in (state %d)", r.inst.uiState().loopState);
    // we are ~0.05 s into the bar. Play: A2 at beat 1, slide to D3 at beat 1.5, release at beat 2
    r.run(0.45); r.inst.key(0, 0, true);
    r.run(0.25); r.inst.key(0, 2, true);
    r.run(0.25); r.inst.key(0, 2, false); r.inst.key(0, 0, false);
    r.run(0.85);
    r.inst.cmd(EV_LOOP, LC_PEDAL); // beat ~3.7: closes at the bar line
    r.run(0.05);
    UiState u = r.inst.uiState();
    CHECK(u.loopState == L_PLAY && u.loopClosing, "pedal closes the loop, snapped to the bar (closing=%d)", u.loopClosing);
    r.run(0.3);
    u = r.inst.uiState();
    CHECK(u.layers == 1 && u.loopBars == 1, "one layer, one bar (layers %d bars %d)", u.layers, u.loopBars);
    // Now nothing is held. Listen to two full cycles: the phrase must come back each bar.
    int cyclesHeard = 0; bool glideSeen = false; bool wasOn = false; float fLo = 1e9f, fHi = 0;
    r.clear();
    r.run(4.0, [&]() {
        bool on = r.gated(2) > 0;
        if (on && !wasOn) ++cyclesHeard;
        wasOn = on;
        for (int i = 0; i < kVoices; ++i) {
            const Voice &v = r.inst.voice(i);
            if (v.active() && v.part == 2 && v.gate) {
                if (v.note > 45.5f && v.note < 49.5f) glideSeen = true;
                if (v.note < fLo) fLo = v.note;
                if (v.note > fHi) fHi = v.note;
            }
        }
    });
    CHECK(cyclesHeard == 2, "recorded phrase replays once per bar (%d in two bars)", cyclesHeard);
    CHECK(glideSeen && fLo < 45.2f && fHi > 49.8f, "the recorded slide is reproduced as a continuous glide (%.2f..%.2f)", fLo, fHi);
    CHECK(rms(r.L.data(), (int)r.L.size()) > 0.01f, "loop is audible with no keys held");
    CHECK(r.gated(0) == 0, "live strings are free while the loop plays");

    SECTION("looper: overdub, undo, stop/start, clear");
    r.inst.cmd(EV_LOOP, LC_PEDAL); r.run(0.1);
    CHECK(r.inst.uiState().loopState == L_OVERDUB, "pedal while playing starts an overdub");
    r.inst.key(2, 3, true); r.run(0.4); r.inst.key(2, 3, false); r.run(0.3);
    r.inst.cmd(EV_LOOP, LC_PEDAL); r.run(0.1);
    u = r.inst.uiState();
    CHECK(u.loopState == L_PLAY && u.layers == 2, "overdub committed as a second layer (%d)", u.layers);
    int l1 = 0, l2 = 0; bool w1 = false, w2 = false;
    r.run(4.0, [&]() {
        bool a = r.gated(2) > 0, b = r.gated(3) > 0;
        if (a && !w1) ++l1;
        if (b && !w2) ++l2;
        w1 = a; w2 = b;
    });
    CHECK(l1 == 2 && l2 == 2, "both layers replay every bar (%d, %d)", l1, l2);
    // live playing during loop playback is not blocked
    r.inst.key(1, 4, true); r.run(0.2);
    CHECK(r.gated(0) == 1, "live note sounds over the loop");
    r.inst.key(1, 4, false);
    r.inst.cmd(EV_LOOP, LC_UNDO); r.run(0.1);
    CHECK(r.inst.uiState().layers == 1, "undo removes the last overdub");
    l2 = 0; w2 = false;
    r.run(2.2, [&]() { bool b = r.gated(3) > 0; if (b && !w2) ++l2; w2 = b; });
    CHECK(l2 == 0, "undone layer no longer plays");
    r.inst.cmd(EV_LOOP, LC_PLAYSTOP); r.run(0.3);
    CHECK(r.inst.uiState().loopState == L_STOPPED && r.gated(2) == 0, "stop halts playback");
    r.inst.cmd(EV_LOOP, LC_PLAYSTOP);
    l1 = 0; w1 = false;
    r.run(2.1, [&]() { bool a = r.gated(2) > 0; if (a && !w1) ++l1; w1 = a; });
    CHECK(r.inst.uiState().loopState == L_PLAY && l1 == 1, "start resumes it (%d phrase)", l1);
    r.inst.cmd(EV_LOOP, LC_CLEAR); r.run(0.2);
    u = r.inst.uiState();
    CHECK(u.loopState == L_EMPTY && u.layers == 0, "clear empties the looper");

    SECTION("looper: long loops and layers keep their own sound");
    r.inst.cmd(EV_LOOP, LC_PEDAL);
    r.run(2.05);
    for (int i = 0; i < 16; ++i) { r.inst.key(i % 3, i % 7, true); r.run(1.0); r.inst.key(i % 3, i % 7, false); r.run(0.125); }
    r.run(0.1); // 18.1 s recorded = 9.05 bars -> snaps to 9 bars
    r.inst.cmd(EV_LOOP, LC_PEDAL); r.run(0.2);
    u = r.inst.uiState();
    CHECK(u.loopBars == 9 && u.layers == 1, "18-second loop recorded (%d bars)", u.loopBars);
    r.inst.setPatch(0, factoryPatch(4)); r.run(0.1);
    CHECK(std::strcmp(r.inst.part(2).patch.name, "Pure") == 0, "loop layer keeps the sound it was recorded with (%s)", r.inst.part(2).patch.name);
}

static void testSessionRoundTrip() {
    SECTION("session snapshot / restore");
    Rig a;
    a.inst.setPatch(0, factoryPatch(6));
    a.inst.set(S_ROOT, 2); a.inst.set(S_SCALE, SC_DORIAN); a.inst.set(S_BPM, 96);
    a.run(0.05);
    for (int d : {0, 5, 3, 4}) a.inst.cmd(EV_CHORD, d);
    a.inst.cmd(EV_LOOP, LC_PEDAL);
    a.run(60.0 / 96 * 4 * 2 + 0.2); // count-in adapts to running backing (<= 6 beats)
    a.inst.key(1, 1, true); a.run(0.5); a.inst.key(1, 1, false); a.run(1.0);
    a.inst.cmd(EV_LOOP, LC_PEDAL); a.run(3.0);
    SessionData s1; a.inst.snapshot(s1);
    CHECK(s1.layers.size() == 1 && s1.layers[0].ev.size() >= 2, "snapshot contains the loop (%zu layers)", s1.layers.size());
    Rig b;
    b.inst.restore(s1);
    SessionData s2; b.inst.snapshot(s2);
    CHECK(s2.settings.tuning.root == 2 && s2.settings.tuning.scale == SC_DORIAN && s2.settings.bpm == 96, "tuning and tempo restored");
    CHECK(std::strcmp(s2.lead.name, "Dreamy Keys") == 0, "lead patch restored");
    CHECK(s2.progLen == 4 && s2.prog[1] == 5, "progression restored");
    CHECK(s2.layers.size() == s1.layers.size() && s2.loopBeats == s1.loopBeats, "loop restored");
    bool same = s2.layers.size() == 1 && s2.layers[0].ev.size() == s1.layers[0].ev.size();
    for (size_t i = 0; same && i < s1.layers[0].ev.size(); ++i)
        if (s1.layers[0].ev[i].beat != s2.layers[0].ev[i].beat || s1.layers[0].ev[i].a != s2.layers[0].ev[i].a) same = false;
    CHECK(same, "loop events identical after restore");
    b.inst.cmd(EV_LOOP, LC_PLAYSTOP);
    int heard = 0; bool w = false;
    b.run(6.0, [&]() { bool on = b.gated(2) > 0; if (on && !w) ++heard; w = on; });
    CHECK(heard >= 1, "restored loop plays back (%d phrases)", heard);
}

static void testPatchSwitchClicks() {
    SECTION("no discontinuity when switching sound under a held note");
    Rig r;
    Patch a = purePatch(); a.p[P_WAVE1] = W_TRI;
    Patch b = a; b.p[P_CUTOFF] = 600; b.p[P_LEVEL] = 0.25f; b.setName("B");
    r.inst.setPatch(0, a);
    r.inst.key(1, 0, true);
    r.run(0.5);
    float steady = 0;
    for (size_t i = r.L.size() - 4800; i < r.L.size(); ++i) steady = std::max(steady, std::fabs(r.L[i] - r.L[i - 1]));
    size_t mark = r.L.size();
    r.inst.setPatch(0, b);
    r.run(0.2);
    float sw = 0;
    for (size_t i = mark; i < r.L.size(); ++i) sw = std::max(sw, std::fabs(r.L[i] - r.L[i - 1]));
    CHECK(sw <= steady * 1.5f + 0.002f, "max sample step at switch %.5f vs steady %.5f", sw, steady);
}

static void testAnalysis() {
    SECTION("listen: key and tempo estimation on synthetic music");
    auto synth = [](const int chords[4][3], float bpm, std::vector<float> &out) {
        float beat = 60.0f / bpm;
        int total = (int)(SR * beat * 16);
        out.assign(total, 0.0f);
        for (int i = 0; i < total; ++i) {
            float t = i / SR;
            int b = (int)(t / beat);
            const int *ch = chords[(b / 4) % 4];
            float tb = t - b * beat;
            float env = std::exp(-tb * 5.0f);
            float s = 0;
            for (int k = 0; k < 3; ++k) {
                float f = (float)midiToHz(ch[k]);
                s += std::sin(2 * kPi * f * t) + 0.4f * std::sin(4 * kPi * f * t);
            }
            out[i] = 0.12f * s * (0.3f + 0.7f * env);
        }
    };
    const int cmaj[4][3] = {{60, 64, 67}, {65, 69, 72}, {67, 71, 74}, {60, 64, 67}};
    const int amin[4][3] = {{57, 60, 64}, {62, 65, 69}, {64, 67, 71}, {57, 60, 64}};
    std::vector<float> buf;
    synth(cmaj, 120, buf);
    ListenResult r1 = analyseAudio(buf.data(), (int)buf.size(), SR);
    CHECK(r1.ok && r1.keys[0].root == 0 && !r1.keys[0].minor, "C major detected (got %s %s, conf %.2f)", noteName(r1.keys[0].root), r1.keys[0].minor ? "min" : "maj", r1.keyConfidence);
    CHECK(near(r1.bpm, 120, 0.03f) || near(r1.bpm, 60, 0.03f), "tempo 120 detected (%.1f, conf %.2f)", r1.bpm, r1.bpmConfidence);
    synth(amin, 96, buf);
    ListenResult r2 = analyseAudio(buf.data(), (int)buf.size(), SR);
    CHECK(r2.ok && r2.keys[0].root == 9 && r2.keys[0].minor, "A minor detected (got %s %s, conf %.2f)", noteName(r2.keys[0].root), r2.keys[0].minor ? "min" : "maj", r2.keyConfidence);
    CHECK(near(r2.bpm, 96, 0.03f), "tempo 96 detected (%.1f)", r2.bpm);
    std::vector<float> quiet((int)(SR * 4), 0.0f);
    ListenResult r3 = analyseAudio(quiet.data(), (int)quiet.size(), SR);
    CHECK(!r3.ok, "silence is reported as unusable, not guessed (%s)", r3.problem);
    Rng rng; std::vector<float> noise((int)(SR * 6));
    for (float &v : noise) v = rng.bipolar() * 0.2f;
    ListenResult r4 = analyseAudio(noise.data(), (int)noise.size(), SR);
    CHECK(r4.keyConfidence < 0.5f, "noise gives low key confidence (%.2f)", r4.keyConfidence);
}

static void bench() {
    Instrument inst(SR);
    enableFlushToZero();
    Patch heavy = factoryPatch(2); // unison + osc B + chorus + reverb
    heavy.p[P_SUB] = 0.4f; heavy.p[P_NOISE] = 0.05f; heavy.p[P_DRIVE] = 0.4f; heavy.p[P_FM] = 0.5f;
    heavy.p[P_ATK] = 0.01f; heavy.p[P_REL] = 6.0f;
    inst.setPatch(0, heavy);
    inst.set(S_SEVENTH, 1);
    float l[256], r[256];
    inst.process(l, r, 256);
    inst.cmd(EV_CHORD, 0);
    // retrigger strings so release tails pile up to the full voice pool
    int maxV = 0;
    auto t0 = std::chrono::steady_clock::now();
    const int blocks = (int)(20.0 * SR / 256);
    double worst = 0;
    for (int b = 0; b < blocks; ++b) {
        int s = (b / 12) % 3, c = (b / 36) % 7;
        if (b % 12 == 0) inst.key(s, c, true);
        if (b % 12 == 6) inst.key(s, c, false);
        auto a = std::chrono::steady_clock::now();
        inst.process(l, r, 256);
        double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count();
        if (dt > worst) worst = dt;
        int v = inst.uiState().voices;
        if (v > maxV) maxV = v;
    }
    double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    double period = 256 / SR;
    std::printf("bench: 20 s of audio, up to %d voices (pool %d), heavy patch + all effects\n", maxV, kVoices);
    std::printf("  render time %.3f s  ->  %.1f%% of one core, %.1fx realtime\n", el, el / 20.0 * 100.0, 20.0 / el);
    std::printf("  worst 256-frame block %.3f ms of %.3f ms budget (%.0f%%)\n", worst * 1e3, period * 1e3, worst / period * 100);
}

int main(int argc, char **argv) {
    if (argc > 1 && std::strcmp(argv[1], "--bench") == 0) { bench(); return 0; }
    testScales();
    testFrequenciesAndGlide();
    testPolyphony();
    testPresetsAndRandomiser();
    testExtraSounds();
    testDrums();
    testTrack();
    testSoundShelf();
    testLimiter();
    testXY();
    testGlitch();
    testBacking();
    testLooper();
    testSessionRoundTrip();
    testPatchSwitchClicks();
    testAnalysis();
    std::printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
