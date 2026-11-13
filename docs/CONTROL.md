# The control socket

venv-vrequency opens a local Unix socket so an AI copilot (an agent such as Claude Code, through an MCP
server that speaks this protocol) can play the synth, edit it and chat with the player in the app's Chat
page. It is **on by default**; start with `vrequency --no-control` to switch it off.

- **Where**: `$XDG_RUNTIME_DIR/vrequency.sock`; if that is not set, `$VREQUENCY_HOME/control.sock` (default
  `~/vrequency/control.sock`). `$VREQUENCY_CONTROL_SOCKET` overrides both.
- **Who**: a Unix socket with permissions `0600`: only the same user can connect. There is no network
  listener of any kind. The file is removed when the app quits. A leftover file from a crashed run is
  replaced at the next start; a socket something still answers on is never taken over.
- **How**: newline-delimited JSON, one object per line, in both directions. Up to 8 clients at once. A request
  line longer than 1 MiB is refused.

```
-> {"id":1,"cmd":"note","string":1,"degree":2,"dur_ms":400}
<- {"id":1,"ok":true,"result":{"played":[{"string":1,"midi":57,"hz":220.0,"id":1024}]}}
-> {"id":2,"cmd":"note","string":9,"degree":0}
<- {"id":2,"ok":false,"error":"'string' must be between 0 and 2"}
```

`id` can be any JSON value and is returned unchanged (`null` if the request had none or was not valid JSON).
Every request is answered, whatever it contains.

**Safety.** Every command runs on the app's GUI thread and reaches the audio thread through the same lock-free
queue the keyboard uses; the audio thread is never blocked. Every argument is validated (type and range);
a bad one gets an error and changes nothing. Numbers are never clamped silently except where a command
says so (`set_param`). Sound goes through the engine's own limits: voice limit of 28, the limiter and soft
clip stay on the output. Changes are applied by the audio thread a few milliseconds after the reply, so
`state` read straight after a command may still show the old value.

Strings are numbered **0 = lowest** (the bottom key row), **1 = middle**, **2 = highest**. A *degree* is a
step of the current scale counted from the string's first key: `0` is the string's first key (`Z`, `A` or
`Q`), `1` its second, `-1` the note below, `5` can be past an octave. A *midi* note is an exact pitch
(12 to 120) that ignores the scale. Notes played over the socket are separate from the player's keys on
the same string: both can sound, and a key pressed on a sounding string slides it, as always.

## Commands

### state
Everything at a glance: `page`, `sound` (name, `slot` 1-10 or null, `slots`, `favourites`, `params` by key),
`scale` (name, index, root, octave), `tempo`, `volume`, `looper`, `beat` (playing, rec, kit, `steps` per drum
as 16 characters), `backing` (playing, mode, style, `chords`, `degrees`), `track` (clips, lanes with placements,
playing, bars, exporting), `playing`, `voices`, `toast` (the last message the app showed), `chat`
(`listening`, `last_seq`, `unread`) and `chat_tail` (last ten messages).
```
{"id":1,"cmd":"state"}
```

### note
Play one note. `string` 0-2 required; exactly one of `degree` (-48..48) or `midi` (12..120); `dur_ms`
20..30000 (default 500); `velocity` 0..1 (default 1).
```
{"id":2,"cmd":"note","string":0,"degree":0,"dur_ms":800}
{"id":3,"cmd":"note","string":2,"midi":81,"velocity":0.6}
```

### notes / chord
Several notes at once. `notes`: a list of objects like `note`'s (each may have its own `dur_ms` and `velocity`;
defaults at the top level). Or `chord` with `degrees` or `midis`: up to 3 values, put on strings 0, 1, 2 in order.
```
{"id":4,"cmd":"notes","notes":[{"string":0,"degree":0},{"string":2,"midi":72}],"dur_ms":600}
{"id":5,"cmd":"chord","degrees":[0,2,4],"dur_ms":1200}
```

