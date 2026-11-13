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
 * ph[0] the carrier, ph[1] the modulator (WOOD, GLASS: the first partial), ph[2] the second partial, s[0] the pitch's
 * envelope, s[3] the ping's, s[4] the tick's noise, s[5] the note's dice, s[7] the tick's envelope */
static const char *const N_DUB_TYPE[] = {"STAB", "ATMOS", "BLEEP", 0};
enum { DUB_STAB, DUB_ATMOS, DUB_BLEEP };
static const char *const N_BLP_WAVE[] = {"SINE", "TRI", "SQUARE", "FM", "FOLD", "WOOD", "GLASS", "DROP", 0};   /* (0.16.1: + 3) */
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
        v->s[7] = 32767;                                 /* (0.16.1: the trigger's tick, its noise the note's own) */
        v->s[4] = (int32_t)(h | 1u);
        v->ph[0] = v->ph[1] = v->ph[2] = 0;
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

/* BLEEP's render. (0.16.1, GG: "they should sound like a modular synthesizer ... a submarine sonar's ping, water drops
 * in a deep cave, old science-fiction computers ... glassy, pure, sine-like, a little metallic or woody-percussive ...
 * an extremely short attack, a short decay like a needle's prick, almost no sustain ... subtle pitch modulation")
 * Struck, not played: a tick of noise at the onset (the trigger's click), at once full, falling (DECAY: 2 ms .. ~1.5 s
 * its time constant), through a low-pass gate: its cutoff follows the note (COLOR: from under it to far above) and the
 * ping (the vactrol's way: as it falls it darkens). WAVE: SINE TRI SQUARE FM FOLD, and WOOD (a struck bar: the sine
 * and its 2.76 and 5.40, the higher falling faster: FM their level), GLASS (the sine and 2.32 and 4.25, a few cents
 * beating, ringing as long), DROP (a water drop: the pitch rising as it falls, BLOOP how far, FM its sheen). The pitch
 * wanders a few cents, slowly (trk_wob, ui_bleep.c bl_tick), each note a few cents its own (RANDOM); GEN > BLEEP's
 * DRIFT opens COLOR and lengthens DECAY over minutes (trk_drift) */
