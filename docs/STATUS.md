# Status — what works, what is unproven, what is missing

Version 0.2.0, 2026-11-13 (front panel redesigned as a bench oscilloscope; the engine is unchanged). "Tested" below means an automated test measured it; it does **not** mean a
person heard it or pressed a physical key. Those checks are listed separately.

## Working and covered by automated tests

`tendril_tests` renders audio offline and measures it (324 checks). `tendril --selftest` drives the real
application with real key and touch events while the real audio thread runs (58 checks).

| Area | What the tests establish |
|---|---|
| Startup | Audio stream opens on PipeWire and runs in real time; every menu and view renders at 720×720 with all 645 touch zones inside the screen and every required control present |
| Notes | Key-to-note tables are right; a key press produces the right frequency (measured: A2 = 110 Hz); a tap shorter than one audio block still sounds |
| Slides | A second key on a held string glides continuously and monotonically, with the same voice and no re-attack; releasing the newer key slides back; glide speed setting is honoured |
| Chord glides | Three strings sound together; re-fingering a held shape glides all three voices to the new chord |
| Scale lock | For every scale and every root, every key is in the scale and pitch rises left to right; chromatic override gives semitones; octave shift sweeps held notes without retriggering |
| Sounds | The 10 factory sounds differ measurably; switching sound under a held note makes no click |
| Randomiser | 120 generated sounds: all audible, all different, none over the level ceiling; mutate stays close; undo and favourites work through the app |
| Backing | Diatonic triads and sevenths named and built correctly; a tapped I–IV–V–IV progression loops in tempo under a live melody; remove / replace / stop / start / clear; hold and drone modes; arpeggio; backing keeps its sound when the lead changes |
| Looper | Count-in, bar snap, playback, overdub, undo, stop/start, clear; a recorded slide replays as a slide; an 18-second loop; a layer keeps the sound it was recorded with |
| Live control | XY filter and vibrato mappings, pitch bend, sustain; touch on a key-deck row plays and slides (synthetic touch events) |
| Front panel | Dragging each of the Cutoff, Resonance, Envelope, Drive, Delay, Reverb, Volume and Glide knobs changes the engine's values; a sideways drag is finer than a vertical one; the BP key switches the filter; the Edit menu plus the Entry knob (stepped, and dragged on screen) edits a sound parameter; the Time menu sets tempo; `Ctrl`+number presses a soft key; the Backing key starts and stops the backing; Time/Div and Auto scale change the picture and leave pitch alone; a partial repaint gives the same picture as a full one |
| Safety | Master output cannot exceed full scale even with a deliberately abusive sound; panic frees every voice |
| Files | Favourites, sessions (key, sounds, progression, loop) and settings save and load back the same; WAV export contains the audio that was played |
| Key handling | Auto-repeat does not retrigger; no input events dropped |
| Listen | Key and tempo found on synthetic test music; silence is refused rather than guessed; noise gives low confidence |

Implemented but with **no automated test**: loops following a tempo change, bends and XY moves inside loops,
voice stealing at the 28-voice limit (benchmarked for speed, not checked for clicks), releasing all notes
when the window loses focus, pulse and up/down arpeggio styles, most individual Edit-menu parameters (one is tested), the scope's
frequency readout and trigger (checked by eye on rendered pictures and against an offline calculation, not asserted), turning the
selected knob from the keyboard (`Ctrl`+arrows) and the trackpad wheel, the X-Y Filter/Hold keys, the Loop / Run-Stop / Undo panel keys
(their keyboard equivalents are tested).

## Measured on this device

| | |
|---|---|
| Engine cost, 28 voices + all effects | 5 % of one core (19.6× faster than real time); worst 256-frame block 0.35 ms of the 5.33 ms budget |
| Engine load while the self-test plays through the real speaker | 1.1 % average, 0 late blocks; the peak varied between 5 % and 30 % from run to run |
| Drawing the whole front panel from scratch | about 3 ms; normally only the tube is redrawn (25 times a second) and the panel only when something on it changes |
| Idle CPU (app open, nothing playing) | 0.4 % of one core measured headless with silent audio. **Not re-measured on the real screen** since the redesign |
| Memory | 49 MB headless; on-screen figure not re-measured since the redesign (was about 120 MB) |
| Launch to first frame | 0.07 s headless; about 0.3 s on screen |
| Output latency added by Tendril | one 5.3 ms block plus PipeWire's own buffering |
| Output latency of the built-in speaker | about 200 ms (Bluetooth; see HARDWARE.md) |

## Needs a person — NOT verified

Nobody has yet confirmed any of these. See the test list at the end of this file.

1. That sound actually comes out of the speaker at a comfortable level.
2. That the physical Q20 keys reach the app as expected (the letter rows almost certainly do; the
   modifier, number-layer, arrow-layer, `$`, Esc and Tab keys are less certain).
