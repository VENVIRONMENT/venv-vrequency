// venv-vrequency — the picture of a pulse pad's sound: the pad's own generator is run offline
// (pulseRender, the code the audio thread plays) and drawn as a thin antialiased outline over a
// faint fill. A picture is kept until the pad's settings, the rate SYNC gives it or its size change.
#pragma once
#include "core/pulse/pulse.h"
#include <QImage>
#include <QSize>

class PulsePicture {
public:
    // the image for these settings (drawn again only when something changed)
    const QImage &get(const td::PulsePad &p, float rateHz, float sampleRate, QSize size, bool centreLine);
    int onsets() const { return nOnsets; }     // hits, clicks or gates in the picture
    int samples() const { return nSamples; }   // how long the picture is, in samples

    static int windowSamples(const td::PulsePad &p, float sampleRate); // a tick is shown over 24 samples

private:
    QImage img;
    td::PulsePad key;
    float keyRate = -1, keySr = 0;
    QSize keySize;
    bool keyLine = false;
    int nOnsets = 0, nSamples = 0;
    void draw(const td::PulsePad &p, float rateHz, float sampleRate, QSize size, bool centreLine);
};