static volatile int8_t trk_wob[NTRK];                   /* (the slow wander, cents) */
static volatile uint8_t trk_drift[NTRK];                /* (DRIFT's reach now, 0 .. 127: COLOR, DECAY, the DELAY send) */
static const uint16_t BLP_PART[3][2] = {{707, 1382}, {594, 1088}, {0, 0}};   /* (WOOD, GLASS: the partials, Q8) */
static __attribute__((noinline)) void dub_bleep(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t ti = (uint32_t)(t - trk) % NTRK, wv = (uint32_t)clamp(p[P_E1], 0, 7), rt = (uint32_t)clamp(p[P_E5], 0, 7);
    uint32_t h = (uint32_t)v->s[5], i, c;
    int32_t dr = trk_drift[ti], col = clamp(p[P_E2] + (dr * 40 >> 7), 0, 127), fm = clamp(p[P_E3], 0, 127);
    int32_t dec = clamp(p[P_E4] + (dr * 14 >> 7), 0, 127), bl = clamp(p[P_E6], -64, 63), rnd = clamp(p[P_E7], 0, 127);
    int32_t lo = clamp(v->s[1], -262144, 262144), bd = clamp(v->s[2], -262144, 262144), eg = clamp(v->s[3], 0, 32767);
    int32_t pe = clamp(v->s[0], 0, 32767), ck = clamp(v->s[7], 0, 32767), ka, kp, kc, y[CTL], ct;   /* (0.16.1: held to their
                                                          * ranges: another engine's state in them) */
    uint32_t ph0 = v->ph[0], ph1 = v->ph[1], ph2 = v->ph[2];
    if (rnd) {                                           /* RANDOM: the note's own, the same each pass */
        fm = clamp(fm + ((((int32_t)(h & 255u) - 128) * rnd) >> 7), 0, 127);
        dec = clamp(dec + ((((int32_t)((h >> 8) & 255u) - 128) * rnd) >> 8), 0, 127);
        bl = clamp(bl + ((((int32_t)((h >> 16) & 255u) - 128) * rnd) >> 8), -64, 63);
        if (wv < 5u && (int32_t)((h >> 24) & 127u) < rnd >> 1) rt = (rt + 1u + (h >> 30)) % 8u;
    }
    ct = (int32_t)trk_wob[ti] + ((((int32_t)((h >> 4) & 63u) - 32) * rnd) >> 9);   /* (the wander, the note's own cents) */
    ka = fmd_fall(TIME_MS_X10[clamp(4 + dec, 0, 127)]);
    kp = fmd_fall(TIME_MS_X10[clamp(wv == 7u ? 10 + dec / 2 : dec / 3, 0, 127)]);   /* (the BLOOP quicker; DROP's rise the ping's) */
    kc = fmd_fall(4);                                    /* (the tick: ~0.4 ms) */
    if (n > CTL)
        n = CTL;
    for (c = 0; c < n; c += 16u) {
        uint32_t e = c + 16u < n ? c + 16u : n, inc, mi, i1 = 0, i2 = 0;
        int32_t idx = (fm * (6000 + ((eg * 3) >> 2))) >> 13, f, a1, a2, b16;   /* (the FM falls with the ping: 0 .. ~470) */
        b16 = wv == 7u ? (bl < 0 ? -bl : bl) * 8 * (32767 - pe) >> 15   /* DROP: up as it falls (BLOOP: how far, +32 st) */
                       : (bl * 6 * pe) >> 15;            /* (BLOOP: +-24 st at the top) */
        inc = cents_inc(m->pitch16 + b16, ct, m->fine);
        mi = (inc >> 8) * BLP_RATIO[rt];
        if (wv == 5u || wv == 6u) {                      /* WOOD, GLASS: two partials; GLASS a few cents apart: beating */
            i1 = (inc >> 8) * BLP_PART[wv - 5u][0] + (wv == 6u ? inc >> 9 : 0u);
            i2 = (inc >> 8) * BLP_PART[wv - 5u][1] - (wv == 6u ? inc >> 8 : 0u);
        }
        a1 = wv == 5u ? (eg * eg) >> 15 : eg;   /* (WOOD's partials fall faster: the bar) */
        a2 = wv == 5u ? (a1 * a1) >> 15 : (eg * 3) >> 2;
        a1 = (a1 * (8 + fm)) >> 8; a2 = (a2 * (4 + fm)) >> 9;
        f = dub_f(m->pitch16 + (col - 52) * 6 + (eg >> 7) * 2);   /* (the gate: the note's own, COLOR, the ping opening it) */
        for (i = c; i < e; i++) {
            int32_t s, hi;
            switch (wv) {
            case 1: s = osc_tri(ph0 + ((uint32_t)(osc_sine(ph1) * idx) << 9)); break;
            case 2: s = clamp(osc_sine(ph0 + ((uint32_t)(osc_sine(ph1) * idx) << 9)) * 3, -32767, 32767); break;   /* (a soft square) */
            case 3: s = osc_sine(ph0 + ((uint32_t)(osc_sine(ph1) * idx) << 10)); break;
            case 4: s = osc_sine(ph0 + ((uint32_t)(osc_sine(ph1) * idx) << 9)) * (2 + fm / 24);   /* FOLD: driven, folded back */
                    while (s > 32767 || s < -32767) s = s > 32767 ? 65534 - s : -65534 - s;
                    break;
            case 5: case 6: s = osc_sine(ph0) + ((osc_sine(ph1) * a1) >> 15) + ((osc_sine(ph2) * a2) >> 15);
                    s = (s * 3) >> 2;
                    break;
            case 7: s = osc_sine(ph0 + ((uint32_t)(osc_sine(ph1) * (idx >> 2)) << 9)); break;   /* (DROP: a sine, FM a sheen) */
            default: s = osc_sine(ph0); break;
            }
            s = (s * eg) >> 15;
            if (ck) {                                    /* (the trigger's tick: needle-sharp) */
                s += (((int32_t)(noise32(&v->s[4]) >> 17) - 16384) * ck) >> (wv == 5u ? 15 : 17);
                ck = (ck * kc) >> 15;
            }
            hi = clamp(s - lo - ((bd >> 8) * 20000 >> 7), -600000, 600000);
            bd = clamp(bd + (((hi >> 3) * f) >> 12), -262144, 262144);
            lo = clamp(lo + (((bd >> 3) * f) >> 12), -262144, 262144);
            y[i] = lo;
            ph0 += inc;
            ph1 += (wv == 5u || wv == 6u) ? i1 : mi;
            ph2 += i2;
        }
        eg = (eg * ka) >> 15;
        pe = (pe * kp) >> 15;
    }
    for (i = 0; i < n; i++)
        out[i] += voice_amp(soft_knee(y[i] + (y[i] >> 1), 28000), m, i);
    v->s[0] = pe; v->s[1] = lo; v->s[2] = bd; v->s[3] = eg; v->s[7] = ck;
    v->ph[0] = ph0; v->ph[1] = ph1; v->ph[2] = ph2;
}

