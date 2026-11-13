/* Tendril — minimal PipeWire playback/capture wrapper (plain C). */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/* Fill `frames` interleaved stereo float frames. Called on PipeWire's
   real-time data thread. */
typedef void (*td_render_fn)(void *user, float *interleaved, int frames);

typedef struct td_pw td_pw;

td_pw *td_pw_start(td_render_fn fn, void *user, int rate, int quantum, char *err, int errlen);
void td_pw_stop(td_pw *s);
/* 1 once the stream is streaming to a sink, 0 otherwise */
int td_pw_streaming(td_pw *s);

/* Record up to `maxFrames` mono frames from the default source. Blocks for
   at most `seconds`. Returns the number of frames captured (0 when no input
   device delivered audio). */
int td_pw_capture(float *dst, int maxFrames, int rate, double seconds, char *err, int errlen);

#ifdef __cplusplus
}
#endif
