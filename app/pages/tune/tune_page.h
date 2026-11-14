// venv-vrequency — the Tune page (PG_TUNE): key, scale, octave, tempo, and Listen, which hears the
// music around you and offers its key and tempo. tune_page.cpp is the controls, tune_listen.cpp the
// Listen panel; listener.* is the microphone work itself.
#pragma once
#include "app/pages/page.h"
#include "app/pages/tune/listener.h"

class TunePage : public Page {
public:
    explicit TunePage(Shell &s) : Page(s), listener(s) {}
    void paint() override;

    Listener listener;                       // the tour fills in a result to draw

private:
    void paintListen();                      // the Listen panel and, after a listen, its answer
};
