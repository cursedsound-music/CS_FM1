/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound */
/* ACID (cursed-os, engine 15): a TB-303 bass voice. The sound is X0X's port of Open303 (x0x/bass303.c, from
 * github.com/charlesvestal/fm1-x0x ca2d377: Open303 by Robin Schmidt (MIT), the Devilfish ranges after jc303 (midilab),
 * the RAT drive after dm-Rat (Dave Mollen), the speed-ups after Optimist (henri / hdavid); GPL-3.0, the Open303 parts
 * MIT): its TeeBeeFilter, the decay envelope with its RC followers, the measured ENV MOD mapping, accent, the 303 square,
 * the pitch slew of a slide. Mono (.poly 1); two instances in the pool, for the first two ACID tracks.
 *
 * What the voice gives it: the note (its start, a slide's note when a legato note arrives with the gate still held), the
 * pitch as it sounds (the phase increment: TUNE, TRANS, the LFO and the matrix on PITCH, a bend), LFO / matrix on FILTER
 * (added to CUTOFF), the gate. Its own envelopes are the amplitude (.ownenv): the track's ADSR does not shape it. Accent:
 * a step's ACC (velocity 120 or more: seq.c plays an accented step at 127). Slide: a step's SLD (seq.c holds the note
 * into the next, the voice goes legato) - the 303's own slew, SLIDE its time.
 *
 * EDIT: CUTOFF RESO ENV MOD DECAY | ACCENT WAVE DRIVE SLIDE (DRIVE: the RAT's, 0 off) | (cursedsound) DEVIL: ACC DEC (the
 * accented notes' own decay, Devilfish: 30 .. 3000 ms), DRV TYPE (RAT or SOFT), SUB (a sine an octave down, after the
 * filter: bass303.c run_sub), KEY TRK (CUTOFF follows the note, 0 .. 100 %). The four ride the track's ATK DEC SUS REL,
 * which ACID's own envelopes leave unused (.ownenv): saved, locked and MORPHed as any parameter (params.c track_desc
 * gives them these names on an ACID track). */
#define BASS303_OS 2                             /* (the same as x0x/acid_glue.c's: one struct layout) */
#define BASS303_LITE 0
#include "x0x/bass303.h"
void acid_hw_init(bass303_t *b);
void acid_hw_set(bass303_t *b, int pot, int v);
void acid_hw_sub(bass303_t *b, int v);
void acid_hw_note_on(bass303_t *b, int note, int accent, int slide);
void acid_hw_note_off(bass303_t *b);
void acid_hw_render(bass303_t *b, uint32_t inc, int32_t *out, int n);
#if !COS_ACID_OBJ                                /* (the host tests, the browser: one translation unit) */
#include "x0x/acid_glue.c"
#endif

static const char *const N_ACID_WAVE[] = {"SAW", "SQR", 0};
static const char *const N_ACID_DRV[] = {"RAT", "SOFT", 0};
/* (cursedsound) EDIT's DEVIL page: the track's P_ATK P_DEC P_SUS P_REL on ACID (params.c track_desc) */
static const param_desc_t ACID_X[4] = {
    {"ACC DEC", F_PCT, 0, 127, 7, 0, 0},           /* (7: 194 ms, the stock 303's 200) */
    {"DRVTYPE", F_ENUM, 0, 1, 0, N_ACID_DRV, 0},   /* (7 letters: the dial's) */
    {"SUB", F_PCT, 0, 127, 0, 0, 0},
    {"KEY TRK", F_PCT, 0, 127, 0, 0, 0},
};
static const uint8_t ACID_POT[8] = {BASS303_CUTOFF, BASS303_RESO, BASS303_ENVMOD, BASS303_DECAY,
                                    BASS303_ACCENT, BASS303_WAVE, BASS303_DRIVE, BASS303_SLIDE};
/* two 303s (~470 B each, in the pool, which keeps 8 KB free): the first two tracks on ACID sound; a slot whose track
 * left ACID goes to the next one that asks (a third ACID track at once stays silent) */
