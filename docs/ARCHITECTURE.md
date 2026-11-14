# Architecture

How the code of venv-vrequency is arranged, and the rules that keep it that way. Two of the rules
are checked by a machine: run `scripts/check_sizes.sh` (exit status 0 when they hold).

## The layers

```
core/    the engine: sound, timing, the looper, the track. Plain C++17, no Qt, no devices.
audio/   the sound card: PipeWire (C), ALSA fallback, a silent clock. Knows only core/dsp.h.
app/     the Qt 6 program: one hand-drawn window, the pages, the control socket, the tour.
tests/   engine tests (no Qt) and the benchmark.
```

An arrow means "may include":

```
app  ──▶  core
app  ──▶  audio  ──▶  core/dsp.h only
tests ─▶  core
```

`core/` never includes Qt or anything outside `core/`, so the engine builds and is tested on its own.

## core/ by domain

| Folder | What |
|---|---|
| `core/synth` | one voice, a patch's parameters, a Part (a sound plus its live modulation), scales |
| `core/fx` | master effects: chorus, delay, reverb, crusher, limiter; `ring.h` is the shared delay-line tap |
| `core/instrument` | the Instrument: event queues (`event_port.h`), strings and lead, backing, beat, looper, voices, the audio block (`process.cpp`), the state published to the UI (`ui_state.*`), sessions |
| `core/drums` | the drum kits, rendered once at start |
| `core/track` | clips, lanes, chopping, picked sounds, track playback |
| `core/presets` | factory sounds, the randomiser, the Keys/Guitar/Bass shelf, loudness normalisation |
| `core/analysis` | key and tempo estimation (Listen) |

Threads: one real-time thread calls `Instrument::process()`. Everything else talks to it through the
lock-free queues of `EventPort` and reads its state through `uiState()` and the scope buffer.

## app/ by domain

| Folder | What |
|---|---|
| `app/shell` | the window (`MainWindow`) and `Shell`, the services a page may use; split by subject: keys, actions, pointer, painting the frame, saving state, the zone catalogue |
| `app/pages/<name>` | one folder per page: track, synth (with the scope and chord pads), sound, beat, backing, tune, system, help, chat |
| `app/ui` | the immediate-mode drawing toolkit, the palette, the zone names |
| `app/control` | the local control socket, one file per group of commands (see `docs/CONTROL.md`) |
| `app/tour` | the interactive tour and its demo pictures |
| `app/guide` | highlights, the chat dock |
| `app/chat` | the chat model behind the Chat page and ticker |
| `app/store` | files on disk: sounds, settings, sessions, the track, WAV writing |
| `app/sounds` | the sound slots and favourites |
| `app/selftest` | the in-app self-test (`--selftest`), one file per area |

### Pages

A page is a class derived from `Page` (`app/pages/page.h`). It owns its state, draws itself with the
toolkit (registering its touch zones as it goes), may take keys first, and drops its armed modes in
`disarm()` when the page changes. It sees the rest of the program only through `Shell`
(`app/shell/shell.h`): the engine, the audio device, the toolkit, the key map, a clock and a few
window services. **A page never includes `app/shell/mainwindow.h`.** If a page needs something new
from the window, add a small virtual to `Shell` and implement it in `MainWindow`.

The window keeps what belongs to every page: the string keys and chord rows, the key map's actions,
the tab row, toasts, saving state, and the table `pageOf` that lists the nine pages.

## The size rule

No source file in `core/`, `audio/`, `app/`, `tests/` or `scripts/` is longer than 200 lines. A file
may be up to 400 lines when it has a line in the table below saying why. A reason for a file that no
longer needs one is reported as stale.

### Files over 200 lines

| File | Why |
|---|---|

(None at the moment.)

## Checking

```
bash scripts/build.sh                                         # build + 522 engine checks
QT_QPA_PLATFORM=offscreen ./build/vrequency --audio null --selftest   # in-app self-test
bash scripts/check_sizes.sh                                   # size and layer rules
```

Run headless with `--audio null`, the self-test reports one expected failure, "audio backend opened:
none (silent)". Its frame-time and "audio never fell behind" checks can fail while other builds load
the machine; rerun when it is quiet.
