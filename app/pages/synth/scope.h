// venv-vrequency — the oscilloscope on the Synth page: what it measures, how it ranges, how it draws.
// scope.cpp draws the screen, scope_measure.cpp reads and measures the signal, scope_trace.cpp
// strokes the phosphor trace, scope_stack.cpp draws the 3D stack of the latest real cycles (and the
// waterfall, which is the same stack made of spectra), scope_spectrum.cpp the spectrum and its FFT,
// scope_phase.cpp the phase figure, scope_level.cpp the loudness history.
#pragma once
#include "app/shell/part.h"
#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QRectF>
#include <vector>

QString hzText(float hz);                    // "441Hz", "1.20kHz" (scope_spectrum.cpp)

class Scope : public Part {
public:
    explicit Scope(Shell &s);
    void draw(QRectF xy);                    // the whole scope inside `xy`; touching it is the X-Y control
    enum Mode { M_STACK, M_SPECTRUM, M_WATERFALL, M_PHASE, M_LEVEL, M_LINE, M_COUNT };
    Mode mode = M_STACK;                     // which view of the output sound is drawn
    const char *cycleMode();                 // the next view; returns its long name (for the toast)
    static const char *modeShort(Mode m);    // the button label

    static constexpr int kFft = 4096, kSpecW = 322, kFigs = 6, kFigPts = 200, kLvl = 400;
    static constexpr int kTraceW = 644, kTraceH = 184;   // the trace area, in pixels
    static constexpr int kTimeSteps = 7, kVoltSteps = 6, kDivsX = 14, kDivsY = 4;
    static constexpr float kMsPerDiv[kTimeSteps] = {0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f};
    static constexpr float kVoltsPerDiv[kVoltSteps] = {0.05f, 0.1f, 0.2f, 0.5f, 1.0f, 2.0f};

private:
    // what one frame looked at: the `total` newest samples, of which `need` (from `start`) are on screen
    struct View { int total = 0, need = 0, start = 0; float span = 0; };

    int tbIdx = 4, vdIdx = 1, rangeT = 0, rangeV = 0; // time/div and volts/div steps, auto-ranged
    float measFreq = 0, measPk = 0, period = 0; // period: in samples, 0 when there is no clear pitch
    bool trigd = false;
    QImage phos;                             // fading trace
    float glitchVis = 0, glitchHue = 0;      // colour streaks while the crusher is on
    QPixmap gratPix;                         // graticule
    std::vector<float> buf;                  // the newest output samples
    static constexpr int kLayers = 26, kPts = 128;
    float layers[kLayers][kPts] = {};        // the stack: [0] is the newest cycle, in volts
    int layerCount = 0;                      // (the waterfall keeps its spectra here too, as 0..1)
    std::vector<float> db;                   // the newest spectrum, dB, bins 0..kFft/2
    float specLine[kSpecW] = {};             // the drawn spectrum, dB, smoothed in time
    float peakHz = 0, peakDb = -90;          // the strongest peak of the newest spectrum
    float figs[kFigs][kFigPts][2] = {};      // phase figures, [0] newest, in volts
    int figCount = 0;
    float figScale = 0, figDelayMs = 2;
    float lvl[kLvl] = {};                    // loudness history, 10 ms steps, newest last (peak, volts)
    float lvlAcc = 0, lvlCarry = 0; int lvlAccN = 0;
    QElapsedTimer lvlClock;
    std::vector<float> lvlBuf;

    View acquire();                          // read, measure, trigger, auto-range (scope_measure.cpp)
    float measureFrequency(const View &v);   // the period in samples, 0 when there is none; sets measFreq
    void autoRange(const View &v, float period);
    void paintGraticule(QRectF gr, qreal div);
    void paintTrace(const View &v, QRectF gr, qreal div); // the phosphor (scope_trace.cpp)
    void captureCycle(const View &v);        // the newest real cycle onto the front of the stack (scope_stack.cpp)
    void paintStack(QRectF gr, qreal div, bool fall = false); // fall: the layers are spectra
    void computeSpectrum(const View &v);     // FFT of the newest 4096 samples (scope_spectrum.cpp)
    void paintSpectrum(QRectF gr);
    void captureSpectrumRow();               // the newest spectrum onto the front of the waterfall
    void paintFallLabels(QRectF gr);
    void capturePhase(const View &v);        // (scope_phase.cpp)
    void paintPhase(QRectF gr, qreal div);
    QString ratioText() const;               // the lead sound's B:A setting
    void updateLevel();                      // (scope_level.cpp)
    void paintLevel(QRectF gr);
    void paintViewReadouts(QRectF ml, const QString &freq, const QString &note); // the measurement line of the new views
    void paintMarkers(QRectF xy, QRectF gr);
    void paintReadouts(QRectF xy, QRectF gr);
};
