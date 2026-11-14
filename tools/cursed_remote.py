# SPDX-License-Identifier: GPL-3.0-only OR MIT
# Copyright (C) 2026 cursedsound
"""CURSED REMOTE: the JIANT FM-1 (cursed-os 0.10.3+) from an iPhone, in Pythonista 3.

Put this file and cursed_remote_proto.py in the same Pythonista folder and run this one.

  LINK     pair the FM-1 over Bluetooth MIDI (iOS's own Bluetooth MIDI window), pick it as the output
  PLAY     PLAY, an exact BPM (128.5), TAP, NUDGE (hold), BPM -/+ 1, sections, FILL / BREAK / ALL BACK, mutes
  FX       the FX pad (hold a key)
  MIX      DJ filter, isolator, macros, morph: tap a value and it jumps back to rest
           (all channel 16 CCs: cos_remote.c; NUDGE and BPM -/+ need cursed-os 0.9.6, the exact BPM 0.9.7)
  LIVE     each drum sound in and out (lit = in), the DJ FILTER, REVERB and DELAY on every track, FEEDBACK, and
           BACK TO NORMAL (cursed-os 0.10.3; on the beat / bar as the FM-1's MUTE Q says)
  VOICE    type words: the first VOICE track sings them from its next note
  LFO      draw the LFO's USER1..4 shapes (cursed-os 0.10.2): the FM-1's LFO page, WAVE = USER1..4

Works over USB MIDI too (a camera adapter / USB-C): pick the FM-1's USB port as the output.
"""
import ctypes, math, os, queue, struct, sys, threading, time
from ctypes import c_void_p, c_uint32, c_int32, c_uint64, c_float, POINTER, byref

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cursed_remote_proto as P

import ui
from objc_util import ObjCClass, ObjCInstance, ns, nsurl, on_main_thread, load_framework

# ------------------------------------------------------------------------------------------------ CoreMIDI
CM = ctypes.CDLL('/System/Library/Frameworks/CoreMIDI.framework/CoreMIDI')
for fn, args, res in [
        ('MIDIClientCreate', [c_void_p, c_void_p, c_void_p, POINTER(c_uint32)], c_int32),
        ('MIDIOutputPortCreate', [c_uint32, c_void_p, POINTER(c_uint32)], c_int32),
        ('MIDIInputPortCreate', [c_uint32, c_void_p, c_void_p, c_void_p, POINTER(c_uint32)], c_int32),
        ('MIDIPortConnectSource', [c_uint32, c_uint32, c_void_p], c_int32),
        ('MIDIGetNumberOfDestinations', [], ctypes.c_ulong),
        ('MIDIGetDestination', [ctypes.c_ulong], c_uint32),
        ('MIDIGetNumberOfSources', [], ctypes.c_ulong),
        ('MIDIGetSource', [ctypes.c_ulong], c_uint32),
        ('MIDIObjectGetStringProperty', [c_uint32, c_void_p, POINTER(c_void_p)], c_int32),
        ('MIDISend', [c_uint32, c_uint32, c_void_p], c_int32)]:
    f = getattr(CM, fn)
    f.argtypes, f.restype = args, res
kMIDIPropertyName = c_void_p.in_dll(CM, 'kMIDIPropertyName')
kMIDIPropertyDisplayName = c_void_p.in_dll(CM, 'kMIDIPropertyDisplayName')

# MIDIReadProc(const MIDIPacketList *, void *readProcRefCon, void *srcConnRefCon)
READPROC = ctypes.CFUNCTYPE(None, c_void_p, c_void_p, c_void_p)
NOTIFYPROC = ctypes.CFUNCTYPE(None, c_void_p, c_void_p)


def _name(obj):
    s = c_void_p()
    for prop in (kMIDIPropertyDisplayName, kMIDIPropertyName):
        if CM.MIDIObjectGetStringProperty(obj, prop, byref(s)) == 0 and s.value:
            return str(ObjCInstance(s.value))
    return '?'


