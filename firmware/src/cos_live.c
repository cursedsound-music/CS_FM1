/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound */
/* (cursedsound) cursed-os's live layer, the ISR's part (included at the end of seq.c; cosfx.c's cfx_run calls live_tick
 * each block). Everything lands on the grid: the next block that starts a beat (or a bar) is where it goes in, so a
 * mute let go on the bar plays the bar's first note.
 *   MUTES, SOLO   HOME's black keys 1..6 (HOME held: SOLO) asked for (live_qmute, live_qsolo): in NOW, on the next beat
 *                 or on the next bar (MUTE Q)
 *   FILL          HOME's black key 7: the drum tracks' last half bar rolled (snare eighths, then sixteenths, the clap
 *                 on the last two, the kit's own hits kept), the bar after back as it was; held: every bar. Steps with
 *                 a FILL condition play in it too (seq_fill)
 *   BREAKDOWN     HOME's black key 8: on the next bar the BREAK (KICK KILL and LOW KILL, the FX pad's), again: the
 *                 BUILD on the next bar (four bars of KICK KILL, HP RISER, NOISE RISER, a fill in the last), then the
 *                 DROP on the bar after them (pressed during the build: on the next bar) */
enum { BD_OFF, BD_ARM_BREAK, BD_BREAK, BD_ARM_BUILD, BD_BUILD, BD_ARM_DROP };
/* (cursed-os 0.20.0, GG: live with a Syntakt and CDJs) the STAGE moves:
 *   MIX IN / OUT  HOME + black 7 / 8: the tracks in (the muted ones unmuted) or out one by one over MIX BARS, from the
 *                 next bar (CHANGES PHRASE: the next phrase): in, the pads first and the kick last (the drop); out, the
 *                 kick first and the pads last. Asked again while it runs: stopped where it is
 *   BASS SWAP     HOME + black 9: the FM-1's lows out (the isolator's LOW KILL, latched) on the next bar, again: in; for
 *                 trading the low end with a record in the mixer
 *   MONO BASS     GLO > STAGE 2: below 80 / 120 / 160 Hz both sides as one (mono_bass_block, from fx.c's master) */
static volatile uint8_t mx_req, bsw_req, bsw_on;   /* main: MIX 1 IN 2 OUT asked; BASS SWAP asked; ISR: the lows out */
static uint8_t mx_trk[NTRK], mx_n, mx_i, mx_bar, mx_wait, mx_len;   /* ISR: the tracks in their order, how far, the bars */
static const uint8_t MX_IN[NTRK] = {4, 5, 0, 2, 3, 1};   /* (T5 T6 pads, T1 drums, T3 acid, T4 bass, T2 the kick last) */
static volatile uint8_t cos_mq = 2;           /* main: MUTE Q, 0 NOW 1 BEAT 2 BAR (ui_polls: the settings') */
static volatile uint8_t live_qmute;           /* main: the tracks whose mute turns over next (bit k) */
static volatile uint8_t live_qback;           /* (0.10.3) main: BACK TO NORMAL asked (the knobs: on the beat / bar, as the
                                               * mutes); the ISR: 2 when it is time (cos.c cos_back_poll) */
static volatile uint8_t live_qlane;           /* (0.10.0) main: the kit's sounds whose mute turns over next (bit l: lane l,
                                               * eng_drum.c DXM_LANE), on the same beat / bar as the tracks' */
static volatile uint8_t live_qsolo, live_qsolo_on;   /* main: the solo asked for (live_qsolo_on: asked) */
static volatile uint8_t fill_req, fill_on;    /* main: a FILL asked for; ISR: the fill bar running */
static volatile uint8_t bd_req, bd_kill, bd_state, bd_bars;   /* main: a press, everything off; ISR: where it is */
#define FILL_N 8u
static struct { uint8_t idx, hit, acc, time; } fill_keep[NTRK][FILL_N];   /* each step the fill touched: where, the hits
                                                * and accents it added, the step's TIME before (an empty one made a hit) */
