#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only OR MIT
# Copyright (C) 2026 cursedsound
"""Put a cursed os release together, from what the build made:

  tools/make_release.py VERSION --fwsc build/felucca.fwsc --site build/site --emu build/emu [--out build/release]

writes OUT/cursed-os-VERSION/ (the folder the download unpacks to), OUT/cursed-os-VERSION.zip, the package on its
own (OUT/cursed-os-VERSION.fwsc) and OUT/NOTES.md (the release notes: VERSION's section of CHANGELOG.md, and what the
download holds). The folder:

  cursed-os-VERSION.fwsc      the firmware package
  installer/                  web/make_site.py's site: the web installer and the editor (python3 -m http.server)
  tools/                      fm1_install.py, fm1_uboot.py, requirements.txt
  emulator/                   the browser emulator (web/emu's build)
  docs/                       GUIDE.md (+ guide/), INSTALL.md
  README.md CHANGELOG.md LICENSE LICENSING.md LICENSES/
  SHA256SUMS                  every file above
VERSION must be the firmware's own (firmware/src/cos.c COS_VERSION), so a tag cannot name another build.
"""
import argparse
import hashlib
import re
import shutil
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def cos_version():
    m = re.search(r'#define COS_VERSION "([^"]+)"', (ROOT / "firmware/src/cos.c").read_text())
    if not m:
        raise SystemExit("make_release: no COS_VERSION in firmware/src/cos.c")
    return m.group(1)


def notes_of(version):
    """VERSION's section of CHANGELOG.md (## X.Y.Z, or the ## X.Y block's line for it)"""
    text = (ROOT / "CHANGELOG.md").read_text()
    m = re.search(rf"^## {re.escape(version)}\n(.*?)(?=^## |\Z)", text, re.M | re.S)
    if m:
        return m.group(1).strip()
    minor = ".".join(version.split(".")[:2])
    m = re.search(rf"^## {re.escape(minor)}\n(.*?)(?=^## |\Z)", text, re.M | re.S)
    if m:
        lines = [l for l in m.group(1).splitlines() if l.startswith(f"- {version}:")]
        if lines:
            return "\n".join(lines)
    raise SystemExit(f"make_release: CHANGELOG.md has nothing for {version}")


def copy(src, dst):
    dst.parent.mkdir(parents=True, exist_ok=True)
    if src.is_dir():
        shutil.copytree(src, dst)
    else:
        shutil.copy2(src, dst)


def main():
    ap = argparse.ArgumentParser(description="put a cursed os release together")
    ap.add_argument("version")
    ap.add_argument("--fwsc", type=Path, required=True)
    ap.add_argument("--site", type=Path, required=True, help="web/make_site.py's output")
    ap.add_argument("--emu", type=Path, required=True, help="the browser emulator's build (web/emu/build.sh)")
    ap.add_argument("--out", type=Path, default=ROOT / "build/release")
    a = ap.parse_args()
    v = a.version.lstrip("v")
    if v != cos_version():
        raise SystemExit(f"make_release: {v} is not the firmware's version ({cos_version()}, firmware/src/cos.c)")
    for p in (a.fwsc, a.site, a.emu):
        if not p.exists():
            raise SystemExit(f"make_release: {p} is missing")
    name = f"cursed-os-{v}"
    rel = a.out / name
    shutil.rmtree(rel, ignore_errors=True)
    rel.mkdir(parents=True)
    copy(a.fwsc, rel / f"{name}.fwsc")
    copy(a.site, rel / "installer")
    for t in ("fm1_install.py", "fm1_uboot.py", "requirements.txt"):
        copy(ROOT / "tools" / t, rel / "tools" / t)
    emu = rel / "emulator"
    emu.mkdir()
    for f in a.emu.iterdir():                         # (the emulator alone: not a firmware copy or installer put there)
        if f.name in ("webapp", "firmware") or f.suffix == ".fwsc":
            continue
        copy(f, emu / f.name)
    copy(ROOT / "docs/GUIDE.md", rel / "docs/GUIDE.md")
    copy(ROOT / "docs/guide", rel / "docs/guide")
    copy(ROOT / "docs/INSTALL.md", rel / "docs/INSTALL.md")
    for f in ("README.md", "CHANGELOG.md", "LICENSE", "LICENSING.md"):
        copy(ROOT / f, rel / f)
    copy(ROOT / "LICENSES", rel / "LICENSES")
    files = sorted(p for p in rel.rglob("*") if p.is_file())
    (rel / "SHA256SUMS").write_text("".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(rel)}\n"
                                            for p in files))
    zp = a.out / f"{name}.zip"
    with zipfile.ZipFile(zp, "w", zipfile.ZIP_DEFLATED) as z:
        for p in sorted(rel.rglob("*")):
            if p.is_file():
                z.write(p, Path(name) / p.relative_to(rel))
    shutil.copy2(a.fwsc, a.out / f"{name}.fwsc")
    sha = hashlib.sha256(a.fwsc.read_bytes()).hexdigest()
    (a.out / "NOTES.md").write_text(
        f"{notes_of(v)}\n\n"
        "### Downloads\n\n"
        f"- `{name}.zip`: everything (the firmware, the web installer, the command-line installer and recovery tool, "
        "the browser emulator, the guide, the licences). Start with `docs/INSTALL.md` inside.\n"
        f"- `{name}.fwsc`: the firmware package alone (SHA-256 `{sha}`).\n\n"
        "Back up your FM-1 before installing (the installer's \"Return to official V15\" saves a complete backup) and "
        "keep the official `FM-1.fwsc` from M-VAVE at hand. Installing custom firmware is at your own risk.\n")
    print(f"release  {rel}/\nzip      {zp}\nnotes    {a.out / 'NOTES.md'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
