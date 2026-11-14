// venv-vrequency — small facts about zone names that the guide's files share.
#pragma once
#include <QRectF>
#include <QString>
#include <QStringList>

// Names that stand for several zones. Only used when no zone has the name itself.
QStringList aliasZones(const QString &n);
// Overlay zones (the dock, the ticker, the key-test panel) are never pointed at.
inline bool hiddenZone(const QString &n) { return n.startsWith("dock") || n == "ticker" || n == "keytestblock"; }
inline QRectF inflated(const QRectF &r, qreal d) { return r.adjusted(-d, -d, d, d); }
