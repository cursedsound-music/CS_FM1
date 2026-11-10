#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 cursedsound
"""cursed-os: the IDLE screen's pictures, a skull and the circle-A (firmware/src/cos_skull.h):
drawn with PIL shapes at 8 x, scaled down to 1 bit.  python3 tools/cos_skull.py  (in an empty folder: writes cos_skull.h, PNGs)"""
from PIL import Image, ImageDraw
import math, sys
S = 8
def canvas(W, H):
    im = Image.new("L", (W*S, H*S), 0); return im, ImageDraw.Draw(im)
def P(x, y): return (x*S, y*S)
def poly(d, pts, f): d.polygon([P(x, y) for x, y in pts], fill=f)
def bar(d, x0, y0, x1, y1, w, f):
    dx, dy = x1-x0, y1-y0; l = math.hypot(dx, dy); nx, ny = -dy/l*w/2, dx/l*w/2
    poly(d, [(x0+nx, y0+ny), (x1+nx, y1+ny), (x1-nx, y1-ny), (x0-nx, y0-ny)], f)
def crescent(d, c0, r0, c1, r1, a0, a1, f, n=160):
    # the band between circle (c0, r0) outside and (c1, r1) inside, from angle a0 to a1 (degrees, clockwise on screen)
    out, inn = [], []
    for i in range(n+1):
        a = math.radians(a0 + (a1-a0)*i/n)
        out.append((c0[0] + r0*math.cos(a), c0[1] + r0*math.sin(a)))
        inn.append((c1[0] + r1*math.cos(a), c1[1] + r1*math.sin(a)))
    poly(d, out + inn[::-1], f)
def finish(im, W, H, name):
    sm = im.resize((W, H), Image.LANCZOS).point(lambda v: 255 if v > 110 else 0).convert("1")
    sm.resize((W*4, H*4), Image.NEAREST).save(name + "_big.png")
    return sm
# ---- the hammer and sickle ----
W, H = 112, 96
im, d = canvas(W, H)
# sickle: blade from its tip at the top right, round the right and the bottom, to the handle at the lower left
crescent(d, (62, 46), 40, (56, 40), 35, -80, 160, 255)
bar(d, 26, 62, 6, 90, 10, 255)                   # its handle
bar(d, 20, 60, 30, 70, 12, 255)                  # .. the ferrule joining it
# hammer: the head at the upper left, the handle down to the lower right
hx0, hy0, hx1, hy1 = 22, 24, 100, 92                 # the handle's ends
bar(d, hx0, hy0, hx1, hy1, 9, 255)
ux, uy = (hx1-hx0), (hy1-hy0); l = math.hypot(ux, uy); ux, uy = ux/l, uy/l   # along the handle
px_, py_ = -uy, ux                                                           # across it
def head(c, half_len, half_th, f):
    cx_, cy_ = c
    poly(d, [(cx_ + px_*half_len + ux*half_th, cy_ + py_*half_len + uy*half_th),
             (cx_ - px_*half_len + ux*half_th, cy_ - py_*half_len + uy*half_th),
             (cx_ - px_*half_len - ux*half_th, cy_ - py_*half_len - uy*half_th),
             (cx_ + px_*half_len - ux*half_th, cy_ + py_*half_len - uy*half_th)], f)
head((hx0 - ux*2, hy0 - uy*2), 18, 8, 255)                                   # the head, across the handle's top
hs = finish(im, W, H, "hs")
# ---- the skull: a striped dome, square sockets, a jaw drawn in outline with its teeth ----
W2, H2 = 72, 96
im, d = canvas(W2, H2)
cx = 36
d.rounded_rectangle([P(cx-30, 4), P(cx+30, 60)], radius=16*S, fill=255)     # the dome down to the cheekbones
for x in range(cx-27, cx+28, 6):                                            # the stripes over the dome
    d.rectangle([P(x, 0), P(x+1.6, 30)], fill=0)
