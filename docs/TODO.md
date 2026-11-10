# cursed-os: to do

Planned work. Done items move to the commit log.

## When the FM-1 arrives

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
