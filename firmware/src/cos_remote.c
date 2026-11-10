/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursed-os 0.9.5) the phone remote, the main loop's part (cos.c cos_input: each frame). tools/cursed_remote.py
 * (Pythonista on the iPhone) talks to it over Bluetooth MIDI ("JIANT FM-1"); USB MIDI works the same (a computer).
 *
 * Channel 16's CCs (the ISR queues them: midi_control.c rmt_cc_put); a press is a value of 64 or more, a release less:
 *   20 PLAY / STOP      22..27 MUTE T1..T6 (as HOME's black keys: MUTE Q)   28..31 SECTION A..D (on the bar)
 *   32 FILL             33 BREAK > BUILD > DROP                              34 ALL BACK (LOOP)   35 TAP TEMPO
 *   40..55 the FX pad's white keys 1..16, 56..66 its black keys 1..11: held while 64 or more (FX + OCT+'s latch too)
 *   70 DJ FILTER  71 72 73 the ISOLATOR's LOW MID HIGH  74..77 MACRO 1..4  78 MORPH A-B   (0..127 over the knob's range)
 *   79 BPM (60 + the value)   85..90 the track 1..6 selected
 *   (0.9.7) 80 81 an exact BPM in tenths (400..2400): 80 its high 7 bits first, then 81 its low 7 bits sets it
 *   (0.9.6) 36 37 NUDGE - + (the tempo 3 % slower / faster while 64 or more; 5 s at most)   38 39 BPM - + 1
 * Channel 16's PROGRAM CHANGE stays a section / a project (midi_control.c).
 *
 * SysEx frames F0 7D 63 <command> .. F7 (7D: non-commercial; 63 'c'), one at a time (midi_control.c rmt_f):
 *   01 TEXT  <track 0..5, or 7F: the first VOICE track> <the words, ASCII 32..126, up to 63>: the track's lyric (LYRIC
 *            TEXT, LANG EN if it was off): it sings them from its next note
 *   10..1F   the sampler's (cos_sample.c rmt_sample)
 * Replies on channel 16 (USB and Bluetooth): CC 118 <seq> an ACK (a frame done; seq: the frame's own), CC 119 <code> a
 * NAK (1 not ours, 2 stop first, 3 no room, 4 bad data, 5 flash failed, 6 out of order) */
#define RMT_ACK 118u
#define RMT_NAK 119u
enum { RN_NONE, RN_BAD, RN_STOP, RN_ROOM, RN_DATA, RN_FLASH, RN_ORDER };
static uint8_t rmt_bpm_hi;                               /* (0.9.7) CC 80's: an exact BPM's high bits */
static int8_t rmt_nudge;                                 /* (0.9.6) the remote's NUDGE held: -1, +1 */
static uint32_t rmt_nudge_ms;                            /* .. since when (5 s at most: a link lost while held) */
static uint32_t rmt_pad;                                 /* the pad's keys the remote holds (bit k: white 0..15, black 16..26) */
static void rmt_reply(uint32_t cc, uint32_t v) { midi_out_event(0x0Bu | 0xBFu << 8 | (cc & 127u) << 16 | (v & 127u) << 24); }
static int32_t rmt_scale(uint32_t id, uint32_t v)        /* 0..127 over a cos knob's range */
{
    int16_t *vp;
    const param_desc_t *d = cos_cdesc(id, &vp);
    return d->min + ((int32_t)v * (d->max - d->min) + 63) / 127;
}
static void rmt_mute(uint32_t j)                         /* (HOME's black key j) */
{
    if (j >= trk_live) return;
    if (!cos_mq) {
        trk[j].p[P_MUTE] = (int16_t)!trk[j].p[P_MUTE];
        cos_say(trk[j].p[P_MUTE] ? "MUTED" : "UNMUTED");
    } else {
        fm1_irq_off();
        live_qmute ^= (uint8_t)(1u << j);
        fm1_irq_on();
    }
}
static void rmt_pad_key(uint32_t k, int down)            /* the FX pad: white 0..15, black 16..26 */
{
    uint32_t bit = 1u << k;
    if (!!(rmt_pad & bit) == !!down) return;             /* (a CC repeated: one press) */
    rmt_pad = down ? rmt_pad | bit : rmt_pad & ~bit;
    fm1_irq_off();                                       /* (the keys' ISR presses them too) */
    cfx_press(k < 16u ? CFX_W[k] : CFX_B[k - 16u], down);
    fm1_irq_on();
}
static void rmt_bpm(int32_t v)                           /* the tempo, in tenths (as the BPM knob) */
{
    if (cos_follows()) { cos_say("EXT CLOCK LEADS"); return; }
    v = clamp(v, 400, 2400);
    song.g[G_BPM] = (int16_t)(v / 10);
    bpm_frac = (uint8_t)(v % 10);
}
static __attribute__((noinline)) void rmt_cc(uint32_t cc, uint32_t v)
{
    int on = v >= 64u;
    if (cc >= 40u && cc <= 66u) { rmt_pad_key(cc - 40u, on); return; }
    if (cc >= 70u && cc <= 78u) {
        static const uint8_t ID[9] = {CC_DJF, CC_EQL, CC_EQM, CC_EQH, CC_MAC0, CC_MAC0 + 1u, CC_MAC0 + 2u, CC_MAC3, CC_MOPOS};
        cos_cput(ID[cc - 70u], rmt_scale(ID[cc - 70u], v));
        return;
    }
    if (cc == 79u) { rmt_bpm((int32_t)(60u + v) * 10); return; }
    if (cc == 80u) { rmt_bpm_hi = (uint8_t)v; return; }  /* (0.9.7) an exact BPM, tenths: 80 then 81 */
    if (cc == 81u) { rmt_bpm((int32_t)((uint32_t)rmt_bpm_hi << 7 | v)); return; }
    if (cc == 36u || cc == 37u) {                        /* (0.9.6) NUDGE - / +: while held */
        int8_t d = (int8_t)(cc == 37u ? 1 : -1);
        if (on) { rmt_nudge = d; rmt_nudge_ms = fm1_ms; }
        else if (rmt_nudge == d) rmt_nudge = 0;
        return;
    }
    if ((cc == 38u || cc == 39u) && on) {                /* (0.9.6) BPM - / + 1 */
        rmt_bpm(song.g[G_BPM] * 10 + bpm_frac % 10u + (cc == 39u ? 10 : -10));
        return;
    }
    if (!on) return;                                     /* (the rest: presses) */
    if (cc == 20u) transport_req = song.playing ? 2u : 1u;
    else if (cc >= 22u && cc <= 27u) rmt_mute(cc - 22u);
    else if (cc >= 28u && cc <= 31u) { if (!cz.pn_ok) cos_proj_scan(); cos_section(cc - 28u); }
    else if (cc == 32u) { fill_req = 1; cos_say(song.playing ? "FILL" : "FILL: PLAY FIRST"); }
    else if (cc == 33u) { bd_req = 1; cos_say("BREAKDOWN"); }
    else if (cc == 34u) { live_return((1u << NTRK) - 1u); cos_say("ALL BACK"); }
    else if (cc == 35u) cos_tap(fm1_ms);
    else if (cc >= 85u && cc <= 90u && cc - 85u < trk_live) track_select(cc - 85u);
}
/* TEXT: the words a VOICE track sings */
static __attribute__((noinline)) uint32_t rmt_text(const uint8_t *a, uint32_t n)
{
    char b[LYR_LEN];
    uint32_t t = n ? a[0] : 0x7Fu, i;
    if (t == 0x7Fu)
        for (t = 0; t < NTRK && trk_eng(&trk[t]) != ENGI_VOICE; t++) {}
    if (t >= NTRK || trk_eng(&trk[t]) != ENGI_VOICE) { cos_say("NO VOICE TRACK"); return RN_BAD; }
    for (i = 0; i + 1u < n && i + 1u < LYR_LEN; i++) b[i] = (char)(a[i + 1u] >= 32u && a[i + 1u] <= 126u ? a[i + 1u] : ' ');
    b[i] = 0;
    lyr_set(t, b);
    trk[t].p[P_VLYR] = 0;                                /* (LYRIC TEXT: the track's own words) */
    if (!trk[t].p[P_VLANG]) trk[t].p[P_VLANG] = LG_EN;
    cos_say("WORDS FROM THE PHONE");
    return RN_NONE;
}
static uint32_t rmt_sample(uint32_t cmd, const uint8_t *a, uint32_t n);   /* (cos_sample.c) */
/* a frame (after F0 7D 63; without F7) */
static __attribute__((noinline)) void rmt_frame(const uint8_t *f, uint32_t n)
{
    uint32_t rc, seq = n >= 2u ? f[1] : 0u;
    if (!n) return;
    if (f[0] == 0x01u) rc = rmt_text(f + 1, n - 1u);
    else if (f[0] >= 0x10u && f[0] <= 0x1Fu) rc = rmt_sample(f[0], f + 1, n - 1u);
    else rc = RN_BAD;
    if (f[0] == 0x01u) seq = 0;
    if (rc) rmt_reply(RMT_NAK, rc);
    else rmt_reply(RMT_ACK, seq);
}
/* each frame: the CCs that came, the frame that waits */
#include "cos_sample.c"                                 /* (the sampler's frames, its memory's directory) */
static __attribute__((noinline)) void cos_remote_poll(void)
{
    static uint8_t booted;
    uint32_t k;
    if (!booted) { smp_boot(); booted = 1; }             /* (once: the sampler's directory found) */
    for (k = 0; k < 16u && rmt_cc_r != rmt_cc_w; k++) {
        uint32_t e;
        RING_PUBLISH();
        e = rmt_cc_q[rmt_cc_r % RMT_CCN];
        rmt_cc_r++;
        rmt_cc(e >> 8, e & 127u);
    }
    if (rmt_fready) {
        RING_PUBLISH();
        rmt_frame(rmt_f + 2, rmt_fn - 2u);
        RING_PUBLISH();
        rmt_fready = 0;
    }
}
