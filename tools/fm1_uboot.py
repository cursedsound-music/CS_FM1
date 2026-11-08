#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 cursedsound
"""Install a cursed-os / Felucca package (.fwsc) on an FM-1 through the chip's own boot mode (UBOOT), straight from
the computer's USB: for an FM-1 whose running firmware will not take an update over USB-MIDI (a cursed-os before
0.7.3 on a flash part it did not know: fm1_install.py says "stopped answering after 0 requests").

The FM-1's chip (JieLi AC791N, "WL80UBOOT1.00", USB 4C4A:8057) shows up as a USB mass-storage device in UBOOT and
takes vendor SCSI commands (16-byte CDBs, Bulk-Only Transport). The protocol and its values are FM-1 Transporter's
(github.com/kurogedelic/FM-1-transporter, MIT: src/jieli_uboot.c, docs/PROTOCOL.md), ported there from jl-uboot-tool
(github.com/kagaimiq/jl-uboot-tool, MIT); here it runs on the computer with libusb instead of through an RP2040:
  ROM:    WRITE_MEMORY (FB06) JieLi's wl82loader.bin to 0x1C02000 (jl-uboot-tool's data/loaderblobs/usb), JUMP (FB08)
  loader: READ_KEY (FC09), GET_ONLINE_DEVICE (FC0A: the flash id), READ_FLASH (FD05), ERASE_SECTOR (FB01),
          WRITE_FLASH (FB04), RUN_APP (FC0C)
What it does: the whole flash read into a backup file first; only [0x4000, 0x93000) written (the app area, as
Felucca's own loader writes it: the head, the SPL and the device data never), only the 4 KiB sectors that differ,
each read back; then the whole area read back once more and the FM-1 started.

  pip install pyusb mido python-rtmidi      (and libusb: brew install libusb)
  sudo "$(which python3)" fm1_uboot.py --loader wl82loader.bin info
  sudo "$(which python3)" fm1_uboot.py --loader wl82loader.bin write felucca.fwsc

sudo: macOS lets a program take a USB mass-storage device from its own driver only as root. The FM-1 goes into
UBOOT by itself (the USB-MIDI soft key, which cursed-os and stock V15 take), or hold OCT- + OCT+ 5 s. If anything
fails before a write, the FM-1 just restarts its firmware after a few seconds (the boot ROM's watchdog). If a write
fails, it stays in UBOOT: run the same command again. Do not unplug it while it writes. """
import argparse
import os
import struct
import sys
import time

VID, PID = 0x4C4A, 0x8057
LOADER_ADDR = 0x1C02000
LOADER_SIZE = 24064
LOADER_SHA256 = "d41da6126760c9d66660bcc0cac8d27d221806c5e369a8036921efe68dca5376"   # jl-uboot-tool main, wl82loader.bin
CHIP_KEY = 0x980F
FLASH_SIZE = 0x100000
WRITE_MIN, WRITE_END = 0x4000, 0x93000
SECTOR, IO = 0x1000, 512
CMD_WRITE_MEMORY, CMD_JUMP = 0xFB06, 0xFB08
CMD_ERASE_SECTOR, CMD_WRITE_FLASH, CMD_READ_FLASH = 0xFB01, 0xFB04, 0xFD05
CMD_READ_KEY, CMD_GET_ONLINE_DEVICE, CMD_RUN_APP, CMD_GET_USB_BUFF_SIZE = 0xFC09, 0xFC0A, 0xFC0C, 0xFC14
SOFT_KEY = [0xF0, 0x22, 0x24, 0x35, 0x7D, 0xF7]          # usb.c UBOOT_KEY (SysEx): the firmware enters UBOOT


class Fail(Exception):
    pass


def crc16(data, crc=0):                                   # CRC-16/XMODEM (jl_crc16)
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def crc_cipher(buf):                                      # jl_crc_cipher, key 0xFFFFFFFF
    magic = bytes([0xC3, 0xCF, 0xC0, 0xE8, 0xCE, 0xD2, 0xB0, 0xAE, 0xC4, 0xE3, 0xA3, 0xAC, 0xD3, 0xF1, 0xC1, 0xD6])
    crc = crc16(b"\xFF\xFF", 0xFFFF)
    out = bytearray(buf)
    for i in range(len(out)):
        crc = crc16(magic[i % 16:i % 16 + 1], crc)
        out[i] ^= crc & 0xFF
    return bytes(out)


