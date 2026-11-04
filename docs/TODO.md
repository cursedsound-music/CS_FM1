# cursed-os: to do

Planned work. Done items move to the commit log.

## When the FM-1 arrives

- **CPU on the device.** The host numbers (tests/cpu_vg.py, `cos/*` sections) are relative only. On the FM-1, run
  GLO > SYSTEM and watch CPU / PEAK / LATE while playing the worst case: all five tracks, the master COMP on with
  SC HPF and MIX, eight FX pad effects held, a BREAKDOWN building. Host figures (instructions a sample, x86):
  CPU TEST song 3072, + COMP 3267, + all eight FX 3857, + TAPE STOP 3390. One effect alone: +1 .. +6 %.
- **The install path.** The install guide (the "Installing cursed-os on the FM-1" doc) is checked against JIANT's
  tools only: try it on the device, including "Return to official V15" from cursed-os.
- **By ear.** The risers' and kills' levels, DRIVE, the gates, TAPE STOP's brake, DUB STAB in the mix.

## Follow-ups

- **FX lane: perform.c's effects.** REPEAT, GLITCH, LPF / HPF sweeps, the mutes and LOOPs are not recorded into the
  FX lane yet (only cosfx.c's own effects).
- **Set list while playing with unsaved sections.** Today a song change waits for STOP when stored sections are not
  written yet; writing them first while playing needs flash writes during audio.
