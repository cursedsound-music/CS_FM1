/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursed-os 0.15.0) DUB (engine 6, TRIO's number: TRIO was folded into ANALOG in JIANT 0.4; a project before it marks
 * its 6s, project.c PROJ_DUB): GG's "dub-chord engine" and "one-knob atmosphere", one voice a whole chord:
 *   STAB   a minor chord of four saws (CHORD), each a few cents its own, through a resonant low-pass (COLOR the
 *          cutoff, RES) opened by its own envelope (DENSITY how far, DECAY how long): the dub techno stab. THROW:
 *          the chance a stab goes into the delay at full (trk_throw: the track's DELAY send lifted for it, ~0.4 s,
 *          fx.c mix_part): the echoes thrown, the rest dry
 *   ATMOS  a bed: the chord's four as soft triangles in the key (whatever key is struck, the chord stands on the
 *          track's ROOT below it: SCL > ROOT, tied to the key), DENSITY how many sound (one .. four: emptier ..
 *          denser), each fading in and out on its own slow wave; NOISE (air) in it; the low-pass COLOR (darker ..
 *          brighter) drifting with MOTION (DECAY's knob). Its presets come with the REVERB send (CLOUD or TIGHT: REV >
 *          TYPE) and the track's slow envelope
 *   BLEEP  (0.15.1, GG: "bleep bloop sounds ... random but consistent") one ping a note, modular style: WAVE (SINE TRI
 *          SQUARE FM FOLD) phase-modulated by a sine at RATIO (FM its depth, falling with the ping), its own fall
 *          (DECAY), a pitch drop or rise into the note (BLOOP: + from above, - from below), through the low-pass
 *          (COLOR). RANDOM: each note's own FM, DECAY, BLOOP and now and then RATIO, from a hash of its key and its
 *          step: random, and the same every pass. Its patterns: GEN > BLEEP (ui_bleep.c)
 * The amplitude is the track's ADSR. State: ph[0..2] and s[0] the four tones' phases, s[1] s[2] the filter (low,
 * band), s[3] STAB's envelope (Q15), s[4] the noise, s[5] the drift's phase, s[6] the tones' fades' phase; BLEEP:
 * ph[0] the carrier, ph[1] the modulator, s[0] the pitch's envelope, s[3] the ping's, s[5] the note's dice */
static const char *const N_DUB_TYPE[] = {"STAB", "ATMOS", "BLEEP", 0};
enum { DUB_STAB, DUB_ATMOS, DUB_BLEEP };
static const char *const N_BLP_WAVE[] = {"SINE", "TRI", "SQUARE", "FM", "FOLD", 0};
static const char *const N_BLP_RATIO[] = {"0.5", "1", "1.5", "2", "3", "3.5", "4", "7", 0};
static const uint16_t BLP_RATIO[8] = {128, 256, 384, 512, 768, 896, 1024, 1792};   /* Q8 */
static const char *const N_DUB_CHORD[] = {"MIN7", "MIN9", "MIN", "MIN11", "SUS4", "5TH", 0};
static const int8_t DUB_CHORD[6][4] = {{0, 3, 7, 10}, {0, 3, 10, 14}, {0, 3, 7, 12}, {0, 5, 10, 15}, {0, 5, 7, 12}, {0, 7, 12, 19}};
static const int8_t DUB_DET[4] = {0, 5, -4, 7};          /* (each tone's own cents: the chord breathes) */
static volatile uint8_t trk_throw[NTRK];                /* (fx.c mix_part: the DELAY send lifted, a stab thrown; falling) */

