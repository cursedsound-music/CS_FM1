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

The FM-1 plays extra voices next to the Syntakt (an acid line, a few drums, bleeps): the Syntakt leads the tempo, PLAY /
STOP and the patterns; the FM-1 follows.

**Cables**
- MIDI: Syntakt **MIDI OUT** -> a DIN-to-3.5 mm TRS MIDI adapter -> the FM-1's TRS MIDI IN.
- Audio: FM-1 out -> Syntakt **INPUT L/R** (3.5 mm stereo to two 6.3 mm mono jacks). The FM-1 then runs through the
  Syntakt's FX block, delay and reverb, and the Syntakt's main out is one channel on the DJ mixer.

**On the FM-1**: GLO > SYNTAKT, turn KNOB 1 to **ON**. That sets CLOCK TRS, follows the Syntakt's patterns on channel 15,
sends nothing back and turns the leveler (AUTO LV, KNOB 4) off; it stays on after a power-off. The page shows what comes in: CABLE, the CLOCK's BPM, PLAYING,
the last PATTERN and the section it plays.

**On the Syntakt** (SETTINGS > MIDI CONFIG):
- SYNC: CLOCK SEND, TRANSPORT SEND and PRG CH SEND on.
- CHANNELS: PROGRAM CHG OUT CH 15 (or set the FM-1's PC CH, KNOB 4, to the one you use).
- PORT CONFIG: OUT PORT FUNC MIDI.
- SETTINGS > AUDIO ROUTING > EXTERNAL IN: stereo. FUNC + FX, trig 16: the input through the FX block (pre) or straight
  to the main mix (post).

**Patterns and sections**: the Syntakt's pattern picks the FM-1's section, its number % 4 (A01 A, A02 B, A03 C, A04 D,
A05 A ...). Set PAT BARS (KNOB 3) to the Syntakt pattern's length in bars: the FM-1 switches when the Syntakt does, at
the end of its pattern. So a jam: build A on both, store it (SAVE + OCT+), then B ...; SONG REC records the order you
play them in as a song.

**A quick start**: SAVE > START, pick a STYLE, DRUMS (PERC by default with SYNTAKT ON: the Syntakt plays the kick),
ELEMENTS (ALL: a complete loop on every track; PICKED: a few voices) and BARS; OCT+ loads it, again another; REC on the
page goes back. STYLE RAND draws every part's role and sound: happy accidents. White keys 1-6 on the page KEEP tracks, so
OCT+ rerolls only the rest. Playing, a new loop lands at the end of the bar (the Syntakt's pattern with SYNTAKT ON).

**In a set**: dial the Syntakt's BPM to the CDJ track and nudge it; the FM-1 follows. With the Syntakt's kick, the
FM-1's own KICK track can stay muted (HOME held + white 1).

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
  home grid of parts, one row of four knobs, the steps along the foot), the browser emulator JIANT's is based on, and
  the ACID engine's 303 voice: X0X's port (`firmware/src/x0x/bass303.c`) of **[Open303](https://github.com/RobinSchmidt/Open303)**
  by Robin Schmidt (MIT), with the Devilfish ranges after jc303 (midilab), the RAT drive after dm-Rat (Dave Mollen) and
  the speed-ups after Optimist (henri / hdavid).
- **[HYP-1](https://github.com/RustyPawz/fm1-HYP1)** by RedtailWorks: the idea of the keys setting the song's key.
  Written fresh for cursed-os, no HYP-1 code.
- **Font**: Press Start 2P (The Press Start 2P Project Authors), SIL OFL 1.1.

## Licence

GPL-3.0-only, like JIANT and Felucca. See [LICENSE](LICENSE) and [LICENSING.md](LICENSING.md) for the parts with their
own licences.

M-VAVE and FM-1 are trademarks of their owners. cursed-os is not affiliated with or endorsed by M-VAVE, Hügelton
Instruments, JIANT FM's author, X0X's or HYP-1's.