static uint8_t fill_trk;

/* the fill written over the drum tracks' next eight sixteenths (the bar's second half), the steps kept */
static __attribute__((noinline)) void fill_write(void)
{
    uint32_t k, j;
    fill_trk = 0;
    for (k = 0; k < trk_live && k < NTRK; k++) {
        track_t *t = &trk[k];
        uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
        if (!drum_track(t) || t->p[P_SDIV] != 2 || t->p[P_MUTE] || len < FILL_N)
            continue;
        fill_trk |= (uint8_t)(1u << k);
        for (j = 0; j < FILL_N; j++) {
            uint32_t i = seq_src(t, (t->seq_idx + 1u + j) % len);   /* (where the step it plays is read: SHIFT's offset) */
            step_t *s = &t->step[i];
            uint8_t add = (uint8_t)(j >= 4u || !(j & 1u) ? 1u << 1 : 0u);   /* SD: eighths, then sixteenths */
            if (j >= 6u) add |= 1u << 2;                                    /* CP on the last two */
            fill_keep[k][j].idx = (uint8_t)i;
            fill_keep[k][j].time = s->time;
            if (s->time != ST_NOTE) {                    /* (an empty step: a hit of its own) */
                memset(s, 0, sizeof *s);
                s->vel = 100;
            }
            fill_keep[k][j].hit = (uint8_t)(add & ~s->hit);   /* (only what it adds comes off again) */
            fill_keep[k][j].acc = (uint8_t)(j >= 6u ? add & ~s->acc : 0u);
            s->time = ST_NOTE;
            s->hit |= add;
            if (j >= 6u) s->acc |= add;
        }
    }
    fill_on = 1;
    seq_fill = 1;
}
static __attribute__((noinline)) void fill_undo(void)
{
    uint32_t k, j;
    for (k = 0; k < NTRK; k++)                      /* (only the fill's own hits off: an edit or a MUTATE made meanwhile
                                                     * stays) */
        if ((fill_trk >> k) & 1u)
            for (j = 0; j < FILL_N; j++) {
                step_t *s = &trk[k].step[fill_keep[k][j].idx];
                s->hit &= (uint8_t)~fill_keep[k][j].hit;
                s->acc &= (uint8_t)~fill_keep[k][j].acc;
                if (fill_keep[k][j].time != ST_NOTE && !s->hit && !s->n)
                    s->time = fill_keep[k][j].time;     /* (a step that was empty: empty again) */
            }
    fill_trk = 0;
    fill_on = 0;
    seq_fill = 0;
}

/* (GLO > LINK's VJ CH) an event out as a note, on and off at once, for visuals: 60 BREAK, 61 BUILD, 62 DROP, 63 FILL,
 * 64 .. 67 the section A .. D going in; CC 20 the BUILD's progress 0 .. 127, a beat at a time (USB, Bluetooth) */
enum { VJ_BREAK = 60, VJ_BUILD, VJ_DROP, VJ_FILL, VJ_SECT };
static void vj_note(uint32_t n)
{
    if (!cos_vj_ch) return;
    mo_send(0x90u | (cos_vj_ch - 1u), n, 127);
    mo_send(0x80u | (cos_vj_ch - 1u), n, 0);
}
static void vj_cc(uint32_t c, uint32_t v)
{
    if (cos_vj_ch) mo_send(0xB0u | (cos_vj_ch - 1u), c, v > 127u ? 127u : v);
}
static int8_t vj_sect = -1;

