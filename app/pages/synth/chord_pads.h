// venv-vrequency — the seven chord pads (with the progression strip on the Synth page): drawn on
// the Synth page and the Backing page alike, so they live here as a helper both are given.
#pragma once
#include "app/shell/part.h"
#include <QRectF>

class ChordPads : public Part {
public:
    using Part::Part;
    void paint(QRectF area, bool withSlots);  // withSlots: the progression strip above the pads
};