3. How many keys the keyboard reports at once, and whether it ghosts (HARDWARE.md has the suspect combinations).
4. That touch on the real screen works the knobs, soft keys, key deck and the X-Y trace (tested only with synthetic pointer events), and
   that the controls are big enough for a finger. They are small on purpose (a knob is 3–5 mm across on this screen); a touch within
   14 px of a control still counts, and every control also has a keyboard route.
7. That the new panel looks right on the real LCD: legibility of the 11 px printed legends, the phosphor glow, and that partial
   repainting leaves no stale patches (verified only against an offscreen render).
5. That the 200 ms loop timing fix feels right on the Bluetooth speaker.
6. Long-session stability (longest automated run is about a minute).

## Partial or simplified, on purpose

- **Three strings, not four.** The Q20 has three letter rows. Chords live on `Alt` + bottom row, the
  `$` chord-row toggle, or the alt key on the on-screen deck. (The old interface had separate chord pads; they are gone.)
- **The Freq readout is the waveform's true repeat rate.** A sound with a sub-oscillator reads an octave below the note name
  shown beside it (Warm Lead playing A3 reads 110 Hz). That is what a real scope would show.
- **It is a rendering, not a photograph.** The chassis, knobs and keys are drawn procedurally with lighting, grain and shadows;
  up close it reads as an illustration of hardware.
- **Bottom string has 7 keys, middle 9, top 10.** The strings still line up in pitch columns.
- **Bluetooth latency** is compensated for loop recording only. Live playing on the built-in speaker
  has an audible delay that software cannot remove.
- **Listen** is implemented but has no input to listen to on this hardware.
- **Windowed mode** (`--windowed`) gets an odd size from GNOME on this screen; full screen is the default and is what was designed.
- **Key remapping** is by editing `~/Tendril/keymap.json`; there is no on-screen editor.
- **WAV export** records what you play from the moment you press record; it cannot render a loop offline after the fact.
- **Looper layers** all share one loop length (the first recording sets it).

## Not implemented

- Tilt / motion control (no sensor in the device).
- MIDI in or out.
- Audio-rate looping (the looper stores notes and gestures, not sound; that is deliberate).
- Launch at boot (not requested).
- A fourth "virtual" string.

## What I changed on the system

- Connected the already-paired Bluetooth speaker `XWF-M18-M28-M38`, which made it the default audio output. Nothing else about audio was touched.
- Installed for this user only: `~/.local/opt/tendril/`, `~/.local/bin/tendril`, `~/.local/share/applications/tendril.desktop`, an icon under `~/.local/share/icons/`. User data goes in `~/Tendril/`.
- No packages installed, no system files edited, no services enabled. `scripts/uninstall.sh` removes all of the above.
- During development a few test windows briefly appeared and took keyboard focus; later tests ran headless.

## Please test (about five minutes)

Start with `tendril`. If there is no sound, check that GNOME's sound output is the built-in speaker.

| # | Do | Expect | If not |
|---|---|---|---|
| 1 | Press `A`, `S`, `D` one at a time | Three rising notes, comfortable volume; the trace stands still and the deck keys light | Note what Utility → Status says under Output |
| 2 | Hold `A`, press and hold `F`, let `F` go | Pitch slides up, then back down, no second attack | |
| 3 | Hold `Z`, `S`, `E` together | Three-note chord | Utility → Key test, see which key is missing |
| 4 | Hold `Shift`, play `A S D F` | Semitone steps | Key test: does Shift appear as held? |
| 5 | Hold `Space`, tap `Q W E`, release `Space` | Notes ring until Space is released | |
| 6 | `Alt` + `Z`, then `Alt` + `V` | Backing chords I and IV start looping | Tap alt on the on-screen deck, then the bottom row; tell me what Alt does |
| 7 | Number layer + `2`, `3`, `4` | Sound changes | Tell me which key is the layer key on your unit |
| 8 | `Enter`, wait for the count, play a bar, `Enter` again | Your phrase loops in time | If it is early or late, change Time → Loop fix and tell me the value that works |
| 9 | Drag a finger across the trace; then drag the Cutoff knob up and down, and sideways | Sound changes with the finger; the knob turns, sideways is finer, the readout on the tube shows the value | Tell me if the knobs are too small to grab |
| 9b | Tap Edit, tap a soft-key label, turn Entry; tap Time/Div's knob while holding a note | The parameter changes; the picture stretches but the pitch does not | |
| 10 | `Ctrl` + `R` a few times, `Ctrl` + `Z` | New sounds each time; undo brings the last one back | |
| 11 | Utility → Key test, then try `Z`+`S`+`W`, `Space`+`Z`+`X`, `Alt`+`A`+`J` | Only the keys you hold are listed | A key you did not press appears: the keyboard ghosts on that combination |
| 12 | `Backspace` | Silence | |

The most useful things to report back: the "Most keys held at once" number, any phantom keys in test 11,
whether the delay between key and sound is tolerable on the built-in speaker, and whether the panel is readable and
the knobs grabbable on the real screen.
