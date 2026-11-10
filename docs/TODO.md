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

- **A MIDI keyboard in Ableton.** The keys (and the knobs as CCs) on USB-MIDI to Live while the FM-1 plays or not:
  a mode where the keys play the computer only (the local sound off), the octave and the channel picked on the device.
- **Multi-track recording in Ableton (if possible).** USB audio carries the master as one stereo pair now. Separate
  tracks need more channels on the USB audio interface (e.g. 4 or 6 stereo pairs, one a track or a bus) and the CPU
  and USB bandwidth for them: check what the chip's USB audio can declare before promising it.
- **Tempo and key synced to Ableton.** The tempo: the FM-1 follows Live's MIDI clock (and start / stop / song
  position); the key: Live has no key message, so a CC or program change map (a Max for Live device or a MIDI clip
  sending the root and the scale), or the FM-1 sending its key to Live.
- **A sampler over Bluetooth.** Samples sent from the phone or computer over Bluetooth into the flash (the BLE
  stack is in the Bluetooth segment already); a SAMPLER engine to play them. Check the flash room and the transfer
  speed (BLE: slow, ~10..30 KB/s).
- **Phone control over Bluetooth MIDI.** BLE-MIDI in and out exists (V10.13: "JIANT FM-1"); what is left: a CC map
  of cursed-os's pages (the knobs, the FX pad, the mutes) and an app or a TouchOSC / Lemur layout for it.
- **Words from the phone.** Custom text typed on the phone sent to VOICE over Bluetooth (BLE-MIDI SysEx, or a GATT
  text characteristic), spoken through VOICE's English dictionary / letter rules.

## VOICE (GG, 2026-11-10)

- **More robotic: the Portal (GLaDOS) voice.** Hard pitch quantization (every syllable snapped to the scale, no glide:
  the auto-tune sound), a flat monotone option, the formants shifted up, a metallic ring (a short comb / ring mod),
  clipped consonants. A VOICE STYLE knob: HUMAN / ROBOT / GLADOS.
- **A talk box synth.** A synth (a saw / square chord from the track or another track) shaped by VOICE's formant
  filters, the words moving the vowels: the talk box / vocoder sound (Zapp, Daft Punk).
