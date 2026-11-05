/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursedsound) cursed-os's values as shown, against what the sound does (test tones through the DSP itself):
 *   BPM (and its tenths, the nudge), the delay's TIME (each division and free time), the COMP (THRESH, RATIO, KNEE,
 *   MAKEUP, INPUT, MIX), the LIMIT (CEILING, DRIVE, RELEASE) and the ISOLATOR's bands (LOW MID HIGH, -12 .. +6 dB)
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/cos_acc_test tests/cos_acc_test.c -lm */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int verbose;
#define SAY(...) do { if (verbose) printf(__VA_ARGS__); } while (0)

static double to_db(double r) { return 20.0 * log10(r); }

/* a knob's value as the screen writes it */
static void shown(const param_desc_t *d, int32_t v, char *b)
{
    const char *u = 0;
    param_format(d, v, b, &u);
    if (u) strcat(b, u);
}

/* ---- BPM ---- */
static void t_bpm(void)
{
    uint32_t b, t, worst = 0;
    song.g[G_CLOCK] = 0; midi_beat_samples = 0; bpm_nudge = 0;
    for (b = 40; b <= 250; b++)
        for (t = 0; t < 10u; t++) {
            double want = 60.0 * 44100.0 / ((double)b + t / 10.0);
            uint32_t got, e;
            song.g[G_BPM] = (int16_t)b; bpm_frac = (uint8_t)t;
            got = beat_samples();
            e = (uint32_t)fabs((double)got - want);
            if (e > worst) worst = e;
            CHECK(fabs((double)got - want) <= 1.0, "BPM %u.%u: a beat %u samples, %.1f wanted", b, t, got, want);
        }
    song.g[G_BPM] = 128; bpm_frac = 0;
    {
        uint32_t b0 = beat_samples(), up, dn;
        bpm_nudge = 1; up = beat_samples();
        bpm_nudge = -1; dn = beat_samples();
        bpm_nudge = 0;
        SAY("  BPM: 40 .. 250 in tenths within %u sample of the beat; nudge %+.2f %% / %+.2f %%\n", worst,
            100.0 * ((double)b0 / up - 1.0), 100.0 * ((double)b0 / dn - 1.0));
        CHECK(fabs((double)b0 / up - 1.03) < 0.002 && fabs((double)b0 / dn - 0.97) < 0.002, "the nudge: 3 %% faster / slower");
    }
}

/* ---- the delay's TIME ---- */
static void t_delay(void)
{
    const param_desc_t *d = &GP[G_DTIME];
    uint32_t i, beat;
    song.g[G_BPM] = 120; bpm_frac = 0;
    beat = beat_samples();
    for (i = 0; i <= (uint32_t)(d->max - d->min); i++) {
        const char *nm = d->names[i];
        double want = 0, got;
        song.g[G_DTIME] = (int16_t)(d->min + (int32_t)i);
        got = delay_samples();
        if (!strcmp(nm, "1/4")) want = beat;
        else if (!strcmp(nm, "1/8")) want = beat / 2.0;
        else if (!strcmp(nm, "1/16")) want = beat / 4.0;
        else if (!strcmp(nm, "1/32")) want = beat / 8.0;
        else if (!strcmp(nm, "8T")) want = beat / 3.0;
        else if (!strcmp(nm, "16T")) want = beat / 6.0;
        else if (!strcmp(nm, "1/2")) want = beat * 2.0;
        else if (!strcmp(nm, "1/1")) want = beat * 4.0;
        else if (!strcmp(nm, "2BAR")) want = beat * 8.0;
        else if (!strcmp(nm, "4BAR")) want = beat * 16.0;
        else if (!strcmp(nm, "1/8D")) want = beat * 0.75;
        else if (!strcmp(nm, "1/16D")) want = beat * 0.375;
        else if (!strcmp(nm, "1/4D")) want = beat * 1.5;
        else if (strstr(nm, "ms")) want = atof(nm) * 44.1;
        else if (nm[strlen(nm) - 1] == 's') want = atof(nm) * 44100.0;
        if (want > DLY_MAX) {                            /* (longer than the line: the line's longest) */
            CHECK(got == DLY_MAX, "TIME %s at 120: longer than the line, %g", nm, got);
            SAY("  TIME %-6s past the line (%u samples, %.0f ms): %.0f ms\n", nm, DLY_MAX, DLY_MAX / 44.1, got / 44.1);
            continue;
        }
        CHECK(want > 0 && fabs(got / want - 1.0) < 0.012, "TIME %s at 120 BPM: %.0f samples, %.0f wanted (%+.1f %%)", nm, got, want,
              100.0 * (got / want - 1.0));
    }
}

