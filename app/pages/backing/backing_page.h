// venv-vrequency — the Backing page (PG_BACK): how a chord tap behaves, the style, the progression
// slots, the chord pads and the backing's own settings.
#pragma once
#include "app/pages/page.h"
#include "app/pages/synth/chord_pads.h"

class Sounds;

class BackingPage : public Page {
public:
    BackingPage(Shell &s, ChordPads &c);
    void paint() override;

private:
    Sounds &sounds;
    ChordPads &pads;
};