d.rounded_rectangle([P(cx-26, 32), P(cx-8, 48)], radius=3*S, fill=0)     # the sockets, apart
d.rounded_rectangle([P(cx+8, 32), P(cx+26, 48)], radius=3*S, fill=0)
poly(d, [(cx, 44), (cx-5, 55), (cx+5, 55)], 0)                              # the nose, under them
d.rectangle([P(cx-30, 56), P(cx+30, 62)], fill=0)                           # (under the cheekbones: open)
# the jaw: an outline, teeth inside
jaw = [(cx-22, 60), (cx+22, 60), (cx+20, 76), (cx+12, 88), (cx-12, 88), (cx-20, 76)]
poly(d, jaw, 255)
poly(d, [(cx-18, 63), (cx+18, 63), (cx+16, 75), (cx+9, 85), (cx-9, 85), (cx-16, 75)], 0)
d.rectangle([P(cx-18, 72), P(cx+18, 74)], fill=255)                         # the bite
for x in range(cx-14, cx+15, 6):
    d.rectangle([P(x, 63), P(x+2, 71)], fill=255)                           # upper teeth
    if abs(x - cx) < 12: d.rectangle([P(x, 75), P(x+2, 83)], fill=255)      # lower teeth
sk = finish(im, W2, H2, "sk")
# ---- the circle-A, a brush's strokes ----
W3, H3 = 104, 96
im, d = canvas(W3, H3)
def apoly(pts, f=255): d.polygon([P(x, y) for x, y in pts], fill=f)
def stroke(x0, y0, x1, y1, w0, w1, f=255):
    # a brush stroke: w0 wide at its start, w1 at its end (a taper)
    dx, dy = x1-x0, y1-y0; l = math.hypot(dx, dy); nx, ny = -dy/l, dx/l
    apoly([(x0+nx*w0/2, y0+ny*w0/2), (x1+nx*w1/2, y1+ny*w1/2), (x1-nx*w1/2, y1-ny*w1/2), (x0-nx*w0/2, y0-ny*w0/2)], f)
cx, cy, r = 52, 52, 33
# the ring: a band 8 wide, a little uneven (a brush), from the top left round, its ends overlapping and tapering
pts_o, pts_i = [], []
for i in range(0, 361):
    a = math.radians(i - 120)
    wob = 0.8 * math.sin(i * 0.09) + 0.5 * math.sin(i * 0.31)
    w = 10 + wob
    if i < 18: w *= i / 18          # (the start tapers in)
    if i > 340: w *= (361 - i) / 21  # (the end tapers out, past the start)
    pts_o.append((cx + (r + w/2) * math.cos(a), cy + (r + w/2) * math.sin(a)))
    pts_i.append((cx + (r - w/2) * math.cos(a), cy + (r - w/2) * math.sin(a)))
apoly(pts_o + pts_i[::-1])
# the A: its apex above the ring, its legs out below it
stroke(52, 3, 15, 95, 7, 12)        # left leg, widening down
stroke(52, 3, 85, 95, 8, 11)        # right leg
stroke(57, 6, 48, 1, 7, 2)          # (the apex's brush tip)
# the crossbar: from the lower left, rising to the right, out past the ring, its end split in two tails
stroke(4, 70, 86, 46, 5, 10)
stroke(86, 46, 103, 40, 10, 1)
stroke(80, 50, 101, 48, 5, 1)
an = finish(im, W3, H3, "an")
def emit(img, name, W, H, f):
    bits = [1 if img.getpixel((x, y)) else 0 for y in range(H) for x in range(W)]
    by = [sum(bits[i+j] << (7-j) for j in range(8)) for i in range(0, len(bits), 8)]
    f.write("#define %s_W %du\n#define %s_H %du\nstatic const uint8_t %s[%d] = {\n" % (name, W, name, H, name, len(by)))
    for i in range(0, len(by), 14):
        f.write("    " + ", ".join("0x%02X" % b for b in by[i:i+14]) + ",\n")
    f.write("};\n")
with open("cos_skull.h", "w") as f:
    f.write("/* SPDX-License-Identifier: GPL-3.0-only\n * Copyright (C) 2026 cursedsound */\n")
    f.write("/* (cursed-os) the IDLE screen's two pictures, drawn for cursed-os (tools/cos_skull.py: PIL shapes at 8 x, scaled down):\n * a skull and the circle-A; a bit a pixel, the rows top down, the left pixel the byte's top bit */\n")
    emit(sk, "SKULL", W2, H2, f); emit(an, "ANAR", W3, H3, f)