/* ---- the COMP ---- */
/* a steady sine at L dB (the mix's 0 dBFS: 32768) through comp_block until it settles: the gain it ends at, dB */
static double comp_gain(double L)
{
    uint32_t b, i;
    double ein = 0, eout = 0, ph = 0, a = 32768.0 * pow(10.0, L / 20.0);
    comp_env = 0; comp_g = 32768; memset(comp_sz, 0, sizeof comp_sz);
    for (b = 0; b < 400u; b++) {                         /* (~0.6 s: settled) */
        for (i = 0; i < CTL; i++) {
            int32_t x = (int32_t)(a * sin(ph));
            ph += 2.0 * M_PI * 220.0 / 44100.0;
            mix_l[i] = mix_r[i] = x;
            wet[i] = 0;
            if (b >= 300u) ein += (double)x * x;
        }
        comp_block(CTL);
        if (b >= 300u) for (i = 0; i < CTL; i++) eout += (double)mix_l[i] * mix_l[i];
    }
    return 10.0 * log10(eout / ein);
}
static void comp_set(int in, int thr, int rat, int mk, int knee, int mix)
{
    mc.in = (int8_t)in; mc.thr = (int8_t)thr; mc.rat = (uint8_t)rat; mc.mk = (int8_t)mk; mc.knee = (uint8_t)knee;
    mc.sc = 0; mc.mix = (uint8_t)mix; mc_atk = 0; mc_rel = 20; mc_amt = 100;
}
static void t_comp(void)
{
    static const double RAT[8] = {1.5, 2, 3, 4, 6, 8, 12, 20};
    int r, thr, over;
    double worst = 0;
    for (r = 0; r < 8; r++)
        for (thr = -36; thr <= -6; thr += 10)
            for (over = 3; over <= 15; over += 6) {
                double want, got;
                comp_set(0, thr, r, 0, 0, 100);
                want = -(double)over * (1.0 - 1.0 / RAT[r]);
                got = comp_gain(thr + over);
                if (fabs(got - want) > worst) worst = fabs(got - want);
                CHECK(fabs(got - want) <= 1.0, "COMP THRESH %d dB RATIO %g:1, %d dB over: %.2f dB, %.2f wanted", thr, RAT[r],
                      over, got, want);
            }
    SAY("  COMP: THRESH -36 .. -6, RATIO 1.5 .. 20:1, 3 .. 15 dB over: within %.2f dB of (over x (1 - 1/R))\n", worst);
    {
        double g;
        comp_set(0, -20, 3, 0, 0, 100);
        g = comp_gain(-26);
        CHECK(fabs(g) <= 0.3, "under THRESH: untouched (%.2f dB)", g);
        comp_set(0, -20, 3, 6, 0, 100);
        g = comp_gain(-26);
        CHECK(fabs(g - 6.0) <= 0.3, "MAKEUP 6 dB: +%.2f dB", g);
        comp_set(6, -10, 3, 0, 0, 100);
        g = comp_gain(-30);                              /* (INPUT +6: -24 in, under -10: just the 6 dB) */
        CHECK(fabs(g - 6.0) <= 0.3, "INPUT +6 dB under the threshold: +%.2f dB", g);
        comp_set(-6, -20, 3, 0, 0, 100);
        g = comp_gain(-14);                              /* (-6: -20 in, at the threshold: -6) */
        CHECK(fabs(g + 6.0) <= 0.5, "INPUT -6 dB onto the threshold: %.2f dB", g);
        comp_set(0, -20, 3, 0, 12, 100);                 /* (KNEE 12: at the threshold (1 - 1/R) x 12 / 8 = 1.1 dB) */
        g = comp_gain(-20);
        CHECK(fabs(g + 0.75 * 12.0 / 8.0 * 1.0) <= 0.6, "KNEE 12 dB at the threshold: %.2f dB, -1.1 wanted", g);
        comp_set(0, -20, 3, 0, 12, 100);                 /* (past the knee: as hard) */
        g = comp_gain(-8);
        CHECK(fabs(g + 9.0) <= 1.0, "KNEE 12, 12 dB over (past its half): %.2f dB, -9 wanted", g);
        comp_set(0, -30, 7, 0, 0, 50);                   /* (MIX 50 %: half dry, half at -28.5 dB: -5.7 dB) */
        g = comp_gain(0);
        {
            double c = pow(10.0, -30.0 * (1.0 - 1.0 / 20.0) / 20.0), want = to_db(0.5 + 0.5 * c);
            CHECK(fabs(g - want) <= 0.6, "MIX 50 %%, 30 dB over at 20:1: %.2f dB, %.2f wanted", g, want);
        }
    }
    mc_amt = 0;
}

