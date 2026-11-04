# cursed-os: to do

Planned work, roughly in order. Done items move to the commit log.

## When the FM-1 arrives

- **CPU on the device.** The host numbers (tests/cpu_vg.py, `cos/*` sections) are relative only. On the FM-1, run
  GLO > SYSTEM and watch CPU / PEAK / LATE while playing the worst case: all five tracks, the master COMP on with
  SC HPF and MIX, eight FX pad effects held, a BREAKDOWN building. Host figures (instructions a sample, x86):
  CPU TEST song 3072, + COMP 3267, + all eight FX 3857, + TAPE STOP 3390. One effect alone: +1 .. +6 %.
- **Flashing and recovery guide.** Step by step: install cursed-os, go back to the stock firmware or JIANT, what to do
  if it does not start (Felucca's loader, the safe mode). Check it against JIANT's installer.

## Live

- **DJ isolator EQ.** Low / mid / high kills or knobs on JIANT's master EQ (eq_v), next to the FX pad's LOW KILL.
- **Booth lights.** LEDs flash on the beat and show the BREAKDOWN state (armed, break, build bars, drop).
- **Tap tempo.** Alongside the nudge (HOME + OCT), for matching a CDJ or vinyl by ear.
- **Set list.** Load the next project on a bar line without a gap in the sound.

## Bigger ideas

- **MIDI out for visuals.** A note or CC on BREAKDOWN, BUILD, DROP and FILL, so the VJ box can cut on the drop.
- **Performance recording.** Knob moves and FX presses recorded into a section, replayable and saved.
- **Dub tools.** A dub delay page (tape echo, a feedback "spin", a filter in the echo path); chord stabs that follow
  the delay's rhythm.