class Midi:
    def __init__(self):
        self.replies = queue.Queue()     # (cc, value) on channel 16: 118 ACK, 119 NAK
        self.dest = 0
        self.dest_name = ''
        self.client, self.outp, self.inp = c_uint32(), c_uint32(), c_uint32()
        self._notify = NOTIFYPROC(self._on_notify)
        self._read = READPROC(self._on_read)
        self._sources = set()
        self.lock = threading.Lock()
        CM.MIDIClientCreate(ns('CURSED REMOTE').ptr, self._notify, None, byref(self.client))
        CM.MIDIOutputPortCreate(self.client, ns('out').ptr, byref(self.outp))
        CM.MIDIInputPortCreate(self.client, ns('in').ptr, self._read, None, byref(self.inp))
        self.listen()

    def _on_notify(self, msg, ref):      # (a device came or went: listen to every source again)
        try:
            self.listen()
        except Exception:
            pass

    def listen(self):
        for i in range(CM.MIDIGetNumberOfSources()):
            s = CM.MIDIGetSource(i)
            if s and s not in self._sources:
                CM.MIDIPortConnectSource(self.inp, s, None)
                self._sources.add(s)

    def _on_read(self, pktlist, a, b):
        try:
            n = ctypes.cast(pktlist, POINTER(c_uint32))[0]
            p = pktlist + 4                     # (MIDIPacket: UInt64 time, UInt16 length, data; packed to 4)
            for _ in range(n):
                ln = ctypes.cast(p + 8, POINTER(ctypes.c_uint16))[0]
                data = ctypes.string_at(p + 10, ln)
                for i in range(len(data) - 2):
                    if data[i] == 0xBF and data[i + 1] in (P.ACK_CC, P.NAK_CC):
                        self.replies.put((data[i + 1], data[i + 2]))
                p += (10 + ln + 3) & ~3
        except Exception:
            pass

    def outputs(self):
        return [(CM.MIDIGetDestination(i), _name(CM.MIDIGetDestination(i)))
                for i in range(CM.MIDIGetNumberOfDestinations())]

    def pick_fm1(self):
        """the FM-1 if it's there (its Bluetooth name, or its USB port's)"""
        for d, nm in self.outputs():
            u = nm.upper()
            if 'JIANT' in u or 'FM-1' in u or 'FM1' in u or 'CURSED' in u:
                self.dest, self.dest_name = d, nm
                return True
        return False

    def send(self, data):
        if not self.dest:
            return False
        data = bytes(data)
        buf = ctypes.create_string_buffer(struct.pack('<IQH', 1, 0, len(data)) + data + b'\0' * 4)
        with self.lock:
            return CM.MIDISend(self.outp, self.dest, buf) == 0


midi = Midi()


def cc(num, val):
    midi.send(P.cc(num, val))


# ------------------------------------------------------------------------------------------------ Bluetooth
@on_main_thread
def bluetooth_window():
    """iOS's own Bluetooth MIDI window: tap the FM-1 there, then swipe the window down"""
    load_framework('CoreAudioKit')
    vc = ObjCClass('CABTMIDICentralViewController').new()
    nav = ObjCClass('UINavigationController').alloc().initWithRootViewController_(vc)
    app = ObjCClass('UIApplication').sharedApplication()
    root = app.keyWindow().rootViewController()
    while root.presentedViewController():
        root = root.presentedViewController()

    nav.setModalPresentationStyle_(1)      # (a page sheet: swipe it down to close; no button of our own)
    root.presentViewController_animated_completion_(nav, True, None)


# ------------------------------------------------------------------------------------------------ the look
BG, PANEL, FG, DIM, HOT = '#0b0b0e', '#1a1a20', '#f2f2f2', '#7a7a85', '#ff3b5c'


def label(text, frame, size=12, color=DIM, align=ui.ALIGN_LEFT):
    l = ui.Label(frame=frame, text=text, text_color=color, alignment=align)
    l.font = ('Menlo', size)
    return l


def button(title, frame, action, color=PANEL, size=12):
    b = ui.Button(title=title, action=action, background_color=color, tint_color=FG, corner_radius=8)
    b.font = ('Menlo-Bold', size)
    b.frame = frame                       # (after the title and the font: Pythonista sizes a button to its title)
    return b


