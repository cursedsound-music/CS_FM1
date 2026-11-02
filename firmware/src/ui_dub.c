/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursedsound) GEN > DUB: dub techno chord stabs in one press. Included by ui_graph.c after ui_acid.c (its hash ac_h and
 * key finder ac_key's way). A groove is two parts, each with its own seed: the RHYTHM (stabs mostly on the offbeat
 * eighths, some pushed to the sixteenth around them, few on the beat) and the MOVE (the chord shape moved in parallel,
 * the dub way: none, a second chord for the loop's second half, or four: 1 x 1 y). Every stab is the same shape:
 * DUB    KNOB 1 CHORD (MIN7 MIN9 MIN11 7SUS4 SUS2 MIN MAJ7), 2 DENSITY, 3 MOVE (OFF, 2, 4 chords), 4 LENGTH
 *        (the track's GATE: a short stab .. held)
 * DUB 2  KNOB 1 BARS (1, 2, 4), 2 VOICING (CLOSE, INV 1, INV 2, OPEN), 3 ECHO (the track's delay and reverb sends),
 *        4 OCTAVE (the track's)
 * DUB 3  KNOB 1 ROOT (AUTO: the song's key, the track's root; or C .. B)
 * OCT+ a new groove (rhythm and move), the one before onto the history; OCT- back through the last eight.
 * Every knob writes the whole groove again (the same seeds: the same groove). The settings are the session's (RAM). */
#define DB_HIST 8u
typedef struct { uint16_t r, m; } dbs_t;
typedef struct {
    uint8_t on, shape, dens, move, len, bars, voic, echo, root, nh;
    dbs_t s, hist[DB_HIST];
} db_t;
static db_t db[NTRK] COS_RAM;
static uint32_t db_ms COS_RAM;                                  /* (the last hand gesture: an UNDO point after 2 s) */
enum { DK_SHAPE, DK_DENS, DK_MOVE, DK_LEN, DK_BARS, DK_VOIC, DK_ECHO, DK_ROOT, DK_N };
static const int8_t DB_SHP[7][4] = {{0, 3, 7, 10}, {0, 3, 10, 14}, {0, 5, 10, 15}, {0, 5, 7, 10}, {0, 2, 7, 12},
                                    {0, 3, 7, 12}, {0, 4, 7, 11}};
static const char *const DB_SHN[] = {"MIN7", "MIN9", "MIN11", "7SUS4", "SUS2", "MIN", "MAJ7", 0};
static const int8_t DB_MV[6] = {5, -2, 3, -4, 7, -5};    /* the parallel moves: up a fourth, down a tone, .. */
static const uint8_t DB_MVW[6] = {30, 25, 15, 12, 10, 8};

static db_t *db_of(uint32_t k)
{
    db_t *a = &db[k % NTRK];
    if (!a->on) {
        a->on = 1;
        a->shape = 0; a->dens = 45; a->move = 0; a->len = 22; a->bars = 1; a->voic = 0; a->echo = 60;
        a->s.r = (uint16_t)(0xD0B1u + k * 7u); a->s.m = (uint16_t)(0x5EC7u + k * 11u);
    }
    return a;
}
static uint32_t db_root(uint32_t k, const db_t *a)
{
    return a->root ? (a->root - 1u) % 12u : (uint32_t)((trk[k % NTRK].p[P_ROOT] % 12 + 12) % 12);
}
/* the move of segment j (of 1, 2 or 4) */
static int32_t db_shift(const db_t *a, uint32_t j)
{
    uint32_t i, r, tot = 0, pick[2];
    int32_t mv[2];
    for (i = 0; i < 6u; i++) tot += DB_MVW[i];
    for (i = 0; i < 2u; i++) {
        uint32_t q;
        r = ac_h(a->s.m, i, 0xDBu) % tot;
        for (q = 0; q < 6u && r >= DB_MVW[q]; q++) r -= DB_MVW[q];
        pick[i] = q < 6u ? q : 0u;
        if (i && pick[1] == pick[0]) pick[1] = (pick[0] + 1u) % 6u;   /* (the fourth chord another one) */
        mv[i] = DB_MV[pick[i]];
    }
    if (!a->move || !(j & 1u)) return 0;                 /* (1 x 1 y: the odd segments move) */
    return a->move == 1u || j == 1u ? mv[0] : mv[1];
}

/* track k's groove written again from its settings */
static void db_gen(uint32_t k, uint32_t hand)
{
    track_t *t = &trk[k % NTRK];
    db_t *a = db_of(k);
    uint32_t n = 16u << (a->bars % 3u), segs = a->move == 2u ? 4u : a->move == 1u ? 2u : 1u, i, b;
    int32_t base = 48 + (int32_t)db_root(k, a);
    uint8_t on[64];
    if (hand && fm1_ms - db_ms > 2000u) undo_mark();
    db_ms = hand ? fm1_ms : db_ms;
    for (i = 0; i < n; i++) {                            /* RHYTHM: the offbeat eighth likeliest; the sixteenth after it, then
                                                         * before it; the beat least; a stab right after one: half as likely */
        static const uint8_t W[4] = {10, 24, 80, 34};
        int32_t p = (int32_t)W[i & 3u] * (int32_t)a->dens / 45;
        if (i && on[i - 1u]) p /= 2;
        on[i] = (uint8_t)(a->dens && ac_ch(a->s.r, i, 0xD1u, clamp(p, 0, 95)));
    }
    for (b = 0; a->dens && b < n; b += 16u) {            /* (every bar a stab at least: its second offbeat) */
        uint32_t any = 0;
        for (i = b; i < b + 16u; i++) any |= on[i];
        if (!any) on[b + 6u] = 1;
    }
    load_begin(t, LOAD_PAT);
    for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
    t->p[P_SLEN] = (int16_t)n;
    t->p[P_VOICE] = V_POLY;                              /* (four notes a stab) */
    t->p[P_SGATE] = (int16_t)clamp((int32_t)a->len * 127 / 100, 1, 127);
    for (i = 0; i < n; i++) {
        step_t *s = &t->step[i];
        int32_t v[4], r = base + db_shift(a, i * segs / n), x;
        uint32_t q, w;
        if (!on[i]) continue;
        while (r > 55) r -= 12;                          /* (the register: the root C3 .. G3) */
        for (q = 0; q < 4u; q++) v[q] = r + DB_SHP[a->shape % 7u][q];
        if (a->voic == 1u || a->voic == 2u) v[0] += 12;
        if (a->voic == 2u) v[1] += 12;
        if (a->voic == 3u) v[1] += 12;                   /* (OPEN: the second note an octave up) */
        for (q = 1; q < 4u; q++)                         /* (low to high) */
            for (w = q; w > 0u && v[w - 1u] > v[w]; w--) { x = v[w]; v[w] = v[w - 1u]; v[w - 1u] = x; }
        s->time = ST_NOTE;
        s->n = 4;
        for (q = 0; q < 4u; q++) s->note[q] = (uint8_t)clamp(v[q], 24, 108);
        s->vel = (uint8_t)(88 + ((i & 3u) == 2u ? 14 : 0) + (int32_t)(ac_h(a->s.r, i, 0xD2u) % 13u) - 6);
    }
    if (hand) {                                          /* ECHO: the sends (the delay and reverb are everyone's) */
        t->p[P_DLY] = (int16_t)((int32_t)a->echo * 127 / 100);
        t->p[P_REV] = (int16_t)((int32_t)a->echo * 100 / 100);
    }
    load_end(t);
    pat_sig[k % NTRK] = ~steps_sig(t);                   /* (the user's steps now) */
    ui.force = 1;
}

/* the knob views */
static int16_t db_v[DK_N] COS_RAM;
static param_desc_t db_d COS_RAM;
static __attribute__((noinline)) const param_desc_t *db_desc(uint32_t k, int16_t **valp)
{
    static const char *const LB[DK_N] = {"CHORD", "DENSITY", "MOVE", "LENGTH", "BARS", "VOICING", "ECHO", "ROOT"};
    static const char *const MV[] = {"OFF", "2", "4", 0};   /* (the chords in the loop: one, two, four) */
    static const char *const BR[] = {"1", "2", "4", 0};
    static const char *const VO[] = {"CLOSE", "INV 1", "INV 2", "OPEN", 0};
    static const char *const RT[] = {"AUTO", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", 0};
    db_t *a = db_of(song.sel);
    k %= DK_N;
    db_d.label = LB[k]; db_d.fmt = F_INT; db_d.min = 0; db_d.max = 100; db_d.def = 0; db_d.names = 0; db_d.unit = 0;
    switch (k) {
    case DK_SHAPE: db_d.fmt = F_ENUM; db_d.max = 6; db_d.names = DB_SHN; db_v[k] = a->shape; break;
    case DK_DENS: db_v[k] = a->dens; break;
    case DK_MOVE: db_d.fmt = F_ENUM; db_d.max = 2; db_d.names = MV; db_v[k] = a->move; break;
    case DK_LEN: db_d.min = 5; db_v[k] = a->len; break;
    case DK_BARS: db_d.fmt = F_ENUM; db_d.max = 2; db_d.names = BR; db_v[k] = a->bars; break;
    case DK_VOIC: db_d.fmt = F_ENUM; db_d.max = 3; db_d.names = VO; db_v[k] = a->voic; break;
    case DK_ECHO: db_v[k] = a->echo; break;
    default: db_d.fmt = F_ENUM; db_d.max = 12; db_d.names = RT; db_v[k] = a->root; break;
    }
    *valp = &db_v[k];
    return &db_d;
}
/* knob k turned to v: the setting, then the groove again */
static __attribute__((noinline)) void db_put(uint32_t k, int32_t v)
{
    db_t *a = db_of(song.sel);
    uint8_t *f[DK_N] = {&a->shape, &a->dens, &a->move, &a->len, &a->bars, &a->voic, &a->echo, &a->root};
    static const uint8_t LO[DK_N] = {0, 0, 0, 5, 0, 0, 0, 0}, HI[DK_N] = {6, 100, 2, 100, 2, 3, 100, 12};
    k %= DK_N;
    *f[k] = (uint8_t)clamp(v, LO[k], HI[k]);
    ui.force = 1;
    if (drum_track(TSEL)) { ui_message("DUB: PICK A SYNTH TRACK"); return; }
    if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
    db_gen(song.sel, 1);
}
/* OCT+: a new groove, the one before onto the history; OCT-: back one */
static void db_throw(uint32_t k, int up)
{
    db_t *a = db_of(k);
    uint32_t i;
    if (up) {
        uint32_t r = fm1_ms * 2654435761u ^ ac_h(a->s.r ^ a->s.m, a->nh, 0xDBu);
        for (i = DB_HIST - 1u; i > 0u; i--) a->hist[i] = a->hist[i - 1u];
        a->hist[0] = a->s;
        a->nh = (uint8_t)(a->nh < DB_HIST ? a->nh + 1u : DB_HIST);
        a->s.r = (uint16_t)(ac_h(r, 1, 7) | 1u);
        a->s.m = (uint16_t)(ac_h(r, 2, 8) | 1u);
        ui_message("NEW GROOVE");
    } else {
        if (!a->nh) { ui_message("NO GROOVE BEFORE"); return; }
        a->s = a->hist[0];
        for (i = 0; i + 1u < DB_HIST; i++) a->hist[i] = a->hist[i + 1u];
        a->nh--;
        ui_message("GROOVE BEFORE");
    }
    db_gen(k, 1);
}
static __attribute__((noinline)) void db_oct(int up)
{
    if (drum_track(TSEL)) { ui_message("DUB: PICK A SYNTH TRACK"); return; }
    if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
    db_throw(song.sel, up);
}
/* the track's sound: ANALOG's DUB STAB (a filtered saw stab, delay and reverb on it) */
static __attribute__((noinline)) void db_sound(uint32_t k)
{
    uint32_t i;
    set_engine_of(&trk[k % NTRK], ENGI_ANALOG);
    for (i = 0; i < ENG_ANALOG.npresets; i++)
        if (str_eq(ENG_ANALOG.presets[i].name, "DUB STAB")) { apply_preset_to(&trk[k % NTRK], i); break; }
}