def enc(buf, key=0xFFFF):                                 # the package's ENC stream cipher (fm1pkg_make.py enc)
    out = bytearray(buf)
    for i in range(len(out)):
        out[i] ^= key & 0xFF
        key = ((key << 1) ^ (0x1021 if key & 0x8000 else 0)) & 0xFFFF
    return bytes(out)


def package_flash(path):
    """flash.bin of a .fwsc (fm1pkg_make.py ufw): the first 20 0x2F-byte blocks each carry a marker byte (the
    identity); then an ENC'd header and entry list; entry type 0 is flash.bin, its CRC checked"""
    raw = open(path, "rb").read()
    logical, product = bytearray(), ""
    for i in range(20):
        logical += raw[i * 0x30:i * 0x30 + 0x2F]
        m = raw[i * 0x30 + 0x2F]
        if m != 0x7D:
            product += chr((m - i - 1) & 0xFF)
    logical += raw[20 * 0x30:]
    hdr = enc(logical[0:0x40])
    n = struct.unpack_from("<H", hdr, 8)[0]
    if not 1 <= n <= 8:
        raise Fail(f"{path}: not a .fwsc package (entries {n})")
    for i in range(n):
        e = enc(logical[0x40 + i * 0x50:0x40 + (i + 1) * 0x50])
        typ, _, dcrc, _, off, size = struct.unpack_from("<HHHHII", e, 0)
        if typ == 0:
            d = bytes(logical[off:off + size])
            if len(d) != size or crc16(d) != dcrc:
                raise Fail(f"{path}: flash.bin damaged (CRC)")
            if size != WRITE_END:
                raise Fail(f"{path}: flash.bin is {size:#x} bytes, expected {WRITE_END:#x}")
            return d, product
    raise Fail(f"{path}: no flash.bin in the package")


