#include "analysis.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace td {

ListenResult analyseAudio(const float *mono, int n, float sr) {
    ListenResult res;
    if (n < (int)(sr * 2.0f)) { res.problem = "recording too short"; return res; }

    // ---- level check ----
    double e = 0;
    for (int i = 0; i < n; ++i) e += (double)mono[i] * mono[i];
    res.level = (float)std::sqrt(e / n);
    if (res.level < 0.003f) { res.problem = "too quiet to analyse"; return res; }

    // ---- decimate to ~12 kHz ----
    int dec = std::max(1, (int)(sr / 12000.0f + 0.5f));
    float dsr = sr / dec;
    int m = n / dec;
    std::vector<float> x(m);
    for (int i = 0; i < m; ++i) {
        float s = 0;
        for (int k = 0; k < dec; ++k) s += mono[i * dec + k];
        x[i] = s / dec;
    }

    // ---- chromagram (Goertzel per semitone, C3..B6) ----
    const int W = 2048, H = 1024;
    std::vector<float> win(W);
    for (int i = 0; i < W; ++i) win[i] = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * i / (W - 1));
    double chroma[12] = {0};
    std::vector<float> frame(W);
    for (int start = 0; start + W <= m; start += H) {
        for (int i = 0; i < W; ++i) frame[i] = x[start + i] * win[i];
        for (int note = 48; note < 96; ++note) {
            float f = 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
            if (f > dsr * 0.45f) break;
            float c = 2.0f * std::cos(2.0f * 3.14159265f * f / dsr);
            float s0 = 0, s1 = 0, s2 = 0;
            for (int i = 0; i < W; ++i) { s0 = frame[i] + c * s1 - s2; s2 = s1; s1 = s0; }
            float p = s1 * s1 + s2 * s2 - c * s1 * s2;
            chroma[note % 12] += std::sqrt(std::max(p, 0.0f));
        }
    }
    static const double MAJ[12] = {6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88};
    static const double MIN[12] = {6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17};
    auto corr = [&](const double *prof, int root) {
        double mx = 0, my = 0;
        for (int i = 0; i < 12; ++i) { mx += chroma[i]; my += prof[i]; }
        mx /= 12; my /= 12;
        double sxy = 0, sxx = 0, syy = 0;
        for (int i = 0; i < 12; ++i) {
            double a = chroma[(i + root) % 12] - mx, b = prof[i] - my;
            sxy += a * b; sxx += a * a; syy += b * b;
        }
        return (sxx > 0 && syy > 0) ? sxy / std::sqrt(sxx * syy) : 0.0;
    };
    KeyGuess all[24];
    for (int r = 0; r < 12; ++r) {
        all[r] = {r, false, (float)corr(MAJ, r)};
        all[12 + r] = {r, true, (float)corr(MIN, r)};
    }
    std::sort(all, all + 24, [](const KeyGuess &a, const KeyGuess &b) { return a.score > b.score; });
    for (int i = 0; i < 3; ++i) res.keys[i] = all[i];
    float margin = all[0].score - all[1].score;
    res.keyConfidence = std::clamp(0.5f * std::clamp(all[0].score, 0.0f, 1.0f) + 0.5f * std::clamp(margin / 0.2f, 0.0f, 1.0f), 0.0f, 1.0f);
    if (all[0].score < 0.3f) res.keyConfidence *= 0.3f;
    // tonal music has strongly uneven pitch classes; noise and drums do not
    double cm = 0, cv = 0;
    for (double c : chroma) cm += c;
    cm /= 12;
    for (double c : chroma) cv += (c - cm) * (c - cm);
    cv = cm > 0 ? std::sqrt(cv / 12) / cm : 0;
    res.keyConfidence *= (float)std::clamp((cv - 0.05) / 0.2, 0.0, 1.0);

    // ---- tempo: autocorrelation of the onset envelope ----
    const int hop = 128, ewin = 512;
    int frames = (m - ewin) / hop;
    if (frames < 64) { res.ok = true; return res; }
    std::vector<float> env(frames, 0.0f);
    float prev = 0;
    for (int f = 0; f < frames; ++f) {
        double en = 0;
        for (int i = 0; i < ewin; ++i) { float v = x[f * hop + i]; en += v * v; }
        float le = (float)std::log(1e-7 + en / ewin);
        float d = le - prev;
        env[f] = f > 0 && d > 0 ? d : 0;
        prev = le;
    }
    float mean = 0;
    for (float v : env) mean += v;
    mean /= frames;
    for (float &v : env) v -= mean;
    float envRate = dsr / hop;
    int maxLag = std::min(frames / 2, (int)(envRate * 60.0f / 40.0f) * 2 + 2);
    std::vector<double> ac(maxLag + 2, 0.0);
    for (int l = 0; l <= maxLag + 1; ++l) {
        double a = 0;
        for (int i = 0; i + l < frames; ++i) a += env[i] * env[i + l];
        ac[l] = a / (frames - l); // unbiased
    }
    auto acAt = [&](float lag) {
        int l0 = (int)lag;
        if (l0 + 1 > maxLag) return 0.0;
        float fr = lag - l0;
        return ac[l0] * (1 - fr) + ac[l0 + 1] * fr;
    };
    double best = 0; float bestBpm = 0;
    if (ac[0] > 1e-9) {
        float prevScore = -1e9f, prevPrev = -1e9f;
        for (float bpm = 50; bpm <= 200; bpm += 0.25f) {
            float lag = envRate * 60.0f / bpm;
            // a real pulse also correlates at two and four beats
            double sc = (acAt(lag) + 0.5 * acAt(2 * lag) + 0.25 * acAt(4 * lag)) / (1.75 * ac[0]);
            // previous step was a local maximum: weigh it with a broad tempo prior
            if (prevScore > prevPrev && prevScore >= sc) {
                float pb = bpm - 0.25f;
                double lg = std::log2(pb / 110.0);
                double w = prevScore * std::exp(-0.5 * lg * lg);
                if (w > best && pb >= 60 && pb <= 190) { best = w; bestBpm = pb; }
            }
            prevPrev = prevScore; prevScore = (float)sc;
        }
    }
    res.bpm = bestBpm;
    res.bpmConfidence = (float)std::clamp(best * 2.5, 0.0, 1.0);
    res.ok = true;
    return res;
}

} // namespace td