class Pad(ui.View):
    """a key held while touched: CC value 127 down, 0 up (the FX pad's latch is the FM-1's)"""
    def __init__(self, title, num, size=11, **kw):
        super().__init__(**kw)
        self.num, self.background_color, self.corner_radius = num, PANEL, 8
        self.l = label(title, (0, 0, self.width, self.height), size, FG, ui.ALIGN_CENTER)
        self.l.flex = 'WH'
        self.l.number_of_lines = 2
        self.add_subview(self.l)

    def touch_began(self, t):
        self.background_color = HOT
        cc(self.num, 127)

    def touch_ended(self, t):
        self.background_color = PANEL
        cc(self.num, 0)


class Fader(ui.View):
    """a slider under its title; its value on the right is a button: tap it and the slider jumps back to rest"""
    def __init__(self, title, num, rest=0, fmt=None, **kw):
        super().__init__(**kw)
        self.num, self.fmt, self.last, self.rest = num, fmt, -1, rest
        self.add_subview(label(title, (0, 0, self.width - 80, 14), 10))
        self.s = ui.Slider(frame=(0, 14, self.width - 76, self.height - 14), value=rest / 127, action=self.moved)
        self.s.flex = 'W'
        self.s.tint_color = HOT
        self.add_subview(self.s)
        self.v = button('', (self.width - 68, 4, 68, self.height - 8), self.reset)
        self.v.flex = 'L'
        self.add_subview(self.v)
        self.show()

    def show(self):
        x = int(round(self.s.value * 127))
        self.v.title = self.fmt(x) if self.fmt else str(x)
        return x

    def moved(self, s):
        x = self.show()
        if x != self.last:
            self.last = x
            if self.num >= 0:
                cc(self.num, x)

    def reset(self, s=None):
        self.s.value = self.rest / 127
        self.s.action(self.s)


# ------------------------------------------------------------------------------------------------ pages
W = min(ui.get_screen_size())


class ConnectPage(ui.View):
    def __init__(self, app, **kw):
        super().__init__(**kw)
        self.app = app
        self.add_subview(button('BLUETOOTH MIDI…', (10, 10, W - 20, 44), lambda s: bluetooth_window(), HOT))
        self.add_subview(label('Turn Bluetooth on on the FM-1 (GLO), tap it in the window, swipe it down.\n'
                               'Then pick it below (USB works too).', (10, 60, W - 20, 50), 11))
        self.subviews[-1].number_of_lines = 3
        self.add_subview(button('REFRESH', (10, 116, W - 20, 36), self.refresh))
        self.tv = ui.TableView(frame=(10, 160, W - 20, 300), background_color=BG)
        self.tv.flex = 'H'
        self.tv.data_source = self.tv.delegate = self
        self.add_subview(self.tv)
        self.rows = []
        self.refresh(None)

    def refresh(self, s):
        midi.listen()
        self.rows = midi.outputs()
        if not midi.dest:
            midi.pick_fm1()
        self.tv.reload()
        self.app.status()

    def tableview_number_of_rows(self, tv, section):
        return len(self.rows)

    def tableview_cell_for_row(self, tv, section, row):
        c = ui.TableViewCell()
        c.background_color = BG
        d, nm = self.rows[row]
        c.text_label.text = ('● ' if d == midi.dest else '   ') + nm
        c.text_label.text_color = HOT if d == midi.dest else FG
        c.text_label.font = ('Menlo', 13)
        return c

    def tableview_did_select(self, tv, section, row):
        midi.dest, midi.dest_name = self.rows[row]
        tv.reload()
        self.app.status()


def tap(n):
    def f(s):
        cc(n, 127)
        cc(n, 0)
    return f


