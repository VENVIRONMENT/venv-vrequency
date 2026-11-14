// venv-vrequency — scales, string layout and diatonic chords.
#pragma once
#include <cmath>
#include <cstdio>

namespace td {

enum ScaleId {
    SC_MAJOR, SC_MINOR, SC_MAJPENT, SC_MINPENT, SC_BLUES, SC_DORIAN, SC_MIXO, SC_CHROMATIC,
    SC_COUNT
};

struct ScaleDef {
    const char *name;
    const char *shortName;
    int n;
    int steps[12];
    int chordParent; // 7-note scale used to build backing chords
};

inline const ScaleDef &scaleDef(int id) {
    static const ScaleDef S[SC_COUNT] = {
        {"Major", "MAJ", 7, {0, 2, 4, 5, 7, 9, 11}, SC_MAJOR},
        {"Natural minor", "MIN", 7, {0, 2, 3, 5, 7, 8, 10}, SC_MINOR},
        {"Major pentatonic", "MAJ PENT", 5, {0, 2, 4, 7, 9}, SC_MAJOR},
        {"Minor pentatonic", "MIN PENT", 5, {0, 3, 5, 7, 10}, SC_MINOR},
        {"Blues", "BLUES", 6, {0, 3, 5, 6, 7, 10}, SC_MINOR},
        {"Dorian", "DORIAN", 7, {0, 2, 3, 5, 7, 9, 10}, SC_DORIAN},
        {"Mixolydian", "MIXO", 7, {0, 2, 4, 5, 7, 9, 10}, SC_MIXO},
        {"Chromatic", "CHROM", 12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, SC_MAJOR},
    };
    if (id < 0) id = 0;
    if (id >= SC_COUNT) id = SC_COUNT - 1;
    return S[id];
}

inline const char *noteName(int pc) {
    static const char *N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return N[((pc % 12) + 12) % 12];
}

inline double midiToHz(double m) { return 440.0 * std::pow(2.0, (m - 69.0) / 12.0); }

// k-th note of the scale counted from the root at MIDI `rootMidi` (k may be negative).
inline int scaleNote(int scale, int rootMidi, int k) {
    const ScaleDef &s = scaleDef(scale);
    int oct = (k >= 0) ? k / s.n : -((-k + s.n - 1) / s.n);
    int idx = k - oct * s.n;
    return rootMidi + 12 * oct + s.steps[idx];
}

// Index of the scale note nearest to `midi` (ties resolve downward).
inline int nearestScaleIndex(int scale, int rootMidi, int midi) {
    const ScaleDef &s = scaleDef(scale);
    int k = (int)std::floor((midi - rootMidi) / 12.0) * s.n;
    int best = k, bestD = 1000;
    for (int i = k - 1; i <= k + s.n + 1; ++i) {
        int d = std::abs(scaleNote(scale, rootMidi, i) - midi);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

// Lowest note of the bottom string for a given key: keeps the instrument in a
// comfortable register whatever the root (E2..D#3).
inline int baseRootMidi(int root) { return (root >= 4 ? 36 : 48) + root; }

struct Tuning {
    int root = 9;            // pitch class, 9 = A
    int scale = SC_MINPENT;
    int octave = 0;          // octave shift
    int rowInterval = 5;     // semitones between adjacent strings

    // rowFromBottom: 0 = lowest string. col: 0 = leftmost key.
    int noteFor(int rowFromBottom, int col, bool chromatic) const {
        int rootMidi = baseRootMidi(root) + 12 * octave;
        int target = rootMidi + rowInterval * rowFromBottom;
        int k0 = nearestScaleIndex(scale, rootMidi, target);
        if (chromatic) return scaleNote(scale, rootMidi, k0) + col;
        return scaleNote(scale, rootMidi, k0 + col);
    }
};

struct Chord {
    int notes[5]; // [0] = bass, then upper voices
    int count = 0;
    char name[12] = {0};
};

// Diatonic chord on `degree` (0..6) of the scale's 7-note parent.
// Upper voices are folded into one fixed register, which gives smooth voice
// leading between successive chords without any explicit inversion logic.
inline Chord buildChord(const Tuning &t, int degree, bool seventh, int octaveShift = 0, bool withName = true) {
    Chord c;
    int parent = scaleDef(t.scale).chordParent;
    const ScaleDef &ps = scaleDef(parent);
    degree = ((degree % 7) + 7) % 7;
    int iv[4];
    int nTones = seventh ? 4 : 3;
    for (int i = 0; i < nTones; ++i) {
        int k = degree + 2 * i;
        iv[i] = ps.steps[k % 7] + 12 * (k / 7);
    }
    int rootPc = (t.root + iv[0]) % 12;
    int bass = 36 + rootPc;                 // C2..B2
    if (bass > 45) bass -= 12;              // keep the bass in A#1..A2
    bass += 12 * octaveShift;
    c.notes[c.count++] = bass;
    const int lo = 52 + 12 * octaveShift;   // upper voices live in [lo, lo+12)
    for (int i = 0; i < nTones; ++i) {
        int pc = (t.root + iv[i]) % 12;
        int n = lo + ((pc - lo) % 12 + 12) % 12;
        c.notes[c.count++] = n;
    }
    // sort the upper voices so voice index tracks pitch order
    for (int i = 1; i < c.count; ++i)
        for (int j = i + 1; j < c.count; ++j)
            if (c.notes[j] < c.notes[i]) { int x = c.notes[i]; c.notes[i] = c.notes[j]; c.notes[j] = x; }
    if (!withName) return c; // real-time callers skip the string formatting
    int third = iv[1] - iv[0], fifth = iv[2] - iv[0];
    int sev = seventh ? iv[3] - iv[0] : 0;
    const char *q = "";
    if (third == 4 && fifth == 7) q = seventh ? (sev == 11 ? "maj7" : "7") : "";
    else if (third == 3 && fifth == 7) q = seventh ? "m7" : "m";
    else if (third == 3 && fifth == 6) q = seventh ? (sev == 10 ? "m7b5" : "dim7") : "dim";
    else if (third == 4 && fifth == 8) q = "aug";
    std::snprintf(c.name, sizeof c.name, "%s%s", noteName(rootPc), q);
    return c;
}

inline const char *romanNumeral(int degree, bool minorish) {
    static const char *U[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    static const char *L[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    degree = ((degree % 7) + 7) % 7;
    return minorish ? L[degree] : U[degree];
}

} // namespace td
