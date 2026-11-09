#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 cursedsound
"""cursed-os: the IDLE screen's skull over a crossed hammer and sickle (firmware/src/cos_skull.h): drawn with PIL shapes
at 8 x, scaled down to 1 bit.  python3 tools/cos_skull.py  (in an empty folder: writes skull.h, skull.png)"""
from PIL import Image, ImageDraw
import math
W, H, S = 112, 96, 8
im = Image.new("L", (W*S, H*S), 0)
d = ImageDraw.Draw(im)
def P(x, y): return (x*S, y*S)
def poly(pts, f): d.polygon([P(x, y) for x, y in pts], fill=f)
def ell(x0, y0, x1, y1, f): d.ellipse([P(x0, y0), P(x1, y1)], fill=f)
def rect(x0, y0, x1, y1, f): d.rectangle([P(x0, y0), P(x1, y1)], fill=f)
def bar(x0, y0, x1, y1, w, f):
    dx, dy = x1-x0, y1-y0; l = math.hypot(dx, dy); nx, ny = -dy/l*w/2, dx/l*w/2
    poly([(x0+nx, y0+ny), (x1+nx, y1+ny), (x1-nx, y1-ny), (x0-nx, y0-ny)], f)
cx = 56
# sickle (top left blade, handle down to the lower right): a crescent, a disc less an offset disc
ell(2, 2, 54, 54, 255)                 # the blade's outer edge (centre 28, 28, r 26)
ell(12, 0, 58, 46, 0)                  # less this: a crescent, its point to the upper right
rect(0, 40, 26, 60, 0)                 # (its foot cut where the handle takes over)
bar(14, 40, 26, 58, 6, 255)            # .. the neck down to the handle
bar(22, 56, 100, 92, 7, 255)           # the handle, to the lower right
# hammer (head top right, handle down to the lower left)
bar(88, 22, 14, 90, 7, 255)            # the handle
bar(76, 10, 100, 34, 12, 255)          # the head, across it
# the skull, a black margin round it first
def skull(f, g):
    ell(cx-26-g, 10-g, cx+26+g, 60+g, f)
    poly([(cx-21-g, 46), (cx+21+g, 46), (cx+15+g, 66+g), (cx-15-g, 66+g)], f)
    rect(cx-13-g, 60, cx+13+g, 78+g, f)
skull(0, 3); skull(255, 0)
poly([(cx-20, 32), (cx-5, 36), (cx-6, 47), (cx-12, 50), (cx-19, 45)], 0)
poly([(cx+20, 32), (cx+5, 36), (cx+6, 47), (cx+12, 50), (cx+19, 45)], 0)
poly([(cx, 52), (cx-4, 60), (cx, 58), (cx+4, 60)], 0)
d.line([P(cx+6, 11), P(cx+10, 18), P(cx+7, 23), P(cx+12, 29)], fill=0, width=S)
d.line([P(cx-15, 15), P(cx-11, 21)], fill=0, width=S)
sm = im.resize((W, H), Image.LANCZOS).point(lambda v: 255 if v > 110 else 0)
dd = ImageDraw.Draw(sm)
dd.point([(cx-10, 41), (cx-11, 41), (cx-10, 42), (cx-11, 42), (cx+10, 41), (cx+11, 41), (cx+10, 42), (cx+11, 42)], fill=255)   # the eyes' glints
dd.line([(cx-12, 68), (cx+12, 68)], fill=0)                                  # the teeth: the bite
for x in range(cx-10, cx+11, 4): dd.line([(x, 63), (x, 76)], fill=0)        # .. and the gaps
sm = sm.convert("1")
sm.save("skull.png"); sm.resize((W*4, H*4), Image.NEAREST).save("skull_big.png")
bits = [1 if sm.getpixel((x, y)) else 0 for y in range(H) for x in range(W)]
by = [sum(bits[i+j] << (7-j) for j in range(8)) for i in range(0, len(bits), 8)]
with open("skull.h", "w") as f:
    f.write("/* (cursed-os) the IDLE screen's skull over a crossed hammer and sickle, drawn for cursed-os: %d x %d, a bit a\n * pixel, the rows top down, the left pixel the byte's top bit (made by a script: PIL shapes, 8 x, scaled down) */\n" % (W, H))
    f.write("#define SKULL_W %du\n#define SKULL_H %du\nstatic const uint8_t SKULL[%d] = {\n" % (W, H, len(by)))
    for i in range(0, len(by), 14):
        f.write("    " + ", ".join("0x%02X" % b for b in by[i:i+14]) + ",\n")
    f.write("};\n")
print(len(by))