#define ACID_NB 2u
static bass303_t acid_b[ACID_NB] __attribute__((section(".pool")));
static int16_t acid_pv[ACID_NB][11] __attribute__((section(".pool")));  /* the pots set last (-1: not yet): the eight,
                                                         * ACC DEC, DRV TYPE, SUB */
static uint8_t acid_own[ACID_NB] __attribute__((section(".pool")));     /* 1 + its track; 0 free */
/* (acid_note: the note sounding, a legato one a slide; above acid_pots, for KEY TRK) */

/* track t's 303 (a free one or one whose track left ACID taken, set up from silence), -1 none */
static __attribute__((noinline)) int32_t acid_slot(const track_t *t)
{
    uint32_t k = (uint32_t)(t - trk) % NTRK, s, i;
    for (s = 0; s < ACID_NB; s++)
        if (acid_own[s] == k + 1u) return (int32_t)s;
    for (s = 0; s < ACID_NB; s++)
        if (!acid_own[s] || trk[(acid_own[s] - 1u) % NTRK].engine != ENGI_ACID) break;
    if (s == ACID_NB) return -1;
    acid_hw_init(&acid_b[s]);
    for (i = 0; i < 11u; i++)
        acid_pv[s][i] = -1;
    acid_own[s] = (uint8_t)(k + 1u);
    return (int32_t)s;
}
/* the EDIT knobs into the 303's pots (only what changed: a pot's maths is a few exps); CUTOFF with the LFO and the
 * matrix on FILTER (m: 0 at a note's start) */
static uint8_t acid_note[ACID_NB] __attribute__((section(".pool")));
static void acid_pots(const track_t *t, uint32_t s, const vmod_t *m)
{
    uint32_t i;
    for (i = 0; i < 11u; i++) {
        int32_t v;
        if (i < 8u) {
            v = t->p[P_E0 + i];
            if (i == 0u && m)
                v += m->cutoff >> 8;
            if (i == 0u && t->p[P_REL] > 0)        /* (KEY TRK: CUTOFF's pot 2.93 octaves over 127, 3.61 a semitone; C3
                                                         * the centre) */
                v += ((int32_t)acid_note[s] - 48) * 361 * clamp(t->p[P_REL], 0, 127) / 12700;
            v = clamp(v, 0, i == 5u ? 1 : 127);
        } else
            v = clamp(t->p[P_ATK + (i - 8u)], 0, i == 9u ? 1 : 127);
        if (v != acid_pv[s][i]) {
            acid_pv[s][i] = (int16_t)v;
            if (i < 8u) acid_hw_set(&acid_b[s], ACID_POT[i], v);
            else if (i == 8u) acid_hw_set(&acid_b[s], BASS303_ACCDEC, v);
            else if (i == 9u) acid_hw_set(&acid_b[s], BASS303_DRVTYPE, v ? BASS303_DRV_SOFT : BASS303_DRV_RAT);
            else acid_hw_sub(&acid_b[s], v);
        }
    }
}
static int acid_note_of(const voice_t *v) { return clamp((v->pitch16 + 8) >> 4, 0, 127); }

static void acid_note_on(track_t *t, voice_t *v)
{
    int32_t s = acid_slot(t);
    bass303_t *b;
    if (s < 0) return;
    b = &acid_b[s];
    acid_note[s] = (uint8_t)acid_note_of(v);            /* (first: KEY TRK's cutoff from it) */
    acid_pots(t, (uint32_t)s, 0);
    acid_hw_note_on(b, acid_note[s], v->vel >= 120u, t->slide_glide && b->gate);
}

