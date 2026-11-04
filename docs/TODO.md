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
- **By ear.** The risers' and kills' levels, DRIVE, the gates, TAPE STOP's brake, DUB STAB in the mix, RUMBLE's
  LEVEL range and DUCK curve against a Syntakt kick.

## Follow-ups

- **Set list with unsaved sections.** A second press drops them; keeping them needs either a flash write during audio
  (it stalls the audio) or RAM for another song's four sections (15 KB). Revisit if the device has the RAM.
- **LINK 2 with the Syntakt.** Check which CCs the Syntakt's track or FX parameters answer to on its auto channel and
  set M1 CC to them; OUT needs a USB host or Bluetooth MIDI between the two (the FM-1's jack is MIDI in only).
