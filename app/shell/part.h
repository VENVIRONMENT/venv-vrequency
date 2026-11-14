// venv-vrequency — a piece of the app: a page, or a helper several pages share. It is given the
// Shell and keeps the four things nearly every piece reads close at hand.
#pragma once
#include "app/shell/shell.h"

class Part {
public:
    explicit Part(Shell &s) : sh(s), ui(s.ui), inst(s.inst), u(s.u) {}

protected:
    Shell &sh;
    Ui &ui;
    td::Instrument &inst;
    const td::UiState &u;                    // the engine's state as of this frame
};