static void dub_note_on(track_t *t, voice_t *v)
{
    uint32_t seed = (v->note + 7u) * 0x9E3779B9u ^ v->age * 0x85EBCA6Bu ^ (uint32_t)t->loops * 0xC2B2AE35u;
    int32_t thr = clamp(t->p[P_E6], 0, 127);
    if (!v->s[4]) v->s[4] = (int32_t)(seed | 1u);
    v->s[3] = 32767;                                     /* (STAB: the envelope from the top; BLEEP: the ping's) */
    if (clamp(t->p[P_E0], 0, 2) == DUB_BLEEP) {          /* BLEEP: from the top, the note's dice (its key, its step) */
        uint32_t h = (v->note + 1u) * 0x9E3779B9u ^ ((uint32_t)t->seq_idx + 1u) * 0x85EBCA6Bu;
        h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
        v->s[5] = (int32_t)h;
        v->s[0] = 32767;
        v->ph[0] = v->ph[1] = 0;
        return;
    }
    if (!clamp(t->p[P_E0], 0, 1) && thr && (noise32(&v->s[4]) >> 25) < (uint32_t)thr)   /* (STAB: thrown, THROW's chance) */
        trk_throw[(uint32_t)(t - trk) % NTRK] = 127;
}

/* the cutoff (1/16 st) as the state-variable filter's f (Q15, at most ~5.6 kHz: stable) */
static int32_t dub_f(int32_t c16)
{
    int32_t f = (int32_t)(((pitch_inc((uint32_t)clamp(c16, 0, 2047)) >> 15) * 51472u) >> 15);
    return clamp(f, 40, 26000);
}

/* BLEEP's render */
static __attribute__((noinline)) void dub_bleep(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t wv = (uint32_t)clamp(p[P_E1], 0, 4), rt = (uint32_t)clamp(p[P_E5], 0, 7), h = (uint32_t)v->s[5], i, c;
    int32_t col = clamp(p[P_E2], 0, 127), fm = clamp(p[P_E3], 0, 127), dec = clamp(p[P_E4], 0, 127);
    int32_t bl = clamp(p[P_E6], -64, 63), rnd = clamp(p[P_E7], 0, 127);
    int32_t lo = v->s[1], bd = v->s[2], eg = v->s[3], pe = v->s[0], ka, kp, y[CTL];
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1];
    if (rnd) {                                           /* RANDOM: the note's own, the same each pass */
        fm = clamp(fm + ((((int32_t)(h & 255u) - 128) * rnd) >> 7), 0, 127);
        dec = clamp(dec + ((((int32_t)((h >> 8) & 255u) - 128) * rnd) >> 8), 0, 127);
        bl = clamp(bl + ((((int32_t)((h >> 16) & 255u) - 128) * rnd) >> 8), -64, 63);
        if ((int32_t)((h >> 24) & 127u) < rnd >> 1) rt = (rt + 1u + (h >> 30)) % 8u;
    }
    ka = fmd_fall(TIME_MS_X10[clamp(10 + dec, 0, 127)]);
    kp = fmd_fall(TIME_MS_X10[clamp(dec / 3, 0, 127)]);  /* (the BLOOP quicker than the ping) */
    if (n > CTL)
        n = CTL;
    for (c = 0; c < n; c += 16u) {
        uint32_t e = c + 16u < n ? c + 16u : n, inc, mi;
        int32_t idx = (fm * (6000 + ((eg * 3) >> 2))) >> 13, f;   /* (the FM falls with the ping: 0 .. ~470) */
        inc = cents_inc(m->pitch16 + ((bl * 6 * pe) >> 15), 0, m->fine);   /* (BLOOP: +-24 st at the top) */
        mi = (inc >> 8) * BLP_RATIO[rt];
        f = dub_f((40 + col * 80 / 127) * 16 + (eg >> 7) * 3);   /* (the ping a little brighter at its start) */
        for (i = c; i < e; i++) {
            int32_t mo = osc_sine(ph1), s, hi;
            uint32_t pm = ph0 + ((uint32_t)(mo * idx) << (wv == 3u ? 10 : 9));
            switch (wv) {
            case 1: s = osc_tri(pm); break;
            case 2: s = clamp(osc_sine(pm) * 3, -32767, 32767); break;   /* (a soft square) */
            case 4: s = osc_sine(pm) * (2 + fm / 24);   /* FOLD: driven, folded back at full scale */
                    while (s > 32767 || s < -32767) s = s > 32767 ? 65534 - s : -65534 - s;
                    break;
            default: s = osc_sine(pm); break;
            }
            s = (s * eg) >> 15;
            hi = clamp(s - lo - ((bd >> 8) * 18000 >> 7), -600000, 600000);
            bd = clamp(bd + (((hi >> 3) * f) >> 12), -262144, 262144);
            lo = clamp(lo + (((bd >> 3) * f) >> 12), -262144, 262144);
            y[i] = lo;
            ph0 += inc;
            ph1 += mi;
        }
        eg = (eg * ka) >> 15;
        pe = (pe * kp) >> 15;
    }
    for (i = 0; i < n; i++)
        out[i] += voice_amp(soft_knee(y[i] + (y[i] >> 1), 28000), m, i);
    v->s[0] = pe; v->s[1] = lo; v->s[2] = bd; v->s[3] = eg;
    v->ph[0] = ph0; v->ph[1] = ph1;
}

