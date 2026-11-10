#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""the phone remote end to end on the host: tools/cursed_remote_proto.py's frames for a sound fed to the device's code
(tests/remote_test.c: cos_remote.c, cos_sample.c, eng_sampler.c). Checks: every frame ACKed in order, the slot holds
the sound (its length, its rate), the device decodes exactly what the Python decoder does (the same ADPCM), the decode
near the original (SNR), the SAMPLER plays it (a level; an octave up twice the crossings), a resend of a DATA frame
ACKed without harm, a damaged frame stream refused.
    python3 tests/remote_proto_test.py build/t/remote_test"""
import math, os, subprocess, sys, tempfile
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools"))
import cursed_remote_proto as P

exe = sys.argv[1]
fails = 0


def check(ok, what):
    global fails
    print(("ok   " if ok else "FAIL ") + what)
    fails += not ok


def run(frames, slot):
    with tempfile.NamedTemporaryFile(delete=False) as f:
        for fr in frames:
            f.write(fr)
        name = f.name
    out = subprocess.run([exe, name, str(slot)], capture_output=True, text=True).stdout.split("\n")
    os.unlink(name)
    i = out.index(next(l for l in out if l.startswith("DEC")))
    replies = out[:i]
    n, rate = map(int, out[i].split()[1:])
    dec = [int(x) for x in out[i + 1:i + 1 + n]]
    play = next((l for l in out if l.startswith("PLAY")), "PLAY 0 0 0").split()[1:]
    return replies, n, rate, dec, [float(play[0]), int(play[1]), int(play[2])]


rate = 22050
src = [int(12000 * math.sin(2 * math.pi * 220 * i / rate) + 3000 * math.sin(2 * math.pi * 1375 * i / rate))
       for i in range(int(rate * 0.8))]
fr = P.sample_frames(3, "test tone", rate, src)
replies, n, r, dec, play = run([f for _, f in fr], 3)
want = ["ACK %d" % s for s, _ in fr]
check(replies == want, "every frame ACKed with its seq (%d frames; %s)" % (len(fr), replies[:3]))
check(n == len(src) and r == rate, "the slot: %d samples at %d Hz" % (n, r))
pyd = P.adpcm_decode(P.adpcm_encode(src), len(src))
check(dec == pyd, "the device decodes as the Python decoder (%d samples)" % len(dec))
err = sum((a - b) ** 2 for a, b in zip(src, dec)) / len(src)
sig = sum(a * a for a in src) / len(src)
snr = 10 * math.log10(sig / max(err, 1))
check(snr > 25, "ADPCM near the original: SNR %.1f dB" % snr)
check(play[0] > 500, "the SAMPLER plays it (rms %.0f)" % play[0])
check(1.7 < play[2] / max(play[1], 1) < 2.3, "an octave up: twice the crossings (%d, %d)" % (play[1], play[2]))

dup = [f for _, f in fr]
dup.insert(3, dup[2])                                # (a DATA frame twice: its ACK lost, the phone resent it)
replies, n, r, dec2, _ = run(dup, 3)
check(replies[3] == replies[2] and n == len(src) and dec2 == pyd, "a DATA frame resent: ACKed again, the sound as it was")

bad = [f for _, f in fr]
b2 = bytearray(bad[2]); b2[10] ^= 0x15; bad[2] = bytes(b2)   # (a byte damaged: the CRC at END)
replies, n, r, _, _ = run(bad, 3)
check(replies[-1] == "NAK 4" and n == 0, "a damaged byte: END NAK 4 (bad data), no sound kept (%s)" % replies[-1])

long_src = [0] * (rate * 12)
fr = P.sample_frames(5, "long", rate, long_src)
replies, n, _, _, _ = run([fr[0][1]], 5)
check(replies == ["NAK 3"], "12 s: NAK 3 (no room / too long) (%s)" % replies)

print("remote_proto_test: %s (%d failures)" % ("ok" if not fails else "FAILED", fails))
sys.exit(fails != 0)
