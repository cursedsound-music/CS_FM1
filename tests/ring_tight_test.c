/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) cursed-os 0.13.1: DIST RING (fx.c dist_ring, after the LXR's Ringmod.c) and the reverb TIGHT (fx.c
 * rev_tight, G_RTYPE 6, after the LXR's Reverb.c).
 *   RING: a 1 kHz sine through it, DIST 127: the 1 kHz mostly gone (the sidebands instead), bounded at full scale;
 *         DIST 4: mostly dry; TONE moves the sidebands; its knob reads FREQ.
 *   TIGHT: a noise burst then silence: a tail, bounded, dying away; longer with SIZE; its level within 6 dB of
 *          PLATE's; EARLY (WIDE's slot) adds level at the start; reachable as TYPE 6, named TIGHT.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/ring_tight_test tests/ring_tight_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } else { printf("ok: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* the energy at f (Hz) in x[0..n) (a single DFT bin) */
static double bin(const int32_t *x, uint32_t n, double f)
{
    double re = 0, im = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        re += x[i] * cos(2 * M_PI * f * i / FS);
        im += x[i] * sin(2 * M_PI * f * i / FS);
    }
    return (re * re + im * im) / ((double)n * n);
}
#define RN 8192u
static int32_t rin[RN], rout[RN];
static int32_t ring_run(int32_t dist, int32_t tone, int32_t amp, int32_t *pk)
{
    track_t *t = &trk[0];
    uint32_t i, b;
    t->p[P_DTYPE] = 8;
    t->p[P_DIST] = (int16_t)dist;
    t->p[P_DTONE] = (int16_t)tone;
    t->dist_hold = 0;
    dist_w[0] = 0;
    *pk = 0;
    for (b = 0; b < 4u; b++) {                         /* (the blend settled first) */
        int32_t s[CTL];
        for (i = 0; i < CTL; i++) s[i] = 0;
        track_dist(t, s, CTL);
    }
    for (i = 0; i < RN; i++) rin[i] = rout[i] = (int32_t)(amp * sin(2 * M_PI * 1000.0 * i / FS));
    for (b = 0; b + CTL <= RN; b += CTL) track_dist(t, rout + b, CTL);
    for (i = 0; i < RN; i++) if (abs(rout[i]) > *pk) *pk = abs(rout[i]);
    return 0;
}

static uint32_t xs = 0x2545F491u;
static int32_t noise(int32_t amp)
{
    xs ^= xs << 13; xs ^= xs >> 17; xs ^= xs << 5;
    return (int32_t)(((int64_t)(int32_t)xs * amp) >> 31);
}
/* the reverb model ty: a 200 ms noise burst into the bus, then silence; the wet's energy in windows of 100 ms */
#define WN 60u
static double win[WN], first_ms;
static int32_t verb(uint32_t ty, int32_t size, int32_t early)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t t, i, ns = WN * (FS / 10u);
    int32_t pk = 0;
    rev_clear();
    song.g[G_RTYPE] = (int16_t)ty;
    fx.rtype = ty;
    song.g[G_RSIZE] = (int16_t)size;
    song.g[G_RWIDE] = (int16_t)early;
    song.g[G_RPRE] = 0;
    song.g[G_RMOD] = 0;
    song.g[G_DMIX] = 0;
    song.g[G_RFILT] = 0;
    memset(win, 0, sizeof win);
    first_ms = 0;
    for (t = 0; t < 400u * CTL; t += CTL) {            /* (the bus's own fades and smoothing settled) */
        for (i = 0; i < CTL; i++) { c[i] = d[i] = r[i] = 0; fx_sl[i] = fx_sr[i] = 0; }
        fx_buses(c, d, r, w, CTL);
    }
    xs = 0x2545F491u;
    for (t = 0; t < ns; t += CTL) {
        for (i = 0; i < CTL; i++) {
            c[i] = d[i] = 0;
            r[i] = t + i < FS / 5u ? noise(12000) : 0;
            fx_sl[i] = fx_sr[i] = 0;
        }
        fx_buses(c, d, r, w, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t l = w[i] + fx_sl[i];
            win[(t + i) / (FS / 10u)] += (double)l * l;
            if (t + i < FS / 50u) first_ms += (double)l * l;   /* (the first 20 ms) */
            if (abs(l) > pk) pk = abs(l);
        }
    }
    return pk;
}
static double dbe(double e) { return 10 * log10(e + 1e-9); }
/* the RT60 (s): when the windows' energy falls 30 dB under the first window after the burst, x 2 */
static double rt(void)
{
    uint32_t k;
    double ref = win[2];
    for (k = 3; k < WN; k++) if (dbe(win[k]) < dbe(ref) - 30) return (k - 2) * 0.1 * 2;
    return WN * 0.2;
}

