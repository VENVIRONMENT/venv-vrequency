// venv vrequency — engine tests. Renders the instrument offline and measures what
// actually comes out (frequencies, envelopes, voices), rather than trusting
// internal state alone.
//
//   vrequency_tests          run all checks
//   vrequency_tests --bench  CPU benchmark at maximum polyphony
#include "analysis.h"
#include "instrument.h"
#include "presets.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
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
    testLimiter();
    testXY();
    testBacking();
    testLooper();
    testSessionRoundTrip();
    testPatchSwitchClicks();
    testAnalysis();
    std::printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