class Uboot:
    def __init__(self, dev):
        import usb.util
        self.dev, self.tag = dev, 0
        try:
            if dev.is_kernel_driver_active(0):
                dev.detach_kernel_driver(0)
        except Exception as e:                            # (not every libusb can ask; claiming may still work)
            print(f"  (kernel driver: {e})")
        try:
            dev.set_configuration()
        except Exception:
            pass
        usb.util.claim_interface(dev, 0)
        intf = dev.get_active_configuration()[(0, 0)]
        self.ep_out = usb.util.find_descriptor(intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT)
        self.ep_in = usb.util.find_descriptor(intf, custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN)
        if not self.ep_out or not self.ep_in:
            raise Fail("UBOOT device without bulk endpoints")
        self.chunk = IO

    def ready(self):
        """as macOS and FM-1 Transporter do before the first command: the LUN count, then INQUIRY until it answers"""
        try:
            self.dev.ctrl_transfer(0xA1, 0xFE, 0, 0, 1, 1000)
        except Exception:
            pass
        for _ in range(30):
            try:
                r = self.bot(bytes([0x12, 0, 0, 0, 36, 0]), data_in=36)
                print(f"  {r[8:16].decode(errors='replace').strip()} {r[16:32].decode(errors='replace').strip()} "
                      f"{r[32:36].decode(errors='replace').strip()}")
                return
            except Exception:
                time.sleep(0.3)
        raise Fail("the UBOOT device never answered INQUIRY")

    def bot(self, cdb, data_in=0, data_out=b"", timeout=5000):
        """one Bulk-Only Transport command: CBW, data, CSW"""
        import usb.core
        self.tag = (self.tag + 1) & 0xFFFFFFFF
        dlen = data_in or len(data_out)
        cbw = struct.pack("<IIIBBB", 0x43425355, self.tag, dlen, 0x80 if data_in else 0, 0, len(cdb)) + cdb.ljust(16, b"\0")
        self.ep_out.write(cbw, timeout)
        got = b""
        try:
            if data_in:
                got = bytes(self.ep_in.read(data_in, timeout))
            elif data_out:
                self.ep_out.write(data_out, timeout)
        except usb.core.USBError as e:
            if getattr(e, "errno", None) == 32 or "Pipe" in str(e):   # a stall: clear it, the CSW follows
                self.dev.clear_halt(self.ep_in if data_in else self.ep_out)
            else:
                raise
        csw = bytes(self.ep_in.read(13, timeout))
        if len(csw) != 13 or csw[:4] != b"USBS" or struct.unpack_from("<I", csw, 4)[0] != self.tag:
            raise Fail(f"bad CSW for {cdb[:2].hex()}")
        if csw[12] != 0:
            raise Fail(f"command {cdb[:2].hex()} failed (status {csw[12]})")
        return got

    @staticmethod
    def cdb(cmd, args=b""):
        c = bytearray(b"\xFF" * 16)
        c[0], c[1] = cmd >> 8, cmd & 0xFF
        c[2:2 + len(args)] = args
        return bytes(c)

    def cmd(self, cmd, args=b""):
        r = self.bot(self.cdb(cmd, args), data_in=16)
        if len(r) < 2 or (r[0] << 8 | r[1]) != cmd:
            raise Fail(f"command {cmd:04X} answered {r[:2].hex()}")
        return r

    def load(self, blob):
        for off in range(0, len(blob), IO):
            b = blob[off:off + IO]
            c = crc16(b)
            args = struct.pack(">IH", LOADER_ADDR + off, len(b)) + bytes([0, c & 0xFF, c >> 8])
            self.bot(self.cdb(CMD_WRITE_MEMORY, args), data_out=b)
        self.cmd(CMD_JUMP, struct.pack(">IH", LOADER_ADDR, 1))
        time.sleep(0.3)
        try:
            r = self.cmd(CMD_GET_USB_BUFF_SIZE)
            size = struct.unpack_from(">I", r, 2)[0]
            size = min(size, 4096)
            if size >= IO:
                self.chunk = size - size % IO
        except Fail:
            self.chunk = IO

    def info(self):
        r = self.cmd(CMD_READ_KEY, struct.pack(">I", 0x00AC6900))
        k = crc_cipher(bytes([r[2 + 5], r[2 + 4]]))
        key = k[0] | k[1] << 8
        r = self.cmd(CMD_GET_ONLINE_DEVICE)
        fid = struct.unpack_from("<I", r, 4)[0]
        return key, r[2], fid

    def read(self, addr, n, progress=None):
        out = bytearray()
        while len(out) < n:
            k = min(self.chunk, n - len(out))
            d = self.bot(self.cdb(CMD_READ_FLASH, struct.pack(">IH", addr + len(out), k)), data_in=k)
            if len(d) != k:
                raise Fail(f"flash read at {addr + len(out):#x}: {len(d)} of {k} bytes")
            out += d
            if progress:
                progress(len(out), n)
        return bytes(out)

    def write_sector(self, addr, data):
        if addr < WRITE_MIN or addr >= WRITE_END or addr % SECTOR or len(data) != SECTOR:
            raise Fail(f"refused: a write at {addr:#x}")
        self.cmd(CMD_ERASE_SECTOR, struct.pack(">I", addr))
        for off in range(0, SECTOR, IO):
            b = data[off:off + IO]
            c = crc16(b)
            args = struct.pack(">IH", addr + off, IO) + bytes([0, c & 0xFF, c >> 8])
            self.bot(self.cdb(CMD_WRITE_FLASH, args), data_out=b)
        if self.read(addr, SECTOR) != data:
            raise Fail(f"sector {addr:#x}: read back differs")

    def run_app(self):
        try:
            self.cmd(CMD_RUN_APP, struct.pack(">I", 1))
        except Exception:
            pass                                          # (it may restart before it answers)


def soft_key():
    """the FM-1 into UBOOT through its running firmware (USB-MIDI SysEx); False when no FM-1 port was found"""
    try:
        import mido
    except ImportError:
        return False
    for name in mido.get_output_names():
        if any(s in name.lower() for s in ("fm-1", "felucca", "jiant", "sinco", "usb-midi")):
            with mido.open_output(name) as o:
                o.send(mido.Message("sysex", data=SOFT_KEY[1:-1]))
            print(f"sent the UBOOT key to {name}")
            return True
    return False