class PlayPage(ui.View):
    """the transport, the tempo, the sections, the mutes: big keys, no scrolling"""
    def __init__(self, **kw):
        super().__init__(**kw)
        g, y, fw = 8, 8, W - 20
        w2, w3, w4 = (fw - g) / 2, (fw - 2 * g) / 3, (fw - 3 * g) / 4
        self.add_subview(button('PLAY / STOP', (10, y, fw, 54), tap(P.CC_PLAY), HOT, 18))
        y += 54 + g
        self.bpm = ui.TextField(frame=(10, y, w2, 48), placeholder='BPM  e.g. 128.5', background_color=PANEL,
                                text_color=FG, alignment=ui.ALIGN_CENTER, keyboard_type=ui.KEYBOARD_DECIMAL_PAD,
                                bordered=False, corner_radius=8)
        self.bpm.font = ('Menlo-Bold', 18)
        self.add_subview(self.bpm)
        self.add_subview(button('SET BPM', (10 + w2 + g, y, w2, 48), self.set_bpm, HOT, 16))
        y += 48 + g
        self.add_subview(Pad('NUDGE\n\u2212', P.CC_NUDGE0, 15, frame=(10, y, w4, 110)))
        self.add_subview(button('TAP', (10 + w4 + g, y, 2 * w4 + g, 110), tap(P.CC_TAP), '#2a2a33', 34))
        self.add_subview(Pad('NUDGE\n+', P.CC_NUDGE0 + 1, 15, frame=(10 + 3 * (w4 + g), y, w4, 110)))
        y += 110 + g
        self.add_subview(button('BPM \u2212 1', (10, y, w2, 40), self.step(-1)))
        self.add_subview(button('BPM + 1', (10 + w2 + g, y, w2, 40), self.step(1)))
        y += 40 + 2 * g
        for i in range(4):
            self.add_subview(button('ABCD'[i], (10 + i * (w4 + g), y, w4, 52), tap(P.CC_SECT0 + i), PANEL, 22))
        y += 52 + g
        for i, (t, n) in enumerate([('FILL', P.CC_FILL), ('BREAK', P.CC_BREAK), ('ALL BACK', P.CC_BACK)]):
            self.add_subview(button(t, (10 + i * (w3 + g), y, w3, 44), tap(n)))
        y += 44 + g + 2
        self.add_subview(label('MUTE', (10, y, 80, 14), 10))
        y += 16
        w6 = (fw - 5 * g) / 6
        for i in range(6):
            self.add_subview(button(str(i + 1), (10 + i * (w6 + g), y, w6, 48), tap(P.CC_MUTE0 + i), PANEL, 18))

    def touch_began(self, t):             # (a touch beside the keys: the keyboard away)
        self.bpm.end_editing()

    def bpm_now(self):
        try:
            return float(self.bpm.text.replace(',', '.'))
        except ValueError:
            return None

    def send_bpm(self, v):
        v = max(40.0, min(240.0, round(v, 1)))
        self.bpm.text = ('%.1f' % v).rstrip('0').rstrip('.')
        midi.send(P.bpm_exact(v))

    def set_bpm(self, s):
        self.bpm.end_editing()
        v = self.bpm_now()
        if v is None:
            self.bpm.text = ''
            return
        self.send_bpm(v)

    def step(self, d):
        def f(s):
            self.bpm.end_editing()
            v = self.bpm_now()
            if v is None:
                tap(P.CC_BPMDN + (d > 0))(s)        # (no tempo typed: the FM-1's own, a whole one)
            else:
                self.send_bpm(v + d)
        return f