/* the BREAKDOWN's effects (cosfx.c cfx_auto) as its state has them */
static uint32_t bd_bits(uint32_t s)
{
    return s == BD_BREAK || s == BD_ARM_BUILD ? CX_BIT(CX_KICK) | CX_BIT(CX_LOWK)
         : s == BD_BUILD || s == BD_ARM_DROP ? CX_BIT(CX_KICK) | CX_BIT(CX_HPR) | CX_BIT(CX_NOISE) : 0u;
}
static void bd_step(uint32_t bar)                 /* a press (bar: 0 stopped, at once) or a bar line */
{
    if (bar) {
        uint32_t was = bd_state;
        if (bd_state == BD_ARM_BREAK) bd_state = BD_BREAK;
        else if (bd_state == BD_ARM_BUILD) { bd_state = BD_BUILD; bd_bars = 4; }
        else if (bd_state == BD_ARM_DROP) bd_state = BD_OFF;
        else if (bd_state == BD_BUILD && bd_bars && !--bd_bars) bd_state = BD_OFF;   /* (the four bars: the DROP) */
        if (bd_state != was)
            vj_note(bd_state == BD_BREAK ? VJ_BREAK : bd_state == BD_BUILD ? VJ_BUILD : VJ_DROP);
        if (bd_state == BD_BUILD && bd_bars == 1u) fill_req = 1;                       /* (a roll into it) */
        return;
    }
    if (bd_state == BD_OFF) bd_state = BD_ARM_BREAK;
    else if (bd_state == BD_BREAK) bd_state = BD_ARM_BUILD;
    else if (bd_state == BD_BUILD) bd_state = BD_ARM_DROP;
}

/* each block (cosfx.c cfx_run), at its end: does the next block start a beat, a bar? */
static __attribute__((noinline)) void live_tick(uint32_t n)
{
    uint32_t b = beat_samples(), beat = song.playing && beat_pos + n >= b, bar = beat && beat_n == 3u,
             half = beat && beat_n == 1u, now = !song.playing;
    if (bd_kill) { bd_kill = 0; bd_state = BD_OFF; bd_req = 0; }
    if (cos_mc_ch && cos_mc_dir != 2u) {              /* (LINK 2: a macro that moved, out as its CC; one a block) */
        static uint8_t rr;
        uint32_t k = rr++ % 5u;
        if (k < 4u && macro_v[k] != cos_mc_sent[k]) {
            cos_mc_sent[k] = macro_v[k];
            mo_send(0xB0u | (cos_mc_ch - 1u), cos_mc_cc[k] & 127u, macro_v[k] & 127u);
        } else if (k == 4u && cos_mo_cc && !cos_mo_in) {   /* (MORPH's A-B, unless one just came in) */
            uint32_t v = ((uint32_t)clamp(mph.pos, 0, 1000) * 127u + 500u) / 1000u;
            if (v != cos_mo_sent) {
                cos_mo_sent = (uint8_t)v;
                mo_send(0xB0u | (cos_mc_ch - 1u), (cos_mo_cc - 1u) & 127u, v);
            }
        }
    }
    if (cos_pc_out_req) {                             /* (LINK: a section asked for, its PROGRAM CHANGE out) */
        if (cos_pc_ch) mo_send(0xC0u | (cos_pc_ch - 1u), cos_pc_out_req - 1u, 0);
        cos_pc_out_req = 0;
    }
    if (bd_req) {
        bd_req = 0;
        bd_step(0);
        if (now) bd_step(1);                          /* (stopped: at once) */
    }
    if (bar) bd_step(1);
    if (mx_req) {                                     /* (0.20.0) MIX IN / OUT asked: the plan (again while one runs: off) */
        uint32_t k, in = mx_req == 1u;
        mx_req = 0;
        if (mx_n) mx_n = 0;
        else {
            for (k = 0; k < NTRK; k++) {
                uint32_t t = in ? MX_IN[k] : MX_IN[NTRK - 1u - k];
                if (t < trk_live && (trk[t].p[P_MUTE] != 0) == (int)in) mx_trk[mx_n++] = (uint8_t)t;
            }
            mx_i = mx_bar = 0;
            mx_len = (uint8_t)(LV_MIXB(cos_lv) == 3u ? 4u : 8u << LV_MIXB(cos_lv));
            mx_wait = (uint8_t)((cos_lv & LV_PHQ) != 0u);
        }
    }
    if (mx_n && (bar || now)) {                       /* (each change on a bar: the i-th at round(i * bars / (n - 1))) */
        if (!now && mx_wait && ((bar_n + 1u) & (lv_phrase() - 1u)))
            ;                                         /* (CHANGES PHRASE: from the next phrase) */
        else {
            mx_wait = 0;
            while (mx_i < mx_n && (now || mx_n < 2u ||
                                   ((uint32_t)mx_i * mx_len * 2u + mx_n - 1u) / (2u * (mx_n - 1u)) <= mx_bar)) {
                track_t *t = &trk[mx_trk[mx_i]];
                t->p[P_MUTE] = (int16_t)!t->p[P_MUTE];
                mx_i++;
            }
            mx_bar++;
            if (mx_i >= mx_n) mx_n = 0;
        }
    }
    if (bsw_req && (bar || now)) { bsw_req = 0; bsw_on ^= 1; }   /* (0.20.0) BASS SWAP on the bar */
    cfx_auto = bd_bits(bd_state) | (bsw_on ? CX_BIT(CX_LOWK) : 0u);
    if (beat && (bd_state == BD_BUILD || bd_state == BD_ARM_DROP))   /* (the build's progress, a beat at a time) */
        vj_cc(20, (4u - bd_bars) * 32u + (beat_n + 1u) * 8u);
    if (live.cur != vj_sect) {                        /* (a section went in) */
        vj_sect = live.cur;
        if (vj_sect >= 0) vj_note(VJ_SECT + (uint32_t)vj_sect);
    }
    if (live_qmute && (now || cos_mq == 0u || (cos_mq == 1u ? beat : bar))) {
        uint32_t k, m = live_qmute;
        live_qmute = 0;
        for (k = 0; k < NTRK; k++)
            if ((m >> k) & 1u) trk[k].p[P_MUTE] = (int16_t)!trk[k].p[P_MUTE];
    }
    if (live_qback == 1u && (now || cos_mq == 0u || (cos_mq == 1u ? beat : bar)))
        live_qback = 2;
    if (live_qlane && (now || cos_mq == 0u || (cos_mq == 1u ? beat : bar))) {
        uint32_t m = live_qlane;
        live_qlane = 0;
        dx_mute_set(dx_mute ^ (m << 8));
    }
    if (live_qsolo_on && (now || cos_mq == 0u || (cos_mq == 1u ? beat : bar))) {
        live_qsolo_on = 0;
        perf_solo = live_qsolo;
    }
    if (fill_on && (bar || now))
        fill_undo();
    else if (fill_req && half && !fill_on) {          /* (the bar's second half from the next block) */
        fill_req = 0;
        fill_write();
        vj_note(VJ_FILL);
    }
}

