# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 cursedsound
"""cursed-os's phone remote protocol (firmware/src/cos_remote.c), pure Python: used by
tools/cursed_remote.py (Pythonista on the iPhone).

Channel 16 CCs (performance), SysEx frames F0 7D 63 <cmd> .. F7 (text), replies on channel 16:
CC 118 <seq> ACK, CC 119 <code> NAK."""

CH = 15                                   # MIDI channel 16
ACK_CC, NAK_CC = 118, 119
NAK_TEXT = {1: "not understood (no VOICE track?)"}

# the performance CCs (cos_remote.c)
CC_PLAY, CC_MUTE0, CC_SECT0, CC_FILL, CC_BREAK, CC_BACK, CC_TAP = 20, 22, 28, 32, 33, 34, 35
CC_NUDGE0, CC_BPMDN = 36, 38                # (0.9.6) 36 37 NUDGE - + (held), 38 39 BPM - + 1
CC_BPMX = 80                                # (0.9.7) 80 81: an exact BPM in tenths, high 7 bits then low
CC_DRUM0 = 91                                # (0.10.0) 91..98 a drum sound OUT (64+) / IN
DRUM_SOUNDS = ["KICK", "SNARE", "CLAP", "HAT", "OPEN HAT", "TOM", "RIM", "BELL"]
CC_PADW0, CC_PADB0 = 40, 56
CC_DJF, CC_ISO0, CC_MAC0, CC_MORPH, CC_BPM, CC_TRK0 = 70, 71, 74, 78, 79, 85

PAD_WHITE = ["RPT 1/8", "RPT 1/16", "RPT 1/32", "GATE 16", "GATE 8", "TAPE", "GLITCH", "LPF", "HPF", "DRIVE",
             "CRUSH", "THROW", "WASH", "FREEZE", "HP RISE", "NOISE"]
PAD_BLACK = ["M1", "M2", "M3", "M4", "M5", "M6", "KICK KILL", "LOW KILL", "DRUMS", "LOOP 3", "LOOP 7"]


def cc(num, val):
    """a channel-16 CC: three bytes"""
    return bytes([0xB0 | CH, num & 127, max(0, min(127, int(val)))])


def bpm_exact(bpm):
    """an exact tempo (40.0 .. 240.0, a tenth): CC 80 then CC 81 (cursed-os 0.9.7)"""
    t = max(400, min(2400, int(round(float(bpm) * 10))))
    return (cc(CC_BPM, max(0, min(127, (t + 5) // 10 - 60)))    # (0.9.6: the whole BPM; 0.9.7 on: the exact one after)
            + cc(CC_BPMX, t >> 7) + cc(CC_BPMX + 1, t & 127))


def lfo_frame(shape, points):
    """the LFO's USER shape 0..3 (32 points, -127..127): frame 02 (cursed-os 0.10.2)"""
    pts = [max(0, min(127, 64 + int(round(v / 2.0)))) for v in list(points)[:32]]
    return frame(0x02, [shape & 3] + pts + [64] * (32 - len(pts)))


def frame(cmd, payload):
    return bytes([0xF0, 0x7D, 0x63, cmd]) + bytes(payload) + b"\xF7"


def text_frame(words, track=0x7F):
    w = "".join(c if 32 <= ord(c) <= 126 else " " for c in words)[:63]
    return frame(0x01, [track & 127] + [ord(c) for c in w])