def backend():
    """libusb, Homebrew's too (its /opt/homebrew/lib is not where ctypes looks, and sudo drops DYLD_* paths)"""
    try:
        import usb.backend.libusb1 as lb
    except ImportError:
        return None
    for path in ("/opt/homebrew/lib/libusb-1.0.dylib", "/usr/local/lib/libusb-1.0.dylib", None):
        if path is None or os.path.exists(path):
            b = lb.get_backend(find_library=(lambda _x, p=path: p) if path else None)
            if b is not None:
                return b
    return None


def find_uboot(wait):
    import usb.core
    be = backend()
    t = time.time()
    while time.time() - t < wait:
        d = usb.core.find(idVendor=VID, idProduct=PID, backend=be) if be else usb.core.find(idVendor=VID, idProduct=PID)
        if d is not None:
            return d
        time.sleep(0.05)
    return None


def connect(loader, wait=40):
    blob = open(loader, "rb").read()
    if len(blob) != LOADER_SIZE:
        raise Fail(f"{loader}: {len(blob)} bytes, wl82loader.bin is {LOADER_SIZE}")
    import hashlib
    if hashlib.sha256(blob).hexdigest() != LOADER_SHA256:
        print("warning: this wl82loader.bin is not the one FM-1 Transporter was checked with")
    dev = find_uboot(0.2)
    if dev is None:
        if not soft_key():
            print("no FM-1 found over USB-MIDI: hold OCT- + OCT+ on the FM-1 for 5 seconds now")
        print("waiting for the FM-1 in UBOOT (4C4A:8057) ...")
        dev = find_uboot(wait)
        if dev is None:
            raise Fail("no UBOOT device appeared")
    u = Uboot(dev)
    u.ready()
    print("UBOOT: loading the loader")
    u.load(blob)
    key, typ, fid = u.info()
    print(f"chip key {key:04X}, flash type {typ}, flash id {fid:06X}")
    if key != CHIP_KEY:
        raise Fail(f"chip key {key:04X}, not the FM-1's {CHIP_KEY:04X}: not writing")
    return u


def bar(k, n):
    sys.stdout.write(f"\r  {k * 100 // n:3d} %")
    sys.stdout.flush()
    if k == n:
        sys.stdout.write("\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--loader", required=True, help="wl82loader.bin (jl-uboot-tool data/loaderblobs/usb)")
    ap.add_argument("what", choices=["info", "backup", "write"])
    ap.add_argument("package", nargs="?", help="felucca.fwsc (write) / the backup file (backup)")
    ap.add_argument("--yes", action="store_true", help="do not ask before writing")
    a = ap.parse_args()
    try:
        if a.what == "write":
            if not a.package:
                raise Fail("write: give the .fwsc")
            img, product = package_flash(a.package)
            print(f"package {product}: flash.bin {len(img):#x} bytes, CRC good")
        u = connect(a.loader)
        if a.what == "info":
            u.run_app()
            return 0
        print("reading the whole flash (the backup) ...")
        now = u.read(0, FLASH_SIZE, bar)
        name = a.package if a.what == "backup" and a.package else time.strftime("fm1_backup_%Y%m%d_%H%M%S.bin")
        with open(name, "wb") as f:
            f.write(now)
        print(f"backup: {os.path.abspath(name)}")
        if a.what == "backup":
            u.run_app()
            return 0
        diff = [s for s in range(WRITE_MIN, WRITE_END, SECTOR) if now[s:s + SECTOR] != img[s:s + SECTOR]]
        print(f"{len(diff)} of {(WRITE_END - WRITE_MIN) // SECTOR} sectors in [0x4000, 0x93000) to write")
        if not diff:
            print("the FM-1 holds this package already")
            u.run_app()
            return 0
        if not a.yes and input("Write? Do not unplug the FM-1 while writing. [y/N] ").strip().lower() != "y":
            u.run_app()
            return 1
        for i, s in enumerate(diff):
            u.write_sector(s, img[s:s + SECTOR])
            bar(i + 1, len(diff))
        print("reading the app area back ...")
        if u.read(WRITE_MIN, WRITE_END - WRITE_MIN, bar) != img[WRITE_MIN:WRITE_END]:
            raise Fail("the read back differs: the FM-1 stays in UBOOT, run this again")
        print("written and checked; starting the FM-1")
        u.run_app()
        print("done: if the screen stays dark, switch the FM-1 off and on")
        return 0
    except Fail as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    except ImportError as e:
        print(f"error: {e} (pip install pyusb; brew install libusb)", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