### slide
Play `from`, then glide to `to` on the same string (at the sound's own glide time). `string`, `from`, `to`
required; `unit` `"degree"` (default) or `"midi"`; `ms` 50..30000 total (default 600); `velocity`.
```
{"id":6,"cmd":"slide","string":1,"from":0,"to":3,"ms":900}
```

### release / panic
`release` lets go of the notes played over the socket (optionally one `string`). `panic` silences everything,
as `Backspace` does (also stops beat, backing, loop and track).
```
{"id":7,"cmd":"release","string":1}
{"id":8,"cmd":"panic"}
```

### set_param / get_params
`set_param`: `name` is a key from `get_params` (`cutoff`, `attack`, `wave1`, `reverbmix`...), `value` is
**clamped** to the parameter's range (stepped ones rounded); the reply says the value used and whether it was
clamped. `get_params` lists every parameter with its value, range and default.
```
{"id":9,"cmd":"set_param","name":"cutoff","value":1200}
{"id":10,"cmd":"get_params"}
```

### sound
`slot` 1-10, or `name`: a favourite's name, `"random"`, `"mutate"` or `"undo"`.
```
{"id":11,"cmd":"sound","slot":2}
{"id":12,"cmd":"sound","name":"Haze"}
{"id":13,"cmd":"sound","name":"mutate"}
```

### save_favourite
Saves the current sound as a favourite under `name` (1-24 characters).
```
{"id":14,"cmd":"save_favourite","name":"Rook's pad"}
```

### scale
`name` (major, minor, majpent, minpent, blues, dorian, mixolydian, chromatic, or the full names such as
"Minor pentatonic") and/or `root` (a note name `A`, `C#`, `Bb`, or 0-11).
```
{"id":15,"cmd":"scale","name":"dorian","root":"D"}
```

### tempo, octave, bend, sustain
`tempo`: `bpm` 40-240. `octave`: `value` -3..3 or `delta`. `bend`: `value` -1..1 (held until changed), optional
`ms` to spring back after that long. `sustain`: `on` true/false.
```
{"id":16,"cmd":"tempo","bpm":96}
{"id":17,"cmd":"octave","delta":1}
{"id":18,"cmd":"bend","value":0.5,"ms":400}
{"id":19,"cmd":"sustain","on":true}
```

### loop
`action`: `record` (the pedal: count-in, record, or finish/overdub), `play`, `stop`, `clear`, `undo`.
`stop` on a loop that is not playing and `play` with nothing recorded are refused.
```
{"id":20,"cmd":"loop","action":"record"}
```

### backing
`chords`: a new progression of 1-8 chords, each 1-7 or a numeral `"I"`..`"VII"` (replaces the old one and
starts it). Or `action`: `play`, `stop`, `clear`, `remove_last`. Optional: `style` (sustain, pulse, arp up,
arp down, arp up-dn), `seventh` (bool), `beats` per chord 1-8.
```
{"id":21,"cmd":"backing","chords":["I","vi",4,5],"style":"pulse"}
{"id":22,"cmd":"backing","action":"stop"}
```

### beat
Any of: `kit` (soft, crunchy, glitchy), `set_step` (one `{drum, step, on}` or a list; `drum` 0-7 or a name:
kick, snare, hat, openhat, clap, rim, tom, cowbell; `step` 0-15, `on` default true), `hit` (`{drum}` or just
the drum: plays it now), `action` `play` | `stop` | `clear`.
```
{"id":23,"cmd":"beat","kit":"crunchy","set_step":[{"drum":"kick","step":0},{"drum":"snare","step":4}],"action":"play"}
{"id":24,"cmd":"beat","hit":{"drum":"clap"}}
```

### track
`action`: `keep` (copies the live beat, chords and loop to the shelf), `place` (`clip` index, `bar` 0-63,
optional `lane` 0-3 = beat, chords, loop A, loop B), `play`, `stop`, `clear` (empties the track), `export`
(asynchronous: replies at once with the `path` the WAV will have; it is complete when `state.track.exporting`
is false).
```
{"id":25,"cmd":"track","action":"keep"}
{"id":26,"cmd":"track","action":"place","clip":0,"bar":0}
{"id":27,"cmd":"track","action":"export"}
```

### page
`name`: track, synth, sound, beat, backing, tune, system, help or chat.
```
{"id":28,"cmd":"page","name":"chat"}
```

### session
`action` save or load, `name` (letters, digits, `-`, `_`), in the app's sessions folder.
```
{"id":29,"cmd":"session","action":"save","name":"jam-1"}
```

### screenshot
Writes a 720x720 PNG of the screen to the temporary folder and returns its `path`. The newest six are kept.
```
{"id":30,"cmd":"screenshot"}
```

## Chat

Messages have a `seq` (1, 2, 3...), `from`, `text`, `kind` (`say`, `tip`, `ask`), `ts` (ms since the epoch) and
`user` (true when the player typed it, who is shown as `you`). The last 200 are kept in the state file, so the
history survives restarts and the numbering carries on.

### chat_say
`text` 1-2000 characters; `from` (default `"rook"`, up to 16 characters; `"you"` is the player and refused);
`kind` `say` (default), `tip` or `ask`. Shows on the Chat page and, while the player is on another page, in a
two-line ticker at the bottom of the screen for a few seconds (it takes no key and no touch).
```
{"id":31,"cmd":"chat_say","from":"claude","kind":"ask","text":"Want a slower tempo?"}
```

### chat_poll
`since` (default 0), optional `limit`. Returns every message with `seq` > `since`, from both sides, and
`last_seq`. Counts as "the copilot is listening" for 45 s.
```
{"id":32,"cmd":"chat_poll","since":12}
```

### chat_wait
Long poll: `since`, `timeout_ms` 0-30000 (default 25000). The reply comes as soon as a message **typed by the player**
with `seq` > `since` exists, with `timed_out` false; otherwise at the timeout with `timed_out` true and an
empty `messages` list. Nothing else is blocked meanwhile, and several clients can wait at once.
```
{"id":33,"cmd":"chat_wait","since":14,"timeout_ms":20000}
```
While any chat_wait is pending, the Chat page says `copilot: listening`.

## Errors you may see

`bad JSON: ...`, `a request must be a JSON object`, `missing 'cmd'`, `unknown command 'x'`,
`'string' must be between 0 and 2`, `'x' must be a number` / `a string` / `true or false`, `missing 'x'`,
`request line too long`. A refused request changes nothing.
