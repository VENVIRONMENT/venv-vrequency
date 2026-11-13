// Tendril — things the interface's translation units share: panel geometry
// (in the 720x720 canvas) and what is known about the audio output device.
#pragma once
#include <QRectF>
#include <QString>

namespace ui {
constexpr qreal CW = 720;

// display assembly (left), as on the reference instrument
const QRectF MOULD(8, 30, 462, 380);   // moulded frame round the tube
const QRectF TUBE(30, 44, 418, 330);   // dark tube surround
const QRectF GLASS(36, 50, 406, 318);  // lit raster
const QRectF BAND(36, 50, 406, 18);    // status line (inverse video)
const QRectF GRAT(39, 70, 400, 232);   // 10 x 8 divisions
const QRectF MEAS(36, 304, 406, 30);   // two measurement lines
const QRectF SOFTS(36, 334, 406, 34);  // soft-key labels
const qreal SOFT_W = 406.0 / 6.0;
const QRectF UNDER(8, 412, 462, 54);   // strip under the display
const QRectF PANEL(476, 30, 236, 436); // control groups (right)
const QRectF DECK(8, 472, 704, 242);   // key deck

// key deck rows
constexpr qreal DECK_X0 = 22, DECK_DX = 68;
constexpr qreal NUM_Y = 478, NUM_H = 46, ROW_Y0 = 526, ROW_H = 62;

extern QString sinkDescription;
extern bool sinkIsDummy, sinkIsBluetooth;
extern int sinkGeneration; // bumps when the output device type changes

// scope ranges
constexpr int TB_COUNT = 7, VD_COUNT = 5;
extern const float TB_MS[TB_COUNT];   // ms per division
extern const float VD_V[VD_COUNT];    // volts (full scale = 1.0) per division
QString tbText(int i);
QString vdText(int i);
QString nice(const char *label); // "WAVE A" -> "Wave A"
} // namespace ui
