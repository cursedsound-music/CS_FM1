# Changelog

cursed os versions, newest first. The full detail of each one is in the git history (`git log`).

## 0.20.0

Live performance.

- Keys by name and Camelot together, everywhere: `AMIN/8A` on the KEY page and at the top of every page. KEY's new
  knobs turn through the keys by NAME or by CAMELOT (1A … 12B, root and minor/major together).
- If the external clock stops while playing, the FM-1 keeps going at the last tempo (CLOCK LOST) and follows the clock
  again when it returns (CLOCK BACK). Before, it stopped after half a second.
- GLO > STAGE: LIVE LOCK (no new project, emptied track or erased slot by accident), PHRASE (16 / 32 bars), CHANGES (on
  the bar or the phrase), MIX BARS. GLO > STAGE 2: MONO BASS (80 / 120 / 160 Hz).
- HOME: DOWNBEAT (HOME + PLAY), MIX IN / OUT (HOME + black 7 / 8), BASS SWAP (HOME + black 9), the phrase's bar on the
  bottom line.
- Channel 16: CCs 103–107 for DOWNBEAT, MIX IN, MIX OUT, BASS SWAP and the Camelot key, so a Syntakt MIDI track can
  run them.
- Fixed: a key change waiting for its bar didn't land while the IDLE screen was up.
- Settings, projects and sounds from 0.19.x load unchanged; the new settings start as before (lock off, changes on the
  bar, mono bass off).

## 0.19.0

The FM-1 as an extra voice for an Elektron Syntakt, with sounds the Syntakt has no machine for.

- MODAL: struck sounds in the ADD engine (new modes MARIMBA, KALIMBA, STEEL, TUBE, LOG, GLASS). Each mode rings out as
  its material damps it, the mallet hit is heard, and velocity makes it harder and brighter. Eight new presets:
  ROSEWOOD, KALIMBA, STEEL PAN, TUBULAR, LOG DRUM, WINE GLASS, SING BOWL, GAMELAN. ADD's older modes and sounds are
  unchanged.
- The voice set: 32 sounds in eight groups (keys, bells, mallets, strings, textures, voices, pads, nature) that a
  Syntakt MIDI track picks with its program change.
- GLO > SYNTAKT 2: PROGRAMS (VOICES, ENGINE or LIST), FOLLOW (a track the Syntakt plays goes quiet on its own steps),
  HOLD (notes the Syntakt holds keep their voices) and TOUCH (velocity and aftertouch open the brightness). Turning
  SYNTAKT on sets VOICES, FOLLOW, HOLD and TOUCH LOW until you change them.
