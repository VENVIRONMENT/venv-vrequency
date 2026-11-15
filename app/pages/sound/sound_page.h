// venv-vrequency — the Sound page (PG_SOUND): the ten slots, the quick controls, the "All parameters"
// view and the favourites. sound_page.cpp lays out the slots and buttons, sound_params.cpp holds the
// sliders and the All parameters view, sound_favourites.cpp the favourites.
#pragma once
#include "app/pages/page.h"
#include "app/pages/sound/sound_pictures.h"

class Sounds;

class SoundPage : public Page {
public:
    explicit SoundPage(Shell &s);
    void paint() override;
    void disarm() override;                  // leaving the page drops All parameters, Delete and Store

    // what the window, the tour and the self-test reach
    bool deepEdit = false;                   // the All parameters view is showing
    int deepPage = 0;                        // which of its four pages it shows
    bool favDelete = false;                  // Delete is armed: the next favourite tapped is removed
    int favPage = 0;                         // which six favourites are showing
    int favouritesPage() const { return favPage; }

private:
    Sounds &sounds;
    SoundPictures pics;                      // the settings pictures of the All parameters view

    void paintSlots();                       // the ten slots and the buttons under them
    void paintQuick();                       // eight quick sliders
    void paintDeep();                        // every parameter, in four pages (sound_deep.cpp)
    void paintFavourites();
    void paramSlider(QRectF r, int param);
};

// The All parameters pages: which parameters each shows, and which pictures sit above them.
constexpr int kDeepPages = 4;
const int *deepParams(int page, int &count);
int deepPageOf(int param);

QString paramText(int param, float v);       // "440 Hz", "Saw", "-6.0 dB": a parameter as the slider shows it
float paramToNorm(int param, float v);
float normToParam(int param, float n);
