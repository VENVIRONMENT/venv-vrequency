// venv-vrequency — small drawing helpers the settings pictures share: smooth thin lines, a grey
// fill, dashed and dotted rules, a dot. White and greys on black only.
#pragma once
#include "app/ui/palette.h"
#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace picdraw {

inline QColor grey(int a) { QColor c = pal::INK; c.setAlpha(a); return c; }

inline QPen pen(const QColor &c, qreal w, Qt::PenStyle s = Qt::SolidLine) {
    QPen p(c, w, s, Qt::RoundCap, Qt::RoundJoin);
    return p;
}

// a dashed pen whose dashes are the same length however thick the line
inline QPen dashed(const QColor &c, qreal w, qreal on, qreal off) {
    QPen p(c, w, Qt::CustomDashLine, Qt::FlatCap, Qt::RoundJoin);
    p.setDashPattern({on / w, off / w});
    return p;
}

// Points (x[i], y[i]) joined by a line; with a base y, closed down to it for a fill.
template <class X, class Y>
QPainterPath line(int n, X x, Y y, bool fillTo = false, qreal base = 0) {
    QPainterPath path;
    if (fillTo) { path.moveTo(x(0), base); path.lineTo(x(0), y(0)); }
    else path.moveTo(x(0), y(0));
    for (int i = 1; i < n; ++i) path.lineTo(x(i), y(i));
    if (fillTo) { path.lineTo(x(n - 1), base); path.closeSubpath(); }
    return path;
}

inline void dot(QPainter *g, QPointF c, qreal r) {
    g->setPen(pen(pal::BG, 2.0));
    g->setBrush(pal::INK);
    g->drawEllipse(c, r, r);
    g->setBrush(Qt::NoBrush);
}

// one picture's drawing with antialiasing on, restored when it goes out of scope
struct Smooth {
    QPainter *g; bool was;
    explicit Smooth(QPainter *p) : g(p), was(p->testRenderHint(QPainter::Antialiasing)) {
        g->setRenderHint(QPainter::Antialiasing, true);
        g->save();
    }
    ~Smooth() { g->restore(); g->setRenderHint(QPainter::Antialiasing, was); }
};

} // namespace picdraw