static void dub_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t at = (uint32_t)clamp(p[P_E0], 0, 2), ch = (uint32_t)clamp(p[P_E1], 0, 5), i, c, k;
    int32_t col = clamp(p[P_E2], 0, 127), den = clamp(p[P_E3], 0, 127), dec = clamp(p[P_E4], 0, 127);
    int32_t res = clamp(p[P_E5], 0, 127), nz = clamp(p[P_E7], 0, 127);
    int32_t lo = clamp(v->s[1], -262144, 262144), bd = clamp(v->s[2], -262144, 262144), eg = v->s[3], rng = v->s[4];   /* (locals:
                                                          * out[] may alias v->s[]; 0.16.0: the filter's range, another engine's
                                                          * state left in them) */
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
    /* BLEEP: {TYPE, WAVE, COLOR, FM, DECAY, RATIO, BLOOP, RANDOM} (0.16.1: GG's sonar, drops, glass and wood; the echoes
     * and the room theirs: GEN > BLEEP 4 SPACE sets the delay ping-pong; 0.16.1 RELEASE 90: the ping's own fall
     * heard whole, not cut by the step's gate) */
    {"SONAR PING", {2, 0, 66, 0, 50, 1, 0, 12}, {0, 0, 127, 90}, 0, TAG(BELL), FX(0, 0, 72, 64), PAT(0)},
    {"WATER DROP", {2, 7, 92, 8, 22, 1, 22, 30}, {0, 0, 127, 90}, 0, TAG(PLUCK), FX(0, 0, 64, 56), PAT(0)},
    {"CAVE DRIP", {2, 7, 58, 0, 34, 1, 34, 50}, {0, 0, 127, 90}, 0, TAG(PLUCK), FX(0, 0, 76, 84), PAT(0)},
    {"GLASS PING", {2, 6, 112, 60, 50, 1, 0, 20}, {0, 0, 127, 90}, 0, TAG(BELL), FX(0, 0, 66, 52), PAT(0)},
    {"WOOD BLOCK", {2, 5, 62, 72, 16, 1, 0, 30}, {0, 0, 127, 90}, 0, TAG(PLUCK), FX(0, 0, 58, 36), PAT(0)},
    {"BLEEP SINE", {2, 0, 90, 6, 30, 1, 4, 24}, {0, 0, 127, 90}, 0, TAG(PLUCK), FX(0, 0, 64, 40), PAT(0)},
    {"RANDOM PINGS", {2, 3, 90, 50, 30, 2, 16, 110}, {0, 0, 127, 90}, 0, TAG(PLUCK), FX(0, 0, 66, 44), PAT(0)},
    {"R2 BLIPS", {2, 3, 100, 64, 20, 4, -18, 100}, {0, 0, 127, 90}, 0, TAG(PLUCK), FX(0, 0, 60, 40), PAT(0)},
    {"BLOOP DROP", {2, 0, 80, 14, 44, 2, 40, 40}, {0, 0, 127, 90}, 0, TAG(PLUCK), FX(0, 0, 64, 44), PAT(0)},
    {"METAL TICK", {2, 3, 120, 46, 10, 7, 0, 30}, {0, 0, 127, 90}, 0, TAG(BELL), FX(0, 0, 60, 48), PAT(0)},
};

/* ATMOS: DECAY's knob is its MOTION, THROW none; BLEEP: its own seven */
static const param_desc_t *dub_desc(const track_t *t, uint32_t k)
{
    static const param_desc_t A[2] = {{"MOTION", F_PCT, 0, 127, 40, 0, 0}, {"-", F_PCT, 0, 127, 0, 0, 0}};
    static const param_desc_t B[8] = {{"TYPE", F_ENUM, 0, 2, 0, N_DUB_TYPE, 0}, {"WAVE", F_ENUM, 0, 7, 0, N_BLP_WAVE, 0},
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
