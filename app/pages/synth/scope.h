// venv-vrequency — the oscilloscope on the Synth page: what it measures, how it ranges, how it draws.
// scope.cpp draws the screen, scope_measure.cpp reads and measures the signal, scope_trace.cpp
// strokes the phosphor trace, scope_stack.cpp draws the 3D stack of the latest real cycles.
#pragma once
#include "app/shell/part.h"
#include <QImage>
#include <QPixmap>
#include <QRectF>
#include <vector>

class Scope : public Part {
public:
    explicit Scope(Shell &s);
    void draw(QRectF xy);                    // the whole scope inside `xy`; touching it is the X-Y control
    bool stackView = true;                   // true: the 3D stack of cycles; false: the classic moving line

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
    int layerCount = 0;

    View acquire();                          // read, measure, trigger, auto-range (scope_measure.cpp)
    float measureFrequency(const View &v);   // the period in samples, 0 when there is none; sets measFreq
    void autoRange(const View &v, float period);
    void paintGraticule(QRectF gr, qreal div);
    void paintTrace(const View &v, QRectF gr, qreal div); // the phosphor (scope_trace.cpp)
    void captureCycle(const View &v);        // the newest real cycle onto the front of the stack (scope_stack.cpp)
    void paintStack(QRectF gr, qreal div);
    void paintMarkers(QRectF xy, QRectF gr);
    void paintReadouts(QRectF xy, QRectF gr);
};