static void dub_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t at = (uint32_t)clamp(p[P_E0], 0, 2), ch = (uint32_t)clamp(p[P_E1], 0, 5), i, c, k;
    int32_t col = clamp(p[P_E2], 0, 127), den = clamp(p[P_E3], 0, 127), dec = clamp(p[P_E4], 0, 127);
    int32_t res = clamp(p[P_E5], 0, 127), nz = clamp(p[P_E7], 0, 127);
    int32_t lo = v->s[1], bd = v->s[2], eg = v->s[3], rng = v->s[4];   /* (locals: out[] may alias v->s[]) */
    uint32_t ph[4] = {v->ph[0], v->ph[1], v->ph[2], (uint32_t)v->s[0]}, dph = (uint32_t)v->s[5], fph = (uint32_t)v->s[6];
    int32_t q = 32767 - res * 230, ng = (nz * nz) >> 3, base = m->pitch16, y[CTL], bl = (col * col) >> 7;   /* (bl: 0 .. 126) */
    int32_t kd = fmd_fall(TIME_MS_X10[clamp(20 + dec * 3 / 4, 0, 127)]);   /* (STAB: its envelope's fall) */
    uint32_t dinc = (uint32_t)(40000 + dec * dec * 60) * 16u;   /* (ATMOS MOTION: the drift ~0.02 .. ~1.2 Hz, per chunk) */
    if (at == DUB_BLEEP) { dub_bleep(t, v, out, n, m); return; }
    if (at) {                                            /* ATMOS: the chord on the ROOT at or below the key struck */
        int32_t r = (int32_t)v->note - clamp(p[P_ROOT], 0, 11);
        base -= ((r % 12 + 12) % 12) * 16;
    }
    if (n > CTL)
        n = CTL;
    for (c = 0; c < n; c += 16u) {
        uint32_t e = c + 16u < n ? c + 16u : n, inc[4];
        int32_t g[4], f, cut;
        if (at) {
            int32_t dr = (osc_sine(dph) * (6 + col / 16)) >> 15;   /* (the drift: +-6 .. 13 st) */
            cut = (36 + col * 72 / 127 + dr) * 16;
            for (k = 0; k < 4u; k++) {                   /* each tone's fade: in with DENSITY (the first always), on
                                                         * its own slow wave */
                int32_t on = clamp(den * 4 - (int32_t)k * 127, 0, 127) * 258;
                int32_t w = 20000 + ((osc_sine(fph * (k + 2u) + k * 0x40000000u) * 12000) >> 15);
                g[k] = k ? (on >> 8) * w >> 7 : 32767;
            }
            dph += dinc;
            fph += dinc >> 2;
        } else {
            cut = (24 + col * 72 / 127) * 16 + (((24 + den * 36 / 127) * 16 * eg) >> 15);
            for (k = 0; k < 4u; k++) g[k] = 32767;
            eg = (eg * kd) >> 15;
        }
        f = dub_f(cut);
        for (k = 0; k < 4u; k++)
            inc[k] = cents_inc(base + DUB_CHORD[ch][k] * 16, DUB_DET[k] + (at ? 0 : (den >> 5)), m->fine);
        for (i = c; i < e; i++) {
            int32_t x = 0, hi;
            if (at) {
                for (k = 0; k < 4u; k++) {               /* (ATMOS: a triangle, towards a plain saw with COLOR: under
                                                         * the low-pass, a pad's; a tone faded out costs nothing) */
                    if (g[k]) {
                        int32_t tr = osc_tri(ph[k]), sw = (int32_t)(ph[k] >> 16) - 32768;
                        x += ((tr + (((sw - tr) >> 2) * bl >> 5)) * (g[k] >> 3)) >> 14;
                    }
                    ph[k] += inc[k];
                }
            } else {
                for (k = 0; k < 4u; k++) {
                    x += osc_saw(ph[k], inc[k]) >> 2;    /* (each at most 1/4) */
                    ph[k] += inc[k];
                }
            }
            if (ng)
                x += (((int32_t)(noise32(&rng) >> 17) - 16384) * ng) >> 10;
            hi = x - lo - ((bd >> 8) * q >> 7);          /* the state-variable filter (q: 1 / Q, Q15) */
            hi = clamp(hi, -600000, 600000);
            bd = clamp(bd + (((hi >> 3) * f) >> 12), -262144, 262144);   /* (>> 3: the products fit 32 bits) */
            lo = clamp(lo + (((bd >> 3) * f) >> 12), -262144, 262144);
            y[i] = lo;
        }
    }
    for (i = 0; i < n; i++)
        out[i] += voice_amp(soft_knee(at ? y[i] + (y[i] >> 1) : y[i] * 5 >> 1, 28000), m, i);   /* (a STAB's four saws
                                                         * through the filter: up to a pad's level) */
    v->ph[0] = ph[0]; v->ph[1] = ph[1]; v->ph[2] = ph[2]; v->s[0] = (int32_t)ph[3];
    v->s[1] = lo; v->s[2] = bd; v->s[3] = eg; v->s[4] = rng; v->s[5] = (int32_t)dph; v->s[6] = (int32_t)fph;
}

