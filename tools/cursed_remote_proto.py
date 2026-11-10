# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 cursedsound
"""cursed-os's phone remote protocol (firmware/src/cos_remote.c, cos_sample.c), pure Python: used by
tools/cursed_remote.py (Pythonista on the iPhone) and by tests/remote_proto_test.py (the host, against the C side).

Channel 16 CCs (performance), SysEx frames F0 7D 63 <cmd> .. F7 (text, the sampler), replies on channel 16:
CC 118 <seq> ACK, CC 119 <code> NAK."""

CH = 15                                   # MIDI channel 16
ACK_CC, NAK_CC = 118, 119
NAK_TEXT = {1: "not understood", 2: "stop the music first", 3: "no room (or too long)", 4: "bad data",
            5: "flash failed", 6: "out of order"}

# the performance CCs (cos_remote.c)
CC_PLAY, CC_MUTE0, CC_SECT0, CC_FILL, CC_BREAK, CC_BACK, CC_TAP = 20, 22, 28, 32, 33, 34, 35
CC_PADW0, CC_PADB0 = 40, 56
CC_DJF, CC_ISO0, CC_MAC0, CC_MORPH, CC_BPM, CC_TRK0 = 70, 71, 74, 78, 79, 85

PAD_WHITE = ["RPT 1/8", "RPT 1/16", "RPT 1/32", "GATE 16", "GATE 8", "TAPE", "GLITCH", "LPF", "HPF", "DRIVE",
             "CRUSH", "THROW", "WASH", "FREEZE", "HP RISE", "NOISE"]
PAD_BLACK = ["M1", "M2", "M3", "M4", "M5", "M6", "KICK KILL", "LOW KILL", "DRUMS", "LOOP 3", "LOOP 7"]


def cc(num, val):
    """a channel-16 CC: three bytes"""
    return bytes([0xB0 | CH, num & 127, max(0, min(127, int(val)))])


def frame(cmd, payload):
    return bytes([0xF0, 0x7D, 0x63, cmd]) + bytes(payload) + b"\xF7"


def text_frame(words, track=0x7F):
    w = "".join(c if 32 <= ord(c) <= 126 else " " for c in words)[:63]
    return frame(0x01, [track & 127] + [ord(c) for c in w])


def u21(v):
    return [v & 127, (v >> 7) & 127, (v >> 14) & 127]


def pack7(data):
    """bytes -> 7-bit: each 8 bytes a byte of the next 7's high bits (bit k: byte k), then the 7"""
    out = []
    for i in range(0, len(data), 7):
        g = data[i:i + 7]
        hi = 0
        for k, b in enumerate(g):
            hi |= ((b >> 7) & 1) << k
        out.append(hi)
        out.extend(b & 127 for b in g)
    return out


# IMA ADPCM, blocks of 256 samples: 4-byte header (predictor int16 LE, step index, 0) + 128 bytes (low nibble first)
STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
        107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
        876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
        5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
        27086, 29794, 32767]
IDX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]
BLK, BLKB = 256, 132


def _dec(nib, pred, idx):
    st = STEP[idx]
    d = st >> 3
    if nib & 4: d += st
    if nib & 2: d += st >> 1
    if nib & 1: d += st >> 2
    pred = pred - d if nib & 8 else pred + d
    pred = max(-32768, min(32767, pred))
    idx = max(0, min(88, idx + IDX[nib]))
    return pred, idx


def adpcm_encode(samples):
    """int16 samples -> bytes (whole blocks, the last padded with silence)"""
    out = bytearray()
    pred, idx = 0, 0
    n = len(samples)
    for b0 in range(0, max(n, 1), BLK):
        out += bytes([pred & 255, (pred >> 8) & 255, idx, 0])
        nibs = []
        for i in range(b0, b0 + BLK):
            x = samples[i] if i < n else 0
            st = STEP[idx]
            diff = x - pred
            nib = 0
            if diff < 0:
                nib = 8
                diff = -diff
            if diff >= st: nib |= 4; diff -= st
            if diff >= st >> 1: nib |= 2; diff -= st >> 1
            if diff >= st >> 2: nib |= 1
            pred, idx = _dec(nib, pred, idx)       # (the decoder's state, as the device will have it)
            nibs.append(nib)
        for k in range(0, BLK, 2):
            out.append(nibs[k] | nibs[k + 1] << 4)
    return bytes(out)


def adpcm_decode(data, n):
    out = []
    pred = idx = 0
    for i in range(n):
        b, w = divmod(i, BLK)
        o = b * BLKB
        if w == 0:
            pred = int.from_bytes(data[o:o + 2], "little", signed=True)
            idx = data[o + 2] % 89
        nib = (data[o + 4 + (w >> 1)] >> ((w & 1) * 4)) & 15
        pred, idx = _dec(nib, pred, idx)
        out.append(pred)
    return out


def crc32(data):
    import zlib
    return zlib.crc32(bytes(data)) & 0xFFFFFFFF


def sample_frames(slot, name, rate, samples):
    """the frames that send a sound (int16 mono samples at rate) into slot 0..15: [(seq, frame bytes)]"""
    data = adpcm_encode(samples)
    nm = [ord(c) if 32 <= ord(c) <= 126 else 32 for c in (name.upper() + " " * 8)[:8]]
    seq = 0
    frames = [(seq, frame(0x10, [seq, slot] + nm + u21(rate) + u21(len(samples)) + u21(len(data))))]
    for i in range(0, len(data), 256):
        seq = (seq + 1) & 127
        frames.append((seq, frame(0x11, [seq] + pack7(data[i:i + 256]))))
    seq = (seq + 1) & 127
    c = crc32(data)
    frames.append((seq, frame(0x12, [seq] + [(c >> s) & 127 for s in (0, 7, 14, 21, 28)])))
    return frames


def delete_frame(slot, seq=0):
    return frame(0x13, [seq, slot & 15])


def resample(samples, src_rate, dst_rate=22050):
    """linear: enough for a sampler's sound (the phone's decoder gives floats or ints at the file's rate)"""
    if src_rate == dst_rate or not samples:
        return list(samples)
    n = int(len(samples) * dst_rate / src_rate)
    r = src_rate / dst_rate
    out = []
    for i in range(n):
        x = i * r
        k = int(x)
        f = x - k
        a = samples[k]
        b = samples[k + 1] if k + 1 < len(samples) else a
        out.append(int(a + (b - a) * f))
    return out


def normalize(samples, peak=0.98):
    m = max((abs(s) for s in samples), default=0)
    if not m:
        return list(samples)
    g = peak * 32767 / m
    return [int(max(-32768, min(32767, s * g))) for s in samples]
