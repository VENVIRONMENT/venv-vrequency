// venv-vrequency — the Sound page (PG_SOUND): the ten slots, the quick controls, the "All parameters"
// view and the favourites. sound_page.cpp lays out the slots and buttons, sound_params.cpp holds the
// sliders and the All parameters view, sound_favourites.cpp the favourites.
#pragma once
#include "app/pages/page.h"

class Sounds;

class SoundPage : public Page {
public:
    explicit SoundPage(Shell &s);
    void paint() override;
    void disarm() override;                  // leaving the page drops All parameters, Delete and Store

    // what the window, the tour and the self-test reach
    bool deepEdit = false;                   // the All parameters view is showing
    int deepPage = 0;                        // which twelve of the parameters it shows
    bool favDelete = false;                  // Delete is armed: the next favourite tapped is removed
    int favPage = 0;                         // which six favourites are showing
    int favouritesPage() const { return favPage; }

private:
    Sounds &sounds;

    void paintSlots();                       // the ten slots and the buttons under them
    void paintQuick();                       // eight quick sliders
    void paintDeep();                        // every parameter, twelve to a page
    void paintFavourites();
    void paramSlider(QRectF r, int param);
};

QString paramText(int param, float v);       // "440 Hz", "Saw", "-6.0 dB": a parameter as the slider shows it
float paramToNorm(int param, float v);
float normToParam(int param, float n);
