// venv-vrequency — engine tests: the warp parameters at 0 leave every old sound bit-identical.
//
// The checksums below were taken from the build before Warp existed (commit e6031c1, this
// compiler, aarch64): every factory and extra sound playing a three-note chord, and every pulse
// pad's default hit, hashed sample by sample. If the compiler or the machine changes, the
// numbers may move without any sound changing: re-take them from a pre-Warp build.
#include "tests/support.h"
#include "core/presets/presets.h"
#include "core/pulse/pulse_voice.h"
#include <cstring>
#include <vector>

namespace {
struct Fnv {
    uint64_t h = 1469598103934665603ull;
    void add(const float *x, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            uint32_t b; std::memcpy(&b, &x[i], 4);
            for (int k = 0; k < 4; ++k) { h ^= (b >> (8 * k)) & 0xFF; h *= 1099511628211ull; }
        }
    }
};
uint64_t soundsHash() {
    Fnv f;
    for (int i = 0; i < kFactoryCount + kExtraCount; ++i) {
        Rig r;
        r.inst.setPatch(0, i < kFactoryCount ? factoryPatch(i) : extraPatch(i - kFactoryCount));
        r.inst.key(0, 0, true); r.inst.key(1, 2, true); r.inst.key(2, 4, true);
        r.run(0.4);
        r.inst.key(0, 0, false); r.inst.key(1, 2, false); r.inst.key(2, 4, false);
        r.run(0.4);
        f.add(r.L.data(), r.L.size()); f.add(r.R.data(), r.R.size());
    }
    return f.h;
}
uint64_t pulseHash() {
    Fnv f;
    std::vector<float> b((size_t)(2.1 * SR));
    for (int p = 0; p < kPulsePads; ++p) {
        PulsePad d = pulseDefault(p);
        pulseRender(d, SR, d.rate, b.data(), (int)b.size());
        f.add(b.data(), b.size());
    }
    return f.h;
}
} // namespace

void testWarpGolden() {
    SECTION("warp off: old sounds and pulse pads are bit-identical to the build before Warp");
    const uint64_t s = soundsHash(), p = pulseHash();
    std::printf("    sounds %016llx  pulse %016llx\n", (unsigned long long)s, (unsigned long long)p);
    CHECK(s == 0xf7d2c8063b06d41bull, "fifteen sounds, sample for sample, as before Warp");
    CHECK(p == 0x1fc80e83a0dcc7a3ull, "eight pulse pads, sample for sample, as before Warp");
}
