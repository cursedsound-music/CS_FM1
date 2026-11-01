# cursed-os

**Custom firmware for the M-VAVE FM-1, made by cursedsound to sit next to an Elektron Syntakt in a DJ set.**

cursed-os is an Elektron-style interface built on [JIANT FM](https://github.com/juanjiant-bit/JIANT)'s sound engines and
sequencer. One button per mode, its pages as tabs, the 16 white keys as trigs, four knobs per page. The screen is black and
white; red is kept for what is live: the playhead, the knob being turned, REC, a key change waiting for the bar.

**The browser preview**: the real firmware compiled to WebAssembly, with sound, played with your computer keyboard and
mouse. Every push to `main` builds it (Actions, the run's "cursed-os-site" download: unzip, then
`python3 -m http.server -d cursed-os-site` and open <http://localhost:8000>).

> **Status: early and untested on hardware.** Everything here has been built and run in the host tests and the browser
> emulator, not yet on a real FM-1. Back up your FM-1 before installing, and keep the web installer's
> "Return to official" in mind.

## What it does

- **ACID generator** (ARP): controlled-random 303 lines. Rhythm, notes and accents/slides each have their own seed, so
  LOCK can keep the groove while OCT+ rethrows the notes; OCT- walks back through the last eight lines. Density, slide,
  accent, octave jumps, length (4-64 steps), size of the note pool, ties, and bar-synced mutation.
- **The keys set the song's key** (SCL): hold SCL and tap a key, and every synth track moves to that key on the next bar,
  by the nearest interval. Drums stay. The notes themselves move, so MIDI out follows.
- **Step sequencing** (SEQ): the white keys are the 16 trigs, OCT-/OCT+ the banks of 16 (up to 64 steps). On a drum kit
  the black keys pick the sound.
- **HOME**: every track's steps at a glance; white keys pick a track, black keys mute.
- **Sound pages**: SRC (the engine), AMP / FLTR (ENV), LFO, FX sends, MIX (GLO), KEY (SCL).
- **Everything JIANT FM has underneath**: 11 engines (DRUM-X with 808/909 kits, a 6-operator FM engine, analog, VOICE
  that sings words, Karplus-Strong strings, additive, ...), Euclidean patterns, songs, MIDI clock in, USB/Bluetooth MIDI
  out, USB audio. **HOME + GLO** switches to JIANT's own interface and back, for anything cursed-os doesn't cover yet
  (saving, REC, the song arranger, the menu).

## Controls

| Control | cursed-os |
| --- | --- |
| HOME / SEQ / EDIT / ENV / LFO / FX / ARP / GLO / SCL | HOME / TRIG / SRC / AMP+FLTR / LFO / FX / GEN / MIX / KEY; press again for the next page |
| SELECT | the page |
| ALGORITHM | the track |
| KNOB 1-4 | the page's four values |
| PLAY | start / stop |
| OCT- / OCT+ | the bank of 16 (TRIG), the line before / a new line (GEN), the keyboard octave elsewhere |
| SCL held + a key | the song's key |
| HOME + GLO | JIANT's interface and back |

## With the Syntakt

- **Clock**: Syntakt MIDI OUT -> a DIN-to-TRS adapter -> the FM-1's TRS MIDI IN; on the FM-1, GLO > GLOBAL > CLOCK = TRS.
- **Notes into the Syntakt**: the FM-1 sends MIDI over USB and Bluetooth only, and the Syntakt's USB is a device, so a
  USB-MIDI host box or a Bluetooth-MIDI adapter on the Syntakt's MIDI IN goes in between. FM-1 track N plays on MIDI
  channel N (MENU > CONTROL > MIDI OUT = ALL).

## Building

See [BUILDING.md](BUILDING.md) (JIANT's): `tools/get_toolchain.sh` for JieLi's toolchain, the AC79 SDK, then
`./build.sh` makes `build/felucca.fwsc`. `tests/run_tests.sh` runs the host tests, including cursed-os's
(`tests/cos_test.c`, `tests/acid_test.c`). The browser preview: `web/emu/build.sh` (Emscripten) or
`web/emu/build_clang.sh` (plain clang and a WASI libc), then `python3 -m http.server -d build/emu`.

Every push to `main` runs `.github/workflows/site.yml` (after JIANT's): the tests, the preview and, when JieLi's toolchain
downloads on the runner, the firmware file and the web installer, as a private download on the run's page. Publishing
them on GitHub Pages is a separate, manual step (Run workflow, "publish"): a Pages site is public.

## Credits

cursed-os stands on other people's work. Thank you.

- **[JIANT FM](https://github.com/juanjiant-bit/JIANT)** by Juan Jiant: the base of cursed-os. Its engines, sequencer,
  songs, MIDI, storage and its own interface are JIANT's; cursed-os is a fork of JIANT V10.19 and keeps its history.
  JIANT's own README is in [docs/JIANT-README.md](docs/JIANT-README.md).
- **[Felucca](https://github.com/hugelton/Felucca)** by Leo Kuroshita (Hügelton Instruments): the platform JIANT is built
  on (hardware layer, USB, update loader, installer, storage).
- **[SLOOP](https://github.com/isod89/sloop-fm1)**: JIANT's song system.
- **[X0X](https://github.com/charlesvestal/fm1-x0x)** by Charles Vestal: the groovebox layout cursed-os follows (the
  home grid of parts, one row of four knobs, the steps along the foot), and the browser emulator JIANT's is based on.
  No X0X code is in cursed-os yet; if its engines are ported, they will be credited here and in LICENSING.md.
- **[HYP-1](https://github.com/RustyPawz/fm1-HYP1)** by RedtailWorks: the idea of the keys setting the song's key.
  Written fresh for cursed-os, no HYP-1 code.
- **Fonts**: Press Start 2P (The Press Start 2P Project Authors) and Jersey 10 (The Soft Type Project Authors), both
  SIL OFL 1.1.

## Licence

GPL-3.0-only, like JIANT and Felucca. See [LICENSE](LICENSE) and [LICENSING.md](LICENSING.md) for the parts with their
own licences.

M-VAVE and FM-1 are trademarks of their owners. cursed-os is not affiliated with or endorsed by M-VAVE, Hügelton
Instruments, JIANT FM's author, X0X's or HYP-1's.
