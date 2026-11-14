// venv-vrequency — engine tests: "listen": key and tempo estimation.
#include "tests/support.h"
#include "core/analysis/analysis.h"
#include "core/synth/scale.h"
#include <cmath>
#include <vector>

void testAnalysis() {
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
