#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only OR MIT
# Copyright (C) 2026 cursedsound
"""tools/fm1_uboot.py against a simulated FM-1 in UBOOT: a USB mass-storage device that answers Bulk-Only Transport
(CBW, data, CSW) with the WL82 ROM and loader commands as FM-1 Transporter documents them (docs/PROTOCOL.md,
src/jieli_uboot.c). A package made by tools/fm1pkg_make.py's ufw(); a flash with an unknown part's id. Checks: the
package's flash.bin found and its CRC; the loader uploaded whole and checked; the key and the flash id read; the
backup equal to the flash; only [0x4000, 0x93000) written, only the sectors that differ; the head and the device data
untouched; a write refused outside; a damaged package refused.  python3 tests/fm1_uboot_test.py"""
import os
import random
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools"))
import fm1_uboot as fu          # noqa: E402
import fm1pkg_make as pm        # noqa: E402

fails = 0


def check(c, msg):
    global fails
    if not c:
        fails += 1
        print("FAIL:", msg)


class Ep:
    def __init__(self, addr, dev):
        self.bEndpointAddress, self.dev = addr, dev

    def write(self, data, timeout=0):
        self.dev.out(bytes(data))
        return len(data)

    def read(self, n, timeout=0):
        return self.dev.inp(n)


class FakeFM1:
    """the chip in UBOOT: ROM until JUMP, then the loader"""
    def __init__(self, flash, flash_id=0x5E6014):
        self.flash, self.flash_id = bytearray(flash), flash_id
        self.mem, self.loader, self.state, self.pending, self.q = {}, False, "cbw", None, []
        self.writes, self.erases, self.ran = [], [], False
        self.ep_out, self.ep_in = Ep(0x01, self), Ep(0x81, self)

    # pyusb's device surface the tool uses
    def is_kernel_driver_active(self, i): return True
    def detach_kernel_driver(self, i): pass
    def set_configuration(self): pass
    def ctrl_transfer(self, *a): return b"\x00"
    def clear_halt(self, ep): pass
    def get_active_configuration(self): return {(0, 0): [self.ep_out, self.ep_in]}

    def csw(self, tag, status=0):
        self.q.append(struct.pack("<4sIIB", b"USBS", tag, 0, status))

    def out(self, d):
        if self.state == "cbw":
            sig, tag, dlen, flags, lun, clen = struct.unpack_from("<IIIBBB", d, 0)
            assert sig == 0x43425355 and len(d) == 31
            cdb = d[15:15 + clen]
            if flags & 0x80 or not dlen:
                self.q.append(self.answer(cdb, dlen))
                self.csw(tag)
            else:
                self.state, self.pending = "data", (tag, cdb, dlen)
        else:
            tag, cdb, dlen = self.pending
            assert len(d) == dlen
            self.state = "cbw"
            self.data_out(cdb, d)
            self.csw(tag)

    def inp(self, n):
        r = self.q.pop(0)
        return r[:n]

    def answer(self, cdb, n):
        if cdb[0] == 0x12:
            return (b"\x00" * 8 + b"JIELI   " + b"UBOOT1.00       " + b"1.00").ljust(36, b"\0")[:n]
        cmd = cdb[0] << 8 | cdb[1]
        r = bytearray(b"\x00" * 16)
        r[0], r[1] = cdb[0], cdb[1]
        if cmd == fu.CMD_JUMP:
            addr = struct.unpack_from(">I", cdb, 2)[0]
            blob = bytes(self.mem.get(a, 0) for a in range(fu.LOADER_ADDR, fu.LOADER_ADDR + fu.LOADER_SIZE))
            assert addr == fu.LOADER_ADDR and blob == self.blob, "the loader in RAM whole before JUMP"
            self.loader = True
        elif cmd == fu.CMD_READ_KEY:
            assert self.loader
            ks = fu.crc_cipher(b"\x00\x00")          # the key's two bytes, ciphered and reversed in the answer
            r[2 + 5], r[2 + 4] = 0x0F ^ ks[0], 0x98 ^ ks[1]
        elif cmd == fu.CMD_GET_ONLINE_DEVICE:
            r[2] = 1
            struct.pack_into("<I", r, 4, self.flash_id)
        elif cmd == fu.CMD_GET_USB_BUFF_SIZE:
            struct.pack_into(">I", r, 2, 8192)
        elif cmd == fu.CMD_ERASE_SECTOR:
            a = struct.unpack_from(">I", cdb, 2)[0]
            self.erases.append(a)
            self.flash[a:a + 0x1000] = b"\xFF" * 0x1000
        elif cmd == fu.CMD_READ_FLASH:
            a, k = struct.unpack_from(">IH", cdb, 2)
            assert k <= 4096
            return bytes(self.flash[a:a + k])
        elif cmd == fu.CMD_RUN_APP:
            self.ran = True
        return bytes(r[:n])

    def data_out(self, cdb, d):
        cmd = cdb[0] << 8 | cdb[1]
        a, k = struct.unpack_from(">IH", cdb, 2)
        crc = cdb[2 + 7] | cdb[2 + 8] << 8                 # (args[7..8], after the cmd)
        assert k == len(d) and crc == fu.crc16(d), "the block's length and CRC in the CDB"
        if cmd == fu.CMD_WRITE_MEMORY:
            assert not self.loader
            for i, b in enumerate(d):
                self.mem[a + i] = b
        elif cmd == fu.CMD_WRITE_FLASH:
            assert self.loader and a >= 0x4000 and a + k <= 0x93000
            self.writes.append(a)
            for i, b in enumerate(d):
                self.flash[a + i] &= b                   # (NOR: bits only cleared)


