#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only OR MIT
# Copyright (C) 2026 cursedsound
"""The firmware's own tables into the web editor (web/editor.html, between /*FW_TABLES*/ and /*/FW_TABLES*/):

  tools/gen_editor_tables.py [build/host/desc.json]

desc.json is tests/descdump.c's (run_tests.sh writes it): the parameter descriptors, the engines (names, page titles,
EDIT values, presets), the display order and the power-on sounds. The editor's mock device and its engine order read
them, so the editor follows the firmware's engines (cursed os's KICK, DUB, FMDRUM, ACID among them) instead of a
hand-kept copy; web/test_web.mjs checks the two are the same, so run this after a change to the tables.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "build/host/desc.json"
    d = json.loads(src.read_text())
    keep = {k: d[k] for k in ("TP", "GP", "ENG", "ORDER", "TRK_DEF")}
    for e in keep["ENG"]:                         # (the mock's presets: name, E values, envelope, mono, pattern, xp)
        e["presets"] = [{k: p.get(k, 0) for k in ("name", "e", "env", "mono", "pat", "xp")} for p in e["presets"]]
    js = json.dumps(keep, separators=(",", ":"), ensure_ascii=False)
    page = ROOT / "web/editor.html"
    html = page.read_text()
    block = f"/*FW_TABLES*/const FW_TABLES = {js};/*/FW_TABLES*/"
    new, n = re.subn(r"/\*FW_TABLES\*/.*?/\*/FW_TABLES\*/", lambda m: block, html, flags=re.S)
    if n != 1:
        raise SystemExit("gen_editor_tables: no /*FW_TABLES*/ block in web/editor.html")
    if new != html:
        page.write_text(new)
        print(f"web/editor.html: the firmware's tables ({len(js)} B)")
    else:
        print("web/editor.html: the firmware's tables, unchanged")
    return 0


if __name__ == "__main__":
    sys.exit(main())
