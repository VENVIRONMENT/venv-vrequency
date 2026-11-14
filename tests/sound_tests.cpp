// venv-vrequency — engine tests: the factory sounds, the randomiser, the dream sounds and the shelf.
#include "tests/support.h"
#include "core/presets/presets.h"
#include "core/presets/sound_shelf.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

void testPresetsAndRandomiser() {
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

void testExtraSounds() {
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

void testSoundShelf() {
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
