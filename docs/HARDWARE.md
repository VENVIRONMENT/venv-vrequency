# Hardware audit — this Hackberry Pi CM5

Everything below was read from the running system on 2026-11-13. Nothing is assumed.

## The machine

| | |
|---|---|
| Board | Raspberry Pi Compute Module 5 Lite, 4 × Cortex-A76, 16 GB RAM, NVMe |
| OS | Ubuntu 24.04 (aarch64), kernel 6.8.0-1065-raspi, GNOME on Wayland |
| Display | 720 × 720, scale 1, multi-touch |
| CPU governor | `ondemand` (not changed) |
| Root access | **None.** `sudo` is blocked for this session ("no new privileges"), so nothing was installed system-wide. Build headers are unpacked into `third_party/` inside the project. |
| Input devices | The user is not in the `input` group, so raw `/dev/input` events cannot be read. venv-vrequency uses normal window key events instead. |

## Audio

| Finding | Consequence |
|---|---|
| Sound server: PipeWire 1.0.5 + WirePlumber | venv-vrequency talks to PipeWire natively (256-frame quantum at 48 kHz = 5.3 ms). ALSA is a fallback. |
| ALSA cards: only the two HDMI outputs | No analogue/I²S sound card exists. |
| **The built-in speakers are a Bluetooth module**, `XWF-M18-M28-M38` (A2DP, SBC codec) | It was paired but disconnected, so PipeWire showed only "Dummy Output". **I connected it** (`bluetoothctl connect` with its address, which `bluetoothctl devices` lists); it is now the default sink at 60 % volume. |
| PipeWire reports **≈200 ms latency** for that speaker | This is the Bluetooth link, not the application: you will hear a note about a fifth of a second after pressing a key. It cannot be removed in software. venv-vrequency's own share is ~5–10 ms. |
| No capture device of any kind | No microphone. The Bluetooth module has no hands-free/mic profile. |
| AirPods Pro are paired (not connected) | They would work, with similar or worse latency. |

**For low latency** plug a USB audio adapter (or USB-C headphones/DAC) into the Hackberry, or use HDMI audio.
venv-vrequency follows the system default output, so nothing needs configuring: choose the output in GNOME's sound menu.
The System page shows which output is in use and whether it is Bluetooth.

**Loop timing on Bluetooth.** Because you hear yourself late, you naturally play late against the metronome.
When the output is Bluetooth, venv-vrequency shifts recorded loop events earlier by 200 ms (System → Loop fix,
adjustable 0–500 ms). On a wired output it is set to 0.

## Sensors

| Looked for | Found |
|---|---|
| Accelerometer / gyroscope (IIO) | **None.** No tilt control was built; the touch XY pad does that job. |
| Microphone | **None.** The Listen feature exists and is unit-tested on synthetic audio, but on this device it can only report "no audio input". Plug in a USB microphone and it has something to hear. |

## Keyboard (Q20, ZitaoTech RP2040 firmware, Vial/QMK)

The layout was read from the keyboard's Vial layout file. The keyboard is an 8 × 7 switch matrix:

| Matrix row | Keys |
|---|---|
| 0 | (mouse btn), W, G, S, L, H |
| 1 | Q, R, E, O, U |
| 2 | Caps/tap-dance, Left Alt, F, Left Ctrl, K, J |
| 3 | Space, C, Z, M, N |
| 4 | Super, Layer-1, T, D, I, Y |
| 5 | Esc/tap-dance, Left Shift, V, X, `$`, B |
| 6 | A, Layer-2, P, Backspace, Enter |
| 7 | Tab/tap-dance |

- Letter rows: `QWERTYUIOP` (10), `ASDFGHJKL` (9), `ZXCVBNM` (7).
- Digits are on firmware layer 1 (hold the layer key: `W E R` = 1 2 3, `S D F` = 4 5 6, `Z X C` = 7 8 9, Alt position = 0).
- Arrows are on firmware layer 2 (`W` up, `S` down, `A` left, `D` right).
- Esc and Tab are tap-dance keys (so they act on release, slightly late).
- The firmware sends ordinary USB key codes with press and release; auto-repeat comes from the desktop and
  venv-vrequency ignores repeats (a held key is one note).

### Rollover and ghosting

**Found on the device:** the Q20 registers about 11 to 12 keys at once. Pressing `Z` + `S` + `W`
together can produce a ghost `Space`. That is one of the combinations the analysis below predicts,
which suggests the matrix has no diodes. Avoid those combinations when playing, or check them with
Key test.

QMK boards normally have a diode per key (no ghosting). The analysis below was done before the keys
were tried on the device.

`scripts/ghost_analysis.py` works out which combinations *would* misbehave if there are no diodes
(three pressed keys forming a rectangle in the matrix make a fourth appear):

- 20 of the 630 possible one-key-per-string chords, e.g. `Z`+`S`+`W` → phantom `Space`.
- 45 modifier + same-row pairs, e.g. `Space`+`Z`+`X` → phantom `Shift`, `Alt`+`A`+`J` → phantom `Enter`.

Run `python3 scripts/ghost_analysis.py` for the full list.

**To find out:** open venv-vrequency, Menu → System → Key test (or run `vrequency --keytest`). It shows every key the
app currently believes is held, a log of the latest press/release events and the highest number held at once.
Try the combinations above. If only the keys you are pressing light up, the matrix has diodes and none of
this matters.
