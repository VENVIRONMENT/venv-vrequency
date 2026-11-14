// venv-vrequency — the System page (PG_SYS): volumes, touch-pad mapping, recording, sessions, the
// status of the audio, and the key test panel. system_page.cpp is the controls, system_status.cpp
// the saved sessions and the status box, system_keytest.cpp the key test (drawn over any page).
#pragma once
#include "app/pages/page.h"
#include <QStringList>

class SystemPage : public Page {
public:
    explicit SystemPage(Shell &s) : Page(s) {}
    void paint() override;
    void paintKeyTest();                     // the key test panel, over whatever page is showing

    QStringList sessions;                    // the saved sessions on disk, newest first (the tour fills in some)

private:
    void paintSessions();
    void paintStatus();
};
