/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* ACID's float side (eng_acid.c): X0X's bass303.c and the few calls the engine makes, with integers only across the
 * boundary. The firmware is integer code built without the FPU; this file is built on its own with the AC79's
 * single-precision FPU (tools/build.py: -mcpu=r3 -mfprev1 -ffp-contract=off, as X0X builds bass303.c), so no
 * soft-float routine is needed and no float crosses a call. The host tests and the browser emulator include it
 * into the one translation unit (eng_acid.c), hence the prefixed names. */
#include <stdint.h>
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
#include "bass303.c"
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


void acid_hw_init(bass303_t *b)
{
    bass303_init(b);
    bass303_set(b, BASS303_DRVTYPE, BASS303_DRV_RAT);
    bass303_set(b, BASS303_VOLUME, 110);          /* (-8 dB: as loud as the other engines' basses) */
}
void acid_hw_set(bass303_t *b, int pot, int v) { bass303_set(b, pot, v); }
void acid_hw_sub(bass303_t *b, int v) { bass303_set_sub(b, v); }   /* (cursedsound) the sub's level */
void acid_hw_note_on(bass303_t *b, int note, int accent, int slide) { bass303_note_on(b, note, accent, slide); }
void acid_hw_note_off(bass303_t *b) { bass303_note_off(b); }
/* n samples (n <= 64) at the pitch the phase increment inc gives (2^32 a cycle at 44.1 kHz: the slew's target), as
 * integers, full scale about +-52000 */
void acid_hw_render(bass303_t *b, uint32_t inc, int32_t *out, int n)
{
    float buf[64];
    int i;
    if (n > 64) n = 64;
    b->osc_freq = (float)inc * (44100.0f / 4294967296.0f);
    bass303_render(b, buf, n);
    for (i = 0; i < n; i++) {
        float x = buf[i] * 52000.0f;
        out[i] = x > 65535.0f ? 65535 : x < -65535.0f ? -65535 : (int32_t)x;
    }
}