/* {TYPE, CHORD, COLOR, DENSITY, DECAY, RES, THROW, NOISE}; env {ATK DEC SUS REL}; DUB's presets keep their sends (ui.c
 * apply_preset_to: the delay and the reverb are the sound here) */
static const preset_t DUB_PRESETS[] = {
    {"DUB STAB", {0, 0, 48, 64, 40, 70, 0, 0}, {0, 52, 0, 60}, 0, TAG(KEYS), FX(0, 0, 45, 25), PAT(0)},
    {"THROW STAB", {0, 0, 54, 60, 34, 74, 70, 0}, {0, 46, 0, 54}, 0, TAG(KEYS), FX(0, 0, 10, 20), PAT(0)},
    {"DEEP CHORD", {0, 1, 36, 70, 62, 58, 30, 6}, {0, 70, 0, 80}, 0, TAG(KEYS), FX(0, 0, 50, 40), PAT(0)},
    {"BASIC CHORD", {0, 2, 44, 50, 48, 84, 40, 0}, {0, 56, 0, 62}, 0, TAG(KEYS), FX(0, 0, 55, 30), PAT(0)},
    {"MINOR 11", {0, 3, 52, 64, 44, 64, 24, 0}, {0, 54, 0, 64}, 0, TAG(KEYS), FX(0, 0, 45, 30), PAT(0)},
    {"ATMOS DARK", {1, 0, 30, 70, 30, 20, 0, 40}, {84, 40, 127, 112}, 0, TAG(PAD), FX(0, 0, 0, 100), PAT(0)},
    {"ATMOS WIDE", {1, 1, 56, 110, 50, 28, 0, 28}, {90, 40, 127, 116}, 0, TAG(PAD), FX(0, 30, 0, 110), PAT(0)},
    {"ATMOS VOID", {1, 5, 18, 40, 16, 12, 0, 70}, {96, 40, 127, 120}, 0, TAG(PAD), FX(0, 0, 0, 120), PAT(0)},
    {"ATMOS GLOW", {1, 4, 76, 90, 64, 40, 0, 20}, {80, 40, 127, 108}, 0, TAG(PAD), FX(0, 20, 20, 100), PAT(0)},
    /* BLEEP: {TYPE, WAVE, COLOR, FM, DECAY, RATIO, BLOOP, RANDOM} */
    {"BLEEP SINE", {2, 0, 90, 10, 40, 1, 6, 30}, {0, 0, 127, 46}, 0, TAG(PLUCK), FX(0, 0, 50, 30), PAT(0)},
    {"BLOOP DROP", {2, 0, 80, 22, 52, 2, 40, 40}, {0, 0, 127, 50}, 0, TAG(PLUCK), FX(0, 0, 55, 30), PAT(0)},
    {"FM BLIP", {2, 3, 100, 70, 30, 4, 0, 50}, {0, 0, 127, 40}, 0, TAG(PLUCK), FX(0, 0, 50, 25), PAT(0)},
    {"GLASS PING", {2, 3, 120, 46, 60, 7, 0, 30}, {0, 0, 127, 60}, 0, TAG(BELL), FX(0, 0, 45, 40), PAT(0)},
    {"MODULAR TRI", {2, 1, 70, 30, 44, 3, -12, 60}, {0, 0, 127, 46}, 0, TAG(PLUCK), FX(0, 0, 50, 30), PAT(0)},
    {"FOLD PLUCK", {2, 4, 76, 60, 34, 1, 10, 40}, {0, 0, 127, 40}, 0, TAG(PLUCK), FX(0, 0, 45, 25), PAT(0)},
    {"RANDOM PINGS", {2, 3, 90, 60, 40, 2, 20, 110}, {0, 0, 127, 46}, 0, TAG(PLUCK), FX(0, 0, 60, 35), PAT(0)},
    {"SQUARE BLEEP", {2, 2, 64, 20, 36, 1, 4, 20}, {0, 0, 127, 40}, 0, TAG(PLUCK), FX(0, 0, 50, 25), PAT(0)},
};

