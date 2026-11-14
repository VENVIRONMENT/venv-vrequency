#include "audio/capture.h"
#ifdef VREQUENCY_PIPEWIRE
#include "audio/pw_backend.h"
#endif

int captureMono(std::vector<float> &dst, int rate, double seconds, std::string &err) {
#ifdef VREQUENCY_PIPEWIRE
    dst.assign((size_t)(rate * seconds), 0.0f);
    char e[160] = {0};
    int n = td_pw_capture(dst.data(), (int)dst.size(), rate, seconds, e, sizeof e);
    dst.resize(n > 0 ? n : 0);
    if (n <= 0) err = e[0] ? e : "no input device";
    return n;
#else
    err = "audio capture needs the PipeWire backend";
    dst.clear();
    return 0;
#endif
}
