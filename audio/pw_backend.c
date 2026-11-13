#include "pw_backend.h"
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct td_pw {
    struct pw_thread_loop *loop;
    struct pw_stream *stream;
    struct spa_hook listener;
    td_render_fn fn;
    void *user;
    volatile int streaming;
};

static int g_inited = 0;
static void ensure_init(void) { if (!g_inited) { pw_init(NULL, NULL); g_inited = 1; } }

static void on_process(void *data) {
    struct td_pw *s = data;
    struct pw_buffer *b = pw_stream_dequeue_buffer(s->stream);
    if (!b) return;
    struct spa_buffer *buf = b->buffer;
    float *dst = buf->datas[0].data;
    if (dst) {
        const uint32_t stride = sizeof(float) * 2;
        uint32_t n = buf->datas[0].maxsize / stride;
        if (b->requested && b->requested < n) n = (uint32_t)b->requested;
        s->fn(s->user, dst, (int)n);
        buf->datas[0].chunk->offset = 0;
        buf->datas[0].chunk->stride = stride;
        buf->datas[0].chunk->size = n * stride;
    }
    pw_stream_queue_buffer(s->stream, b);
}

static void on_state(void *data, enum pw_stream_state old, enum pw_stream_state state, const char *error) {
    struct td_pw *s = data;
    s->streaming = state == PW_STREAM_STATE_STREAMING;
}

static const struct pw_stream_events out_events = {
    PW_VERSION_STREAM_EVENTS,
    .state_changed = on_state,
    .process = on_process,
};

td_pw *td_pw_start(td_render_fn fn, void *user, int rate, int quantum, char *err, int errlen) {
    ensure_init();
    struct td_pw *s = calloc(1, sizeof *s);
    s->fn = fn; s->user = user;
    s->loop = pw_thread_loop_new("tendril-audio", NULL);
    if (!s->loop) { snprintf(err, errlen, "cannot create PipeWire loop"); free(s); return NULL; }
    char lat[32];
    snprintf(lat, sizeof lat, "%d/%d", quantum, rate);
    struct pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_APP_NAME, "Tendril", PW_KEY_NODE_NAME, "tendril", PW_KEY_NODE_DESCRIPTION, "Tendril synth",
        PW_KEY_NODE_LATENCY, lat, NULL);
    s->stream = pw_stream_new_simple(pw_thread_loop_get_loop(s->loop), "tendril", props, &out_events, s);
    if (!s->stream) { snprintf(err, errlen, "cannot create PipeWire stream (is PipeWire running?)"); pw_thread_loop_destroy(s->loop); free(s); return NULL; }
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof buffer);
    const struct spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_F32, .channels = 2, .rate = rate));
    int r = pw_stream_connect(s->stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS, params, 1);
    if (r < 0 || pw_thread_loop_start(s->loop) < 0) {
        snprintf(err, errlen, "cannot connect to PipeWire");
        pw_stream_destroy(s->stream); pw_thread_loop_destroy(s->loop); free(s);
        return NULL;
    }
    return s;
}

int td_pw_streaming(td_pw *s) { return s ? s->streaming : 0; }

void td_pw_stop(td_pw *s) {
    if (!s) return;
    pw_thread_loop_stop(s->loop);
    pw_stream_destroy(s->stream);
    pw_thread_loop_destroy(s->loop);
    free(s);
}

/* ------------------------------------------------------------- capture */

struct cap {
    struct pw_thread_loop *loop;
    struct pw_stream *stream;
    float *dst;
    int max;
    volatile int got;
    int channels;
};

static void cap_process(void *data) {
    struct cap *c = data;
    struct pw_buffer *b = pw_stream_dequeue_buffer(c->stream);
    if (!b) return;
    struct spa_buffer *buf = b->buffer;
    const float *src = buf->datas[0].data;
    if (src) {
        uint32_t n = buf->datas[0].chunk->size / sizeof(float);
        int got = c->got;
        for (uint32_t i = 0; i < n && got < c->max; ++i) c->dst[got++] = src[i];
        c->got = got;
    }
    pw_stream_queue_buffer(c->stream, b);
}

static const struct pw_stream_events cap_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = cap_process,
};

int td_pw_capture(float *dst, int maxFrames, int rate, double seconds, char *err, int errlen) {
    ensure_init();
    struct cap c;
    memset(&c, 0, sizeof c);
    c.dst = dst; c.max = maxFrames;
    c.loop = pw_thread_loop_new("tendril-listen", NULL);
    if (!c.loop) { snprintf(err, errlen, "cannot create PipeWire loop"); return 0; }
    struct pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_APP_NAME, "Tendril", PW_KEY_NODE_NAME, "tendril-listen", NULL);
    c.stream = pw_stream_new_simple(pw_thread_loop_get_loop(c.loop), "tendril-listen", props, &cap_events, &c);
    if (!c.stream) { snprintf(err, errlen, "cannot create capture stream"); pw_thread_loop_destroy(c.loop); return 0; }
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof buffer);
    const struct spa_pod *params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_F32, .channels = 1, .rate = rate));
    pw_stream_connect(c.stream, PW_DIRECTION_INPUT, PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS, params, 1);
    pw_thread_loop_start(c.loop);
    struct timespec ts = {0, 50 * 1000 * 1000};
    int ticks = (int)(seconds * 20) + 10;
    for (int i = 0; i < ticks && c.got < c.max; ++i) {
        nanosleep(&ts, NULL);
        if (i == 30 && c.got == 0) break; /* 1.5 s without a single frame: no input device */
    }
    pw_thread_loop_stop(c.loop);
    pw_stream_destroy(c.stream);
    pw_thread_loop_destroy(c.loop);
    if (c.got == 0) snprintf(err, errlen, "no audio arrived from an input device");
    return c.got;
}
