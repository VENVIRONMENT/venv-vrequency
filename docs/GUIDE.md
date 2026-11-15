# How to use venv-vrequency

A guide to playing the synth, page by page. Everything here describes version 1.0.0 and the default
key bindings. The Track page and the Beat page (drum kits and the step sequencer) are gone; the Pulse
page took the Beat page's place.

The short version is also built into the app: press `Ctrl`+`H`, or tap Menu and then the Help tab.

## Contents

1. [Start and quit](#1-start-and-quit)
2. [Your first minute](#2-your-first-minute)
3. [The Synth page](#3-the-synth-page)
4. [Playing the strings](#4-playing-the-strings)
5. [The scope and touch pad](#5-the-scope-and-touch-pad)
6. [Sounds](#6-sounds)
7. [Backing chords](#7-backing-chords)
8. [Pulse](#8-pulse)
9. [The loop pedal](#9-the-loop-pedal)
10. [Key, scale and tempo](#10-key-scale-and-tempo)
11. [Saving and recording](#11-saving-and-recording)
12. [System page](#12-system-page)
13. [All the keys](#13-all-the-keys)
14. [When something is wrong](#14-when-something-is-wrong)
15. [Chat with a copilot](#15-chat-with-a-copilot)
16. [Asking Rook while you play (the chat dock)](#16-asking-rook-while-you-play-the-chat-dock)
17. [The tour](#17-the-tour)
18. [Freq](#18-freq)

## 1. Start and quit

Start it from the app grid ("venv-vrequency"), or from a terminal:

```bash
vrequency
```

It opens full screen. To quit, press `Ctrl`+`Q`, or tap Menu, then System, then Quit. Quitting
saves your setup (sounds, key, tempo, settings) and it comes back the same next time.

`vrequency --windowed` runs it in a 720×720 window instead.

## 2. Your first minute

1. Press `A`, then `S`, then `D`. Each key is a note on the middle string.
2. Hold `A` and press `F` while still holding. The note slides up instead of starting again.
   Let go of `F` and it slides back to `A`.
3. Press `1` to `9` and `0` to try the ten sounds. On the Q20 the digits are on the number layer.
4. Drag a finger across the scope at the top of the screen. That changes the tone.
5. Tap two or three of the chord pads (Am, C, Em ...). The first tap starts the backing, and the
   chords loop as you add them, so you can play over them. (The Play button next to the pads stops
   it, and starts it again.)
6. Press `Backspace` if anything gets stuck. It silences everything.

Every key is locked to a scale, so nothing you press is a wrong note.

## 3. The Synth page

This is the instrument itself, and the page the app opens on. Its tab is called Synth.

![The Synth page](shots/0-play.png)

From top to bottom:

| Part | What it shows | What tapping it does |
|---|---|---|
| Header, left | Sound name, slot number, how many voices are sounding | Opens the Sound page |
| Header, right | Key, scale, tempo with a beat lamp, octave | Opens the Tune page |
| Level strip | Output level, as a row of segments | Nothing |
| Scope | The real output wave, with frequency and level readouts | Dragging is the touch pad (section 5) |
| Strings | Three strings with each note's key letter above it | Press to play, drag to slide |
| Progression strip | The chords you have queued, up to eight | Tap a filled slot to select it |
| Chord pads | The seven chords of the current key, with the mode and Play | Adds, holds or drones a chord (section 7) |
| Bottom row | Loop, Undo, loop play/stop, Sus, octave − and +, New, Menu | See below |

The bottom row:

- **Loop**: the loop pedal (section 9).
- **Undo**: removes the last loop layer.
- **▶ / ■**: starts or stops the loop.
- **Sus**: sustain stays on until you tap it again.
- **− / +**: octave down or up.
- **New**: invents a new random sound.
- **Menu**: opens the Sound page, which has the tab strip along the top for all the other pages:
  Synth, Sound, Pulse, Freq, Backing, Tune, System, Help and Chat, in that order.

`Tab` steps through the pages and `Esc` returns to Synth.

A small tag can appear under the sound name: "Limit" when the output is loud enough to be held
back, "REC" while recording to a file, and "No audio device" when there is nothing to play through.

## 4. Playing the strings

The three letter rows of the keyboard are three strings. Left to right goes up the scale.

| Keys | String |
|---|---|
| `Q` to `P` | High |
| `A` to `L` | Middle |
| `Z` to `M` | Low |

- **Slide:** hold one key and press another on the same row. The pitch glides to the new note
  without restarting. Release the newer key and it glides back.
- **Chords that glide:** hold a shape across two or three rows and move your fingers to a new shape.
  Each string slides to its new note.
- **Sustain:** hold `Space`. Released notes keep ringing, and new keys on a ringing string slide it.
- **Chromatic:** hold `Shift`. Keys then step in semitones, which lets you reach notes outside the scale.
- **Bend:** hold `Left` or `Right` (also `[` and `]`) to bend the pitch down or up.
- **Octave:** `Up` and `Down` (also `=` and `-`). Held notes sweep to the new octave.
- **Touch:** press a note box on screen to play it, and drag along the string to slide.

Notes with a bright frame are the root of the key. While a string sounds, a pair of brackets shows
where the pitch really is, so you can watch a slide travel.

On the Q20 the arrows and digits are on the symbol and number layers.

## 5. The scope and touch pad

The scope shows the sound actually leaving the synth. It holds the wave still and picks its own
scale. The readouts along its edges are the scale per division, the measured frequency and note,
and the level. None of this changes the sound.

Dragging on the scope does change the sound. Left to right is one control (X) and bottom to top is
another (Y). The button in the scope's corner shows which pair is active, and tapping it moves to
the next pair. `Ctrl`+`G` does the same.

| Mapping | Left to right (X) | Bottom to top (Y) |
|---|---|---|
| Filter | Cutoff | Resonance |
| Wobble | Cutoff | Vibrato |
| Morph | Timbre | Drive |
| Space | Delay | Reverb |
| Bend | Pitch | Cutoff |
| Glitch | Coarse | Grit |

**Hold** (next to it) keeps the position when you lift your finger. With Hold off, the sound
returns to normal when you let go.

### Glitch

![The Glitch mapping](shots/7-glitch.png)

Glitch crushes everything you hear (lead, backing, loop and pulses) into a rough, low-quality digital
sound for as long as your finger is on the pad.

- **Left to right (Coarse):** the sound gets blockier and more metallic.
- **Bottom to top (Grit):** the sound gets rougher and noisier.

While it is on, the scope leaves coloured copies of the wave streaking away diagonally. The white
wave on top is still the real sound, which now looks like steps. Further right makes the streaks
spread wider, and further up makes them last longer. Lift your finger and both the sound and the
picture go back to normal. `Backspace` (panic) also switches it off.

## 6. Sounds

![The Sound page](shots/1-sound.png)

**Pick a sound.** Tap one of the ten slots, or press `1` to `9` and `0`.

**Make a new one.**

- **New sound** (`Ctrl`+`R`) invents a complete new instrument.
- **Mutate** (`Ctrl`+`N`) changes the current sound a little.
- **Undo** (`Ctrl`+`Z`) goes back one step. The button shows how many steps are stored.

**Shape it.** The eight sliders are the quick controls: Cutoff, Reso, Attack, Release, Glide,
Drive, Delay and Reverb. Drag one to change it. **All parameters** swaps them for every parameter
the sound has, twelve to a page, with Prev and Next at the bottom (the fifth page is Warp, below).

**Bend, warp and break it.** Tap **Warp**, beside All parameters, and the eight sliders become the
Warp view. Tap it again to go back.

![The Warp view on the Sound page](shots/29-sound-warp.png)

- **IN > OUT**, the picture on the left, shows what the warp does to a sound. Imagine a signal
  rising from its lowest (left) to its highest (right); the line is what comes out. A straight line
  means clean. A line that bends flat at the ends is drive. A line that turns back on itself is fold.
  A staircase is crush. It is drawn by running the synth's own warp code, so it is the truth.
- **Drive** is the same Drive as in the quick sliders: warm, then fuzzy.
- **Fold** folds the loud parts of the wave back down instead of clipping them: bright, hollow,
  bell-like or metallic.
- **Crush** rounds the sound to fewer steps: off, then 12 bits (barely there) down to 2 bits (a
  coarse buzz), like an old sampler or a game console.
- **Downsample** holds each sample longer, as if the sound were played at 1/2 down to 1/32 of its
  normal rate: a metallic whine, and high notes that fold back into odd low ones.
- **Ring mod** mixes in the note multiplied by a sine 1.41 times its pitch. You hear clangy,
  out-of-tune partials, like a bell or a robot voice. At 100% the note itself disappears.
- **Bend in** makes every new note start up to two octaves away (up or down) and swoop onto its
  pitch in about 70 ms. 0 is off; a small negative bend sounds like a singer scooping into a note.

Each note is warped on its own, so chords stay clear instead of turning to mush. With every warp
slider at 0 a sound is exactly what it was before. Warp is saved with the sound: in slots, favourites,
sessions and Undo, like every other setting. **New sound** leaves the warp off; **Mutate** leaves it
as it is.

**Keep it.** There are two places:

- **A slot.** Tap **Store**, then tap the slot to put it in. This replaces what was in that slot and cannot be undone.
- **A favourite.** Tap **Save** (`Ctrl`+`F`). Favourites are listed at the bottom of the page, six
  to a page. Tap one to recall it. The page button shows `‹ 1/2 ›`: tap its left half to go back a page,
  its right half to go forward; both wrap round. To remove one, tap **Delete** and then the favourite.
  Saving under a name you already use updates that favourite (the toast says "Updated ...") instead of
  adding a copy. Delete, Store and Erase only stay armed while you stay on their page.

Factory sounds can be put back at any time: System page, **Factory sounds**. That overwrites slots
1 to 0. It leaves your own favourites alone and puts back any of the five dream sounds you deleted.

### Dream sounds

Five favourites are there from the first start: **Haze**, **Fuzz**, **Teeth**, **Bend** and
**Grain**. They are for slow, soft, washed-out playing. Each is built around a sine wave, so the
scope draws a round wave instead of the jagged one Warm Lead makes, and each adds its own kind of buzz.

![The wave each dream sound draws, next to Warm Lead](shots/8-dream-waves.png)

| Sound | What it is |
|---|---|
| Haze | A sine with a quiet sine an octave above it. The plainest of the five. |
| Fuzz | A sine pushed into soft distortion, with a low note underneath. |
| Teeth | A sine with a quiet, bright sawtooth riding on it, which shows as small teeth on the wave. |
| Bend | A sine whose shape is bent by a second sine. The dents drift slowly as you hold a note. |
| Grain | A triangle and a sine that beat against each other, with a little hiss and a slow tremolo. |

All five fade in gently, ring on for a couple of seconds after you let go, wobble slowly in pitch
and sit in a lot of chorus, echo and reverb. They suit held notes, slides and chords on three
strings more than fast playing. If the wash is too much, pull **Reverb** and **Delay** down.

To put one on a number key: tap it, tap **Store**, then tap a slot. If you delete one, it stays
deleted until you use **Factory sounds** on the System page.

## 7. Backing chords

![The Backing page](shots/2-backing.png)

The seven chord pads are the chords that belong to the current key (a small numeral is a minor
chord). They sit under the strings on the Synth page and again on the Backing page. What a tap does
depends on the mode, shown on the button beside the pads on the Synth page (tap it to change, or
`Ctrl`+`B`) and as three buttons at the top of the Backing page:

| Mode | A chord tap ... |
|---|---|
| Progression | Adds the chord to a looping sequence of up to eight |
| Hold | Plays that one chord until you pick another |
| Drone | Switches a drone note for that chord on or off |

To build a progression:

1. Make sure the mode is Progression.
2. Tap chords in the order you want. They fill the strip above the pads. The first chord tap
   already starts the backing, and the loop grows as you add chords.
3. **Play** (or `Ctrl`+`Space` from any page) stops the backing, and starts it again from the first
   chord. Tapping it while the backing is already running stops it.

To change one chord, tap its slot and then tap the new chord. **Remove last** (also
`Ctrl`+`Backspace`) and **Clear all** are on the Backing page. Panic (`Backspace`) stops the
backing too.

Also on the Backing page:

- **Style**: Sustain, Pulse, Arp up, Arp down, Arp up-dn (`Ctrl`+`A` steps through them). The Pulse
  style re-strikes the chord on every beat; it is not the Pulse page.
- **7th chords**: four-note chords instead of three.
- **Beats per chord** and **Arp steps per beat** (the arpeggio styles only; the Pulse style always
  strikes once per beat).
- **Tempo**, **Metronome** and **Backing volume**.
- **Use the current lead sound for the backing**. The backing keeps the sound it started with, so
  you can change the lead without changing the chords. Tap this when you do want them to match.

The backing shares its bar lines with the loop pedal (and, with Sync on, the pulse pads), so they
stay together whichever you start first.

From the keyboard, hold `Alt` and press `Z` to `M` for chords one to seven. `$` turns the whole
bottom row into chord keys until you press it again.

## 8. Pulse

![The Pulse page](shots/9-pulse.png)

The Pulse page has eight **pulse pads**: short, exact percussive sounds built from sines, square
waves and noise. There is no step sequencer; you play them, latch
them, sync them to the tempo and record them with the loop pedal.

**Play the pads.** Tap a pad, or use the keyboard: `Q` `W` `E` `R` (or `U` `I` `O` `P`) are pads 1
to 4, `A` `S` `D` `F` (or `H` `J` `K` `L`) pads 5 to 8. The bottom row still plays the low string.
A tap also **selects** the pad; `Left` / `Right` select the pad before or after without playing it.
Each pad shows a small picture of its sound. The picture is made by running the pad's own
generator, the same code the audio uses, so it is the sound you hear (drawn again whenever a
setting changes).

**The eight kinds.** Pad *n* starts as the *n*-th kind, but any pad can be any kind: tap the
selected pad's **name** (the boxed word in the line above the big picture, `TOM ›`) and it becomes
the next kind. What **Tone** means depends on the kind:

| Kind | What it is | Tone |
|---|---|---|
| Click | a damped cosine: one spike and a short ring | the ring, Hz |
| Metal | six square waves at the clashing ratios of an analogue hi-hat, through a high band and two high-passes | the lowest square; the band sits 20 times higher |
| Snap | a short body dropping from twice Tone to Tone in 8 ms, under a burst of band-passed noise, like a snare | the body; the noise band sits 25 times higher |
| Scratch | a turntable scratch: a rough tone (a sine with its second and third harmonics, and a little grit) pushed forward to 1.6 times speed, stopped, then pulled back to 1.2 times, over Length | pitch at normal speed |
| Clap | band-passed noise in three 3.5 ms bursts 11 ms apart, then a tail | the band |
| Tom | an 808-style tom: a sine falling from 2.5 times Tone to Tone in about 20 ms, with a 2 ms noise strike | where it lands |
| Sub thump | a low sine falling from four times Tone to Tone in about 15 ms | where it falls to |
| 808 | a long, tuned sub boom: a sine gliding down from twice Tone (40 ms), ringing over Length, softly saturated | the note it lands on: tune it to the song |

**Length** is how long one hit lasts; every kind fits its decay into it. **Rate** is how often a
latched pad is hit again.

**The selected pad.** The big picture shows its sound over its whole length, with the length at the
top left and, at the top right, its tone, band or sweep. The four
sliders beside it set **Tone** (20 Hz to 18 kHz), **Length** (1 ms to 2 s), **Rate** (0.25 to 200 Hz)
and **Level**; the first three move in octaves.

**Last 4 s.** The strip under it has one lane per pad. Every hit is a tick at the moment the engine
played it, oldest on the left, with a thin bar for its length.

**The soft keys.**

| Key | Does |
|---|---|
| Loop | the loop pedal (the same as `Enter`): pad taps made while it records go into the loop and play back with the pad's settings of the moment |
| Latch | the selected pad is hit again Rate times a second until you tap it (or Latch) again. Several pads can be latched; `Backspace` stops them all |
| Sync | every pad's Rate snaps to the nearest division of the tempo (from four bars to 1/512), shown beside the rate; latched one-shots then fall on the beat clock's grid, in time with the loop |
| Warp | the four sliders become the selected pad's warp: Drive, Fold, Crush and Bend (below). Tap again for Tone, Length, Rate and Level |

**Warp a pad.** With **Warp** on, the sliders beside the big picture bend and break the selected pad:

![The Pulse page with Warp on](shots/30-pulse-warp.png)

- **Drive** pushes the hit through a soft curve: rounder and louder at first, fuzzy at the top.
- **Fold** folds the loud start of the hit back on itself: brighter, knocking, metallic.
- **Crush** gives it fewer bits and a lower sample rate together: lo-fi grit.
- **Bend** sweeps the pitch across the hit, up to two octaves up or down: -12 on a tom is a falling
  laser drop, +12 on a click a rising chirp. On Metal and Clap, which have no pitch, it moves their
  noise band instead.

The top right of the big picture names the warp in use, and both the big picture and the small
ones on the pads show the warped sound: they are made by the same code the audio plays. Leaving
the page turns the Warp key off again (the pads keep their warp).

The pads' settings (warp included) and Sync are saved in your state and in sessions; latches are not, so nothing
starts by itself. Latched repeats are not written into a loop, only taps.

## 9. The loop pedal

One button, or the `Enter` key, does everything. The Loop button on the Synth page always says what
the next tap will do.

1. Press `Enter`. The button counts you in.
2. Play. The button shows REC and the bar number.
3. Press `Enter` again. Recording runs to the end of the bar, then your phrase loops.
4. Press `Enter` again to add a layer on top (DUB), and once more to finish the layer.

| Do this | Key | Button |
|---|---|---|
| Remove the last layer | `Ctrl`+`U` | Undo |
| Stop or restart the loop | `Ctrl`+`L` | ▶ / ■ |
| Erase the loop | `Ctrl`+`D` | (keyboard only) |

A loop can be up to 64 bars with six layers. Each layer remembers the sound it was played with,
and the loop follows the tempo if you change it.

Loop volume is on the System page.

## 10. Key, scale and tempo

![The Tune page](shots/3-tune.png)

- **Key**: tap one of the twelve notes. `Ctrl`+`K` steps up a semitone.
- **Scale**: Major, Natural minor, Major pentatonic, Minor pentatonic, Blues, Dorian, Mixolydian,
  Chromatic. `Ctrl`+`S` steps to the next one. With `Shift` held as well, both shortcuts go the
  other way.
- **Octave**: moves the whole instrument up or down.
- **String gap**: how far apart the three strings are tuned, in semitones.
- **Bend range**: how far the bend keys move the pitch.
- **Tempo**: the − and + buttons, or tap the **Tap** button in time (`Ctrl`+`T`).
- **Click**: the metronome (`Ctrl`+`M`).
- **Glide speed**: makes every slide faster or slower. All the way down is instant.

**Listen** tries to work out the key and tempo of music playing nearby. It needs a microphone,
and this Hackberry does not have one, so it will say so.

## 11. Saving and recording

Everything is kept in the `vrequency` folder in your home folder.

| What | How | Where it goes |
|---|---|---|
| Your setup | Automatic when you quit | `state.json` |
| A favourite sound | Sound page, Save, or `Ctrl`+`F` | `favourites/` |
| A session: sounds, the backing, loops and the pulse pads together | System page, Save session | `sessions/` |
| A recording of what you hear | System page, Record, or `Ctrl`+`E`. The same again stops it. | `recordings/` |

**Listening back.** System page, **Play last** (or `Ctrl`+`W`) plays your newest recording through
the speaker. The same again stops it. You can play along while it runs.

To load a session, tap it in the list on the System page, then start the loop, or tap a chord.
Sessions saved before 1.0 load too; their old drum kit and beat settings are left out.

## 12. System page

![The System page](shots/4-system.png)

- **Master volume** and **Loop volume**.
- **Touch pad mapping** and **Hold**: the same as the buttons on the scope.
- **Chord row**: the bottom letter row plays chords (same as `$`).
- **Record**, **Play last** (listen to the newest recording), **Save session** and the list of saved sessions.
- **Key test**: shows which keys the keyboard reports, for checking how many it can read at once.
- **Status**: the audio device in use, how hard the engine is working, and where files are kept.
- **Panic**: all notes off (same as `Backspace`).
- **Loop fix**: shifts recorded loops earlier or later in milliseconds. Change it if your loops
  play back out of time with what you played.
- **Factory sounds**: restores slots 1 to 0 and puts back any deleted dream sounds.
- **Quit**.

## 13. All the keys

Playing:

| Key | Does |
|---|---|
| `Q`‥`P`, `A`‥`L`, `Z`‥`M` | High, middle and low strings |
| `Shift` (hold) | Chromatic |
| `Space` (hold) | Sustain |
| `Backspace` | Panic |
| `Enter` | Loop pedal |
| `Left` / `Right`, or `[` / `]` | Bend down / up |
| `Down` / `Up`, or `-` / `=` | Octave down / up |
| `1`‥`9`, `0` | Sounds 1 to 10 |
| `Alt` + `Z`‥`M` | Chords I to VII |
| `$` | Bottom row plays chords, on and off |
| `Tab` / `Esc` | Next page / back to Synth |

Commands, with `Ctrl` held:

| Key | Does | Key | Does |
|---|---|---|---|
| `R` | New sound | `U` | Undo loop layer |
| `N` | Mutate | `L` | Loop play / stop |
| `Z` | Undo sound | `D` | Clear loop |
| `F` | Save favourite | `E` | Record WAV |
| `K` | Key up | `W` | Play last recording |
| `S` | Next scale | `Q` | Quit |
| `T` | Tap tempo | `H` | Help |
| `M` | Metronome | `O` | Tour |
| `G` | Next touch-pad mapping | `V` | Scope: 3D stack or line |
| `P` | Strings / Poly keyboard | `C` (hold) | Open the filter |
| `Space` | Backing play / stop | `X` (hold) | Close the filter |
| `B` | Backing mode | `A` | Backing style |
| `Backspace` | Remove the last chord | | |
| `Y` | Chat dock: ask Rook on this page (`Shift`: the Chat page) | | |

On the Pulse page only:

| Key | Does |
|---|---|
| `Q` `W` `E` `R`, or `U` `I` `O` `P` | Pads 1 to 4 |
| `A` `S` `D` `F`, or `H` `J` `K` `L` | Pads 5 to 8 |
| `Left` / `Right` | Select the pad before / after |

On the Freq page only (see [chapter 18](#18-freq)):

| Key | Does |
|---|---|
| `1`‥`4` | Select a tone |
| `Left` / `Right` | Move the selected tone 0.5 Hz (with `Shift`: 10 Hz) |
| `Up` / `Down` | Move it a semitone |
| Letters, with Keys on | Set tone 1 to that note |

To change any binding, edit `keymap.json` in the `vrequency` folder and restart the app. Deleting
that file restores the defaults.

## 14. When something is wrong

| Problem | Try this |
|---|---|
| No sound, and "No audio device" under the sound name | The Bluetooth speaker has dropped. Reconnect it, then check Output on the System page. |
| Sound on a Bluetooth speaker plays well for a few seconds, then stutters, again and again | Close the Bluetooth settings window. While it is open the device keeps searching for other devices, and that interrupts the music. |
| A note is stuck | `Backspace`. |
| Notes come out late | Normal on the built-in speaker: it is Bluetooth and runs about 0.2 s behind. Wired headphones do not. |
| Loops play back early or late | System page, Loop fix. Adjust in steps until a recorded bar lines up. |
| Some keys do not sound when held together | Open Key test on the System page and hold the same keys. A key missing there is a limit of the keyboard, not the synth. |
| A slot was overwritten by mistake | Storing to a slot cannot be undone. Factory sounds on the System page puts the ten originals back, replacing anything else you stored in slots. |
| Pulse taps in a loop land late | The Bluetooth speaker is late, so you tap late. System page, Loop fix moves recorded taps earlier; raise it if they land late, lower it if early. |
| The sound is distorted or quiet with "Limit" showing | Lower Master volume, or the sound's Drive. |

To reconnect the speaker from a terminal, list the paired devices to find its address
(the Hackberry's built-in speaker is the one named `XWF-M18-M28-M38`), then connect to it:

```bash
bluetoothctl devices
```

```bash
bluetoothctl connect XX:XX:XX:XX:XX:XX
```

## 15. Chat with a copilot

![The Chat page](shots/14-chat.png)

The last tab, **Chat**, is a conversation between you and an AI copilot (for example the agent Rook, or
Claude Code) that is connected to the app through its control socket. It can play notes, change the sound,
shape and play the pulse pads and look at the screen while you talk. The commands it uses are in
[CONTROL.md](CONTROL.md). The socket is a local file only your user can open; `vrequency --no-control`
switches it off, and then the Chat page simply stays quiet.

- **Type** on the keyboard: on this page letters type instead of playing strings. `Enter` sends,
  `Backspace` deletes a letter, `Up` / `Down` (or a finger drag on the history) scroll, `Esc` leaves the page.
  The **Send** button sends by touch. `Tab` and the `Ctrl` commands still work.
- **Hotkey**: `Ctrl`+`Shift`+`Y` jumps to this page from any page, and `Ctrl`+`Y` here takes you back to where
  you were. (On the other pages `Ctrl`+`Y` opens the chat dock instead, chapter 16. It is bound as `chat`
  in `keymap.json`; the other `Ctrl` letters are unchanged.)
- The copilot's messages have a white tag with its name; yours have an outlined `YOU`. A `?` after the
  name means it is asking you something, `tip` marks a suggestion.
- **The ticker**: when the copilot says something while you are on another page, the message appears in a
  two-line strip at the bottom of the screen for about seven seconds, and the Chat tab shows how many are
  unread (`Chat 2`). It never takes the keyboard away, so you can keep playing; it does cover the
  bottom row of controls while it shows. Touch it, or press `Ctrl`+`Y`, to answer in the chat dock (chapter 16).
- **`copilot: listening`** at the top right means a copilot is connected and waiting for you
  (it has asked for your messages in the last 45 seconds, or is waiting right now); otherwise it says
  `copilot: not connected`. Messages you type then wait for it: they are kept.
- The last 200 messages are kept between runs.

![A message arriving while playing](shots/15-ticker.png)

## 16. Asking Rook while you play (the chat dock)

![The chat dock on the Synth page, with OCTAVE highlighted](shots/16-dock-synth.png)

You do not have to leave the page you are on to talk to the copilot. Press `Ctrl`+`Y` on any page
except Chat and a **dock** opens over the bottom of the screen: the last two or three messages, an
input line and Send. The page behind it stays visible and keeps working above the dock.

- **Keys.** `Ctrl`+`Y` opens the dock and gives it the keyboard: letters type (they do not play strings),
  `Enter` sends, `Backspace` deletes, `Up` / `Down` scroll. `Esc` gives the keyboard back to the page and
  leaves the dock showing, so the keys play again; `Esc` once more closes the dock. `Ctrl`+`Y` while the dock
  is open switches the keyboard between the two. `Ctrl`+`Shift`+`Y` opens the full Chat page. By touch:
  tap the input line to type, **Send**, **Full** (the Chat page), **Hide**.
- **From the ticker.** When Rook speaks while the dock is closed, the ticker shows the message; touch it, or
  press `Ctrl`+`Y`, to answer straight away.
- **Pointing.** Rook can ring the controls it is talking about with a pulsing double outline, sometimes with a
  small caption tag next to it. The dock's header then says what is being discussed (`▸ OCTAVE`). The highlight
  goes away after a few seconds, or as soon as you change page. If the dock would cover what Rook points at, it
  gets shorter or moves to the top of the screen. When you answer, your message tells Rook which page you
  were on and what was highlighted, so "what does this do?" works.
- **On the Help page** every row is touchable. Tap a row and the dock opens with the question already started
  ("What does "Enter: loop pedal ..." do?"); press `Enter`. **Ask Rook** at the top opens the dock for the section you
  are reading. Rook can highlight a row ("the Loop pedal row") while it explains.

![The Help page with the dock, one row highlighted](shots/17-dock-help.png)

![A caption tag on a highlighted control](shots/18-caption.png)

Rook needs to be connected (the dock says `Rook is listening`); if it is not, your messages wait for it.
The commands Rook uses are in [CONTROL.md](CONTROL.md).

## 17. The tour

![The tour on the Synth page, OCTAVE selected](shots/20-tour-synth.png)

The tour is a picture of every page with a card for every control on it. Nothing in it is live: it never plays,
never changes your sound or settings, and the keys do not reach the synth while it is open.

**Opening it.** Tap **Tour** at the top of the Help page, or press `Ctrl`+`O` on any page. The first time the
app starts it offers the tour in a banner at the top (`Enter` takes it, `Esc` or waiting says later; it is only
offered once). Rook can also open it for you. It reopens on the page you left it on.

**Reading it.** The chips under the title pick the page (Synth, Sound, Edit, Pulse,
Freq, Bkng, Tune, Sys, Keys, Help, Chat). Tap any control in the picture: it is ringed with a pulsing double rule,
the rest dims, and the card on the right says what it does, how to use it by touch and by keys, and **Recall**,
which is where to find it again. **Related** chips jump to the next card. Readouts that are not buttons (the
level strip, scope readouts, the Listen result, the status box, the key list rows) have cards
too. Tap empty space to clear. **Concepts** lists the ideas behind the controls (scale lock, tempo, bar lines...).
The Edit chip is the All parameters editor; its Prev and Next flip through the four parameter pages.

**Keys.** Arrows move between controls, `Tab` / `Shift`+`Tab` go in reading order, `,` and `.` change page,
`C` opens Concepts, `V` switches picture variant where a page has more than one (the All parameters
editor), `Space` or `Page Down` scrolls a long card, `Home` clears the selection, `Esc` closes a card, then the
concept list, then the selection, then the tour.

**Start here.** The first time, or from the banner, the tour walks nine pages in order (Synth, Sound, Tune,
Backing, Pulse, Freq, System, Help, Chat) with Back, Next and **Explore freely**.

**Ask Rook.** The button at the bottom right leaves the tour, goes to the real page the card belongs to, and
opens the chat dock with a question already written (not sent). Press `Enter` to send it; Rook is told the
page and which control you were reading (`via` is `tour`).

![A concept card, with Recall](shots/23-tour-concept-recall.png)

The first time the tour opens in a run it draws its pictures (two or three seconds on the Pi; it says so).
The cards come from `docs/helpcards.json`, built into the program; a damaged file shows a "Help unavailable"
card, never a crash. `scripts/check_helpcards.py` checks that every control has a card.

## 18. Freq

![The Freq page](shots/27-freq.png)

The Freq page plays up to four **steady tones at exact frequencies**. It is not about melodies: it is
about what happens between tones. Two tones a little apart **beat** (the sound swells and fades), tones
in simple ratios (3:2, 2:1) sound still and fused, and tone 2 can bend tone 1 into new sounds.

**Start.** Open the **Freq** tab (after Pulse) and tap **Hold**. Tones 1 and 2 start at 220 Hz and
221.5 Hz, so you hear one tone swelling and fading one and a half times a second. Tap **Hold** again
(or press `Backspace`) to stop. Nothing sounds when the app starts.

**The big view** shows the Freq sound's last 1.5 seconds, drawn from what actually came out. A steady
tone is a flat band; beating tones make swells. The top right says the beat and how long one swell takes,
for example *beat 1.50 Hz (0.67 s)*. The line under the tabs says which two tones make it: the beat is
always the difference between the two lowest tones that are within 20 Hz of each other.

Under it, **2 cycles** shows two waves of tone 1 from the real sound (so you see its shape change when
you change waves or warp it), and **spectrum** shows which frequencies the sound is made of: every tone
is a peak.

**The four tone rows.** Each row is one tone:

| Part | What it is |
|---|---|
| Number | Tap the row to select it (it gets a double frame). Tapping a row that is off also switches it on |
| Wave | Sine, Tri, Square, Saw or Noise. Tap to step through them. Noise is hiss narrowed to a band at the tone's Hz |
| Hz | The exact frequency, to two decimals |
| Note | The nearest note (A3, E4...). For a tone within 20 Hz of tone 1 it shows the difference instead, like +1.50 |
| Ratio | How the tone sits against tone 1: 3:2 is exact, ~3:2 is close, - is nothing simple |
| Level | Tap or drag the bar |
| ON / off | Switches the tone in or out (its settings are kept) |

**Typing or scrubbing an Hz.** Tap a tone's Hz and type the number you want (`.` for decimals, `Backspace` to fix), then `Enter`; `Esc` leaves it as it was. Or drag sideways across the Hz: 40 px is a semitone.

**Moving a tone.** Select it (tap it, or press `1` to `4`), then `Left` / `Right` move it by 0.5 Hz
(hold `Shift` for 10 Hz) and `Up` / `Down` by a semitone. Every change glides over about 20 ms, so it
never clicks. Try this: Hold, select tone 2, and press `Right` a few times. The beat speeds up by half a
swell a second each time.

**Bend and warp.** Six cells. Drag across one to set it; a tap on any but 2 > 1 sets it back to 0 (Filter: open).

| Cell | Does |
|---|---|
| 2 > 1 | Tone 2 bends tone 1. Tap to choose **FM** (tone 2 wobbles tone 1's wave, adding new peaks) or **RING** (the two multiplied: you hear their sum and difference) or OFF; drag sideways for how much. Turn tone 2's level down to hear only its effect |
| Fold | Folds loud parts of the wave back down: hollow, metallic overtones |
| Filter | Takes away the highs above the setting, 40 Hz to 18 kHz (open) |
| Drive | Soft overdrive: rounder, warmer, louder |
| Crush | Off at the far left, then fewer levels and a lower sample rate: digital grit |
| Delay | Sends the Freq sound into the echo the synth uses (its time comes from the current sound) |

Fold, Drive, Crush and Filter work on all four tones together. However far you push them, the Freq sound
never goes past full scale.

**The soft keys.**

| Key | Does |
|---|---|
| Hold | The tones sound and keep going |
| Gate | The tones pulse on and off at the rate shown; `+` and `-` beside it change the rate. When **Sync** is on (Pulse page) the rate snaps to a note value of the tempo and the pulses land on the beat. Gate on its own also starts the tones |
| Keys | The letter keys set tone 1 to the notes of your key and scale, the same notes the strings play. The other tones follow: a tone close to tone 1 keeps its Hz difference (so the beat stays the same), the others keep their ratio. Without Hold the tones sound only while you hold a key |
| Snap | Moves the selected tone to the nearest simple ratio of tone 1 (1:1, 2:1, 3:2, 4:3, 5:4...). With tone 1 selected it moves to the nearest note instead. The small print says where it will go |

`Enter` is still the loop pedal, `Space` sustain, `Backspace` panic (it stops Freq too), and with
Keys off the letters play the synth as on every other page. Your tones, waves, bend and warp settings,
the gate rate and Keys are saved with your state and in sessions; Hold and Gate are not, so the tones
never start by themselves.
