// venv-vrequency — three "settings pictures" of a sound, drawn from its real parameters: the
// oscillators (one cycle, before the filter), the filter's response and the amp envelope. They are
// plain drawing: not touchable by themselves, and any page can hold a SoundPictures and call
// paint with a rectangle and a patch's parameter array. The curves are kept until a parameter they
// depend on changes (sound_curves.h computes them with the engine's own maths).
#pragma once
#include "app/pages/sound/sound_curves.h"
#include "app/ui/ui.h"

class SoundPictures {
public:
    explicit SoundPictures(Ui &u) : ui(u) {}

    // gate: is a note sounding now; nowSec: a steady clock in seconds (for where the note is in its envelope)
    void note(bool gate, double nowSec);
    void oscillators(QRectF r, const float *p);
    void filter(QRectF r, const float *p);
    void envelope(QRectF r, const float *p);

private:
    Ui &ui;
    td::OscCurve osc;
    td::FilterCurve flt;
    td::EnvCurve env;
    float oscKey[td::P_COUNT], fltKey[td::P_COUNT], envKey[td::P_COUNT];
    bool oscOk = false, fltOk = false, envOk = false;
    bool gate = false;
    double tOn = 0, tOff = -1, tNow = 0;      // when the note started, when it was let go, and the clock now

    QRectF panel(QRectF r, const QString &title, const QString &value);   // frame + heading; returns the plot area
    bool stale(const float *p, const float *key, const int *ids, int n) const;
};
