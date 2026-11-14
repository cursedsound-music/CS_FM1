/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound */
/* (cursedsound) cursed-os's RUMBLE (FX > RUMBLE): the kick's dark tail, the hypnotic-techno low end. The drum tracks'
 * sum, low-passed under ~180 Hz (the kick, hardly anything else of a kit), goes into a small dark reverb of its own
 * (four feedback lines, a Hadamard mix, a damping low-pass in each loop), run at an eighth of the rate (5.5 kHz: all it
 * holds is under 250 Hz); its return low-passed again (TONE), ducked by every kick (DUCK: down at the kick, back up
 * over three quarters of a beat, so it swells into the offbeat) and mixed mono into the master before the DJ FILTER,
 * TAPE, the ISOLATOR and the COMP: LOW KILL, a HP sweep and the isolator take it too.
 *   rmb_v  LEVEL 0..100 (0: off, nothing of this runs), DECAY 0..100, TONE 0..100 (60 .. 250 Hz), DUCK 0..100
 * The feed: fx.c mix_part, a drum track's block at its LEVEL (rmb_feed, each core into its own buffer); the rest:
 * mix_block, once its parts are in (rmb_block). Both out of the RAM code (through a register, as tape_block) */
#define RB_D 8u                                 /* the reverb's rate: an eighth */
#define RB_L0 157u                              /* its lines (5.5 kHz samples: 28 36 48 60 ms; primes) */
#define RB_L1 199u
#define RB_L2 263u
#define RB_L3 331u
enum { RB_LVL, RB_DEC, RB_TONE, RB_DUCK, RB_N };
static const uint8_t RB_DEF[RB_N] = {0, 70, 50, 80};
static uint8_t rmb_v[RB_N] = {0, 70, 50, 80};   /* the knobs (a project's: 'RB', project.c proj_rb) */
static int32_t rmb_in[2][CTL];
static volatile uint8_t fx_stems;               /* (fx.c's, below: tentative here) STEMS on; */
static int32_t stem_buf[NPART + 1][CTL];        /* .. each part's stem: RUMBLE goes into the first kit's */                  /* the drum tracks' feed, a core each (mix_part runs on both) */
static int16_t rb_line[RB_L0 + RB_L1 + RB_L2 + RB_L3];
static struct {
    uint16_t w[4];                              /* each line's write position */
    int32_t pz1, pz2, lz[4], oz1, oz2;          /* the low-passes: the feed's, the loops' damping, the return's */
    int32_t acc, y0, y1;                        /* the eighth's sum; the reverb's last two outputs */
    uint32_t ph;                                /* the sample in the eighth */
    int32_t env, g;                             /* the kick's envelope (Q15); the gain at the block's end (Q15) */
    uint8_t on, att;                            /* running since it was last turned on; DUCK's attack seen */
    int32_t pk;                                 /* (tests) the return's peak this block */
} rb;

static __attribute__((noinline)) void rmb_feed(const int32_t *b, uint32_t n, int32_t lq, int32_t dl, uint32_t c1)
{
    int32_t *d = rmb_in[c1 & 1u];
    uint32_t i;
    for (i = 0; i < n; i++, lq += dl)
        d[i] += ((b[i] >> 2) * (lq >> 8)) >> 10;   /* (as mix_part's own: the part at its LEVEL) */
}

/* the reverb, a sample at the eighth's rate: x in, its output */
static int32_t rb_step(int32_t x, int32_t g, int32_t kd)
{
    static const uint16_t L[4] = {RB_L0, RB_L1, RB_L2, RB_L3};
    int16_t *ln[4] = {rb_line, rb_line + RB_L0, rb_line + RB_L0 + RB_L1, rb_line + RB_L0 + RB_L1 + RB_L2};
    int32_t s[4], h[4], a, b, c, d, y;
    uint32_t j;
    for (j = 0; j < 4u; j++)
        s[j] = (int32_t)ln[j][rb.w[j]] << 2;
    a = s[0] + s[1]; b = s[0] - s[1]; c = s[2] + s[3]; d = s[2] - s[3];
    h[0] = (a + c) >> 1; h[1] = (b + d) >> 1; h[2] = (a - c) >> 1; h[3] = (b - d) >> 1;   /* (orthonormal: 1/2) */
    for (j = 0; j < 4u; j++) {
        int32_t f = ((h[j] >> 3) * g) >> 12;    /* (h within +-262136: >> 3 keeps the product in 32 bits) */
        rb.lz[j] += (((f - rb.lz[j]) >> 2) * kd) >> 13;   /* (the loop's damping: darker the longer it rings) */
        f = rb.lz[j] + ((j & 1u) ? -x : x);
        ln[j][rb.w[j]] = (int16_t)clamp(f >> 2, -32767, 32767);
        if (++rb.w[j] >= L[j]) rb.w[j] = 0;
    }
    y = (s[0] - s[1] + s[2] - s[3]) >> 1;
    return y;
}

