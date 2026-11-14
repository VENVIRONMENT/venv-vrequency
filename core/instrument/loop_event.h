// venv-vrequency — one recorded event of a loop layer.
#pragma once
#include <cstdint>

namespace td {

enum LoopOp : uint8_t { OP_ON, OP_GLIDE, OP_OFF, OP_BEND, OP_XY, OP_PATCH, OP_PARAM };

struct LoopEv {
    double beat = 0;
    float a = 0, b = 0;
    uint32_t seq = 0;
    uint8_t op = 0, slot = 0;
};

} // namespace td
