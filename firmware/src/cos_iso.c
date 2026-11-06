/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursedsound) cursed-os's ISOLATOR (HOME > ISOLATOR, HOME's black 9-11 and the FX pad's black 8 its kills; included
 * by cosfx.c): a DJ mixer's three-band isolator on the master, LOW (under ~200 Hz), MID, HIGH (over ~2.9 kHz), each
 * KILL, -24 .. +6 dB.
 * The split is Linkwitz-Riley, 24 dB an octave at both crossovers (two Butterworth state-variable filters a side, the
 * ZDF form: Simper, "Linear Trapezoidal Integrated SVF", 2013). Five filters a side, not a 3-way tree's seven: with
 * AP1, AP2 the crossovers' all-passes (LP4 + HP4 at 200 Hz, at 2.9 kHz),
 *     y = AP2 (gM AP1 x + (gL - gM) LP4(200) x) + (gH - gM) HP4(2.9k) AP1 x
 * at gL = gM = gH = 1 the all-pass AP1 AP2: flat (tests/cos_acc_test.c: within 0.1 dB everywhere; KILL -55 dB at
 * 40 Hz and 16 kHz, -40 at the mids' middle, 762 Hz: the other two's slopes reach it). With all three at 0 dB and no kill it is out of the path (iso.w: in and out in 2.9 ms, the kills' ramp), so it costs nothing
 * and the master is as it was. Replaces the kills' own filters (JIANT's master EQ, FX > EQ, untouched: the ISOLATOR
 * rode it before 0.6, a one-pole split that cut a band by 6 .. 9 dB at most) */
#define ISO_KILL (-25)                        /* iso_v: a band off */
#define ISO_SLOPE 256                         /* a gain's step a sample (Q15): 0 dB to KILL in 2.9 ms, as the kills */
/* a crossover's Butterworth SVF, Q16: g = tan(pi fc / 44100), 2R + g - 1 (R = 1 / sqrt 2), 1 / (1 + 2R g + g^2) */
static const uint16_t ISO_C[2][3] = {{934, 28080, 64229}, {13735, 40881, 48896}};   /* 200 Hz, 2.9 kHz */
#define ISO_AP 54292u                         /* (2 sqrt 2 - 2), Q16: the all-pass, x - 2 sqrt 2 band */
/* -24 .. +6 dB, Q15 (+6 under 2^16: mulq15s's k) */
static const int32_t ISO_G[31] = {2068, 2320, 2603, 2920, 3277, 3677, 4125, 4629, 5193, 5827, 6538, 7336, 8231, 9235,
                                  10362, 11627, 13045, 14637, 16423, 18427, 20675, 23198, 26029, 29205, 32768, 36766,
                                  41252, 46286, 51934, 58270, 65381};
static volatile int8_t iso_v[3];              /* main: LOW MID HIGH, dB (ISO_KILL: off); a project's ('IS') */
static struct {
    int32_t s[2][5][2];                       /* L R: the five filters' states */
    int32_t g[3];                             /* the bands' gains now (Q15), on their way to the targets */
    int32_t w;                                /* in the path (Q15, cx_ramp) */
} iso = {.g = {32768, 32768, 32768}};

/* one SVF step: o[0] low, o[1] band, o[2] high (x and the states scaled up by 8 for the 200 Hz filters' sake) */
static inline void iso_svf(int32_t x, int32_t *s, const uint16_t *c, int32_t *o)
{
    int32_t h = mulq16(x - s[0] - mulq16(s[0], c[1]) - s[1], c[2]), t = mulq16(h, c[0]), b = t + s[0], u;
    s[0] = t + b;
    u = mulq16(b, c[0]);
    o[0] = u + s[1];
    s[1] = u + o[0];
    o[1] = b;
    o[2] = h;
}
/* a x k / 32768, k signed Q15 under 2^16 in size, a up to 2^30 (no 64-bit) */
static inline int32_t mulq15s(int32_t a, int32_t k) { return (a >> 15) * k + (((a & 0x7FFF) * k) >> 15); }
/* the all-pass from an SVF step at the same input: x - 2 sqrt 2 band */
static inline int32_t iso_ap(int32_t x, int32_t b) { return x - 2 * b - mulq16(b, ISO_AP); }
/* a channel: the three bands at their gains */
static inline int32_t iso_ch(int32_t x, int32_t (*s)[2], const int32_t *g)
{
    int32_t a[3], lo[3], d[3], hi[3], o[3], ap1, z;
    x <<= 3;
    iso_svf(x, s[0], ISO_C[0], a);              /* 200 Hz: its all-pass, and the lows (LR4: two low-passes) */
    ap1 = iso_ap(x, a[1]);
    iso_svf(a[0], s[1], ISO_C[0], lo);
    iso_svf(ap1, s[2], ISO_C[1], d);            /* 2.9 kHz: the highs (two high-passes) of the all-passed */
    iso_svf(d[2], s[3], ISO_C[1], hi);
    z = mulq15s(ap1, g[1]) + mulq15s(lo[0], g[0] - g[1]);
    iso_svf(z, s[4], ISO_C[1], o);              /* .. and its all-pass on the lows and mids */
    return (iso_ap(z, o[1]) + mulq15s(hi[2], g[2] - g[1])) >> 3;
}
/* a band's target: its knob, or 0 while its kill is held */
static inline int32_t iso_target(int32_t v, int kill) { return kill || v <= ISO_KILL ? 0 : ISO_G[clamp(v, -24, 6) + 24]; }
