// venv-vrequency — "listen": estimate musical key and tempo from recorded audio.
// Chromagram + Krumhansl-Kessler key profiles; onset-envelope autocorrelation
// for tempo. Results carry confidences and alternatives: it never pretends to
// be certain about ambiguous input.
#pragma once

namespace td {

struct KeyGuess {
    int root = 0;       // pitch class 0..11
    bool minor = false;
    float score = 0;    // correlation with the key profile (-1..1)
};

struct ListenResult {
    bool ok = false;
    const char *problem = "";  // set when !ok (e.g. input too quiet)
    KeyGuess keys[3];          // best first
    float keyConfidence = 0;   // 0..1
    float bpm = 0;
    float bpmConfidence = 0;   // 0..1
    float level = 0;           // input RMS
};

ListenResult analyseAudio(const float *mono, int n, float sampleRate);

} // namespace td