/* ---- the LIMIT ---- */
/* a sine far over the ceiling through master_out: the output's peak, dBFS of the 16-bit output */
static double lim_peak(double amp)
{
    uint32_t i;
    int32_t pk = 0;
    double ph = 0;
    lev_cur = 32768; lev_dg = 0; lim_env = LIM_T0; fx_lowcut = 0;
    for (i = 0; i < 44100u; i++) {
        int32_t l = (int32_t)(amp * sin(ph)), r = l;
        ph += 2.0 * M_PI * 110.0 / 44100.0;
        master_out(&l, &r);
        if (i > 22050u && abs(l) > pk) pk = abs(l);
    }
    return to_db(pk / 32767.0);
}
static void t_limit(void)
{
    int k;
    double top = 0;
    for (k = 0; k <= 12; k++) {
        double p;
        favorites.factory[15][7] = (uint8_t)k;
        ui_polls();
        p = lim_peak(200000.0);
        if (!k) top = p;
        SAY("  LIMIT CEILING %3d dBFS: the output peaks at %.2f dBFS\n", -6 - k, p);
        CHECK(fabs((p - top) + k) <= 0.25, "CEILING %d dB: %.2f dB under CEILING 0", -k, top - p);
        CHECK(fabs(p - (-6.0 - k)) <= 0.25, "CEILING %d dBFS: the output's peak %.2f dBFS", -6 - k, p);
        {
            int16_t *vp;
            char b[24];
            const param_desc_t *d = cos_cdesc(CC_LIM, &vp);
            shown(d, *vp, b);
            CHECK(*vp == -6 - k, "the dial shows %s for the byte %d", b, k);
        }
    }
    favorites.factory[15][7] = 0;
    {                                                    /* DRIVE: into the limiter, dB */
        int dr;
        for (dr = 0; dr <= 12; dr += 6) {
            double g;
            uint32_t i;
            favorites.factory[6][15] = (uint8_t)dr;
            ui_polls();
            for (i = 0; i < CTL; i++) { mix_l[i] = mix_r[i] = 10000; wet[i] = 0; }
            lim_drive(CTL);
            g = to_db(mix_l[0] / 10000.0);
            CHECK(fabs(g - dr) <= 0.2, "DRIVE %d dB: %+.2f dB", dr, g);
        }
        favorites.factory[6][15] = 0;
    }
    {                                                    /* RELEASE: its time constant, as named */
        static const double MS[6] = {5, 10, 25, 45, 90, 180};
        int r;
        for (r = 0; r < 6; r++) {
            double tau;
            favorites.factory[6][16] = (uint8_t)(r + 1);
            ui_polls();
            tau = (double)(1u << lim_rs) / 44.1;
            CHECK(fabs(tau / MS[r] - 1.0) < 0.2, "RELEASE %gMS: the release's time constant %.1f ms", MS[r], tau);
        }
        favorites.factory[6][16] = 0;
        ui_polls();
    }
}

/* ---- the ISOLATOR ---- */
/* a sine at f through eq_block with the bands set: its gain, dB */
static double eq_gain_at(double f)
{
    uint32_t b, i;
    double ein = 0, eout = 0, ph = 0;
    memset(&eqs, 0, sizeof eqs);
    for (b = 0; b < 300u; b++) {
        for (i = 0; i < CTL; i++) {
            int32_t x = (int32_t)(30000.0 * sin(ph));
            ph += 2.0 * M_PI * f / 44100.0;
            mix_l[i] = mix_r[i] = x; wet[i] = 0;
            if (b >= 200u) ein += (double)x * x;
        }
        eq_block(CTL);
        if (b >= 200u) for (i = 0; i < CTL; i++) eout += (double)mix_l[i] * mix_l[i];
    }
    return 10.0 * log10(eout / ein);
}
static double eq_last;
static void t_iso(void)
{
    static const double F[3] = {50, 1000, 12000};       /* (each band's own: under ~200 Hz, the middle, over ~4 kHz) */
    static const char *const NM[3] = {"LOW", "MID", "HIGH"};
    int b, v;
    for (b = 0; b < 3; b++)
        for (v = -12; v <= 6; v++) {                      /* (every step: what the dial says, measured) */
            double g;
            int16_t *vp;
            char sh[24];
            const param_desc_t *d;
            memset(eq_v, 0, sizeof eq_v);
            cos_cput(CC_EQL + (uint32_t)b, v);
            g = eq_gain_at(F[b]);
            d = cos_cdesc(CC_EQL + (uint32_t)b, &vp);
            shown(d, *vp, sh);
            SAY("  ISOLATOR %-4s shows %-7s: %+.2f dB at %g Hz\n", NM[b], sh, g, F[b]);
            CHECK(fabs(g - atof(sh)) <= 0.1, "ISOLATOR %s shows %s: %+.2f dB at %g Hz", NM[b], sh, g, F[b]);
            CHECK(v == -12 || g > eq_last + 0.3, "ISOLATOR %s: a step right is more (%.2f after %.2f)", NM[b], g, eq_last);
            eq_last = g;
        }
    memset(eq_v, 0, sizeof eq_v);
}

int main(void)
{
    uint32_t i;
    verbose = getenv("ACC_LIST") != 0;
    ui_power_on();
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1;
    ui_polls();
    t_bpm();
    t_delay();
    t_comp();
    t_limit();
    t_iso();
    printf("cos_acc_test: %d checks, %s (%d failures)\n", checks, fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
