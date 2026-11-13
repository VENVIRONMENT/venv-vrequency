// Tendril — hardware rendering kit.
// Stateless drawing routines for the instrument's physical parts: moulded
// chassis, bezels, rotary knobs, key caps, rubber keys, lamps. Everything is
// lit from the top-left. Parts that never change shape are rendered once into
// sprites, so a full repaint is mostly blits.
#pragma once
#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QRectF>
#include <QString>

namespace hw {

// --- materials ---
extern const QColor CHASSIS, CHASSIS_HI, CHASSIS_LO, INK, INK_SOFT;
extern const QColor BAR_TIME, BAR_FILTER, BAR_MENU, BAR_RUN, BAR_SPACE;
extern const QColor LAMP_GREEN, LAMP_RED, LAMP_AMBER;
extern const QColor DECK, STRIP, LEG_BLUE, LEG_RED, LEG_ORANGE, KEY_GREEN;
// --- CRT phosphor ---
extern const QColor CRT_BG, CRT_P, CRT_DIM, CRT_GRAT, CRT_BAND, CRT_DARK;

enum CapStyle { CAP_GREY, CAP_WHITE, CAP_OVAL, CAP_SOFT, CAP_RUN };

QFont labelFont(int px, bool bold = false); // narrow grotesque, printed legends
QFont crtFont(int px, bool bold = false);   // raster tube text (no antialiasing)
QFont wordmarkFont(int px);

// moulded plastic: base colour, fine grain, a broad top-left light falloff
QImage plastic(int w, int h, QColor base, int grain, quint32 seed, int falloff = 8);

void groove(QPainter &p, QRectF r, qreal rad);                       // engraved outline
void plate(QPainter &p, QRectF r, qreal rad, QColor face, int lift); // raised (lift > 0) or sunk (lift < 0) area
void sectionTitle(QPainter &p, QRectF frame, const QString &title, QColor bar);
void legend(QPainter &p, QRectF r, const QString &s, int px, int align = Qt::AlignCenter, QColor c = QColor(), bool bold = false);
void screw(QPainter &p, QPointF c, qreal r, qreal slotDeg);

// knobs: the seat (shadow, collar, printed scale) is static; the knob turns
void knobSeat(QPainter &p, QPointF c, qreal r, int ticks, bool endless);
void knob(QPainter &p, QPointF c, qreal r, qreal angleDeg, bool grabbed);

// push buttons with their recessed housings (sprite cached per look)
void cap(QPainter &p, QRectF r, const QString &label, bool down, QColor lit, int style);
void lamp(QPainter &p, QPointF c, qreal r, QColor col, bool on);
void rubberKey(QPainter &p, QRectF r, bool down, QColor glow, QColor body);

// tube face overlay: vignette, corner masks, faint reflection, scan lines
QPixmap glass(int w, int h, qreal cornerRad);

} // namespace hw
