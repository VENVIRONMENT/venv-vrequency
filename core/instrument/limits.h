// venv-vrequency — the instrument's fixed sizes: strings, voices, parts, loop layers.
#pragma once

namespace td {

constexpr int kStrings = 3;
constexpr int kMaxHeld = 12;
constexpr int kVoices = 28;
constexpr int kLayers = 6;
constexpr int kParts = 2 + kLayers;  // 0 = lead, 1 = chord pads, 2.. = loop layers
constexpr int kSlots = 12;          // voice slots per part
constexpr int kChordSlots = 5;      // chord pad slots 0..4 = the held chord's voices
constexpr int kDroneSlot0 = 5;      // chord pad slots 5..11 = drones (degree 0..6)
constexpr int kLayerEvents = 6000;
constexpr int kLayerPatches = 8;
constexpr double kMaxLoopBeats = 256; // 64 bars
constexpr int kScopeSize = 16384; // 340 ms at 48 kHz: enough for the slowest timebase

} // namespace td