/* (0.20.0) MONO BASS: below the corner both sides as one. The side (L - R) / 2 high-passed, 12 dB an octave (a state
 * variable filter, Butterworth: well-behaved this low), the mid untouched: L = M + S', R = M - S'. A mono signal comes
 * out exactly as it went in; a side below the corner goes (at 120 Hz: 50 Hz -15 dB, 30 Hz -24 dB). On the dry mix
 * before the leveler (the reverb's return is one for both already) */
static int32_t mb_lp, mb_bp;
static __attribute__((noinline)) void mono_bass_block(uint32_t n)
{
    static const uint16_t F[4] = {0, 747, 1120, 1493};   /* (2 sin(pi f / FS), Q16: 80 120 160 Hz) */
    uint32_t i;
    int32_t f = F[LV_MONO(cos_lv)], lp = mb_lp, bp = mb_bp;
    for (i = 0; i < n; i++) {
        int32_t m = (mix_l[i] + mix_r[i]) >> 1, sd = (mix_l[i] - mix_r[i]) >> 1, hp;
        hp = sd - lp - (bp * 362 >> 8);                  /* (1 / Q = sqrt 2, Q8) */
        bp += (hp >> 6) * f >> 10;
        lp += (bp >> 6) * f >> 10;
        mix_l[i] = m + hp;
        mix_r[i] = m - hp;
    }
    mb_lp = lp;
    mb_bp = bp;
}
