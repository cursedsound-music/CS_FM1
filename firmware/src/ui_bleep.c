/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursed-os 0.15.1) GEN > BLEEP: hypnotic techno's bleeps and bloops (GG: "random but consistent and good"): a line
 * of pings in the key, from a SEED (the same seed: the same line, always), on a synth track (its sound: DUB's BLEEP
 * presets, eng_dub.c; SOUND loads one). Included by ui_graph.c after ui_dub.c (ac_h, ac_key's way).
 * BLEEP    KNOB 1 SEED (1 .. 999), 2 DENSITY, 3 RANGE (how many of the scale's notes: 2 .. 14), 4 OCTAVE (the track's)
 * BLEEP 2  (0.15.1) KNOB 1 LENGTH (1, 2, 4 bars), 2 VARY (the bars after the first: their steps thrown again, VARY %),
 *          3 ACCENT, 4 SLIDE (the pitch gliding into the next ping)
 * (0.16.1, GG: "a short motif repeating all the time (often 3, 5 or 6 notes instead of the usual 4), shifting against a
 * straight 4/4 techno beat (polyrhythm) ... the sound stays almost the same but changes microscopically slowly over
 * minutes (the filter opens a little, the echo gets denser)": the pages as now)
 * BLEEP 2  KNOB 1 MOTIF (OFF, 3, 5, 6, 7: the line that many steps long, looping against the bar; OFF the bars as
 *          before), 2 LENGTH (1, 2, 4 bars; MOTIF OFF), 3 VARY, 4 ACCENT
 * BLEEP 3  KNOB 1 EVERY (OFF, 1 .. 16 bars: a mutation on the bar, VARY % of the steps), 2 SLIDE, 3 RATCHET (pings
 *          twice), 4 ECHO (the track's DELAY send; REVERB nine tenths of it)
 * BLEEP 4  KNOB 1 DRIFT (over ~96 bars and back: COLOR opening, DECAY a little longer, the echo denser; DRIFT how far;
 *          from the top at each PLAY), 2 SPACE (KEEP, or the shared delay to PING-PONG at 1/8D, 1/4D, 1/8 or 1/16D,
 *          FEEDBK at least 65), 3 SOUND (KEEP or one of DUB's BLEEP sounds)
 * Every BLEEP sound's pitch wanders a few cents, slowly (trk_wob, bl_tick)
 * The rhythm: the offbeat sixteenths likelier than the beat (the kick's), never the same density twice in a row; the
 * notes: a walk in the key that comes back (a repeat likelier than a leap, the root the commonest). OCT+ a new seed,
 * OCT- back through the last four; on BLEEP 3 OCT+ a mutation now, OCT- the line as thrown. Every knob writes it again
 * (the same seed: the same line). The settings are the session's (RAM) */
#define BL_HIST 2u
typedef struct {
    uint8_t on, dens, range, len, vary, acc, slide, ratch, echo, every, bar, nh, snd, motif;
    uint8_t drift, space, lb, rsv;                      /* (0.16.1) DRIFT, SPACE, the beat last counted */
    uint16_t seed, ms, hist[BL_HIST], beats;            /* (0.16.1 beats: DRIFT's clock, since PLAY) */
} bl_t;
static bl_t bl[NTRK] COS_RAM;
enum { BK_SEED, BK_DENS, BK_RANGE, BK_LEN, BK_VARY, BK_ACC, BK_SLIDE, BK_RATCH, BK_ECHO, BK_EVERY, BK_SND, BK_MOTIF, BK_DRIFT,
       BK_SPACE, BK_N };
static const uint8_t BL_MOT[5] = {0, 3, 5, 6, 7};      /* (MOTIF: its steps) */
static const char *const BL_SPN[] = {"KEEP", "1/8D", "1/4D", "1/8", "1/16D", 0};
static const uint8_t BL_SPT[5] = {0, DT_DOT, DT_DOT + 2u, 1, DT_DOT + 1u};   /* (SPACE: G_DTIME, as DUB 3's DELAY) */

static bl_t *bl_of(uint32_t k)
{
    bl_t *a = &bl[k % NTRK];
    if (!a->on) {
        a->on = 1;
        a->dens = 45; a->range = 5; a->len = 1; a->vary = 20; a->acc = 25; a->slide = 15; a->ratch = 8; a->echo = 50;
        a->motif = 2; a->drift = 40; a->vary = 12; a->slide = 10; a->ratch = 4; a->echo = 60;   /* (0.16.1: a 5-step motif,
                                                          * drifting, deep in the echoes) */
        a->seed = (uint16_t)(101u + k * 37u);
    }
    return a;
}
/* the sound list: KEEP, then DUB's presets of TYPE BLEEP */
static uint32_t bl_nsnd(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < ENG_DUB.npresets; i++) n += ENG_DUB.presets[i].e[0] == DUB_BLEEP;
    return n;
}
static uint32_t bl_snd_preset(uint32_t j)                /* the j-th BLEEP sound (1..): its preset index */
{
    uint32_t i;
    for (i = 0; i < ENG_DUB.npresets; i++)
        if (ENG_DUB.presets[i].e[0] == DUB_BLEEP && !--j) return i;
    return 0;
}

/* track k's line written again from its settings */
static void bl_gen(uint32_t k, uint32_t hand)
{
    track_t *t = &trk[k % NTRK];
    bl_t *a = bl_of(k);
    uint32_t mo = BL_MOT[a->motif % 5u], n = mo ? mo : 16u << (a->len % 3u), i, np = 0, deg0 = 0, cnt = 0, tot = 0;
    uint32_t sd = (uint32_t)a->seed * 2654435761u;
    int32_t sc[14], prev = 0;
    eu_t key;
    ac_t dummy;
    memset(&dummy, 0, sizeof dummy);
    ac_key(k, &dummy, &key);
    for (i = 0; i < 24u && np < 14u; i++)                /* the scale's notes from the root up, two octaves */
        if ((key.mask >> (i % 12u)) & 1u) sc[np++] = (int32_t)i;
    if (!np) { sc[0] = 0; np = 1; }
    if (hand && fm1_ms - db_ms > 2000u) undo_mark();
    db_ms = hand ? fm1_ms : db_ms;
    load_begin(t, LOAD_PAT);
    for (i = 0; i < NSTEP; i++) { memset(&t->step[i], 0, sizeof t->step[i]); t->step[i].time = ST_REST; }   /* (all of it: the
                                                         * same seed, the same bytes) */
    t->p[P_SLEN] = (int16_t)n;
    t->p[P_VOICE] = V_POLY;                              /* (the pings ring over each other) */
    t->p[P_SGATE] = 30;
    for (i = 0; i < n; i++) {
        static const uint8_t W[4] = {22, 52, 38, 58};    /* (the beat least: the kick's) */
        uint32_t p = i % 16u, mut = i >= 16u && a->vary && ac_ch(sd ^ (i / 16u) * 0x9E37u, p, 0xB0u, a->vary);
        uint32_t s0 = mut ? sd ^ (i / 16u + 1u) * 0x85EBu : sd;   /* (a varied step: thrown again, the bar's own) */
        uint32_t ms = a->ms && ac_ch(a->ms, i, 0xB9u, a->vary) ? a->ms : 0u;   /* (EVERY's mutation) */
        int32_t w = mo ? (i ? 18 + (int32_t)a->dens * 3 / 4 : 100) : (int32_t)W[p & 3u] * (int32_t)a->dens / 45, d;   /* (a MOTIF:
                                                          * its first step always, the rest DENSITY's) */
        step_t *s = &t->step[i];
        if (!p) { prev = 0; cnt = 0; }                   /* (each bar its own walk: VARY 0, the bars the same) */
        if (ms) s0 ^= ms;
        if ((!a->dens && !(mo && !i)) || (w < 100 && !ac_ch(s0, p, 0xB1u, clamp(w, 0, 92)))) continue;
        {                                                /* the note: a walk (repeat, a step, a leap), the root likeliest */
            uint32_t r = ac_h(s0, p, 0xB2u) % 100u, rng = clamp(a->range, 2, 14) < np ? (uint32_t)clamp(a->range, 2, 14) : np;
            if (!cnt || r < (mo ? 10u : 18u)) d = 0;
            else if (r < 46u) d = prev;
            else if (r < 80u) d = prev + ((ac_h(s0, p, 0xB3u) & 1u) ? 1 : -1);
            else d = (int32_t)(ac_h(s0, p, 0xB4u) % rng);
            d = clamp(d, 0, (int32_t)rng - 1);
            prev = d;
        }
        s->time = ST_NOTE;
        s->n = 1;
        s->note[0] = (uint8_t)clamp(60 + (int32_t)key.root + sc[d], 36, 108);
        s->vel = (uint8_t)(78 + (int32_t)(ac_h(s0, p, 0xB5u) % 22u));
        if (ac_ch(s0, p, 0xB6u, a->acc)) { s->flags |= SF_ACCENT; s->vel = 120; }
        if (ac_ch(s0, p, 0xB7u, a->slide)) s->flags |= SF_SLIDE;
        if (ac_ch(s0, p, 0xB8u, a->ratch)) step_set_ratchet(s, 2);
        cnt++;
        tot++;
        (void)deg0;
    }
    if (!tot) {                                          /* (at least one ping a loop: the root, on the second sixteenth) */
        t->step[1].time = ST_NOTE; t->step[1].n = 1; t->step[1].note[0] = (uint8_t)(60 + key.root); t->step[1].vel = 96;
    }
    if (hand) {                                          /* ECHO: the sends */
        t->p[P_DLY] = (int16_t)((int32_t)a->echo * 127 / 100);
        t->p[P_REV] = (int16_t)((int32_t)a->echo * 90 / 100);   /* (0.16.1: the room as much: was a third) */
    }
    load_end(t);
    pat_sig[k % NTRK] = ~steps_sig(t);
    ui.force = 1;
}

/* the knob views */
static int16_t bl_v[BK_N] COS_RAM;                     /* (0.16.1: its own, BK_N past DUB's DK_N) */
#define bl_d db_d                                       /* (DUB's desc: one page shown at a time) */
static __attribute__((noinline)) const param_desc_t *bl_desc(uint32_t k, int16_t **valp)
{
    static const char *const LB[BK_N] = {"SEED", "DENSITY", "RANGE", "LENGTH", "VARY", "ACCENT", "SLIDE", "RATCHET", "ECHO",
                                         "EVERY", "SOUND", "MOTIF", "DRIFT", "SPACE"};
    static const char *const MO[] = {"OFF", "3", "5", "6", "7", 0};
    static const char *const BR[] = {"1 BAR", "2 BARS", "4 BARS", 0};
    static const char *const EV[] = {"OFF", "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS", 0};
    static const char *SN[12];                          /* (.bss: the pool's room is the sound's) */
    bl_t *a = bl_of(song.sel);
    k %= BK_N;
    bl_d.label = LB[k]; bl_d.fmt = F_INT; bl_d.min = 0; bl_d.max = 100; bl_d.def = 0; bl_d.names = 0; bl_d.unit = 0;
    switch (k) {
    case BK_SEED: bl_d.min = 1; bl_d.max = 999; bl_v[k] = (int16_t)a->seed; break;
    case BK_DENS: bl_v[k] = a->dens; break;
    case BK_RANGE: bl_d.min = 2; bl_d.max = 14; bl_v[k] = a->range; break;
    case BK_LEN: bl_d.fmt = F_ENUM; bl_d.max = 2; bl_d.names = BR; bl_v[k] = a->len; break;
    case BK_VARY: bl_v[k] = a->vary; break;
    case BK_ACC: bl_v[k] = a->acc; break;
    case BK_SLIDE: bl_v[k] = a->slide; break;
    case BK_RATCH: bl_v[k] = a->ratch; break;
    case BK_ECHO: bl_v[k] = a->echo; break;
    case BK_EVERY: bl_d.fmt = F_ENUM; bl_d.max = 5; bl_d.names = EV; bl_v[k] = a->every; break;
    case BK_MOTIF: bl_d.fmt = F_ENUM; bl_d.max = 4; bl_d.names = MO; bl_v[k] = a->motif; break;
    case BK_DRIFT: bl_v[k] = a->drift; break;
    case BK_SPACE: bl_d.fmt = F_ENUM; bl_d.max = 4; bl_d.names = BL_SPN; bl_v[k] = a->space; break;
    default: {
        uint32_t j, ns = bl_nsnd();
        SN[0] = "KEEP";
        for (j = 1; j <= ns && j < 11u; j++) SN[j] = ENG_DUB.presets[bl_snd_preset(j)].name;
        SN[j] = 0;
        bl_d.fmt = F_ENUM; bl_d.max = (int16_t)(j - 1u); bl_d.names = SN; bl_v[k] = a->snd;
        break;
    }
    }
    *valp = &bl_v[k];
    return &bl_d;
}
/* knob k turned to v: the setting, then the line again */
static __attribute__((noinline)) void bl_put(uint32_t k, int32_t v)
{
    bl_t *a = bl_of(song.sel);
    k %= BK_N;
    if (k == BK_DRIFT) { a->drift = (uint8_t)clamp(v, 0, 100); ui.force = 1; return; }
    if (k == BK_SPACE) {                                 /* (the delay is everyone's: only when turned, KEEP leaves it) */
        a->space = (uint8_t)clamp(v, 0, 4);
        if (a->space) {
            song.g[G_DTIME] = (int16_t)BL_SPT[a->space];
            song.g[G_DPING] = 1;
            if (song.g[G_DFDBK] < 65) song.g[G_DFDBK] = 65;
        }
        ui.force = 1;
        return;
    }
    if (drum_track(TSEL)) { ui_message("BLEEP: PICK A SYNTH TRACK"); return; }
    if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
    switch (k) {
    case BK_SEED: a->seed = (uint16_t)clamp(v, 1, 999); a->ms = 0; break;
    case BK_DENS: a->dens = (uint8_t)clamp(v, 0, 100); break;
    case BK_RANGE: a->range = (uint8_t)clamp(v, 2, 14); break;
    case BK_LEN: a->len = (uint8_t)clamp(v, 0, 2); break;
    case BK_VARY: a->vary = (uint8_t)clamp(v, 0, 100); break;
    case BK_ACC: a->acc = (uint8_t)clamp(v, 0, 100); break;
    case BK_SLIDE: a->slide = (uint8_t)clamp(v, 0, 100); break;
    case BK_RATCH: a->ratch = (uint8_t)clamp(v, 0, 100); break;
    case BK_ECHO: a->echo = (uint8_t)clamp(v, 0, 100); break;
    case BK_EVERY: a->every = (uint8_t)clamp(v, 0, 5); ui.force = 1; return;
    case BK_MOTIF: a->motif = (uint8_t)clamp(v, 0, 4); break;
    default:                                             /* SOUND: one of DUB's BLEEP sounds onto the track */
        a->snd = (uint8_t)clamp(v, 0, (int32_t)bl_nsnd());
        if (a->snd) {
            int16_t vo = TSEL->p[P_VOICE];               /* (its VOICE kept: GEN's pages follow POLY / MONO, the page
                                                         * stays where it is) */
            set_engine_of(TSEL, ENGI_DUB);
            apply_preset_to(TSEL, bl_snd_preset(a->snd));
            TSEL->p[P_VOICE] = vo;
        }
        ui.force = 1;
        return;
    }
    bl_gen(song.sel, 1);
}
/* OCT+: a new seed (the one before onto the history); OCT-: back one; on BLEEP 3 (mu): a mutation now / the line */
static __attribute__((noinline)) void bl_oct(int up, int mu)
{
    bl_t *a = bl_of(song.sel);
    uint32_t i;
    if (drum_track(TSEL)) { ui_message("BLEEP: PICK A SYNTH TRACK"); return; }
    if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
    if (mu) {
        a->ms = up ? (uint16_t)(ac_h(fm1_ms, song.sel, a->ms ^ 0xB1u) | 1u) : 0u;
        ui_message(up ? "MUTATED" : "AS THROWN");
    } else if (up) {
        for (i = BL_HIST - 1u; i > 0u; i--) a->hist[i] = a->hist[i - 1u];
        a->hist[0] = a->seed;
        a->nh = (uint8_t)(a->nh < BL_HIST ? a->nh + 1u : BL_HIST);
        a->seed = (uint16_t)(1u + ac_h(fm1_ms, a->seed, 0xBEu) % 999u);
        a->ms = 0;
        ui_message("NEW BLEEPS");
    } else {
        if (!a->nh) { ui_message("NO SEED BEFORE"); return; }
        a->seed = a->hist[0];
        for (i = 0; i + 1u < BL_HIST; i++) a->hist[i] = a->hist[i + 1u];
        a->nh--;
        a->ms = 0;
        ui_message("SEED BEFORE");
    }
    bl_gen(song.sel, 1);
}
/* every UI frame: the wander of every BLEEP sound's pitch (two slow sines, a few cents, each track its own); DRIFT's
 * reach (0.16.1); playing, each track with EVERY on mutates in the last beat of every EVERY bars (as db_tick) */
static __attribute__((noinline)) void bl_tick(void)
{
    uint32_t k, b = bar_n;
    for (k = 0; k < NTRK; k++) {
        bl_t *a = &bl[k];
        int32_t w = osc_sine(fm1_ms * 976128u + k * 0x2AAAAAABu) * 4 + osc_sine(fm1_ms * 605000u + k * 0x5A5A5A5Au) * 2;
        trk_wob[k] = (int8_t)(w / 32767);               /* (+-6 cents, ~4.4 s and ~7.1 s) */
        if (!song.playing) { a->bar = 0; a->beats = 0; a->lb = 0xFF; trk_drift[k] = 0; continue; }
        if (!a->on || !a->drift) { trk_drift[k] = 0; continue; }
        if (a->lb != (uint8_t)beat_n) { a->lb = (uint8_t)beat_n; a->beats++; }
        {
            uint32_t ph = a->beats % 384u, tri = ph < 192u ? ph : 384u - ph;   /* (up over 48 bars, back over 48) */
            trk_drift[k] = (uint8_t)(((uint32_t)a->drift * 127u / 100u) * tri / 192u);
        }
    }
    if (!song.playing || chain_busy() || beat_n != 3u || beat_pos < beat_samples() / 2u) return;
    for (k = 0; k < NTRK; k++) {
        bl_t *a = &bl[k];
        uint32_t ev = AC_EVERY[a->every % 6u];
        if (!a->on || !ev || !a->vary || drum_track(&trk[k]) || a->bar == (uint8_t)(b + 1u) || (b + 1u) % ev) continue;
        a->bar = (uint8_t)(b + 1u);
        a->ms = (uint16_t)(ac_h(fm1_ms, k, a->ms ^ 0xB1u) | 1u);
        bl_gen(k, 0);
    }
}
