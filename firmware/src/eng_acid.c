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
#define BASS303_OS 2
#define BASS303_LITE 0
/* bass303.c's file-scope names, prefixed: it is compiled in this one translation unit with the rest of the firmware */
#define blep b3_blep
#define bq_get b3_bq_get
#define bq_low_shelf b3_bq_low_shelf
#define bq_lowpass b3_bq_lowpass
#define bq_pass b3_bq_pass
#define bq_put b3_bq_put
#define bq_run b3_bq_run
#define bql_run b3_bql_run
#define c_expf b3_c_expf
#define c_exp2f b3_c_exp2f
#define c_log2f b3_c_log2f
#define c_powf b3_c_powf
#define c_sinf b3_c_sinf
#define c_cosf b3_c_cosf
#define c_db2lin b3_c_db2lin
#define calc_envmod b3_calc_envmod
#define clear_audio_state b3_clear_audio_state
#define dcb_pass b3_dcb_pass
#define decim_hb1 b3_decim_hb1
#define decim_hb2 b3_decim_hb2
#define down_pass b3_down_pass
#define drive_block b3_drive_block
#define drive_rat b3_drive_rat
#define drive_soft b3_drive_soft
#define env_tables b3_env_tables
#define fixed_init b3_fixed_init
#define hb b3_hb
#define idle_advance b3_idle_advance
#define ladder_samples b3_ladder_samples
#define osc_samples b3_osc_samples
#define powers b3_powers
#define rat_coeffs b3_rat_coeffs
#define rc_coeff b3_rc_coeff
#define run b3_run
#define run_amp b3_run_amp
#define run_control b3_run_control
#define run_decim b3_run_decim
#define run_ladder b3_run_ladder
#define run_osc_saw b3_run_osc_saw
#define run_osc_sqr b3_run_osc_sqr
#define run_post b3_run_post
#define run_voice b3_run_voice
#define set_accent_state b3_set_accent_state
#define up_pass b3_up_pass
#define wave_names b3_wave_names
#define drv_names b3_drv_names
#define params b3_params
#define recip b3_recip
#include "x0x/bass303.c"
#undef blep
#undef bq_get
#undef bq_low_shelf
#undef bq_lowpass
#undef bq_pass
#undef bq_put
#undef bq_run
#undef bql_run
#undef c_expf
#undef c_exp2f
#undef c_log2f
#undef c_powf
#undef c_sinf
#undef c_cosf
#undef c_db2lin
#undef calc_envmod
#undef clear_audio_state
#undef dcb_pass
#undef decim_hb1
#undef decim_hb2
#undef down_pass
#undef drive_block
#undef drive_rat
#undef drive_soft
#undef env_tables
#undef fixed_init
#undef hb
#undef idle_advance
#undef ladder_samples
#undef osc_samples
#undef powers
#undef rat_coeffs
#undef rc_coeff
#undef run
#undef run_amp
#undef run_control
#undef run_decim
#undef run_ladder
#undef run_osc_saw
#undef run_osc_sqr
#undef run_post
#undef run_voice
#undef set_accent_state
#undef up_pass
#undef wave_names
#undef drv_names
#undef params
#undef recip
#undef FSO
#undef BASS303_SUB
#undef BASS303_DSUB

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
    bass303_init(&acid_b[k]);
    bass303_set(&acid_b[k], BASS303_DRVTYPE, BASS303_DRV_RAT);
    bass303_set(&acid_b[k], BASS303_VOLUME, 110);      /* (-8 dB: as loud as the other engines' basses) */
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
            bass303_set(&acid_b[k], ACID_POT[i], v);
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
    bass303_note_on(b, acid_note[k], v->vel >= 120u, t->slide_glide && b->gate);
}

static void acid_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t k = acid_k(t), i;
    bass303_t *b = &acid_b[k];
    float buf[CTL];
    acid_ready(t);
    acid_pots(t, m);
    if (v->gate && b->gate && acid_note_of(v) != acid_note[k]) {   /* legato (a slide): no retrigger, the slew */
        acid_note[k] = (uint8_t)acid_note_of(v);
        bass303_note_on(b, acid_note[k], v->vel >= 120u, 1);
    }
    if (!v->gate && b->gate)
        bass303_note_off(b);
    b->osc_freq = (float)m->inc * (44100.0f / 4294967296.0f);   /* (the pitch as the voice has it: the slew's target) */
    n = n < CTL ? n : CTL;
    bass303_render(b, buf, (int)n);
    for (i = 0; i < n; i++) {
        int32_t x = (int32_t)(buf[i] * 52000.0f);
        x = clamp(x, -65535, 65535);
        out[i] += voice_amp(x >> 1, m, i) << 1;
    }
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
