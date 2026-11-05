# cursed-os: to do

Planned work. Done items move to the commit log.

## When the FM-1 arrives

- **CPU on the device.** The host numbers (tests/cpu_vg.py, `cos/*` sections) are relative only. On the FM-1, run
  GLO > SYSTEM and watch CPU / PEAK / LATE while playing the worst case: all five tracks, the master COMP on with
  SC HPF and MIX, eight FX pad effects held, a BREAKDOWN building, RUMBLE on. Host figures (instructions a sample,
  x86): CPU TEST song 3072, + COMP 3267, + all eight FX 3857, + TAPE STOP 3390, + RUMBLE 3450. One effect alone:
  +1 .. +6 %.
- **The install path.** The install guide (the "Installing cursed-os on the FM-1" doc) is checked against JIANT's
  tools only: try it on the device, including "Return to official V15" from cursed-os.
- **By ear.** CEILING's -6 .. -18 dBFS against the CDJ channel's gain (a loud enough peak with headroom), the risers' and kills' levels, DRIVE, the gates, TAPE STOP's brake, DUB STAB in the mix, RUMBLE's
  LEVEL range and DUCK curve against a Syntakt kick.

## Follow-ups

- **An Octatrack leading.** Its crossfader's CC on MO CC (check which CC it sends on its auto channel), the clock
  through the Syntakt's MIDI out / thru to the FM-1, the header's tempo against the Octatrack's.

- **A steeper ISOLATOR.** Its knobs ride JIANT's master EQ, a one-pole split: at its far left LOW and MID cut their
  band by about 9 dB, HIGH by 6 (the dial shows the measured value, tests/cos_acc_test.c). A 24 dB/octave split (as
  the black-key kills, cosfx.c) would reach a real -12 .. kill, at about the kills' CPU; JIANT's FX > EQ unchanged.

- **Set list with unsaved sections.** A second press drops them; keeping them needs either a flash write during audio
  (it stalls the audio) or RAM for another song's four sections (15 KB). Revisit if the device has the RAM.
- **LINK 2 with the Syntakt.** The maps match its manual's appendix B (TRACK 74 75 84 85, FX 70 71 24 30). On the
  device: that the delay's and reverb's CCs (24, 30) answer on the FX channel (the appendix names none), the
  Syntakt sending and receiving CCs in its MIDI config. OUT needs a USB host or Bluetooth MIDI between the two (the
  FM-1's jack is MIDI in only).
