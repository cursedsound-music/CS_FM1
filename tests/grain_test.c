/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) cursed-os 0.13.1, GG 2026-11-11: "hiss and artifacts, especially with limiter drive and sidechain".
 * The master COMP, the limiter's DRIVE and its gain reduction multiplied (x >> 4) * g: the mix cut to 11 bits. Now
 * fx.c mix_mul keeps every bit: a quiet sine (-70 dBFS, a tail's level) through each comes out as itself times the gain,
 * within a unit, never stepped to 16.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/grain_test tests/grain_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); } else printf("ok: "); printf(__VA_ARGS__); printf("\n"); } while (0)

static int32_t in[CTL];
static void fill(uint32_t b, double amp)
{
    uint32_t i;
    for (i = 0; i < CTL; i++) {
        in[i] = (int32_t)lround(amp * sin(2 * M_PI * 997.0 * (b * CTL + i) / FS));
        mix_l[i] = mix_r[i] = in[i];
        wet[i] = 0;
    }
}
int main(void)
{
    uint32_t b, i;
    int32_t worst = 0, x;
    /* mix_mul itself, against the exact product, over the whole range */
    for (x = -(1 << 21) + 1; x < (1 << 21); x += 997) {
        static const int32_t G[5] = {32768, 16384, 47000, 131072, 262143};
        for (i = 0; i < 5u; i++) {
            int64_t e = ((int64_t)x * (G[i] >> 4) + 1024) >> 11;
            int32_t d = (int32_t)(mix_mul(x, G[i]) - e);
            d = d < 0 ? -d : d;
            if (d > worst) worst = d;
        }
    }
    CHECK(worst <= 1, "mix_mul: within 1 of the exact product, +-2^21 x up to x8 (worst %d)", worst);
    CHECK(mix_mul(-7, 32768) == -7 && mix_mul(5, 32768) == 5 && mix_mul(-1, 32768) == -1, "mix_mul at unity: x itself");
    /* the COMP on, nothing over its threshold, INPUT 0, MAKEUP 0: the mix as it was */
    mc_amt = 1; mc.in = 0; mc.thr = 0; mc.mk = 0; mc.rat = 3; mc.knee = 0; mc.sc = 0; mc.mix = 100;
    comp_env = 0; comp_g = 32768; comp_hp = 0; comp_hc = 0;
    worst = 0;
    for (b = 0; b < 64u; b++) {
        fill(b, 10.4);                                  /* (-70 dBFS) */
        comp_block(CTL);
        for (i = 0; i < CTL; i++) { int32_t d = abs(mix_l[i] - in[i]); if (d > worst) worst = d; }
    }
    CHECK(worst <= 1, "COMP at unity: a -70 dBFS sine unchanged (worst %d; 15 before: 11 bits)", worst);
    /* MAKEUP +12: four times it */
    mc.mk = 12;
    worst = 0;
    for (b = 0; b < 64u; b++) {
        fill(b, 10.4);
        comp_block(CTL);
        if (b > 8u) for (i = 0; i < CTL; i++) { int32_t d = abs(mix_l[i] - 4 * in[i]); if (d > worst) worst = d; }
    }
    CHECK(worst <= 4, "COMP MAKEUP +12 dB: x4, not stepped (worst %d; 60 before)", worst);
    /* MIX 50 %: between, smooth */
    mc.mk = 0; mc.mix = 50;
    worst = 0;
    for (b = 0; b < 64u; b++) {
        fill(b, 10.4);
        comp_block(CTL);
        if (b > 8u) for (i = 0; i < CTL; i++) { int32_t d = abs(mix_l[i] - in[i]); if (d > worst) worst = d; }
    }
    CHECK(worst <= 1, "COMP MIX 50 %%: the quiet sine unchanged (worst %d; 15 before)", worst);
    mc_amt = 0;
    /* the limiter's DRIVE +12 dB */
    lim_in = 131072;
    worst = 0;
    for (b = 0; b < 8u; b++) {
        fill(b, 10.4);
        lim_drive(CTL);
        for (i = 0; i < CTL; i++) { int32_t d = abs(mix_l[i] - 4 * in[i]); if (d > worst) worst = d; }
    }
    CHECK(worst <= 1, "limiter DRIVE +12 dB: x4 exactly (worst %d; 60 before)", worst);
    lim_in = 32768;
    /* the limiter reducing (a loud peak held): a quiet sine under it, scaled, not stepped */
    {
        int32_t l, r, g;
        double err = 0;
        uint32_t k;
        lim_env = 4 * LIM_T; lim_hold = 60000; lim_g = 32768;
        memset(lim_dl, 0, sizeof lim_dl); memset(lim_dr, 0, sizeof lim_dr);
        lev_cur = 32768; lev_dg = 0;
        for (k = 0; k < 4000u; k++) {
            int32_t s = (int32_t)lround(400.0 * sin(2 * M_PI * 997.0 * k / FS));
            l = r = s;
            master_out(&l, &r);
            if (k > 200u) {
                g = lim_g;
                int32_t want = softclip((int32_t)lround(400.0 * sin(2 * M_PI * 997.0 * (k - LIM_LA) / FS) * g / 32768.0));
                err = fmax(err, fabs((double)(l - want)));
            }
        }
        CHECK(err <= 2, "limiter at -12 dB of gain: a quiet sine scaled within 2 (worst %.0f; 4 before)", err);
    }
    printf(fails ? "grain: %d FAILED\n" : "grain: all ok\n", fails);
    return fails != 0;
}
