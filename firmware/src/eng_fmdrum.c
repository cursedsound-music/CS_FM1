/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursed-os 0.11.0) FMDRUM (engine 9, PHYS's number): drums, cymbals and hats by FM, after the voices of the LXR drum
 * machine (Erica Synths' firmware, GPL-3.0-only: its DrumVoice, CymbalVoice and HiHat), written again here in
 * integers for the FM-1 (no FPU). A carrier (WAVE) with a pitch envelope (SWEEP, S.TIME), phase-modulated by a second
 * oscillator (M.WAVE at RATIO times the carrier) whose depth (FM) falls with the same envelope, so a hit opens bright
 * and closes to its body; NOISE mixed in. TYPE:
 *   DRUM    the carrier, the noise through a high-pass at ~1.2 kHz (kicks, toms, percussion)
 *   SNARE   a second carrier partial at 1.6 x (a drum's second mode), the noise brighter: the snare's wires
 *   CYMBAL  two modulators (RATIO and RATIO x 1.48, inharmonic), the sum through two high-passes at ~3 kHz
 *   HAT     as CYMBAL, at ~7 kHz
 * The amplitude is the track's ADSR (the presets: SUS 0, the hit's length DEC = REL). The keys play it chromatic: a
 * tom's or a kick's notes.
 * TRANSIENT (P_DXIMG the kind, P_MORPH its level; KICK's too, eng_drum.c drum_render): a short attack added to the
 * hit: SNAP a pitch spike (+55 st falling in ~4 ms), or one of the LXR's twelve transient samples (~50 ms each, as its
 * firmware has them: xr_tables.h by tools/gen_xr_tables.py; names of our own, its sources' in the table's comments).
 * State: ph[0] the carrier, ph[1] the modulator, ph[2] the second modulator (CYMBAL HAT) or partial (SNARE); s[0] the
 * pitch envelope (Q24), s[1] the noise, s[2] SNAP's envelope (Q15), s[3] s[4] the high-passes, s[5] the transient's
 * place (samples) */
static const char *const N_FMD_TYPE[] = {"DRUM", "SNARE", "CYMBAL", "HAT", 0};
enum { FMD_DRUM, FMD_SNARE, FMD_CYM, FMD_HAT };
static const char *const N_FMD_WAVE[] = {"SINE", "TRI", "SAW", "SQUARE", 0};
static const char *const N_FMD_RATIO[] = {"0.5", "1", "1.5", "2", "3", "4", "5", "8", "1.41", "2.76", "3.73", "5.33", 0};
static const uint16_t FMD_RATIO[12] = {128, 256, 384, 512, 768, 1024, 1280, 2048, 361, 707, 955, 1364};   /* Q8 */
#define FMD_NRATIO 12

/* the TRANSIENT layer, FMDRUM's and KICK's: P_DXIMG the kind, P_MORPH its level (params.c track_desc) */
static const char *const N_TRN[] = {"OFF", "SNAP", "CLICK", "CLICK 2", "ATTACK", "ACOUST", "RIM", "DRIP", "PEDAL", "CLAP",
                                    "TOY", "DIGI 1", "DIGI 2", "DIGI 3", 0};
#define TRN_SNAP 1
#define TRN_N 14
static const param_desc_t TRN_DESC[2] = {{"TRANS", F_ENUM, 0, TRN_N - 1, 0, N_TRN, 0}, {"T.AMT", F_PCT, 0, 127, 0, 0, 0}};
_Static_assert(TRN_N - 2 == XR_TRN_N, "a transient name each table");
/* a sample transient (kind 2..) from *pos into y[0..n), level lv 0..127 */
static void trn_add(int32_t *y, uint32_t n, int32_t kind, int32_t lv, int32_t *pos)
{
    uint32_t p = (uint32_t)*pos, i;
    int32_t g = lv * 258;                                /* Q15 */
    const int8_t *w;
    if (kind < 2 || kind >= TRN_N || lv <= 0 || p >= XR_TRN_LEN)
        return;
    w = XR_TRN[kind - 2];
    for (i = 0; i < n && p < XR_TRN_LEN; i++, p++)
        y[i] += (w[p] * g) >> 6;                         /* (x 512: up to twice full scale, into the knee) */
    *pos = (int32_t)p;
}
/* SNAP: the pitch added (1/16 st) at SNAP's envelope e (Q15), level lv */
static int32_t trn_snap(int32_t e, int32_t lv)
{
    int32_t e2 = (e * e) >> 15;
    return (((880 * lv) >> 7) * e2) >> 15;               /* (55 st at the top) */
}

static void fmd_note_on(track_t *t, voice_t *v)
{
    uint32_t seed = (v->note + 1u) * 0x9E3779B9u ^ (uint32_t)v->fine * 0x85EBCA6Bu;
    (void)t;
    v->s[0] = 1 << 24;
    v->s[1] = (int32_t)(seed ? seed : 1u);
    v->s[2] = 32767;
    v->s[3] = v->s[4] = v->s[5] = 0;
    v->ph[0] = v->ph[1] = v->ph[2] = 0;                  /* (a hit from the same place: each the same; a retrigger
                                                          * keeps them, voice.c) */
}

static inline int32_t fmd_wave(uint32_t w, uint32_t ph, uint32_t inc)
{
    switch (w & 3u) {
    case 1: return osc_tri(ph);
    case 2: return osc_saw(ph, inc);
    case 3: return osc_pulse(ph, inc, 0x80000000u);
    default: return osc_sine(ph);
    }
}

/* the per-chunk factor (Q15) of an exponential fall of time constant ms10 (1/10 ms), over 16 samples */
static int32_t fmd_fall(uint32_t ms10)
{
    uint32_t tau = ms10 * (FS / 100u) / 100u + 1u;     /* samples */
    uint32_t d = (16u << 15) / tau;
    return 32768 - (int32_t)(d > 30000u ? 30000u : d);
}

static void fmd_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t type = (uint32_t)clamp(p[P_E0], 0, 3), wv = (uint32_t)clamp(p[P_E1], 0, 3), mw = (uint32_t)clamp(p[P_E7], 0, 3);
    uint32_t r = FMD_RATIO[clamp(p[P_E5], 0, FMD_NRATIO - 1)], i, c;
    int32_t sweep = (clamp(p[P_E2], 0, 127) * 768) / 127, fm = clamp(p[P_E4], 0, 127), nz = clamp(p[P_E6], 0, 127);
    int32_t kind = clamp(p[P_DXIMG], 0, TRN_N - 1), lv = clamp(p[P_MORPH], 0, 127);
    int32_t eg = v->s[0], rng = v->s[1], se = v->s[2], h1 = v->s[3], h2 = v->s[4], tp = v->s[5];   /* (locals: out[] may
                                                          * alias v->s[]) */
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    int32_t kp = fmd_fall(TIME_MS_X10[clamp(p[P_E3], 0, 127)]), ks = fmd_fall(10u);   /* (SNAP: 1 ms) */
    int32_t ha = type == FMD_HAT ? 18000 : type == FMD_CYM ? 12000 : type == FMD_SNARE ? 7000 : 5000;   /* HP, Q15 */
    int32_t ng = (nz * nz) >> 1, cg = type >= FMD_CYM ? 20000 : 32767 - (ng >> 2);   /* noise Q15 (square law), the tone */
    int32_t y[CTL];
    if (n > CTL)
        n = CTL;
    for (c = 0; c < n; c += 16u) {
        uint32_t e = c + 16u < n ? c + 16u : n, inc, mi, mi2;
        int32_t pit = m->pitch16 + ((sweep * (eg >> 9)) >> 15), idx;
        if (kind == TRN_SNAP)
            pit += trn_snap(se, lv);
        inc = cents_inc(pit, 0, m->fine);
        mi = (inc >> 8) * r;
        mi2 = type == FMD_SNARE ? (inc >> 8) * 410u : (mi >> 8) * 379u;   /* (SNARE: 1.6 x the carrier; CYM HAT: 1.48 x mod) */
        idx = (fm * (8192 + ((eg >> 9) * 3 >> 2))) >> 13;  /* the depth: a quarter held, the rest falls with the
                                                          * pitch envelope; 0 .. 508 */
        for (i = c; i < e; i++) {
            int32_t mo = fmd_wave(mw, ph1, mi), s, x;
            uint32_t pm;
            if (type >= FMD_CYM)
                mo = (mo + fmd_wave(mw, ph2, mi2)) >> 1;
            pm = ph0 + ((uint32_t)(mo * idx) << 9);      /* (+-2 cycles at the top) */
            s = fmd_wave(wv, pm, inc);
            if (type == FMD_SNARE)
                s = (s * 3 + osc_sine(ph2)) >> 2;
            x = (int32_t)(noise32(&rng) >> 17) - 16384;  /* -16384 .. 16383 */
            x = (x * ng) >> 12;
            x = clamp(x, -32767, 32767);
            if (type >= FMD_CYM) {                      /* the whole through the high-passes */
                s = ((s * cg) >> 15) + x;
                h1 += ((s - h1) * ha) >> 15;
                s -= h1;
                h2 += ((s - h2) * ha) >> 15;
                s -= h2;
                s = type == FMD_HAT ? s * 5 : s + (s >> 1);   /* (what the high-passes took: HAT's more) */
            } else {                                     /* the noise only */
                h1 += ((x - h1) * ha) >> 15;
                x -= h1;
                s = ((s * cg) >> 15) + x;
            }
            y[i] = s;
            ph0 += inc;
            ph1 += mi;
            ph2 += mi2;
        }
        eg = ((eg >> 9) * kp) >> 6;                     /* (Q24, in 32 bits) */
        se = (se * ks) >> 15;
    }
    if (kind >= 2)
        trn_add(y, n, kind, lv, &tp);
    for (i = 0; i < n; i++)
        out[i] += voice_amp(soft_knee(y[i], 28000), m, i);
    v->s[0] = eg;
    v->s[1] = rng;
    v->s[2] = se;
    v->s[3] = h1;
    v->s[4] = h2;
    v->s[5] = tp;
    v->ph[0] = ph0;
    v->ph[1] = ph1;
    v->ph[2] = ph2;
}

/* {TYPE, WAVE, SWEEP, S.TIME, FM, RATIO, NOISE, M.WAVE}; env {ATK DEC SUS REL}; xp: the TRANSIENT, kind | level / 8 << 4 */
#define TRN(k, l) .xp = (uint8_t)((k) | (((l) * 15 + 63) / 127) << 4)
#define TRN_LV(xp) ((int16_t)(((uint32_t)(xp) >> 4) * 127u / 15u))   /* (ui.c apply_preset_to) */
static const preset_t FMD_PRESETS[] = {
    {"FM KICK", {FMD_DRUM, 0, 70, 30, 30, 1, 0, 0}, {0, 72, 0, 72}, 0, TAG(DRUM), FX(0, 0, 0, 0), PAT(0), TRN(2, 80)},
    {"FM TOM", {FMD_DRUM, 0, 40, 40, 20, 2, 10, 0}, {0, 70, 0, 70}, 0, TAG(DRUM), FX(0, 0, 0, 20), PAT(0), TRN(1, 60)},
    {"FM SNARE", {FMD_SNARE, 1, 30, 20, 25, 9, 90, 0}, {0, 62, 0, 62}, 0, TAG(DRUM), FX(0, 0, 0, 25), PAT(0), TRN(6, 70)},
    {"CLAP-ISH", {FMD_SNARE, 0, 10, 10, 10, 10, 120, 0}, {0, 58, 0, 58}, 0, TAG(DRUM), FX(0, 0, 0, 40), PAT(0), TRN(9, 110)},
    {"RIDE", {FMD_CYM, 3, 0, 60, 90, 9, 30, 3}, {0, 95, 0, 95}, 0, TAG(DRUM), FX(0, 0, 0, 30), PAT(0), TRN(8, 50)},
    {"CRASH", {FMD_CYM, 3, 10, 70, 110, 11, 70, 3}, {0, 102, 0, 102}, 0, TAG(DRUM), FX(0, 0, 0, 40), PAT(0), TRN(4, 60)},
    {"CLOSED HAT", {FMD_HAT, 3, 0, 20, 100, 10, 60, 3}, {0, 40, 0, 40}, 0, TAG(DRUM), FX(0, 0, 0, 10), PAT(0), TRN(8, 60)},
    {"OPEN HAT", {FMD_HAT, 3, 0, 40, 100, 10, 60, 3}, {0, 78, 0, 78}, 0, TAG(DRUM), FX(0, 0, 0, 15), PAT(0), TRN(8, 50)},
    {"COWBELL", {FMD_DRUM, 3, 4, 20, 40, 8, 0, 0}, {0, 60, 0, 60}, 0, TAG(DRUM), FX(0, 0, 0, 15), PAT(0), TRN(6, 40)},
    {"ZAP PERC", {FMD_DRUM, 0, 110, 25, 60, 6, 0, 0}, {0, 55, 0, 55}, 0, TAG(DRUM), FX(0, 0, 20, 20), PAT(0), TRN(11, 70)},
};

static const engine_t ENG_FMDRUM = {
    .name = "FMDRUM",
    .page_title = {"FMDRUM", "FMDRUM 2"},
    .edit = {
        {"TYPE", F_ENUM, 0, 3, 0, N_FMD_TYPE, 0},
        {"WAVE", F_ENUM, 0, 3, 0, N_FMD_WAVE, 0},
        {"SWEEP", F_PCT, 0, 127, 70, 0, 0},
        {"S.TIME", F_PCT, 0, 127, 30, 0, 0},
        {"FM", F_PCT, 0, 127, 30, 0, 0},
        {"RATIO", F_ENUM, 0, FMD_NRATIO - 1, 1, N_FMD_RATIO, 0},
        {"NOISE", F_PCT, 0, 127, 0, 0, 0},
        {"M.WAVE", F_ENUM, 0, 3, 0, N_FMD_WAVE, 0},
    },
    .presets = FMD_PRESETS,
    .npresets = NELEM(FMD_PRESETS),
    .note_on = fmd_note_on,
    .render = fmd_render,
    .knob = {P_E2, P_E3, P_E4, P_E6},
};
/* an engine with the TRANSIENT layer (P_DXIMG, P_MORPH: params.c track_desc, cos.c's TRANSIENT page) */
#define ENG_TRN(e) ((e) == &ENG_FMDRUM || (e) == &ENG_KICK)
