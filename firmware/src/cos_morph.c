/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursedsound) cursed-os's MORPH (HOME > MORPH, included in cos.c): two snapshots, A and B, of what a set is played
 * with, and a glide between them, like a crossfader between two scenes:
 *   each track's LEVEL, DELAY and REVERB sends; the macros M1..M4 (what each sound routes them to: its filter, ...);
 *   the delay's FEEDBK, the reverb's SIZE, the DJ FILTER, RUMBLE's LEVEL.
 * HOME's white 13 (A) and 14 (B): empty, a press stores the music as it is; stored, a press glides there over BARS
 * (from the next bar, while playing; CUT: on the next bar at once); HOME held + the key: stored again. KNOB 1 (A-B) is
 * the crossfader by hand. Only what A and B hold differently is moved: the rest stays as the knobs leave it. A project
 * keeps both ('MO', project.c proj_mo). Main loop only (mph_tick, from ui_input): the knobs' values, as a turn sets them.
 * Included by fx.c (project.c's tests take it without cos.c); the keys' part (mph_key) is cos.c's */
#define MO_TP 3u                                /* a track's: LEVEL DLY REV */
#define MO_GN 4u                                /* the globals': FEEDBK SIZE DJF, RUMBLE */
#define MO_N (NTRK * MO_TP + MO_GN + 4u)        /* .. and M1..M4 */
static const uint8_t MO_TPAR[MO_TP] = {P_LEVEL, P_DLY, P_REV};
static const uint8_t MO_BARS[6] = {0, 1, 2, 4, 8, 16};   /* BARS: CUT 1 2 4 8 16 */
static struct {
    int16_t s[2][MO_N];                         /* A, B */
    uint8_t set;                                /* which are stored (bit 0 A, bit 1 B) */
    uint8_t bars;                               /* BARS: MO_BARS's index */
    int16_t pos;                                /* where it is: 0 A .. 1000 B */
    /* a glide: from, to; where it starts (ms_clock, playing; fm1_ms stopped), its length */
    int16_t from, to;
    uint8_t gl, run, rp;                        /* a glide asked for; started; on which clock (1: the transport's) */
    uint32_t t0, len;
} mph = {.bars = 3};

/* the music's values now, in the snapshot's order */
static void mph_read(int16_t *v)
{
    uint32_t k, i, n = 0;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < MO_TP; i++)
            v[n++] = trk[k].p[MO_TPAR[i]];
    v[n++] = song.g[G_DFDBK];
    v[n++] = song.g[G_RSIZE];
    v[n++] = song.g[G_DJF];
    v[n++] = rmb_v[0];
    for (i = 0; i < 4u; i++)
        v[n++] = macro_v[i];
}
/* at pos (0 .. 1000) between A and B: only what the two hold differently */
static void mph_apply(int32_t pos)
{
    uint32_t k, i, n = 0;
    const int16_t *a = mph.s[0], *b = mph.s[1];
#define MO_AT(n) ((int16_t)(a[n] + ((int32_t)(b[n] - a[n]) * pos + (b[n] >= a[n] ? 500 : -500)) / 1000))
    if ((mph.set & 3u) != 3u) return;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < MO_TP; i++, n++)
            if (a[n] != b[n]) trk[k].p[MO_TPAR[i]] = MO_AT(n);
    if (a[n] != b[n]) song.g[G_DFDBK] = MO_AT(n);
    n++;
    if (a[n] != b[n]) song.g[G_RSIZE] = MO_AT(n);
    n++;
    if (a[n] != b[n]) song.g[G_DJF] = MO_AT(n);
    n++;
    if (a[n] != b[n]) rmb_v[0] = (uint8_t)clamp(MO_AT(n), 0, 100);
    n++;
    for (i = 0; i < 4u; i++, n++)
        if (a[n] != b[n]) macro_v[i] = (uint8_t)clamp(MO_AT(n), 0, 127);
#undef MO_AT
}
static uint32_t mph_bar(void) { return beat_samples() * 4u; }
/* KNOB 1: by hand (0 .. 100), a glide let go */
static void mph_hand(int32_t v)
{
    mph.gl = mph.run = 0;
    mph.pos = (int16_t)(clamp(v, 0, 100) * 10);
    mph_apply(mph.pos);
}
/* the main loop: a glide's next step */
static void mph_tick(void)
{
    uint32_t bar, now, l, t;
    int32_t p;
    if (!mph.gl) return;
    if (mph.run && mph.rp != (song.playing != 0)) {  /* (the transport started or stopped meanwhile: on from here) */
        mph.from = mph.pos;
        mph.run = 0;
    }
    bar = mph_bar();
    l = (uint32_t)MO_BARS[mph.bars % 6u];
    if (!mph.run) {                               /* (it starts on the next bar, playing; stopped: now) */
        if (song.playing) {
            mph.t0 = (ms_clock / (bar ? bar : 1u) + 1u) * bar;
            mph.len = l * bar;
        } else {
            mph.t0 = fm1_ms;
            mph.len = (uint32_t)((uint64_t)l * bar * 1000u / 44100u);
        }
        mph.run = 1;
        mph.rp = song.playing != 0;
    }
    now = song.playing ? ms_clock : fm1_ms;
    if ((int32_t)(now - mph.t0) < 0) return;      /* (the bar not yet in) */
    t = now - mph.t0;
    if (!mph.len || t >= mph.len) {
        p = mph.to;
        mph.gl = mph.run = 0;
    } else {
        p = mph.from + (int32_t)((int64_t)(mph.to - mph.from) * t / mph.len);
    }
    mph.pos = (int16_t)p;
    mph_apply(p);
}
