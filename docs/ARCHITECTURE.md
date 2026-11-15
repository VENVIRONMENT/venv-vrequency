# Architecture

How the code of venv-vrequency is arranged, and the rules that keep it that way. Two of the rules
are checked by a machine: run `scripts/check_sizes.sh` (exit status 0 when they hold).

## The layers

```
core/    the engine: sound, timing, the looper, the pulse pads, the Freq tones. Plain C++17, no Qt, no devices.
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
| `core/fx` | master effects: chorus, delay, reverb, crusher, limiter; `ring.h` is the shared delay-line tap; `warp.*` is a mono drive / fold / crush / low-pass stage any sound can own (the Freq tones use it first) |
| `core/instrument` | the Instrument: event queues (`event_port.h`), strings and lead, the backing chords (`backing.cpp`), the pulse pads' place in it (`pulse_pads.cpp`), the Freq tones' (`freq_tones.cpp`), looper, voices, the audio block (`process.cpp`), the state published to the UI (`ui_state.*`), sessions |
| `core/freq` | the four Freq tones (`freq`: settings, beat, ratio and SNAP sums; `freq_bank`: the audio-thread oscillators, 2 > 1, GATE, warp and a lock-free tap of their output) |
| `core/pulse` | the eight pulse sounds (`pulse_voice`: one generator, used by the audio thread and for the page's pictures), the bank of pads with LATCH, SYNC and a lock-free record of every hit (`pulse_bank`) |
| `core/presets` | factory sounds, the randomiser, loudness normalisation |
| `core/analysis` | key and tempo estimation (Listen) |

Threads: one real-time thread calls `Instrument::process()`. Everything else talks to it through the
lock-free queues of `EventPort` and reads its state through `uiState()` and the scope buffer.

## app/ by domain

| Folder | What |
|---|---|
| `app/shell` | the window (`MainWindow`) and `Shell`, the services a page may use; split by subject: keys, actions, pointer, painting the frame, saving state, the zone catalogue |
| `app/pages/<name>` | one folder per page: synth (with the scope, its shared FFT `spectrum.*` and the chord pads), sound, pulse, freq, backing, tune, system, help, chat |
| `app/ui` | the immediate-mode drawing toolkit, the palette, the zone names |
| `app/control` | the local control socket, one file per group of commands (see `docs/CONTROL.md`) |
| `app/tour` | the interactive tour and its demo pictures |
| `app/guide` | highlights, the chat dock |
| `app/chat` | the chat model behind the Chat page and ticker |
| `app/store` | files on disk: sounds, settings, sessions, WAV writing |
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
the tab row, toasts, saving state, and the table `pageOf` that lists the eight pages.

## Audio thread and the engine lock

One real-time thread runs `Instrument::process()`. The UI thread talks to it through the lock-free
queues of `EventPort` (keys, settings, commands, `setPatch`) and reads `uiState()` and the scope
buffer without locking. Next to that there is one mutex, `Instrument::mx`.

**What `mx` protects.** All the engine's non-atomic state (settings, parts, backing, pulse pads, looper
layers). `process()` holds it for the whole of every callback, so the audio
thread always sees one consistent state. Anything that touches that state from another thread must
take it too.

**Who takes it** (every use in `core/` and `app/`; the Listen page's `mx` is a different mutex):

| Site | Thread | What it does under the lock | Changed |
|---|---|---|---|
| `Instrument::process` (`core/instrument/process.cpp`) | audio | renders the blocks of one callback, then `publish()` | no |
| `Instrument::snapshot` (`session.cpp`) | UI (autosave, save session, tour/self-test) | copies settings, patches, the progression, the pulse pads' settings, and up to 6 loop layers (at most 6 x 6000 events of 24 bytes, about 0.9 MB) into the caller's `SessionData`; allocates the copy | the caller's previous `out.layers` is now freed before the lock |
| `Instrument::restore` (`session.cpp`) | UI (start-up, load session, tour) | `panic()`, `loopClear()`, copies the session into fixed arrays, `publish()`; no allocation, no IO | no |

All file IO and JSON work (`saveState`, `saveSession`, `loadSession`) happens outside the lock, on a
`SessionData` copy.

**The rule for new code.** From the UI thread, never do IO, JSON, preset building or large
allocation while holding `mx`. Build the result first, then swap it in under the lock, or copy a bounded amount of plain data. Do not take `mx` from the audio thread
except in `process()`. Prefer a queue event to the lock for anything small.

**Known risk.** `snapshot()` still allocates and copies the loop layers under the lock, and the
autosave calls it on the UI thread a moment after each edit. While a full loop is copied the audio
callback waits for it; this is expected to be well below one buffer but has not been measured on the
target (Raspberry Pi) hardware. The lock is a plain mutex, so a UI thread that is descheduled while
holding it also stalls the audio thread (priority inversion). Both were left alone on purpose: the
fix would be a different scheme (double-buffered state), which is a design change, not a tidy-up.

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
bash scripts/build.sh                                         # build + 439 engine checks
QT_QPA_PLATFORM=offscreen ./build/vrequency --audio null --selftest   # in-app self-test
bash scripts/check_sizes.sh                                   # size and layer rules
```

Run headless with `--audio null`, the self-test reports one expected failure, "audio backend opened:
none (silent)". Its frame-time and "audio never fell behind" checks can fail while other builds load
the machine; rerun when it is quiet.
