#include "audio.h"
#include "dsp.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#ifdef TENDRIL_PIPEWIRE
#include "pw_backend.h"
#endif
#ifdef TENDRIL_ALSA
#include <alsa/asoundlib.h>
#endif

void AudioOut::renderInterleaved(float *dst, int frames) {
    if (!ftz) { td::enableFlushToZero(); ftz = true; } // first call on the audio thread
    if ((int)bl.size() < frames) { bl.resize(frames); br.resize(frames); }
    auto t0 = std::chrono::steady_clock::now();
    render(bl.data(), br.data(), frames);
    for (int i = 0; i < frames; ++i) { dst[2 * i] = bl[i]; dst[2 * i + 1] = br[i]; }
    double dt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    float l = (float)(dt * rate / (frames > 0 ? frames : 1));
    loadAvg.store(loadAvg.load() * 0.98f + l * 0.02f);
    if (l > loadPeak.load()) loadPeak.store(l);
    if (l > 1.0f) over.fetch_add(1);
}

#ifdef TENDRIL_PIPEWIRE
static void pwRender(void *user, float *dst, int frames) { ((AudioOut *)user)->renderInterleaved(dst, frames); }
#endif

bool AudioOut::start(Render r, int rate_, int quantum_, const std::string &prefer) {
    stop();
    render = std::move(r);
    rate = rate_; quant = quantum_;
    bl.assign(8192, 0.0f); br.assign(8192, 0.0f);
    ftz = false;
    running = true;
#ifdef TENDRIL_PIPEWIRE
    if (prefer == "auto" || prefer == "pipewire") {
        char err[160] = {0};
        pw = td_pw_start(pwRender, this, rate, quant, err, sizeof err);
        if (pw) { backendName = "PipeWire"; return true; }
        error = err;
    }
#endif
#ifdef TENDRIL_ALSA
    if (prefer == "auto" || prefer == "alsa") {
        if (startAlsa()) { backendName = "ALSA"; return true; }
    }
#endif
    backendName = "none (silent)";
    th = std::thread([this] { nullLoop(); });
    return prefer == "null";
}

bool AudioOut::streaming() const {
#ifdef TENDRIL_PIPEWIRE
    if (pw) return td_pw_streaming(pw) != 0;
#endif
    return alsa != nullptr;
}

void AudioOut::stop() {
    running = false;
    if (th.joinable()) th.join();
#ifdef TENDRIL_PIPEWIRE
    if (pw) { td_pw_stop(pw); pw = nullptr; }
#endif
#ifdef TENDRIL_ALSA
    if (alsa) { snd_pcm_close((snd_pcm_t *)alsa); alsa = nullptr; }
#endif
}

void AudioOut::nullLoop() {
    std::vector<float> buf(quant * 2);
    auto next = std::chrono::steady_clock::now();
    while (running) {
        renderInterleaved(buf.data(), quant);
        next += std::chrono::microseconds((long long)(1e6 * quant / rate));
        std::this_thread::sleep_until(next);
    }
}

#ifdef TENDRIL_ALSA
bool AudioOut::startAlsa() {
    snd_pcm_t *pcm = nullptr;
    const char *dev = std::getenv("TENDRIL_ALSA_DEVICE");
    int e = snd_pcm_open(&pcm, dev ? dev : "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (e < 0) { error = std::string("ALSA: ") + snd_strerror(e); return false; }
    unsigned latencyUs = (unsigned)(1e6 * quant * 3 / rate);
    e = snd_pcm_set_params(pcm, SND_PCM_FORMAT_FLOAT_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 2, rate, 1, latencyUs);
    if (e < 0) { error = std::string("ALSA: ") + snd_strerror(e); snd_pcm_close(pcm); return false; }
    alsa = pcm;
    th = std::thread([this] { alsaLoop(); });
    return true;
}

void AudioOut::alsaLoop() {
    snd_pcm_t *pcm = (snd_pcm_t *)alsa;
    std::vector<float> buf(quant * 2);
    while (running) {
        renderInterleaved(buf.data(), quant);
        int done = 0;
        while (done < quant && running) {
            snd_pcm_sframes_t n = snd_pcm_writei(pcm, buf.data() + done * 2, quant - done);
            if (n < 0) { if (snd_pcm_recover(pcm, (int)n, 1) < 0) { running = false; break; } continue; }
            done += (int)n;
        }
    }
}
#else
bool AudioOut::startAlsa() { return false; }
void AudioOut::alsaLoop() {}
#endif

int captureMono(std::vector<float> &dst, int rate, double seconds, std::string &err) {
#ifdef TENDRIL_PIPEWIRE
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