/* ATMOS: DECAY's knob is its MOTION, THROW none; BLEEP: its own seven */
static const param_desc_t *dub_desc(const track_t *t, uint32_t k)
{
    static const param_desc_t A[2] = {{"MOTION", F_PCT, 0, 127, 40, 0, 0}, {"-", F_PCT, 0, 127, 0, 0, 0}};
    static const param_desc_t B[8] = {{"TYPE", F_ENUM, 0, 2, 0, N_DUB_TYPE, 0}, {"WAVE", F_ENUM, 0, 4, 0, N_BLP_WAVE, 0},
                                      {"COLOR", F_PCT, 0, 127, 90, 0, 0}, {"FM", F_PCT, 0, 127, 10, 0, 0},
                                      {"DECAY", F_PCT, 0, 127, 40, 0, 0}, {"RATIO", F_ENUM, 0, 7, 1, N_BLP_RATIO, 0},
                                      {"BLOOP", F_INT, -64, 63, 0, 0, 0}, {"RANDOM", F_PCT, 0, 127, 30, 0, 0}};
    uint32_t ty = (uint32_t)clamp(t->p[P_E0], 0, 2);
    if (ty == DUB_BLEEP) return &B[k & 7u];
    if (ty != DUB_ATMOS) return 0;
    return k == 4u ? &A[0] : k == 6u ? &A[1] : 0;
}
static const engine_t ENG_DUB = {
    .name = "DUB",
    .page_title = {"DUB", "DUB 2"},
    .edit = {
        {"TYPE", F_ENUM, 0, 2, 0, N_DUB_TYPE, 0},
        {"CHORD", F_ENUM, 0, 5, 0, N_DUB_CHORD, 0},
        {"COLOR", F_PCT, 0, 127, 48, 0, 0},
        {"DENSITY", F_PCT, 0, 127, 64, 0, 0},
        {"DECAY", F_PCT, 0, 127, 40, 0, 0},
        {"RES", F_PCT, 0, 127, 70, 0, 0},
        {"THROW", F_PCT, 0, 127, 0, 0, 0},
        {"NOISE", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = DUB_PRESETS,
    .npresets = NELEM(DUB_PRESETS),
    .note_on = dub_note_on,
    .render = dub_render,
    .desc = dub_desc,
    .knob = {P_E2, P_E3, P_E4, P_E6},
};
