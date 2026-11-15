# venv-vrequency

A pocket string synthesiser for the Hackberry Pi CM5 (720×720 screen, Q20 keyboard).
Each keyboard row is a string: hold a key and press another on the same row and the
note *slides* there. Everything is locked to a scale, so it sounds musical whatever
you press. Original code and interface; inspired by the playing mechanics of slide-string
pocket synths, with no code, firmware or branding taken from any of them.

![The Synth page](docs/shots/0-play.png)

```bash
vrequency
```

or tap **venv-vrequency** in the app grid. `vrequency --windowed` runs it in a window,
`vrequency --help` lists the options.

## How it looks

The whole 720×720 screen is drawn the way a bench oscilloscope draws its own display: white on black,
one-pixel rules, raster type, inverse video for whatever is selected. There is no drawn hardware, and
the only colour is in the scope while the Glitch control is on. The scope on the Synth page shows the real output, triggered so the wave stands still, with
its own volts/div and time/div (they follow the signal and never change the pitch) and measured
frequency and level. Touching it is the X-Y sound control.

More pictures: [Sound](docs/shots/1-sound.png), [Backing](docs/shots/2-backing.png), [Tune](docs/shots/3-tune.png),
[System](docs/shots/4-system.png), [Help](docs/shots/5-help.png), [Key test](docs/shots/6-keytest.png), [Glitch](docs/shots/7-glitch.png), [Pulse](docs/shots/9-pulse.png), [Freq](docs/shots/27-freq.png), [Chat with a copilot](docs/shots/14-chat.png), [the chat dock on Synth](docs/shots/16-dock-synth.png), [the dock on Help](docs/shots/17-dock-help.png), [a highlight caption](docs/shots/18-caption.png), [the tour](docs/shots/20-tour-synth.png), [the tour's start-here walk](docs/shots/24-tour-start-here.png).

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

The full how-to is [docs/GUIDE.md](docs/GUIDE.md). A short version is built in: **Menu → Help** (or `Ctrl`+`H`). `Tab` cycles pages, `Esc` returns to the Synth page (the instrument, shown above).

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
- **Warp**: bend, warp and distort any sound. On the Sound page, Warp shows drive, fold, crush,
  downsample, ring mod and bend in, per note, beside an IN > OUT picture drawn by the voice's own code
  ([picture](docs/shots/29-sound-warp.png)). On the Pulse page, Warp gives each pad drive, fold, crush
  and a pitch bend across the hit ([picture](docs/shots/30-pulse-warp.png)).
- **Backing**: diatonic triads/sevenths from the scale. Tap up to eight chords for a looping
  progression, or latch one chord, or latch drone notes. Sustain / pulse / arpeggio styles.
  The backing keeps the sound it started with while you change the lead. `Ctrl`+`Space` starts and stops it.
- **Pulse**: eight percussive pulse pads built from sines, squares and noise: click, metal, snap,
  scratch, clap, tom, sub thump and 808, any pad any kind,
  each with Tone, Length, Rate and Level, and a warp (drive, fold, crush, bend). Each pad shows a picture of its real sound (drawn by its own
  generator); a strip shows every hit of the last four seconds. Latch keeps a pad going, Sync snaps
  rates to the tempo, and pad taps are recorded by the loop pedal.
- **Freq**: up to four steady tones at exact frequencies, each typed in (tap its Hz) or scrubbed by dragging (sine, triangle, square, saw or band-passed noise), with the beat between close tones shown and drawn from the real output, tone 2 bending tone 1 (FM or ring), fold / filter / drive / crush / delay, Hold, Gate (tempo-synced with Sync), the letter keys setting tone 1, and Snap to simple ratios. See [GUIDE chapter 18](docs/GUIDE.md#18-freq).
- **Tour**: an interactive picture of every page; tap any control to read what it does, how to use it by touch and keys, and how to find it again, with an **Ask Rook** button on every card. `Ctrl`+`O` or Help -> Tour; offered once on first run. See [GUIDE chapter 17](docs/GUIDE.md#17-the-tour).
- **Copilot**: a Chat tab and a local control socket let an AI agent play and edit the app live while you talk to it
  (`Ctrl`+`Y` opens a chat dock on whatever page you are on, so you can ask about the control Rook is pointing at without leaving it). Local Unix socket only, off with `--no-control`. See [docs/CONTROL.md](docs/CONTROL.md).
- **Looper**: event-based (notes, slides, bends, XY moves, sound changes), count-in, bar-snapped,
  up to 64 bars, six layers, undo, stop/start, clear. Each layer keeps its own sound. Follows tempo.
- **Saving**: sounds, favourites, sessions (settings + sounds + backing + loops + pulse pads), WAV export and play-back of the last recording
  under `~/vrequency/`. Key bindings are editable in `~/vrequency/keymap.json`.
- **Listen** (optional): estimates key and tempo from the default audio input with confidence
  values. *This Hackberry has no microphone*, so it reports that instead of guessing.

## Build, install, remove

```bash
git clone https://github.com/naninfinite/venv-vrequency.git
cd venv-vrequency
./scripts/build.sh       # fetches audio headers into third_party/ (no root), builds, runs the tests
./scripts/install.sh     # installs to ~/.local/opt/vrequency + launcher + `vrequency` command
./scripts/uninstall.sh   # removes it (add --purge to also delete ~/vrequency)
```

Needs: `g++`, `cmake`, `qt6-base-dev` and the PipeWire/ALSA runtime libraries — all already on this
machine. Nothing is installed system-wide and nothing starts at boot.

Tests:

```bash
./build/vrequency_tests                                   # 519 engine checks (offline rendering)
./build/vrequency_tests --bench                           # CPU at full polyphony
QT_QPA_PLATFORM=offscreen ./build/vrequency --selftest    # 396 checks through the real app + audio thread (one needs a real audio backend)
QT_QPA_PLATFORM=offscreen ./build/vrequency --audio null --render docs/shots   # redraw the pictures
```

## Layout of the code

| Path | What |
|---|---|
| `core/` | The engine, no GUI or device dependencies, in folders by domain: `synth`, `fx`, `instrument`, `pulse`, `freq`, `presets`, `analysis` |
| `audio/` | PipeWire backend (C), ALSA fallback, silent clock |
| `app/` | Qt 6 interface, one custom-painted window: `shell` (the window and the services pages use), `pages/<name>` (one folder per page), `ui` (drawing toolkit), `control` (the control socket), `tour`, `guide`, `chat`, `store`, `sounds`, `selftest` |
| `docs/ARCHITECTURE.md` | The layers, what lives where, and the size and layer rules (`scripts/check_sizes.sh`) |
| `tests/` | Engine tests and benchmark |
| `docs/CONTROL.md` | The control-socket protocol: every command with an example |
| `docs/HARDWARE.md` | What was found on this Hackberry and what it means |
| `docs/STATUS.md` | What is tested, what still needs a person, known limits, and a five-minute test list |

## Honest status

See [docs/HARDWARE.md](docs/HARDWARE.md) for the audit and [docs/STATUS.md](docs/STATUS.md) for what is
verified, what still needs a human with the device in hand, and known limits.

## Licence

MIT: use it, change it and share it freely, keeping the copyright notice. See [LICENSE](LICENSE).
The app uses Qt 6 and PipeWire from the system; they are not included here and keep their own licences.
