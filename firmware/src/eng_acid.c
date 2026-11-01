/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* ACID (cursed-os, engine 15): a TB-303 bass voice. The sound is X0X's port of Open303 (x0x/bass303.c, from
 * github.com/charlesvestal/fm1-x0x ca2d377: Open303 by Robin Schmidt (MIT), the Devilfish ranges after jc303 (midilab),
 * the RAT drive after dm-Rat (Dave Mollen), the speed-ups after Optimist (henri / hdavid); GPL-3.0, the Open303 parts
 * MIT): its TeeBeeFilter, the decay envelope with its RC followers, the measured ENV MOD mapping, accent, the 303 square,
 * the pitch slew of a slide. One instance a track (mono: .poly 1), in the pool.
 *
 * What the voice gives it: the note (its start, a slide's note when a legato note arrives with the gate still held), the
 * pitch as it sounds (the phase increment: TUNE, TRANS, the LFO and the matrix on PITCH, a bend), LFO / matrix on FILTER
 * (added to CUTOFF), the gate. Its own envelopes are the amplitude (.ownenv): the track's ADSR does not shape it. Accent:
 * a step's ACC (velocity 120 or more: seq.c plays an accented step at 127). Slide: a step's SLD (seq.c holds the note
 * into the next, the voice goes legato) - the 303's own slew, SLIDE its time.
 *
 * EDIT: CUTOFF RESO ENV MOD DECAY | ACCENT WAVE DRIVE SLIDE (DRIVE: the RAT's, 0 off). */
#define BASS303_OS 2                             /* (the same as x0x/acid_glue.c's: one struct layout) */
#define BASS303_LITE 0
#include "x0x/bass303.h"
void acid_hw_init(bass303_t *b);
void acid_hw_set(bass303_t *b, int pot, int v);
void acid_hw_note_on(bass303_t *b, int note, int accent, int slide);
void acid_hw_note_off(bass303_t *b);
void acid_hw_render(bass303_t *b, uint32_t inc, int32_t *out, int n);
#if !COS_ACID_OBJ                                /* (the host tests, the browser: one translation unit) */
#include "x0x/acid_glue.c"
#endif

static const char *const N_ACID_WAVE[] = {"SAW", "SQR", 0};
static const uint8_t ACID_POT[8] = {BASS303_CUTOFF, BASS303_RESO, BASS303_ENVMOD, BASS303_DECAY,
                                    BASS303_ACCENT, BASS303_WAVE, BASS303_DRIVE, BASS303_SLIDE};
static bass303_t acid_b[NTRK] __attribute__((section(".pool")));
static int16_t acid_pv[NTRK][8] __attribute__((section(".pool")));   /* the pots set last (-1: not yet) */
static uint8_t acid_rdy[NTRK] __attribute__((section(".pool")));
static uint8_t acid_note[NTRK] __attribute__((section(".pool")));    /* the note sounding (a legato one: a slide) */

static uint32_t acid_k(const track_t *t) { return (uint32_t)(t - trk) % NTRK; }
static __attribute__((noinline)) void acid_ready(track_t *t)
{
    uint32_t k = acid_k(t), i;
    if (acid_rdy[k])
        return;
    acid_hw_init(&acid_b[k]);
    for (i = 0; i < 8u; i++)
        acid_pv[k][i] = -1;
    acid_rdy[k] = 1;
}
/* the EDIT knobs into the 303's pots (only what changed: a pot's maths is a few exps); CUTOFF with the LFO and the
 * matrix on FILTER (m: 0 at a note's start) */
static void acid_pots(track_t *t, const vmod_t *m)
{
    uint32_t k = acid_k(t), i;
    for (i = 0; i < 8u; i++) {
        int32_t v = t->p[P_E0 + i];
        if (i == 0u && m)
            v += m->cutoff >> 8;
        v = clamp(v, 0, i == 5u ? 1 : 127);
        if (v != acid_pv[k][i]) {
            acid_pv[k][i] = (int16_t)v;
            acid_hw_set(&acid_b[k], ACID_POT[i], v);
        }
    }
}
static int acid_note_of(const voice_t *v) { return clamp((v->pitch16 + 8) >> 4, 0, 127); }

static void acid_note_on(track_t *t, voice_t *v)
{
    uint32_t k = acid_k(t);
    bass303_t *b = &acid_b[k];
    acid_ready(t);
    acid_pots(t, 0);
    acid_note[k] = (uint8_t)acid_note_of(v);
    acid_hw_note_on(b, acid_note[k], v->vel >= 120u, t->slide_glide && b->gate);
}

static void acid_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t k = acid_k(t), i;
    bass303_t *b = &acid_b[k];
    int32_t buf[CTL];
    acid_ready(t);
    acid_pots(t, m);
    if (v->gate && b->gate && acid_note_of(v) != acid_note[k]) {   /* legato (a slide): no retrigger, the slew */
        acid_note[k] = (uint8_t)acid_note_of(v);
        acid_hw_note_on(b, acid_note[k], v->vel >= 120u, 1);
    }
    if (!v->gate && b->gate)
        acid_hw_note_off(b);
    n = n < CTL ? n : CTL;
    acid_hw_render(b, m->inc, buf, (int)n);           /* (the pitch as the voice has it: the slew's target) */
    for (i = 0; i < n; i++)
        out[i] += voice_amp(buf[i] >> 1, m, i) << 1;
}
/* the voice ends when the 303 has gone quiet (its amp envelope down, the idle hold-off over) */
static int acid_done(track_t *t, voice_t *v)
{
    const bass303_t *b = &acid_b[acid_k(t)];
    return !v->gate && (b->idle || !acid_rdy[acid_k(t)]);
}

/* {CUTOFF RESO ENVMOD DECAY ACCENT WAVE DRIVE SLIDE}, {A D S R} (unused: its own envelopes), fenv, mono */
static const preset_t ACID_PRESETS[] = {
    {"TB CLASSIC", {64, 80, 64, 50, 64, 0, 0, 21}, {0, 64, 127, 20}, 0, 1 | TAG(BASS), FX(0, 0, 20, 10), PAT(1)},
    {"SQUELCH", {38, 116, 96, 58, 92, 0, 0, 21}, {0, 64, 127, 20}, 0, 1 | TAG(BASS), FX(0, 0, 25, 10), PAT(1)},
    {"SQR ACID", {50, 100, 80, 42, 72, 1, 0, 21}, {0, 64, 127, 20}, 0, 1 | TAG(BASS), FX(0, 0, 20, 10), PAT(1)},
    {"RAT 303", {54, 104, 88, 54, 80, 0, 72, 21}, {0, 64, 127, 20}, 0, 1 | TAG(BASS), FX(0, 0, 15, 10), PAT(1)},
    {"HYPNO", {34, 92, 52, 96, 40, 0, 24, 40}, {0, 64, 127, 20}, 0, 1 | TAG(BASS), FX(0, 0, 35, 25), PAT(1)},
    {"DEEP SUB", {18, 36, 26, 30, 28, 1, 0, 30}, {0, 64, 127, 20}, 0, 1 | TAG(BASS), FX(0, 0, 0, 5), PAT(1)},
    {"LONG SLIDE", {60, 96, 70, 50, 60, 0, 0, 72}, {0, 64, 127, 20}, 0, 1 | TAG(BASS), FX(0, 0, 20, 10), PAT(1)},
    {"SCREAM", {76, 124, 112, 36, 112, 0, 92, 21}, {0, 64, 127, 20}, 0, 1 | TAG(LEAD), FX(0, 0, 25, 15), PAT(1)},
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