- Hand a track back by hand: on GLO > SYNTAKT 2, white 1–6 switch a track between the Syntakt (its MIDI plays it)
  and the FM-1 (its own sequencer, the Syntakt's notes, programs and CCs on that channel ignored).
- A program change and the note right after it: the note now waits for the new sound to load (up to about 100 ms),
  so it no longer plays the old sound.
- SAVE > START: a VOICES style (a mallet line, chords on a pad or a choir, a bell's motif).
- The guide: the Syntakt MIDI track setup, the CCs the FM-1 follows and the voice set.
- Saved projects, sounds, kits, favourites and settings from 0.18.x load unchanged.

## 0.18.5

- cursed os has its own identity, `FM-1_420`, instead of JIANT's `FM-1_909`. Nothing else changes for an install: the update loader and the flash layout are the same, and both
  installers accept it.
- `fmhub.json` for The Firmware Hub's card.
- LICENSING.md: how cursed os keeps Erica Synths' terms for the LXR firmware's transient samples.

## 0.18.4

- Sound: the master's dither is one draw for both sides, so a mono sound stays exactly mono.
- With the SLICER on its buses and FX ON, the reverb's stereo sides go through it too, so the PLATE reverb's tail is
  cut with the rest (it rang on before).
- MIDI: with more than 96 notes held at once (a sustain pedal, several channels), the oldest is now let go
  properly; before, its note-off could be lost and the note hung (since 0.15.2).
- TRIG > COND: the MOVE action shows STP after its value, which had been cut off on the card.
- The sound editor (in the installer) reads cursed os's own tables: KICK, DUB, FMDRUM and ACID with their sounds, the
  engine order, PLATE and TIGHT, the dotted delay times. Every engine has its name, so the editor can pick ACID.
- The host tests all pass again (`tests/run_tests.sh`): the tests were brought up to date with cursed os's engines,
  power-on sounds, pages and storage, and a few test helpers that missed newer state (the PLATE's lines, the limiter's
  look-ahead) now reset it. The golden renders and CPU baselines are taken again for cursed os's sound. GitHub runs the
  whole suite on every push.
- Licensing: cursedsound's own files are also offered under the MIT licence (`LICENSING.md`). The credits are
  complete, and the web installer carries cursed os's name, links and credits.
- Docs: one guide (`docs/GUIDE.md`, with the quick start, the cheat sheet and every mode), an install page
  (`docs/INSTALL.md`) and a changelog (`CHANGELOG.md`). Releases are built by `.github/workflows/release.yml`.

## 0.18

- 0.18.3: long held chords (GEN > DUB LENGTH 1/2 to 4 BARS), held atmospheres, KEY LATCH (SCL + OCT+).
- 0.18.2: STYLE RAND on SAVE > START, KEEP (white 1–6 on START), and new loops that land at the end of the bar.
- 0.18.1: complete loops on SAVE > START (ELEMENTS ALL / PICKED, BARS 1 / 2 / 4); SAT on by default.
- 0.18.0: a clean master with dither, SAT when you want it (GLO > LIMIT), quick starts, undo for GEN, cubic
  upsampling for the drums.

## 0.17

- 0.17.0: GLO > SYNTAKT, one knob to follow an Elektron Syntakt (clock, start / stop, patterns as sections).

## 0.16

- 0.16.1: BLEEP in a modular style (motifs, drift, space; sonar, drops, wood, glass).
- 0.16.0: SIMPLE and DEEP pages (a mode's button walks the everyday pages, SELECT all of them); the KICK's crackle gone.

## 0.15

- 0.15.3: HOME held, the keys light as things sound.
- 0.15.2: more room in RAM.
- 0.15.1: bleeps and bloops (GEN > BLEEP, DUB's BLEEP sounds).
- 0.15.0: DUB, dub chord stabs and atmospheres; Bluetooth shows the FM-1 as "cursed os".

## 0.14

- 0.14.1: the kit's PERC, a 16th grid, PAN and CHANCE, clean sends, a steady external tempo.
- 0.14.0: TRIG > COND: trig conditions, parameter slides, sound locks.

## 0.13

- 0.13.1: DIST RING, REVERB TIGHT, the master COMP and limiter at full resolution.
- 0.13.0: LFO > DRUMS, SAVE > KITS, the knobs as a controller for Ableton, a track's SOUND EXT; a cleaner kick and
  tom, the FM kit sounds' RATIO, WAVE, S.TIME and T.PITCH.

## 0.12

- 0.12.5: BACK TO NORMAL lets the delay and reverb ring out.
- 0.12.4: a step's length (steps held + black 3 / black 4).
- 0.12.3: the ACID's sends in its EDIT pages.
- 0.12.2: the limiter no longer rides the kick.
- 0.12.1: any drum sound into any drum of the kit.
- 0.12.0: FMDRUM sounds inside the DRUM kit.

## 0.11

- 0.11.1: transient samples for FMDRUM.
- 0.11.0: FMDRUM, transients, DIST TUBE and RAT.

## 0.10

- 0.10.4: the keys show what plays.
- 0.10.3: HOME > LIVE and BACK TO NORMAL.
- 0.10.2: ALGORITHM picks the engine and PRESETS the sound inside it; your own sounds; your own LFO shapes.
- 0.10.1: the tempo in hundredths, a FINE knob.
- 0.10.0: a KICK track, the drums in and out, eight songs again.

## 0.9

- 0.9.5 – 0.9.9: the phone remote (NUDGE, BPM steps, an exact BPM) and the Bluetooth sampler; the volume knob always last.
- 0.9.4: VOICE STYLE (ROBOT, GLADOS, TALKBOX); GLO > MIDI > LOCAL OFF.
- 0.9.3: PLATE, a stereo plate reverb (the default); SAVE: REC twice starts a new project.
- 0.9.2: stability fixes; scope, spectrum, stereo and loudness meters after the limiter.
- 0.9.1: an oscilloscope and a spectrum on GLO.
- 0.9.0: the FX pad's keys lit while their effects are on.

## 0.8

- 0.8.9: the keys lit in the song's scale.
- 0.8.3 – 0.8.8: the battery in percent, an IDLE screen and its pictures.
- 0.8.2: a FILTER page in every engine's EDIT.
- 0.8.1: FX opens on DJ: the DJ FILTER and the ISOLATOR.
- 0.8.0: each drum its own sound, envelope and sends; 32 save slots with names.

## 0.7

- 0.7.4: ACID GEN's ROTATE.
- 0.7.3: any plausible flash part is taken, so updates stay open.
- 0.7.1 – 0.7.2: SEQ's three ways (TRIG, STEP, EUCLID); the FX pad's last three heard at once.
- 0.7: SAVE on any flash part, step input, ACID's DEVIL page.

## 0.1 – 0.6

- 0.6: the ISOLATOR as a DJ mixer's; SEQ held + REC twice empties the track; notes, accents and slides by step.
- 0.5: every knob checked for what it does; the delay's TIME by length.
- 0.4: parameter locks on SEQ, the VOICE track's WORDS page.
- 0.3: DRONE, MIDI routing for the Syntakt.
- 0.2: the keys set the song's key (after HYP-1).
- 0.1: an Elektron-style interface over JIANT FM's engines, and the ACID generator.
