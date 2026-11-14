// venv-vrequency — a delay line: a buffer whose length is a power of two, written round and round.
#pragma once
#include <cmath>
#include <vector>

namespace td {

// The sample `d` samples behind the write position `w`, between the two nearest by a straight line.
inline float ringTap(const std::vector<float> &b, int mask, int w, float d) {
    float rp = (float)w - d;
    int i0 = (int)std::floor(rp);
    float fr = rp - i0;
    float a = b[i0 & mask], c = b[(i0 + 1) & mask];
    return a + (c - a) * fr;
}

} // namespace td
