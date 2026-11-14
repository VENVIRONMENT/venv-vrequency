// venv-vrequency — nice(): an engine label ("ARP UP-DN") as the screen shows it ("Arp up-dn").
#include "app/ui/palette.h"

QString nice(const char *label) {
    QString s = QString(label).toLower();
    if (!s.isEmpty()) s[0] = s[0].toUpper();
    for (int i = 1; i + 1 <= s.size(); ++i) // a lone letter is a name: "Wave A", "B mix"
        if (s[i - 1] == ' ' && (i + 1 == s.size() || s[i + 1] == ' ')) s[i] = s[i].toUpper();
    s.replace("Lfo", "LFO").replace("Fm", "FM");
    return s;
}
