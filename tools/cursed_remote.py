# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 cursedsound
"""CURSED REMOTE: the JIANT FM-1 (cursed-os 0.9.5+) from an iPhone, in Pythonista 3.

Put this file and cursed_remote_proto.py in the same Pythonista folder and run this one.

  LINK     pair the FM-1 over Bluetooth MIDI (iOS's own Bluetooth MIDI window), pick it as the output
  PLAY     PLAY, an exact BPM (128.5), TAP, NUDGE (hold), BPM -/+ 1, sections, FILL / BREAK / ALL BACK, mutes
  FX       the FX pad (hold a key)
  MIX      DJ filter, isolator, macros, morph: tap a value and it jumps back to rest
           (all channel 16 CCs: cos_remote.c; NUDGE and BPM -/+ need cursed-os 0.9.6, the exact BPM 0.9.7)
  VOICE    type words: the first VOICE track sings them from its next note
  SAMPLE   a sound from Files (wav, aif, mp3, m4a, caf) or the mic: trim, reverse, normalise, and send it into
           one of the 16 slots (the SAMPLER engine plays it). Stop the music on the FM-1 first.

iOS doesn't let a script record what another app is playing: save the sound to Files (or play it out loud and
use the mic). Works over USB MIDI too (a camera adapter / USB-C): pick the FM-1's USB port as the output.
"""
import ctypes, os, queue, struct, sys, threading, time
from ctypes import c_void_p, c_uint32, c_int32, c_uint64, c_float, POINTER, byref

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cursed_remote_proto as P

import ui, dialogs, sound
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


# the sampler's room: 30 sectors of 4096 bytes, 132 bytes per 256 samples
MAX_SAMPLES = 30 * 4096 // P.BLKB * P.BLK


def decode_file(path):
    """any audio file iOS reads -> (mono int16 list, rate)"""
    load_framework('AVFoundation')
    AVAudioFile = ObjCClass('AVAudioFile')
    AVAudioPCMBuffer = ObjCClass('AVAudioPCMBuffer')
    f = AVAudioFile.alloc().initForReading_error_(nsurl(path), None)
    if not f:
        raise IOError("can't read that file")
    fmt = f.processingFormat()
    rate = int(fmt.sampleRate())
    chans = int(fmt.channelCount())
    n = min(int(f.length()), rate * 60)                  # (a minute at most: trimmed later)
    buf = AVAudioPCMBuffer.alloc().initWithPCMFormat_frameCapacity_(fmt, n)
    f.readIntoBuffer_frameCount_error_(buf, n, None)
    got = int(buf.frameLength())
    chp = ctypes.cast(buf.floatChannelData().ptr if hasattr(buf.floatChannelData(), 'ptr') else buf.floatChannelData(),
                      POINTER(POINTER(c_float)))
    ch = [chp[c][:got] for c in range(chans)]
    if chans == 1:
        mono = ch[0]
    else:
        mono = [sum(v) / chans for v in zip(*ch)]
    return [int(max(-1.0, min(1.0, x)) * 32767) for x in mono], rate