class LivePage(ui.View):
    """the set's moves on one page, as the FM-1's HOME > LIVE (cursed-os 0.10.3): the drums' sounds in and out (lit =
    in), the DJ FILTER, the REVERB and DELAY sent from every track, the delay's FEEDBACK; BACK TO NORMAL puts all of it
    back as it was before the first move (on the beat / bar, the FM-1's MUTE Q: a drop)"""
    def __init__(self, **kw):
        super().__init__(**kw)
        g, fw = 6, W - 20
        w4 = (fw - 3 * g) / 4
        self.out = [False] * 8
        self.b = []
        self.normal = None                                # (the phone's copy of NORMAL: the toggles, the faders)
        for i, t in enumerate(P.DRUM_SOUNDS):
            r, c = divmod(i, 4)
            b = button(t.replace('OPEN HAT', 'OPEN'), (10 + c * (w4 + g), 8 + r * 58, w4, 52), self.flip(i), PANEL, 14)
            self.b.append(b)
            self.add_subview(b)
        y = 8 + 2 * 58 + 4
        self.f = []
        for t, n, rest, fm in [('DJ FILTER', P.CC_DJF, 64, lambda x: 'OFF' if x == 64 else str(x - 64)),
                               ('REVERB (all tracks)', P.CC_LVREV, 0, None),
                               ('DELAY (all tracks)', P.CC_LVDLY, 0, None),
                               ('FEEDBACK', P.CC_LVFB, 64, None)]:
            fd = Fader(t, n, rest, fm, frame=(10, y, fw, 50))
            fd.s.action = self.moved(fd)
            self.f.append(fd)
            self.add_subview(fd)
            y += 54
        b = button('BACK TO NORMAL', (10, y + 6, fw, 72), self.back, HOT, 22)
        self.add_subview(b)
        self.add_subview(label('On the FM-1: HOME > LIVE (white 1\u20138 the sounds, OCT\u2212 back to normal).',
                               (10, y + 84, fw, 30), 10))
        self.show()

    def snap(self):
        if self.normal is None:
            self.normal = (list(self.out), [fd.s.value for fd in self.f])

    def moved(self, fd):
        def m(s):
            self.snap()
            fd.moved(s)
        return m

    def show(self):
        for i, b in enumerate(self.b):
            b.background_color = PANEL if self.out[i] else HOT
            b.tint_color = DIM if self.out[i] else FG

    def flip(self, i):
        def f(s):
            self.snap()
            self.out[i] = not self.out[i]
            cc(P.CC_DRUM0 + i, 127 if self.out[i] else 0)
            self.show()
        return f

    def back(self, s):
        cc(P.CC_BACK, 127)
        cc(P.CC_BACK, 0)
        if self.normal is not None:                       # (the phone's keys and faders as the FM-1 will be)
            self.out = list(self.normal[0])
            for fd, v in zip(self.f, self.normal[1]):
                fd.s.value = v
                fd.last = int(round(v * 127))
                fd.show()
            self.normal = None
        self.show()


class ShapeCanvas(ui.View):
    """32 bars, drawn with a finger: up high, down low (the FM-1's LFO USER shape)"""
    def __init__(self, page, **kw):
        super().__init__(**kw)
        self.page, self.background_color, self.corner_radius = page, PANEL, 8

    def draw(self):
        v = self.page.shapes[self.page.cur]
        w, h = self.width / 32.0, self.height
        ui.set_color('#333340')
        ui.fill_rect(0, h / 2, self.width, 1)
        ui.set_color(HOT)
        for i, x in enumerate(v):
            y = h / 2 - x / 127.0 * (h / 2 - 4)
            ui.fill_rect(i * w + 1, min(y, h / 2), w - 2, max(2, abs(y - h / 2)))

    def at(self, t):
        x, y = t.location
        i = max(0, min(31, int(x / self.width * 32)))
        self.page.shapes[self.page.cur][i] = int(max(-127, min(127, (self.height / 2 - y) / (self.height / 2 - 4) * 127)))
        self.set_needs_display()

    def touch_began(self, t):
        self.at(t)

    def touch_moved(self, t):
        self.at(t)

    def touch_ended(self, t):
        self.page.send()


