/* SPDX-License-Identifier: GPL-3.0-only
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
static volatile uint8_t cos_mq = 2;           /* main: MUTE Q, 0 NOW 1 BEAT 2 BAR (ui_polls: the settings') */
static volatile uint8_t live_qmute;           /* main: the tracks whose mute turns over next (bit k) */
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
    cfx_auto = bd_bits(bd_state);
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
