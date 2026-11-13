# venv vrequency

A pocket string synthesiser for the Hackberry Pi CM5 (720×720 screen, Q20 keyboard).
Each keyboard row is a string: hold a key and press another on the same row and the
note *slides* there. Everything is locked to a scale, so it sounds musical whatever
you press. Original code and interface; inspired by the playing mechanics of slide-string
pocket synths, with no code, firmware or branding taken from any of them.

![The play page](docs/shots/0-play.png)

```bash
vrequency
```

or tap **venv vrequency** in the app grid. `vrequency --windowed` runs it in a window,
`vrequency --help` lists the options.

## How it looks

The whole 720×720 screen is drawn the way a bench oscilloscope draws its own display: white on black,
one-pixel rules, raster type, inverse video for whatever is selected. There is no drawn hardware, and
the only colour is in the scope while the Glitch control is on. The scope on the play page shows the real output, triggered so the wave stands still, with
its own volts/div and time/div (they follow the signal and never change the pitch) and measured
frequency and level. Touching it is the X-Y sound control.

More pictures: [Sound](docs/shots/1-sound.png), [Backing](docs/shots/2-backing.png), [Tune](docs/shots/3-tune.png),
[System](docs/shots/4-system.png), [Help](docs/shots/5-help.png), [Key test](docs/shots/6-keytest.png), [Glitch](docs/shots/7-glitch.png), [Beat](docs/shots/9-beat.png).

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
| Drag on the scope | XY sound control (filter, vibrato, timbre, space, bend, glitch). |
| Drag along a string on screen | Play and slide by touch. |

The full how-to is [docs/GUIDE.md](docs/GUIDE.md). A short version is built in: **Menu → Help** (or `Ctrl`+`H`). `Tab` cycles pages, `Esc` returns to the play page.

## What is in it

- **Strings**: three monophonic strings with true continuous portamento (pitch ramps in the
  oscillator, the envelope is not retriggered), per-sound glide time plus a global glide speed.
- **Scale lock**: major, natural minor, major/minor pentatonic, blues, dorian, mixolydian,
  chromatic; any root; default A minor pentatonic.
- **Synth**: 28-voice pool; per voice two band-limited oscillators (sine, triangle, saw, pulse)
  plus unison copy, sub and noise, FM, ADSR, a zero-delay state-variable filter (low/high/band-pass)
  with its own envelope, vibrato, LFO to cutoff/amplitude/pulse-width, saturation. Master chorus,
  tempo-synced delay, reverb, a bit crusher (the Glitch pad mapping), limiter + soft clip (output can
  never exceed full scale).
- **Sounds**: ten factory sounds in slots 1‥0, five sine-based dream sounds (Haze, Fuzz, Teeth,
  Bend, Grain) offered as favourites, a randomiser that invents playable instruments
  (every result is loudness-normalised), mutate, undo, favourites, an all-parameters editor.
- **Backing**: diatonic triads/sevenths from the scale. Tap up to eight chords for a looping
  progression, or latch one chord, or latch drone notes. Sustain / pulse / arpeggio styles.
  The backing keeps the sound it started with while you change the lead.
- **Beat**: a drum machine page. Eight synthesised drums (no samples) in three kits, eight big pads
  played by touch or from the keyboard, one bar of sixteen steps written by playing with Rec on.
  Locked to the same bar lines as the backing and the looper.
- **Looper**: event-based (notes, slides, bends, XY moves, sound changes), count-in, bar-snapped,
  up to 64 bars, six layers, undo, stop/start, clear. Each layer keeps its own sound. Follows tempo.
- **Saving**: sounds, favourites, sessions (settings + sounds + backing + loops + beat), WAV export and play-back of the last recording
  under `~/vrequency/`. Key bindings are editable in `~/vrequency/keymap.json`.
- **Listen** (optional): estimates key and tempo from the default audio input with confidence
  values. *This Hackberry has no microphone*, so it reports that instead of guessing.

## Build, install, remove

```bash
./scripts/build.sh       # fetches audio headers into third_party/ (no root), builds, runs the tests
./scripts/install.sh     # installs to ~/.local/opt/vrequency + launcher + `vrequency` command
./scripts/uninstall.sh   # removes it (add --purge to also delete ~/vrequency)
```

Needs: `g++`, `cmake`, `qt6-base-dev` and the PipeWire/ALSA runtime libraries — all already on this
machine. Nothing is installed system-wide and nothing starts at boot.

Tests:

```bash
./build/vrequency_tests                                   # 396 engine checks (offline rendering)
./build/vrequency_tests --bench                           # CPU at full polyphony
QT_QPA_PLATFORM=offscreen ./build/vrequency --selftest    # 69 checks through the real app + audio thread
QT_QPA_PLATFORM=offscreen ./build/vrequency --audio null --render docs/shots   # redraw the pictures
```

## Layout of the code

| Path | What |
|---|---|
| `core/` | The engine, no GUI or device dependencies: `voice.h` (synth voice), `fx.h`, `scale.h`, `instrument.*` (strings, backing, looper, mixer), `presets.*` (sounds, randomiser), `analysis.*` (key/tempo) |
| `audio/` | PipeWire backend (C), ALSA fallback, silent clock |
| `app/` | Qt 6 interface, one custom-painted widget: `mainwindow.*` (pages, input, actions), `scope.cpp` (the oscilloscope), `palette.h`, key map, files, self-test |
| `tests/` | Engine tests and benchmark |
| `docs/HARDWARE.md` | What was found on this Hackberry and what it means |
| `docs/STATUS.md` | What is tested, what still needs a person, known limits, and a five-minute test list |

## Honest status

See [docs/HARDWARE.md](docs/HARDWARE.md) for the audit and [docs/STATUS.md](docs/STATUS.md) for what is
verified, what still needs a human with the device in hand, and known limits.
