# cursed-os: to do

Planned work. Done items move to the commit log.

## When the FM-1 arrives

- **The Syntakt link (0.17.0) on the cables.** GLO > SYNTAKT ON: CABLE OK, the BPM, PLAY / STOP, a pattern change
  landing with the Syntakt's at PAT BARS 1 and 4 (if its PRG CH SEND goes out when the pattern changes rather than when
  it is picked, PAT BARS 1 is right); the FM-1's level into the Syntakt's INPUT (no clipping at CEILING -6).

- **CPU on the device.** The host numbers (tests/cpu_vg.py, `cos/*` sections) are relative only. On the FM-1, run
  GLO > SYSTEM and watch CPU / PEAK / LATE while playing the worst case: all five tracks, the master COMP on with
  SC HPF and MIX, the ISOLATOR in use (LOW -12), eight FX pad effects held, a BREAKDOWN building, RUMBLE on. Host figures (instructions a sample,
  x86): CPU TEST song 3072, + COMP 3267, + all eight FX 3857, + TAPE STOP 3390, + RUMBLE 3450, + the ISOLATOR 3617. One effect alone:
  +1 .. +6 %.
- **The install path.** The install guide (the "Installing cursed-os on the FM-1" doc) is checked against JIANT's
  tools only: try it on the device, including "Return to official V15" from cursed-os.
- **By ear.** CEILING's -6 .. -18 dBFS against the CDJ channel's gain (a loud enough peak with headroom), the risers' and kills' levels, DRIVE, the gates, TAPE STOP's brake, DUB STAB in the mix, RUMBLE's
  LEVEL range and DUCK curve against a Syntakt kick.

## Follow-ups

- **An Octatrack leading.** Its crossfader's CC on MO CC (check which CC it sends on its auto channel), the clock
  through the Syntakt's MIDI out / thru to the FM-1, the header's tempo against the Octatrack's.

- **Set list with unsaved sections.** A second press drops them; keeping them needs either a flash write during audio
  (it stalls the audio) or RAM for another song's four sections (15 KB). Revisit if the device has the RAM.