/* mix_block, after the parts and the buses' returns: the block's rumble into the master (mix_l mix_r) */
static __attribute__((noinline)) void rmb_block(uint32_t n)
{
    int32_t *f0 = rmb_in[0], *f1 = rmb_in[1];
    int32_t g, kd, kt, kp = 830, dep, e, gt, g0, dg, lv, pk = 0, *sb = 0;   /* (kp: ~180 Hz at 44.1 kHz) */
    uint32_t i, b;
    if (n > CTL) n = CTL;
    if (!rb.on) {                                /* (from off: an empty reverb, the feed from now) */
        memset(&rb, 0, sizeof rb);
        memset(rb_line, 0, sizeof rb_line);
        rb.g = 32767;
        rb.on = 1;
        for (i = 0; i < n; i++) f0[i] = f1[i] = 0;
    }
    g = 16384 + (int32_t)rmb_v[RB_DEC] * 154;    /* DECAY: 0.5 .. 0.97 a pass */
    kd = 4000 + (int32_t)rmb_v[RB_TONE] * 80;    /* the loops' damping: ~120 .. 400 Hz */
    kt = 280 + (int32_t)rmb_v[RB_TONE] * 865 / 100;   /* the return's: ~60 .. 250 Hz */
    lv = (int32_t)rmb_v[RB_LVL] * rmb_v[RB_LVL] * 3;  /* LEVEL (Q15, squared: a fader's feel), 100: 30000 */
    /* DUCK: a kick (fx.c duck_block's attack, which sets duck_att at each) at full, then down to 0 over three
     * quarters of a beat; the gain 1 - DUCK x (1 - (1 - env)^2) (low for longer, a quick rise: the pump) */
    if (duck_att && !rb.att) rb.env = 32767;
    rb.att = duck_att;
    b = beat_samples() * 3u / 4u;
    e = (int32_t)(32767u * n / (b ? b : 1u));
    rb.env = rb.env > e ? rb.env - e : 0;
    dep = (int32_t)rmb_v[RB_DUCK] * 32767 / 100;
    e = (rb.env * (65536 - rb.env)) >> 15;
    gt = 32767 - mulq15(dep, e > 32767 ? 32767 : e);
    gt = mulq15(gt, lv);
    g0 = rb.g;
    dg = (gt - g0) >> CTL_LOG2;
    if (fx_stems) {                              /* (STEMS: in the first kit's stem, the part it comes from) */
        uint32_t k;
        for (k = 0; k < NPART && ENGINES[trk[k].engine] != &ENG_DRUM; k++) {}
        sb = k < NPART ? stem_buf[k] : 0;
    }
    for (i = 0; i < n; i++, g0 += dg) {
        int32_t x = f0[i] + f1[i], u, o;
        f0[i] = f1[i] = 0;
        rb.pz1 += mulq15(x - rb.pz1, kp);
        rb.pz2 += mulq15(rb.pz1 - rb.pz2, kp);
        rb.acc += rb.pz2;
        if (++rb.ph >= RB_D) {
            rb.ph = 0;
            rb.y0 = rb.y1;
            rb.y1 = rb_step(rb.acc >> 4, g, kd);   /* (the eighth's mean, halved: room in the lines) */
            rb.acc = 0;
        }
        u = rb.y0 + (((rb.y1 - rb.y0) * (int32_t)rb.ph) >> 3);   /* (back up to the rate: a line between them) */
        rb.oz1 += mulq15(u - rb.oz1, kt);
        rb.oz2 += mulq15(rb.oz1 - rb.oz2, kt);
        o = mulq15(clamp(rb.oz2, -196607, 196607), g0 >> 2) * 6;   /* (g0 within 30000: the product fits; x 1.5:
                                                         * LEVEL 100 ~8 dB under the kick) */
        mix_l[i] += o;
        mix_r[i] += o;
        if (sb) sb[i] += o;
        o = o < 0 ? -o : o;
        pk = o > pk ? o : pk;
    }
    rb.g = gt;
    rb.pk = pk;

}