def main():
    rnd = random.Random(7)
    app = bytes(rnd.getrandbits(8) for _ in range(0x93000))
    pkg_flash = bytearray(app)
    pkg_flash[0:0x4000] = b"\xA5" * 0x4000              # (the package's head: not the device's, never written)
    fwsc = pm.ufw(bytes(pkg_flash), b"OTA" * 100, "FM-1_909")
    d = tempfile.mkdtemp()
    pkg = os.path.join(d, "felucca.fwsc")
    open(pkg, "wb").write(fwsc)
    img, product = fu.package_flash(pkg)
    check(img == bytes(pkg_flash) and product == "FM-1_909", "the package's flash.bin and product")
    bad = bytearray(fwsc)
    bad[0x2000] ^= 1
    open(pkg + ".bad", "wb").write(bad)
    try:
        fu.package_flash(pkg + ".bad")
        check(False, "a damaged package refused")
    except fu.Fail:
        pass

    dev_flash = bytearray(rnd.getrandbits(8) for _ in range(0x100000))
    head, data = bytes(dev_flash[:0x4000]), bytes(dev_flash[0x93000:])
    for s in range(0x4000, 0x93000, 0x1000):            # (most sectors already the package's: only the rest written)
        if s % 0x8000:
            dev_flash[s:s + 0x1000] = img[s:s + 0x1000]
    want = [s for s in range(0x4000, 0x93000, 0x1000) if dev_flash[s:s + 0x1000] != img[s:s + 0x1000]]
    fake = FakeFM1(dev_flash)
    blob = bytes(rnd.getrandbits(8) for _ in range(fu.LOADER_SIZE))
    fake.blob = blob
    ldr = os.path.join(d, "wl82loader.bin")
    open(ldr, "wb").write(blob)
    import usb.core
    usb.core.find = lambda **k: fake
    fu.backend = lambda: None
    import usb.util
    usb.util.claim_interface = lambda dev, i: None
    fu.soft_key = lambda: True
    cwd = os.getcwd()
    os.chdir(d)
    sys.argv = ["fm1_uboot.py", "--loader", ldr, "write", pkg, "--yes"]
    rc = fu.main()
    os.chdir(cwd)
    check(rc == 0, f"write: done ({rc})")
    check(bytes(fake.flash[0x4000:0x93000]) == img[0x4000:0x93000], "the app area is the package's")
    check(bytes(fake.flash[:0x4000]) == head and bytes(fake.flash[0x93000:]) == data, "the head and the device data untouched")
    check(fake.erases == want, f"only the sectors that differ written ({len(fake.erases)} of {len(want)})")
    check(fake.ran, "the FM-1 started after")
    backups = [f for f in os.listdir(d) if f.startswith("fm1_backup_")]
    check(len(backups) == 1 and open(os.path.join(d, backups[0]), "rb").read() == bytes(dev_flash),
          "the backup: the whole flash as it was")
    u = fu.Uboot(FakeFM1(bytes(0x100000)))
    try:
        u.write_sector(0x3000, b"\0" * 0x1000)
        check(False, "a write under 0x4000 refused")
    except fu.Fail:
        pass
    try:
        u.write_sector(0x93000, b"\0" * 0x1000)
        check(False, "a write at 0x93000 refused")
    except fu.Fail:
        pass
    print(f"fm1_uboot_test: {'ok' if not fails else 'FAILED'} ({fails} failures)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