static void acid_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    int32_t s = acid_slot(t), buf[CTL];
    uint32_t i;
    bass303_t *b;
    if (s < 0) return;
    b = &acid_b[s];
    acid_pots(t, (uint32_t)s, m);
    if (v->gate && b->gate && acid_note_of(v) != acid_note[s]) {   /* legato (a slide): no retrigger, the slew */
        acid_note[s] = (uint8_t)acid_note_of(v);
        acid_hw_note_on(b, acid_note[s], v->vel >= 120u, 1);
    }
    if (!v->gate && b->gate)
        acid_hw_note_off(b);
    n = n < CTL ? n : CTL;
    acid_hw_render(b, m->inc, buf, (int)n);           /* (the pitch as the voice has it: the slew's target) */
    for (i = 0; i < n; i++)
        out[i] += voice_amp(buf[i] >> 1, m, i) << 1;
}
/* the voice ends when its 303 has gone quiet (its amp envelope down, the idle hold-off over), or it has none */
static int acid_done(track_t *t, voice_t *v)
{
    uint32_t k = (uint32_t)(t - trk) % NTRK, s;
    for (s = 0; s < ACID_NB && acid_own[s] != k + 1u; s++) {}
    return !v->gate && (s == ACID_NB || acid_b[s].idle);
}

/* {CUTOFF RESO ENVMOD DECAY ACCENT WAVE DRIVE SLIDE}, {ACC DEC, DRV TYPE, SUB, KEY TRK} (the track's A D S R: DEVIL),
 * fenv, mono */
static const preset_t ACID_PRESETS[] = {
    {"TB CLASSIC", {64, 80, 64, 50, 64, 0, 0, 21}, {7, 0, 0, 0}, 0, 1 | TAG(BASS), FX(0, 0, 20, 10), PAT(1)},
    {"SQUELCH", {38, 116, 96, 58, 92, 0, 0, 21}, {7, 0, 0, 20}, 0, 1 | TAG(BASS), FX(0, 0, 25, 10), PAT(1)},
    {"SQR ACID", {50, 100, 80, 42, 72, 1, 0, 21}, {7, 0, 0, 0}, 0, 1 | TAG(BASS), FX(0, 0, 20, 10), PAT(1)},
    {"RAT 303", {54, 104, 88, 54, 80, 0, 72, 21}, {7, 0, 0, 0}, 0, 1 | TAG(BASS), FX(0, 0, 15, 10), PAT(1)},
    {"HYPNO", {34, 92, 52, 96, 40, 0, 24, 40}, {40, 1, 30, 0}, 0, 1 | TAG(BASS), FX(0, 0, 35, 25), PAT(1)},
    {"DEEP SUB", {18, 36, 26, 30, 28, 1, 0, 30}, {7, 1, 70, 0}, 0, 1 | TAG(BASS), FX(0, 0, 0, 5), PAT(1)},
    {"LONG SLIDE", {60, 96, 70, 50, 60, 0, 0, 72}, {30, 0, 20, 0}, 0, 1 | TAG(BASS), FX(0, 0, 20, 10), PAT(1)},
    {"SCREAM", {76, 124, 112, 36, 112, 0, 92, 21}, {7, 0, 0, 40}, 0, 1 | TAG(LEAD), FX(0, 0, 25, 15), PAT(1)},
};

static const engine_t ENG_ACID = {
    .name = "ACID",
    .page_title = {"FILTER", "ACCENT"},
    .edit = {
        {"CUTOFF", F_PCT, 0, 127, 64, 0, 0},
        {"RESO", F_PCT, 0, 127, 80, 0, 0},
        {"ENV MOD", F_PCT, 0, 127, 64, 0, 0},
        {"DECAY", F_PCT, 0, 127, 50, 0, 0},
        {"ACCENT", F_PCT, 0, 127, 64, 0, 0},
        {"WAVE", F_ENUM, 0, 1, 0, N_ACID_WAVE, 0},
        {"DRIVE", F_PCT, 0, 127, 0, 0, 0},
        {"SLIDE", F_PCT, 0, 127, 21, 0, 0},
    },
    .presets = ACID_PRESETS,
    .npresets = NELEM(ACID_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = 1,
    .note_on = acid_note_on,
    .render = acid_render,
    .ownenv = 1,
    .done = acid_done,
};