class LfoPage(ui.View):
    """the LFO's USER 1..4 shapes: draw, it is sent when the finger lifts (cursed-os 0.10.2: frame 02)"""
    FILE = os.path.join(os.path.expanduser('~/Documents'), 'cursed_lfo_shapes.json')

    def __init__(self, app, **kw):
        super().__init__(**kw)
        import json
        self.app, self.cur = app, 0
        try:
            with open(self.FILE) as f:
                self.shapes = json.load(f)
            assert len(self.shapes) == 4 and all(len(x) == 32 for x in self.shapes)
        except Exception:
            self.shapes = [[int(127 * math.sin(2 * math.pi * i / 32)) for i in range(32)] for _ in range(4)]
        fw = W - 20
        self.seg = ui.SegmentedControl(frame=(10, 8, fw, 32), segments=['USER1', 'USER2', 'USER3', 'USER4'])
        self.seg.selected_index = 0
        self.seg.action = self.pick
        self.add_subview(self.seg)
        self.cv = ShapeCanvas(self, frame=(10, 48, fw, 200))
        self.add_subview(self.cv)
        g = 6
        w4 = (fw - 3 * g) / 4
        for i, (t, fn) in enumerate([('SINE', lambda i: 127 * math.sin(2 * math.pi * i / 32)),
                                     ('TRI', lambda i: 127 - abs(i - 16) * 254 / 16),
                                     ('RAMP UP', lambda i: -127 + i * 254 / 31),
                                     ('RAMP DN', lambda i: 127 - i * 254 / 31),
                                     ('SQUARE', lambda i: 127 if i < 16 else -127),
                                     ('STEPS', lambda i: [-127, -42, 42, 127][i // 8]),
                                     ('RANDOM', None),
                                     ('INVERT', 'inv')]):
            r, c = divmod(i, 4)
            self.add_subview(button(t, (10 + c * (w4 + g), 258 + r * 44, w4, 38), self.fill(fn)))
        self.add_subview(label('On the FM-1: the LFO page, WAVE = USER1..4. It changes as you draw; it is kept '
                               'once the music stops.', (10, 350, fw, 40), 10))
        self.subviews[-1].number_of_lines = 3

    def pick(self, s):
        self.cur = self.seg.selected_index
        self.cv.set_needs_display()

    def fill(self, fn):
        import random
        def f(s):
            v = self.shapes[self.cur]
            for i in range(32):
                if fn == 'inv':
                    v[i] = -v[i]
                elif fn is None:
                    v[i] = random.randint(-127, 127)
                else:
                    v[i] = int(max(-127, min(127, fn(i))))
            self.cv.set_needs_display()
            self.send()
        return f

    @ui.in_background
    def send(self):
        import json
        try:
            with open(self.FILE, 'w') as f:
                json.dump(self.shapes, f)
        except Exception:
            pass
        drain()
        midi.send(P.lfo_frame(self.cur, self.shapes[self.cur]))
        r = wait_reply(1.0)
        if not r:
            self.app.say('no answer (cursed-os 0.10.2 or later?)')
        elif r[0] != P.ACK_CC:
            self.app.say('the FM-1 said no')


class PadPage(ui.View):
    """the FX pad: an effect runs while its key is touched"""
    def __init__(self, **kw):
        super().__init__(**kw)
        g, y = 8, 10
        pw = (W - 20 - 3 * g) / 4
        for i, t in enumerate(P.PAD_WHITE):
            r, c = divmod(i, 4)
            self.add_subview(Pad(t, P.CC_PADW0 + i, frame=(10 + c * (pw + g), y + r * 64, pw, 56)))
        y += 4 * 64 + 10
        for i, t in enumerate(P.PAD_BLACK):
            r, c = divmod(i, 4)
            self.add_subview(Pad(t, P.CC_PADB0 + i, frame=(10 + c * (pw + g), y + r * 64, pw, 56)))


class MixPage(ui.View):
    """the DJ filter, the isolator, the macros, the morph: tap a value to send it back to rest"""
    def __init__(self, **kw):
        super().__init__(**kw)
        self.sv = ui.ScrollView(frame=(0, 0, self.width, self.height), flex='WH', background_color=BG)
        self.add_subview(self.sv)
        y = 10
        db = lambda x: '%+d dB' % (-25 + (x * 31 + 63) // 127)
        self.f = []
        for t, n, rest, f in [('DJ FILTER   (left: low pass, right: high pass)', P.CC_DJF, 64, lambda x: 'OFF' if x == 64 else str(x - 64)),
                              ('ISOLATOR LOW', P.CC_ISO0, 102, db), ('ISOLATOR MID', P.CC_ISO0 + 1, 102, db),
                              ('ISOLATOR HIGH', P.CC_ISO0 + 2, 102, db),
                              ('MACRO 1', P.CC_MAC0, 0, None), ('MACRO 2', P.CC_MAC0 + 1, 0, None),
                              ('MACRO 3', P.CC_MAC0 + 2, 0, None), ('MACRO 4', P.CC_MAC0 + 3, 0, None),
                              ('MORPH A-B', P.CC_MORPH, 0, lambda x: str((x * 100 + 63) // 127))]:
            fd = Fader(t, n, rest, f, frame=(10, y, W - 20, 50))
            self.f.append(fd)
            self.sv.add_subview(fd)
            y += 56 if n not in (P.CC_DJF, P.CC_ISO0 + 2, P.CC_MAC0 + 3) else 68
        self.sv.add_subview(button('ALL BACK TO REST', (10, y, W - 20, 44), self.all_rest))
        self.sv.content_size = (W, y + 60)

    def all_rest(self, s):
        for fd in self.f:
            fd.reset()


class VoicePage(ui.View):
    def __init__(self, app, **kw):
        super().__init__(**kw)
        self.app = app
        self.add_subview(label('WORDS FOR THE VOICE TRACK (up to 63 letters)', (10, 8, W - 20, 20), 11))
        self.tv = ui.TextView(frame=(10, 30, W - 20, 120), background_color=PANEL, text_color=FG)
        self.tv.font = ('Menlo', 16)
        self.tv.text = 'the cake is a lie'
        self.add_subview(self.tv)
        self.trk = ui.SegmentedControl(frame=(10, 160, W - 20, 32), segments=['FIRST VOICE', '1', '2', '3', '4', '5', '6'])
        self.trk.selected_index = 0
        self.add_subview(self.trk)
        self.add_subview(button('SING THIS', (10, 204, W - 20, 50), self.send, HOT))
        self.add_subview(label('The track sings the words from its next note (LYRIC TEXT). '
                               'Pick VOICE STYLE on the FM-1: HUMAN / ROBOT / GLADOS / TALKBOX.',
                               (10, 262, W - 20, 60), 11))
        self.subviews[-1].number_of_lines = 4

    @ui.in_background
    def send(self, s):
        self.tv.end_editing()
        t = self.trk.selected_index
        drain()
        midi.send(P.text_frame(self.tv.text, 0x7F if t == 0 else t - 1))
        r = wait_reply(1.5)
        if not r:
            self.app.say('no answer (connected? cursed-os 0.9.5 or later?)')
        elif r[0] == P.ACK_CC:
            self.app.say('sent: it sings them from its next note')
        else:
            self.app.say('the FM-1: ' + ('no VOICE track' if r[1] == 1 else P.NAK_TEXT.get(r[1], '?')))


def drain():
    while not midi.replies.empty():
        midi.replies.get_nowait()


def wait_reply(t):
    try:
        return midi.replies.get(timeout=t)
    except queue.Empty:
        return None


# ------------------------------------------------------------------------------------------------ the app
class App(ui.View):
    def __init__(self):
        super().__init__(background_color=BG, name='CURSED REMOTE')
        self.flex = 'WH'
        sw, sh = ui.get_screen_size()
        self.frame = (0, 0, min(sw, sh), max(sw, sh))
        self.tabs = ui.SegmentedControl(frame=(10, 8, W - 20, 32),
                                        segments=['PLAY', 'LIVE', 'FX', 'MIX', 'VOICE', 'LFO', 'LINK'], action=self.show)
        self.tabs.tint_color = HOT
        self.add_subview(self.tabs)
        self.st = label('', (10, 42, W - 20, 18), 10)
        self.add_subview(self.st)
        f = (0, 62, W, self.height - 62)
        self.pages = [PlayPage(frame=f), LivePage(frame=f), PadPage(frame=f), MixPage(frame=f), VoicePage(self, frame=f), LfoPage(self, frame=f),
                      ConnectPage(self, frame=f)]
        for p in self.pages:
            p.flex = 'WH'
            p.background_color = BG
            self.add_subview(p)
        self.tabs.selected_index = 6 if not midi.pick_fm1() else 0
        self.show(None)
        self.status()

    def show(self, s):
        for i, p in enumerate(self.pages):
            p.hidden = i != self.tabs.selected_index

    def status(self):
        self.st.text = ('→ ' + midi.dest_name) if midi.dest else 'not connected'
        self.st.text_color = FG if midi.dest else HOT

    def say(self, t):
        self.st.text = t
        ui.delay(self.status, 3)


App().present('fullscreen', hide_title_bar=False, title_bar_color=BG, title_color=FG)
