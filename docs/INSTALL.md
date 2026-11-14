# Installing cursed os

cursed os is installed over USB, from a computer, in one of two ways: the web installer in Chrome or Edge, or a
command-line tool. Both write the same package (`cursed-os-X.Y.Z.fwsc`), and both can put the official firmware back.

> Installing custom firmware is at your own risk. cursed os is a hobby project, not affiliated with M-VAVE. It runs on
> the author's own FM-1, but it has not been tested on many units. Back up first.

## What is in the download

| File or folder | What it is |
| --- | --- |
| `cursed-os-X.Y.Z.fwsc` | The firmware package |
| `installer/` | The web installer and the sound editor, to run from your own computer |
| `tools/fm1_install.py` | The command-line installer (USB-MIDI) |
| `tools/fm1_uboot.py` | Recovery through the chip's boot mode, for an FM-1 that no longer takes an update over USB-MIDI |
| `tools/requirements.txt` | The Python packages the tools need |
| `emulator/` | The browser emulator: the same firmware, played with your computer's keyboard |
| `docs/` | The guide (quick start, cheat sheet, every mode) and this page |
| `CHANGELOG.md` | What changed, version by version |
| `LICENSE`, `LICENSING.md`, `LICENSES/` | The licences |
| `SHA256SUMS` | Checksums of the files |

## Before you start

1. **Back up.** In the web installer, "Return to official V15" saves a complete backup first: the songs, the saved
   projects, the sound banks and the settings. Keep that `.json` file.
2. **Coming from another custom firmware** (not official V15, JIANT FM or cursed os)? Return to official V15 first,
   then install cursed os. Writing straight over another firmware can leave the FM-1 unable to start.
3. **Get the official firmware file** (`FM-1.fwsc`, V15) from M-VAVE and keep it. It is not part of this download.
4. Use a USB data cable, and keep the FM-1 charged.

## Option 1: the web installer

The web installer needs Web MIDI, so it works in Chrome or Edge, from `localhost` or a secure (https) page.

```sh
cd installer
python3 -m http.server 8000
```

1. Open <http://localhost:8000/webapp/installer/> and connect the FM-1 by USB.
2. Pick the version (the newest is on top) and press Install. Allow MIDI access when the browser asks.
3. Wait until the FM-1 restarts and the page reports the new version. Don't unplug it while it writes.

The installer checks the connected FM-1 first and writes only over official V15, JIANT FM or cursed os. A running
cursed os answers over USB as `FM-1_420` (0.18.4 and older answered `FM-1_909`).

## Option 2: the command line

```sh
pip3 install -r tools/requirements.txt          # mido, python-rtmidi (and pyusb for the recovery tool)
python3 tools/fm1_install.py --info             # which firmware the connected FM-1 runs
python3 tools/fm1_install.py cursed-os-X.Y.Z.fwsc
```

If an install was cut off, run the same command again: the FM-1 stays in update mode and the install finishes the
write.

## Back to the official firmware

- **Web installer:** "Return to official V15", with the `FM-1.fwsc` you downloaded from M-VAVE. It checks the file
  (its size and SHA-256) before it writes, and nothing is uploaded anywhere.
- **Command line:** `python3 tools/fm1_install.py FM-1.fwsc`

## Recovery (boot mode)

If the FM-1 no longer takes an update over USB-MIDI, `tools/fm1_uboot.py` writes the package through the chip's own
boot mode. It reads the whole flash into a backup file first, then writes only the app area. It needs JieLi's loader
blob `wl82loader.bin` (from [jl-uboot-tool](https://github.com/kagaimiq/jl-uboot-tool), not part of this download) and
libusb. On macOS it has to run as root:

```sh
sudo "$(which python3)" tools/fm1_uboot.py --loader wl82loader.bin info
sudo "$(which python3)" tools/fm1_uboot.py --loader wl82loader.bin write cursed-os-X.Y.Z.fwsc
```

The FM-1 goes into boot mode by itself, or hold OCT− and OCT+ for 5 seconds. If something fails before a write, the
FM-1 restarts its firmware after a few seconds. If a write fails, it stays in boot mode: run the same command again.

## Try it first in the browser

The emulator runs the same firmware compiled to WebAssembly, with sound:

```sh
cd emulator
python3 -m http.server 8001
```

Open <http://localhost:8001>, click to start, and play with the keyboard (the key map is in the
[guide's cheat sheet](GUIDE.md#browser-emulator-keyboard)).
