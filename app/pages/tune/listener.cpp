// venv-vrequency — Listen (see listener.h).
#include "app/pages/tune/listener.h"
#include "audio/audio.h"
#include "audio/capture.h"
#include <vector>

using namespace td;

Listener::~Listener() {
    if (thread.joinable()) thread.join();
}

void Listener::start() {
    if (state == 1) return;
    if (thread.joinable()) thread.join();
    state = 1;
    started = sh.clock.elapsed();
    guess = 0;
    int rate = sh.audio.sampleRate();
    thread = std::thread([this, rate] {
        std::vector<float> buf;
        std::string err;
        int n = captureMono(buf, rate, 6.0, err);
        ListenResult r;
        if (n > 0) r = analyseAudio(buf.data(), n, (float)rate);
        std::lock_guard<std::mutex> lk(mx);
        if (n <= 0) { error = "No microphone or audio input delivered sound (" + QString::fromStdString(err) + ")"; state = 3; }
        else if (!r.ok) { error = QString("Heard something, but it was %1").arg(r.problem); state = 3; }
        else { result = r; state = 2; }
    });
}

void Listener::apply(bool key, bool tempo) {
    std::lock_guard<std::mutex> lk(mx);
    if (state != 2) return;
    const KeyGuess &k = result.keys[guess % 3];
    if (key) {
        sh.set(S_ROOT, (float)k.root);
        // stay pentatonic if the player was on a pentatonic; otherwise full major/minor
        bool pent = u.set.tuning.scale == SC_MAJPENT || u.set.tuning.scale == SC_MINPENT;
        sh.set(S_SCALE, (float)(k.minor ? (pent ? SC_MINPENT : SC_MINOR) : (pent ? SC_MAJPENT : SC_MAJOR)));
    }
    if (tempo && result.bpm > 0) sh.set(S_BPM, result.bpm);
}