int main(void)
{
    int32_t pk;
    double e1, e0, s_lo, s_hi, lp, lt, r0, r90, r127;
    ui_power_on();
    load_depth++; fm6_init(); power_on_parts(); load_depth--;
    song.master_q12 = 4096;
    song.playing = 0;
    /* RING */
    ring_run(127, 0, 20000, &pk);
    e0 = bin(rin, RN, 1000), e1 = bin(rout, RN, 1000);
    CHECK(dbe(e1) < dbe(e0) - 12, "RING at DIST 127: the 1 kHz %.1f dB under the input", dbe(e0) - dbe(e1));
    s_lo = bin(rout, RN, 1000 - 330), s_hi = bin(rout, RN, 1000 + 330);   /* (TONE 0: note 64, ~330 Hz) */
    CHECK(dbe(s_lo) > dbe(e0) - 12 && dbe(s_hi) > dbe(e0) - 12, "RING: its sidebands at 1 kHz +- 330 Hz (%.1f %.1f dB)",
          dbe(s_lo) - dbe(e0), dbe(s_hi) - dbe(e0));
    CHECK(pk > 3000 && pk < 40000, "RING: bounded, sounding (peak %d)", pk);
    ring_run(127, 63, 20000, &pk);
    CHECK(dbe(bin(rout, RN, 1000 - 330)) < dbe(e0) - 20, "RING: TONE 63 moves the sidebands away from 330 Hz");
    ring_run(4, 0, 20000, &pk);
    CHECK(dbe(bin(rout, RN, 1000)) > dbe(e0) - 3, "RING at DIST 4: mostly dry");
    ring_run(127, -64, 230000, &pk);
    CHECK(pk <= 262144 * 2, "RING: full scale in, bounded (peak %d)", pk);
    trk[0].p[P_DTYPE] = 8;
    CHECK(!strcmp(track_desc(&trk[0], P_DTONE)->label, "FREQ"), "RING: TONE reads FREQ");
    CHECK(!strcmp(N_DTYPE[8], "RING") && track_desc(&trk[0], P_DTYPE)->max >= 8, "RING: DIST TYPE 8");
    trk[0].p[P_DTYPE] = 0;
    CHECK(!strcmp(track_desc(&trk[0], P_DTONE)->label, "TONE"), "other types: TONE");
    /* TIGHT */
    CHECK(!strcmp(N_RTYPE[6], "TIGHT") && GP[G_RTYPE].max == 6, "TIGHT: REVERB TYPE 6");
    pk = verb(5, 64, 0);
    lp = win[2] + win[3] + win[4] + win[5] + win[6];
    pk = verb(6, 64, 0);
    lt = win[2] + win[3] + win[4] + win[5] + win[6];
    CHECK(fabs(dbe(lt) - dbe(lp)) < 6, "TIGHT's level within 6 dB of PLATE's (%.1f dB)", dbe(lt) - dbe(lp));
    CHECK(pk < 32768 * 3, "TIGHT bounded (peak %d)", pk);
    verb(6, 0, 0); r0 = rt();
    verb(6, 90, 0); r90 = rt();
    pk = verb(6, 127, 0); r127 = rt();
    printf("TIGHT RT60 ~ %.1f / %.1f / %.1f s at SIZE 0 / 90 / 127\n", r0, r90, r127);
    CHECK(r0 < r90 && r90 < r127, "TIGHT: longer with SIZE");
    CHECK(r0 <= 1.0 && r90 >= 0.8 && r90 <= 4.5, "TIGHT: short at 0, ~2 s at 90");
    CHECK(pk < 32768 * 3, "TIGHT at SIZE 127 bounded (peak %d)", pk);
    CHECK(dbe(win[WN - 1]) < dbe(win[2]) - 30, "TIGHT at SIZE 127 dies away (%.1f dB in 6 s)", dbe(win[2]) - dbe(win[WN - 1]));
    verb(6, 64, 0); e0 = win[0];
    verb(6, 64, 127); e1 = win[0];
    CHECK(dbe(e1) > dbe(e0) + 1.5 && dbe(e1) < dbe(e0) + 15, "TIGHT: EARLY adds the reflections (+%.1f dB in the first 100 ms)", dbe(e1) - dbe(e0));
    song.g[G_RTYPE] = 6;
    {
        uint32_t k;
        int16_t *vp;
        const param_desc_t *d = 0;
        for (k = 0; k < sizeof PAGES / sizeof PAGES[0]; k++)
            if (!strcmp(PAGES[k].title, "REVERB 2")) d = page_desc(&PAGES[k], 3, &vp);
        CHECK(d && !strcmp(d->label, "EARLY"), "TIGHT: WIDE reads EARLY");
    }
    printf(fails ? "ring_tight: %d FAILED\n" : "ring_tight: all ok\n", fails);
    return fails != 0;
}
