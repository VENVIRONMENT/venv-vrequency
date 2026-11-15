// venv-vrequency — the seven chord pads of the Synth page, with their mode (Hold or Drone) and
// Play / Stop buttons beside them.
#pragma once
#include "app/shell/part.h"
#include <QRectF>

class ChordPads : public Part {
public:
    using Part::Part;
    void paint(QRectF area);
};
