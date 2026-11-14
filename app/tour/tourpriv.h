// venv-vrequency — facts the split parts of the tour share (page list, walk order, card regions); not part of tour.h.
#pragma once
#include "app/tour/tour.h"
#include <QRectF>
#include <QStringList>

// the walk of the "Start here" guide
int tourPathLen();
const char *tourPathAt(int i);
// concepts worth reading beside each page (the READ ALSO chips of a page's intro card)
QStringList tourReadAlso(const QString &page);

inline QRectF grow(const QRectF &r, qreal d) { return r.adjusted(-d, -d, d, d); }
inline const QRectF kRegionA(396, 62, 316, 380), kRegionB1(8, 452, 420, 222), kRegionB2(436, 452, 276, 206);
inline const qreal kPicSize = 380, kScale = kPicSize / 720.0;
inline const QRectF kAskRect(446, 668, 256, 40);
