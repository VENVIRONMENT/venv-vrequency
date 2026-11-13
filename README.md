# Tendril

A pocket string synthesiser for the Hackberry Pi CM5 (720×720 screen, Q20 keyboard).
Each keyboard row is a string: hold a key and press another on the same row and the
note *slides* there. Everything is locked to a scale, so it sounds musical whatever
you press. Original code and interface; inspired by the playing mechanics of slide-string
pocket synths, with no code, firmware or branding taken from any of them.

![The play page](docs/shots/0-play.png)

```bash
tendril
```

or tap **Tendril** in the app grid. `tendril --windowed` runs it in a window,
`tendril --help` lists the options.

## Play it in ten seconds

| Do this | It does this |
|---|---|
| Press `Q`‥`P`, `A`‥`L`, `Z`‥`M` | Three strings: high, middle, low. Left to right goes up the scale. |
| Hold a key, press another on the same row | The note slides (no re-attack). Let the newer key go and it slides back. |
| Hold a shape across rows, re-finger it | The whole chord glides. |
| `Shift` (hold) | Chromatic: keys step in semitones. |
| `Space` (hold) | Sustain. Keys pressed on a sustained string slide it. |
| `Backspace` | Panic: silence everything. |
| `Enter` | Loop pedal: count-in → record → (press again) loop → (again) overdub. |
| `Left` / `Right`, `Up` / `Down` | Pitch bend, octave (held notes sweep). On the Q20 these are on the symbol layer. |
| `1`‥`9`, `0` | Sounds 1‥10 (number layer on the Q20). |
| `Alt` + `Z`‥`M`, or the pads on screen | Backing chords I‥VII. `$` turns the bottom row into chord keys. |
| `Ctrl` + `R` / `Z` / `F` / `N` | New random sound / undo / save favourite / mutate. |
| Drag on the scope | XY sound control (filter, vibrato, timbre, space, bend). |
| Drag along a string on screen | Play and slide by touch. |

The full guide is built in: **MENU → HELP** (or `Ctrl`+`H`). `Tab` cycles pages, `Esc` returns to the play page.

## What is in it

- **Strings**: three monophonic strings with true continuous portamento (pitch ramps in the
  oscillator, the envelope is not retriggered), per-sound glide time plus a global glide speed.
- **Scale lock**: major, natural minor, major/minor pentatonic, blues, dorian, mixolydian,
  chromatic; any root; default A minor pentatonic.
- **Synth**: 28-voice pool; per voice two band-limited oscillators (sine, triangle, saw, pulse)
  plus unison copy, sub and noise, FM, ADSR, a zero-delay state-variable filter (low/high/band-pass)
  with its own envelope, vibrato, LFO to cutoff/amplitude/pulse-width, saturation. Master chorus,
  tempo-synced delay, reverb, limiter + soft clip (output can never exceed full scale).
- **Sounds**: ten factory sounds in slots 1‥0, a randomiser that invents playable instruments
  (every result is loudness-normalised), mutate, undo, favourites, an all-parameters editor.
- **Backing**: diatonic triads/sevenths from the scale. Tap up to eight chords for a looping
  progression, or latch one chord, or latch drone notes. Sustain / pulse / arpeggio styles.
  The backing keeps the sound it started with while you change the lead.
- **Looper**: event-based (notes, slides, bends, XY moves, sound changes), count-in, bar-snapped,
  up to 64 bars, six layers, undo, stop/start, clear. Each layer keeps its own sound. Follows tempo.
- **Saving**: sounds, favourites, sessions (settings + sounds + backing + loops) and WAV export
  under `~/Tendril/`. Key bindings are editable in `~/Tendril/keymap.json`.
- **Listen** (optional): estimates key and tempo from the default audio input with confidence
  values. *This Hackberry has no microphone*, so it reports that instead of guessing.

## Build, install, remove

```bash
./scripts/build.sh       # fetches audio headers into third_party/ (no root), builds, runs the tests
./scripts/install.sh     # installs to ~/.local/opt/tendril + launcher + `tendril` command
./scripts/uninstall.sh   # removes it (add --purge to also delete ~/Tendril)
```

Needs: `g++`, `cmake`, `qt6-base-dev` and the PipeWire/ALSA runtime libraries — all already on this
machine. Nothing is installed system-wide and nothing starts at boot.

Tests:

```bash
./build/tendril_tests                                   # 324 engine checks (offline rendering)
./build/tendril_tests --bench                           # CPU at full polyphony
QT_QPA_PLATFORM=offscreen ./build/tendril --selftest    # 48 checks through the real app + audio thread
```

## Layout of the code

| Path | What |
|---|---|
| `core/` | The engine, no GUI or device dependencies: `voice.h` (synth voice), `fx.h`, `scale.h`, `instrument.*` (strings, backing, looper, mixer), `presets.*` (sounds, randomiser), `analysis.*` (key/tempo) |
| `audio/` | PipeWire backend (C), ALSA fallback, silent clock |
| `app/` | Qt 6 interface, key map, files, self-test |
| `tests/` | Engine tests and benchmark |
| `docs/HARDWARE.md` | What was found on this Hackberry and what it means |
| `docs/STATUS.md` | What is tested, what still needs a person, known limits, and a five-minute test list |

## Honest status

See [docs/HARDWARE.md](docs/HARDWARE.md) for the audit and [docs/STATUS.md](docs/STATUS.md) for what is
verified, what still needs a human with the device in hand, and known limits.