class SamplerPage(ui.View):   # (Pythonista: ui.ScrollView can't be subclassed: one inside)
    def __init__(self, app, **kw):
        super().__init__(**kw)
        self.sv = ui.ScrollView(frame=(0, 0, self.width, self.height), flex='WH', background_color=BG)
        ui.View.add_subview(self, self.sv)
        self.app, self.src, self.rate, self.path, self.rec, self.busy = app, None, 0, None, None, False
        g = 6
        bw = (W - 20 - g) / 2
        self.sv.add_subview(button('FROM FILES…', (10, 8, bw, 44), self.pick))
        self.recb = button('● RECORD MIC', (10 + bw + g, 8, bw, 44), self.record)
        self.sv.add_subview(self.recb)
        self.info = label('no sound yet', (10, 58, W - 20, 36), 12, FG)
        self.info.number_of_lines = 2
        self.sv.add_subview(self.info)
        self.wave = ui.ImageView(frame=(10, 96, W - 20, 70), background_color=PANEL)
        self.sv.add_subview(self.wave)
        self.start = Fader('START', -1, 0, frame=(10, 168, W - 20, 40))
        self.length = Fader('LENGTH', -1, 127, frame=(10, 208, W - 20, 40))
        for fd in (self.start, self.length):
            fd.s.action = self.trim_moved
            self.sv.add_subview(fd)
        self.rev = ui.Switch(frame=(10, 252, 60, 32)); self.sv.add_subview(self.rev)
        self.sv.add_subview(label('REVERSE', (70, 252, 80, 32), 11))
        self.nrm = ui.Switch(frame=(150, 252, 60, 32), value=True); self.sv.add_subview(self.nrm)
        self.sv.add_subview(label('NORMALISE', (210, 252, 90, 32), 11))
        self.lng = ui.Switch(frame=(10, 290, 60, 32)); self.lng.action = self.trim_moved; self.sv.add_subview(self.lng)
        self.sv.add_subview(label('LONG (11 kHz: twice the time, duller)', (70, 290, W - 80, 32), 11))
        self.sv.add_subview(button('▶ HEAR IT', (10, 330, W - 20, 36), self.hear))
        self.sv.add_subview(label('SLOT', (10, 374, 60, 16), 10))
        self.slot = ui.SegmentedControl(frame=(10, 390, W - 20, 30), segments=[str(i + 1) for i in range(8)])
        self.slot2 = ui.SegmentedControl(frame=(10, 424, W - 20, 30), segments=[str(i + 9) for i in range(8)])
        self.slot.selected_index, self.slot2.selected_index = 0, -1
        self.slot.action = lambda s: setattr(self.slot2, 'selected_index', -1)
        self.slot2.action = lambda s: setattr(self.slot, 'selected_index', -1)
        self.sv.add_subview(self.slot); self.sv.add_subview(self.slot2)
        self.nm = ui.TextField(frame=(10, 462, W - 20, 34), placeholder='NAME (8 letters)', background_color=PANEL,
                                 text_color=FG, autocapitalization_type=ui.AUTOCAPITALIZE_ALL)
        self.sv.add_subview(self.nm)
        self.sendb = button('SEND TO THE FM-1', (10, 506, W - 20, 50), self.send, HOT)
        self.sv.add_subview(self.sendb)
        self.sv.add_subview(button('DELETE THIS SLOT', (10, 564, W - 20, 34), self.delete))
        self.prog = label('', (10, 604, W - 20, 40), 11, FG)
        self.prog.number_of_lines = 2
        self.sv.add_subview(self.prog)
        self.sv.add_subview(label('Stop the music on the FM-1 first. A sound takes a minute or so over Bluetooth. '
                               'Then on a track: ENGINE SAMPLER, SAMPLE = the slot.', (10, 646, W - 20, 50), 10))
        self.sv.subviews[-1].number_of_lines = 3
        self.sv.content_size = (W, 710)

    # -- the sound
    def load(self, samples, rate, nm):
        self.src, self.rate = samples, rate
        self.nm.text = ''.join(c for c in os.path.splitext(os.path.basename(nm))[0].upper() if c.isalnum())[:8]
        self.trim_moved(None)

    def pick(self, s):
        p = dialogs.pick_document(types=['public.audio'])
        if not p:
            return
        self.info.text = 'reading…'
        try:
            smp, rate = decode_file(p)
        except Exception as e:
            self.info.text = 'could not read it: %s' % e
            return
        self.path = p
        self.load(smp, rate, p)

    def record(self, s):
        p = os.path.join(os.path.expanduser('~/Documents'), 'cursed_mic.m4a')
        if self.rec is None:
            self.rec = sound.Recorder(p)
            self.rec.record()
            self.recb.title, self.recb.background_color = '■ STOP', HOT
            self.info.text = 'recording… (11 s at most is kept)'
        else:
            self.rec.stop()
            self.rec = None
            self.recb.title, self.recb.background_color = '● RECORD MIC', PANEL
            try:
                smp, rate = decode_file(p)
            except Exception as e:
                self.info.text = 'could not read the recording: %s' % e
                return
            self.path = p
            self.load(smp, rate, 'MIC')

    def rate_out(self):
        return 11025 if self.lng.value else 22050

    def cut(self):
        """the trimmed, resampled, (reversed, normalised) sound to send"""
        if not self.src:
            return []
        n = len(self.src)
        a = int(self.start.s.value * n)
        most = MAX_SAMPLES * self.rate // self.rate_out()
        ln = max(1, min(n - a, most, int(self.length.s.value * most)))
        x = self.src[a:a + ln]
        if self.rev.value:
            x = x[::-1]
        x = P.resample(x, self.rate, self.rate_out())
        if self.nrm.value:
            x = P.normalize(x)
        return x[:MAX_SAMPLES]

    def trim_moved(self, s):
        if not self.src:
            return
        n = len(self.src)
        most = MAX_SAMPLES * self.rate // self.rate_out()
        a = int(self.start.s.value * n)
        ln = max(1, min(n - a, most, int(self.length.s.value * most)))
        self.start.v.title = '%.2fs' % (a / self.rate)
        self.length.v.title = '%.2fs' % (ln / self.rate)
        self.info.text = '%.1f s at %d Hz; sending %.2f s (%.1f s at most)' % (
            n / self.rate, self.rate, ln / self.rate, MAX_SAMPLES / self.rate_out())
        self.draw_wave(a, ln)

    def draw_wave(self, a, ln):
        w, h = self.wave.width, self.wave.height
        n = len(self.src)
        step = max(1, n // int(w))
        with ui.ImageContext(w, h) as ctx:
            ui.set_color(PANEL); ui.fill_rect(0, 0, w, h)
            ui.set_color('#333340'); ui.fill_rect(a * w / n, 0, ln * w / n, h)
            ui.set_color(HOT)
            for i in range(int(w)):
                seg = self.src[i * step:(i + 1) * step]
                if seg:
                    m = max(abs(min(seg)), abs(max(seg))) / 32768
                    ui.fill_rect(i, h / 2 - m * h / 2, 1, max(1, m * h))
            self.wave.image = ctx.get_image()

    def hear(self, s):
        x = self.cut()
        if not x:
            return
        import wave
        p = os.path.join(os.path.expanduser('~/Documents'), 'cursed_preview.wav')
        with wave.open(p, 'wb') as wf:
            wf.setnchannels(1); wf.setsampwidth(2); wf.setframerate(self.rate_out())
            wf.writeframes(struct.pack('<%dh' % len(x), *x))
        self.player = sound.Player(p)
        self.player.play()

    def slot_no(self):
        return self.slot.selected_index if self.slot.selected_index >= 0 else 8 + max(0, self.slot2.selected_index)

    # -- sending, a frame at a time: each waits for its ACK (resent on a timeout)
    @ui.in_background
    def send(self, s):
        if self.busy:
            return
        x = self.cut()
        if not x:
            self.prog.text = 'pick or record a sound first'
            return
        if not midi.dest:
            self.prog.text = 'not connected: LINK first'
            return
        self.busy = True
        try:
            frames = P.sample_frames(self.slot_no(), self.nm.text or 'SOUND', self.rate_out(), x)
            t0 = time.time()
            drain()
            for i, (seq, fr) in enumerate(frames):
                ok = False
                for attempt in range(6):
                    midi.send(fr)
                    end = time.time() + (3.0 if i == len(frames) - 1 or i == 0 else 1.2)
                    while time.time() < end:
                        r = wait_reply(end - time.time())
                        if not r:
                            break
                        if r[0] == P.NAK_CC:
                            self.prog.text = 'the FM-1 said no: ' + P.NAK_TEXT.get(r[1], str(r[1]))
                            return
                        if r[1] == seq:
                            ok = True
                            break
                    if ok:
                        break
                if not ok:
                    self.prog.text = 'no answer from the FM-1 (frame %d of %d)' % (i + 1, len(frames))
                    return
                if i % 4 == 0 or i == len(frames) - 1:
                    self.prog.text = 'sending… %d%%  (%.0f s)' % ((i + 1) * 100 // len(frames), time.time() - t0)
            self.prog.text = 'in slot %d: %.2f s. On the FM-1: ENGINE SAMPLER, SAMPLE %d' % (
                self.slot_no() + 1, len(x) / self.rate_out(), self.slot_no() + 1)
        finally:
            self.busy = False

    @ui.in_background
    def delete(self, s):
        if dialogs.alert('Delete slot %d on the FM-1?' % (self.slot_no() + 1), '', 'Delete') != 1:
            return
        drain()
        midi.send(P.delete_frame(self.slot_no()))
        r = wait_reply(3.0)
        self.prog.text = ('slot %d emptied' % (self.slot_no() + 1) if r and r[0] == P.ACK_CC else
                          'the FM-1: ' + P.NAK_TEXT.get(r[1], '?') if r else 'no answer')


# ------------------------------------------------------------------------------------------------ the app
class App(ui.View):
    def __init__(self):
        super().__init__(background_color=BG, name='CURSED REMOTE')
        self.flex = 'WH'
        sw, sh = ui.get_screen_size()
        self.frame = (0, 0, min(sw, sh), max(sw, sh))
        self.tabs = ui.SegmentedControl(frame=(10, 8, W - 20, 32),
                                        segments=['PLAY', 'FX', 'MIX', 'VOICE', 'SAMPLE', 'LINK'], action=self.show)
        self.tabs.tint_color = HOT
        self.add_subview(self.tabs)
        self.st = label('', (10, 42, W - 20, 18), 10)
        self.add_subview(self.st)
        f = (0, 62, W, self.height - 62)
        self.pages = [PlayPage(frame=f), PadPage(frame=f), MixPage(frame=f), VoicePage(self, frame=f), SamplerPage(self, frame=f),
                      ConnectPage(self, frame=f)]
        for p in self.pages:
            p.flex = 'WH'
            p.background_color = BG
            self.add_subview(p)
        self.tabs.selected_index = 5 if not midi.pick_fm1() else 0
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