- **LINK 2 with the Syntakt.** The maps match its manual's appendix B (TRACK 74 75 84 85, FX 70 71 24 30). On the
  device: that the delay's and reverb's CCs (24, 30) answer on the FX channel (the appendix names none), the
  Syntakt sending and receiving CCs in its MIDI config. OUT needs a USB host or Bluetooth MIDI between the two (the
  FM-1's jack is MIDI in only).

## Ableton and Bluetooth (GG, 2026-11-10)

- ~~A MIDI keyboard in Ableton~~ (0.9.4): GLO > MIDI > LOCAL OFF: the keys send on the track's channel over USB and
  sound nothing here. Left: the knobs as CCs to Live.
- **Multi-track recording in Ableton (if possible).** USB audio carries the master as one stereo pair now. Separate
  tracks need more channels on the USB audio interface (e.g. 4 or 6 stereo pairs, one a track or a bus) and the CPU
  and USB bandwidth for them: check what the chip's USB audio can declare before promising it.
- **Tempo and key synced to Ableton.** The tempo works now: GLO > MIDI > CLOCK USB, and in Live the FM-1 as a
  clock destination (Preferences > Link/Tempo/MIDI: its output port's Sync on); START / STOP follow. Left: the key
  (Live has no key message: a CC or program change map from a Max for Live device, or the FM-1 sending its key).
- ~~A sampler over Bluetooth~~ (0.9.5, gone in 0.10.0): 11 s in the flash of songs 5..8 was too little to be useful
  (GG); songs 5..8 back. What could use that room instead (wavetables, granular, a one-shot kit) is below.
- ~~Phone control over Bluetooth MIDI~~ (0.9.5): channel 16's CC map (cos_remote.c) and tools/cursed_remote.py.
  Left: try it on GG's iPhone (written blind: the CoreMIDI and Bluetooth window code untested), a TouchOSC layout.
- ~~Words from the phone~~ (0.9.5): SysEx TEXT into the VOICE track's lyric.
- Recording what another app plays on the iPhone: iOS doesn't allow it (Files or the mic instead).
- Ideas for flash room (GG to pick, 2026-11-10): wavetables from the phone for a WAVETABLE engine; granular textures
  from a short sound; a one-shot kit of own hits; a VOICE phrase library.

## Live (GG, 2026-11-11)

- ~~One page for a set's moves, and back to normal for a drop~~ (0.10.3): HOME > LIVE (DJ FILTER, global REVERB and
  DELAY sends, FEEDBACK; white 1..8 the drums, black 1..6 the tracks), OCT- BACK TO NORMAL on the bar, OCT+ this is
  normal; the phone's LIVE tab (CC 99..102). (GG asked HOME + OCT-: that is the NUDGE.)

## Sounds (GG, 2026-11-11)

- ~~Engines and presets on two knobs~~ (0.10.2): ALGO the engine, PRESETS inside it; EDIT + ALGO the track.
- ~~Own presets~~ (0.10.2): SAVE > SOUNDS (U01..U31, OCT+ save, OCT- load, REC x2 erase); from the phone: naming,
  later.
- ~~Own LFO shapes from the phone~~ (0.10.2): USER1..4, 32 points, U32's record. Left: LFO 2 and the autopan with them?

## 0.13.0 (GG, 2026-11-11: "do all 5 of them", "remove the drone")

- ~~DRONE~~ removed; LFO > DRUMS: rolls (white keys held), REPEAT (black 1..4 held), CHANCE per drum (project 'DC')
- ~~User kits~~: SAVE > KITS K01..K16 (kits.c, flash 0xDA000 / 0xDB000)
- ~~Ableton controller~~: GLO > MIDI 2 KNOBS FM-1 / +MIDI / MIDI (relative), CC 20 + page x 4 + knob on CTRL CH + mode
- ~~ACID to a VST~~: SOUND EXT per track (out of the mix, notes still out; accents 127, slides legato)
- ~~Cleaner drums~~: the kick and the tom upsampled by a 4-point half-band (images -3 .. -6 dB at 14..22 kHz; the rest of
  the hash is the sound's own CLICK and drive)
- ~~FMDRUM more in the kit~~: FM sounds' S.TIME, RATIO, WAVE (their LAYER byte); T.PITCH on FMDRUM / KICK (P_WRDEP)
- Left: a per-sound transient pitch for FM kit sounds (no byte left there); CPU baseline rewritten (the limiter's
  look-ahead since 0.12.2: +46 instructions a sample idle)

## Kick noise (GG, 2026-11-11)

- ~~"Noise mixed with aliasing" on the kicks, not on the other drums~~ (0.12.2): the master LIMITER rode the kick's
  waveform (its envelope a step behind |x|, the release running between the kick's cycles; since 0.9.9 it always gets
  the full level, MASTER after it). Now a 32-sample look-ahead, the peak held 27 ms, the gain smoothed: -35 -> -43 dB of
  hash at LIMIT -18, -39 -> -50 at -12. COMP's detector holds its peak 23 ms the same way. Left: DRUM-X runs at half
  rate with a linear upsampler (images near 22 kHz, ~-62 dB on a kick's body); its drive and CLICK (noise) are the
  sound's own.

## A drum synth's voices (GG, 2026-11-11)

- ~~Engines and effects after the LXR drum machine~~ (0.11.0): FMDRUM (engine 9: DRUM / SNARE / CYMBAL / HAT by FM, a
  pitch sweep, noise; 10 presets), the TRANSIENT layer on FMDRUM and KICK (SNAP and the LXR's twelve transient
  samples, GG 2026-11-11: xr_tables.h), DIST TUBE (ECC83) and RAT (op-amp + 1N914) from its curves. Left: a
  transient PITCH knob (the LXR plays them faster / slower)? Left: a hi-hat choke between two FMDRUM tracks? ~~FMDRUM lanes inside the DRUM kit~~ (0.12.0: FILTER FM on a kit sound, drumx_voice.c dx_fmd). ~~Any drum sound into any drum~~ (0.12.1: EDIT > SOUND > SOURCE, 85 sounds).
- ~~More of the LXR (GG 2026-11-11: "RING MOD + LXR reverb")~~ (0.13.1): DIST RING (TONE its carrier, DIST the mix) and
  REVERB TIGHT (the LXR's two-loop tank, its early reflections in WIDE's slot: EARLY). Left: the LXR's ring
  modulator on the whole FX bus? GG to pick from the Elektron-style list: ~~conditions, parameter slide, sound locks~~ (0.14.0, TRIG > COND). Left:
  ratchet velocity fade, control all, an accent lane; a kit's COND per drum (today the whole step's).

## Drums live (GG, 2026-11-10)

- ~~A kick of its own~~ (0.10.0): engine 4 KICK (DRUM-X's kick, its own KIND), T2 at power-on; the kit's kick lane
  at 0. GEN: PATTERN.
- ~~The drums in one at a time, fast~~ (0.10.0): HOME + white 1..8 (on the beat / bar, MUTE Q), the phone's DRUMS page
  (CC 91..98).

- ~~The volume knob after the comp, the limiter and the meters~~ (0.9.9): MASTER always last (USB LEVEL FIXED for
  good): the headphones and the speaker only; the meters, the limiter and USB at the full level.

## VOICE (GG, 2026-11-10)

- ~~More robotic: the Portal (GLaDOS) voice~~ (0.9.4): EDIT > WORDS > STYLE ROBOT, GLADOS.
- ~~A talk box synth~~ (0.9.4): STYLE TALKBOX (a sawtooth, each voice of a chord its own, through the formants). Left:
  another track's sound as the talk box's source (a carrier bus).
