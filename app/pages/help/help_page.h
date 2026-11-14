// venv-vrequency — the Help page (PG_HELP): the guide to the keys, four parts of it, one row per key.
// Every row is a touch zone with a stable name, so Rook can point at it and a tap asks about it.
// help_page.cpp draws it and asks Rook; help_rows.cpp holds the words of each part.
#pragma once
#include "app/pages/page.h"
#include <QHash>
#include <QString>
#include <vector>

class Chat;
class Guide;

struct HelpRow { QString k, d; };            // the keys, and what they do
std::vector<HelpRow> helpRows(const KeyMap &km, int part);   // part 0 playing, 1 commands, 2 beat, 3 track

class HelpPage : public Page {
public:
    HelpPage(Shell &s, Chat &c, Guide &g) : Page(s), chat(c), guide(g) {}
    void paint() override;

    // what the window, the control socket, the tour and the self-test reach
    int sub = 0;                             // which of the four parts is showing
    QString sel;                             // the row last tapped
    QHash<QString, QString> desc;            // row zone -> its description (the guide reads these)

private:
    Chat &chat;
    Guide &guide;
    QString prefill;                         // what Ask Rook last put in the chat box

    void ask();
};
