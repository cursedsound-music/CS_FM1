/* SPDX-License-Identifier: GPL-3.0-only */
/* (cursed-os 0.9.2) the output as it leaves (after the master's COMP and LIMIT; what the DAC and USB get), for GLO's
 * SCOPE, SPECTRUM, STEREO and LOUDNESS (cos.c). Called by audio.c's audio_block with each block (Q15 L R pairs).
 *   scope_buf the left at 44.1 kHz (512: 11.6 ms), scope_r the right; scope_lo the mid (L + R) / 2 and scope_ls the side
 *   (L - R) / 2 at an eighth of the rate (5.5 kHz: 93 ms; low-passed first, two poles at 2 kHz (-1 dB at 700 Hz), then
 *   an average of eight).
 *   The meters: blocks of 100 ms (4410 samples) of the K-weighted power (ITU-R BS.1770: its shelf and high-pass, the
 *   coefficients for 44.1 kHz), L L, R R, L R and the sample peak; the last 32 kept (mt_ring), mt_n counts them.
 * Integers only: the firmware's main unit has no FPU code (tools/build.py: soft floats, ~100 cycles an operation).
 * The filters: direct form I, coefficients Q28, the signal Q23 (Q15 << 8: the high-pass's poles 0.989 near 1 keep
 * their precision), 64-bit sums. Its cost: ~25 multiplies a sample, under 1 % of the CPU (tests/cos_meter_test.c) */
#ifndef SCOPE_N
#define SCOPE_N 512u
static int16_t scope_buf[SCOPE_N];
static uint32_t scope_w;
#endif
static int16_t scope_r[SCOPE_N], scope_lo[SCOPE_N], scope_ls[SCOPE_N];
static uint32_t scope_lw;
static int32_t scope_lp[2][2], scope_acc[2];
typedef struct {
    uint64_t kw;                                        /* the K-weighted power, L and R summed (Q30 a sample) */
    uint64_t ll, rr;                                    /* L L, R R (Q30) */
    int64_t lr;                                         /* L R */
    int32_t pk;                                         /* the sample peak (Q15, either side) */
} mt_blk_t;
static mt_blk_t mt_ring[32];
static volatile uint32_t mt_n;
static mt_blk_t mt_acc;
static uint32_t mt_cnt;
static int32_t mt_x[2][4], mt_y[2][4];                  /* each side's two biquads' x1 x2, y1 y2 (Q23) */
#define MT_Q(v) ((int32_t)((v) * 268435456.0 + ((v) < 0 ? -0.5 : 0.5)))   /* Q28 */
static const int32_t MT_C[2][5] = {                     /* b0 b1 b2 a1 a2 (a1 a2 as y = .. - a1 y1 - a2 y2) */
    {MT_Q(1.5308412), MT_Q(-2.6509800), MT_Q(1.1690791), MT_Q(-1.6636551), MT_Q(0.7125954)},   /* the shelf, +4 dB */
    {MT_Q(1.0), MT_Q(-2.0), MT_Q(1.0), MT_Q(-1.9891697), MT_Q(0.9891990)}};                     /* the high-pass, 38 Hz */
static inline int32_t mt_bq(int32_t x, int32_t *xs, int32_t *ys, const int32_t *c)
{
    int64_t a = (int64_t)c[0] * x + (int64_t)c[1] * xs[0] + (int64_t)c[2] * xs[1] - (int64_t)c[3] * ys[0] - (int64_t)c[4] * ys[1];
    int32_t y = (int32_t)(a >> 28);
    xs[1] = xs[0]; xs[0] = x;
    ys[1] = ys[0]; ys[0] = y;
    return y;
}
static inline int32_t mt_kf(int32_t x15, uint32_t ch)   /* Q15 in, the K-weighted Q15 out */
{
    int32_t v = mt_bq(x15 * 256, &mt_x[ch][0], &mt_y[ch][0], MT_C[0]);
    v = mt_bq(v, &mt_x[ch][2], &mt_y[ch][2], MT_C[1]);
    return v >> 8;
}
static __attribute__((noinline)) void mt_block(const int32_t *out, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t li = clamp(out[2u * i], -32768, 32767), ri = clamp(out[2u * i + 1u], -32768, 32767);
        int32_t m = (li + ri) >> 1, sd = (li - ri) >> 1, c, kl, kr, al = li < 0 ? -li : li, ar = ri < 0 ? -ri : ri;
        scope_buf[scope_w & (SCOPE_N - 1u)] = (int16_t)li;
        scope_r[scope_w++ & (SCOPE_N - 1u)] = (int16_t)ri;
        for (c = 0; c < 2; c++) {                         /* (the rings at 5.5 kHz: the mid, the side) */
            int32_t x = c ? sd : m;
            scope_lp[c][0] += (x - scope_lp[c][0]) >> 2;
            scope_lp[c][1] += (scope_lp[c][0] - scope_lp[c][1]) >> 2;
            scope_acc[c] += scope_lp[c][1];
        }
        if (!(scope_w & 7u)) {
            scope_lo[scope_lw & (SCOPE_N - 1u)] = (int16_t)clamp(scope_acc[0] >> 3, -32768, 32767);
            scope_ls[scope_lw++ & (SCOPE_N - 1u)] = (int16_t)clamp(scope_acc[1] >> 3, -32768, 32767);
            scope_acc[0] = scope_acc[1] = 0;
        }
        kl = mt_kf(li, 0);
        kr = mt_kf(ri, 1);
        mt_acc.kw += (uint64_t)((int64_t)kl * kl) + (uint64_t)((int64_t)kr * kr);
        mt_acc.ll += (uint64_t)((int64_t)li * li);
        mt_acc.rr += (uint64_t)((int64_t)ri * ri);
        mt_acc.lr += (int64_t)li * ri;
        if (al > mt_acc.pk) mt_acc.pk = al;
        if (ar > mt_acc.pk) mt_acc.pk = ar;
        if (++mt_cnt == 4410u) {
            mt_ring[mt_n & 31u] = mt_acc;
            mt_n++;
            mt_acc.kw = mt_acc.ll = mt_acc.rr = 0;
            mt_acc.lr = 0;
            mt_acc.pk = 0;
            mt_cnt = 0;
        }
    }
}
