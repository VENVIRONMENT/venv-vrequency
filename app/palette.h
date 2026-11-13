// venv-vrequency — the display's palette. One white phosphor on black: no colour anywhere.
#pragma once
#include <QColor>
#include <QString>

namespace pal {
const QColor BG(0x04, 0x04, 0x04);   // tube black
const QColor INK(0xf3, 0xf3, 0xef);  // full-brightness phosphor
const QColor MID(0xa2, 0xa2, 0x9e);  // secondary text
const QColor LOW(0x64, 0x64, 0x62);  // idle frames, key letters
const QColor GRAT(0x34, 0x34, 0x33); // empty slots, faint rules
} // namespace pal

enum { LOOK_PLAIN, LOOK_ARMED }; // LOOK_ARMED: double frame, "waiting for a choice"

QString nice(const char *label); // "ARP UP-DN" -> "Arp up-dn"
