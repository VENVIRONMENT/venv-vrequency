// venv-vrequency — a sound written down as a list: only the values that make it itself.
// Shared by the factory sounds (factory.cpp) and the sound shelf (sound_shelf.cpp).
#pragma once
#include "core/presets/presets.h"
#include <cstddef>

namespace td {

struct PV { int id = -1; float v = 0; };     // a parameter and its value; id -1 ends a list of fixed size

// `p` named `name` with the listed values set, then clamped and levelled.
inline Patch withValues(Patch p, const char *name, const PV *vals, size_t n) {
    p.setName(name);
    for (size_t i = 0; i < n && vals[i].id >= 0; ++i) p.p[vals[i].id] = vals[i].v;
    p.clamp();
    normalisePatch(p);
    return p;
}

} // namespace td
