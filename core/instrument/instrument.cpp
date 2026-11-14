// venv-vrequency — the instrument: construction, and the lock-free hand-off of a new sound.
#include "core/instrument/instrument.h"
#include <cstring>

namespace td {

Instrument::Instrument(float sampleRate) : sr(sampleRate) {
    chorus.init(sr); delay.init(sr); reverb.init(sr); crush.init(sr); limiter.init(sr);
    drums.init(sr);
    for (long &s : skipStep) s = -1;
    for (int p = 0; p < kParts; ++p)
        for (int s = 0; s < kSlots; ++s) { slotVoice[p][s] = -1; slotNote[p][s] = 0; }
    for (int s = 0; s < kSlots; ++s) backDesired[s] = -1;
    for (auto &ly : layers) ly.ev.resize(kLayerEvents);
    for (auto &a : sendSm) a[0] = a[1] = a[2] = 0;
    std::memset(scope, 0, sizeof scope);
    recBuf.assign((1u << 18) * 2, 0.0f);
    volSm = st.volume;
    publish();
}

Instrument::~Instrument() {}

void Instrument::setPatch(int part, const Patch &p) {
    PatchMsg m; m.part = part; m.patch = p; m.patch.clamp();
    patchq.push(m);
}

} // namespace td
