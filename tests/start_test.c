/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) cursed-os 0.18.0, GG 2026-11-14: "dither, leveler off ... only syntakt, cleaned soft clip (not always on,
 * i want to set it when I need it), undo for gen, quick starts, drum sample rate conversion".
 *   SAVE > START: each STYLE loads (OCT+) sounds and lines on the tracks it uses, the rest muted; DRUMS FULL / PERC
 *   / NONE (PERC by default with SYNTAKT ON); again: another; REC on the page (UNDO) the music before it.
 *   GEN's REC: the selected track's steps back, the transport not stopped; again: forward.
 *   SATURATE OFF: the master clean (a sine at -12 dBFS: no 3rd harmonic), the ceiling the peak; on: the 3rd up, the
 *   peak still the ceiling. The dither: silence stays 0; MASTER below full, a quiet tail's error noise (no tone);
 *   from the same PLAY, the same output.
 *   The drums' rate conversion: a hat's top mirrored near 22 kHz down (a cubic, not the mean).
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/start_test tests/start_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); } else printf("ok: "); printf(__VA_ARGS__); printf("\n"); } while (0)
static void frame16(void) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); ui_draw(); }
static int32_t o[2 * CTL];
/* the same line: each step's kind, and a note step's notes, velocity and flags (packing clears a rest's leftovers) */
static int same_line(const step_t *a, const step_t *b)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) {
        if (a[i].time != b[i].time) return 0;
        if (a[i].time == ST_NOTE && (a[i].n != b[i].n || a[i].note[0] != b[i].note[0] || a[i].vel != b[i].vel || a[i].flags != b[i].flags)) return 0;
    }
    return 1;
}
static uint32_t notes(uint32_t k)
{
    uint32_t i, c = 0;
    for (i = 0; i < NSTEP; i++) c += trk[k].step[i].time == ST_NOTE || trk[k].step[i].hit;
    return c;
}
/* the energy of r's DFT bin near f (Hz), a Hann window */
static double bin(const double *r, uint32_t n, double f)
{
    double re = 0, im = 0;
    uint32_t i;
    for (i = 0; i < n; i++) {
        double w = 0.5 - 0.5 * cos(2 * M_PI * i / n), a = 2 * M_PI * f * i / FS;
        re += r[i] * w * cos(a); im += r[i] * w * sin(a);
    }
    return re * re + im * im;
}

int main(void)
{
    uint32_t i, k, s;
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1; song.master_q12 = 4096; frame16();
    /* SAVE > START */
    {
        const cpg_t *p; uint32_t n;
        cos_pages(CM_PROJ, &p, &n);
        CHECK(n == 4u && str_eq(p[3].tab, "START") && cos_ess(CM_PROJ, p, 3, n), "SAVE > START: a page, an everyday one");
    }
    cz.mode = CM_PROJ; cz.tab[CM_PROJ] = 3; frame16();
    for (s = 0; s < QS_N; s++) {
        uint32_t used = 0;
        cos_cput(CC_QSTY, (int32_t)s); cos_cput(CC_QDRM, 0);
        cos_proj_oct(1);
        for (k = 0; k < 4u; k++) frame16();
        for (k = 0; k < NTRK; k++) used += !trk[k].p[P_MUTE];
        CHECK(!trk[0].p[P_MUTE] && !trk[1].p[P_MUTE] && notes(0) && notes(1) && trk[5].eng_req == ENGI_DUB && notes(5) && used >= 4u,
              "%s: the kit, the kick, a BLEEP on T6 and more (%u tracks)", QS_NAME[s], used);
        if (s == QS_ACID) CHECK(trk[2].eng_req == ENGI_ACID && notes(2) && trk[4].p[P_MUTE], "ACID: T3 an acid line, T5 muted");
        if (s == QS_DUB) CHECK(!trk[3].p[P_MUTE] && notes(3) && !trk[4].p[P_MUTE] && trk[2].p[P_MUTE], "DUB TECHNO: T4 stabs, T5 the atmosphere, T3 muted");
        if (s == QS_BLEEPS) CHECK(trk[3].eng_req == ENGI_DUB && trk[3].p[P_SLEN] == 3 && trk[5].p[P_SLEN] == 5, "BLEEPS: a 3-step and a 5-step motif across each other");
    }
    /* again: another */
    {
        static step_t keep[NSTEP];
        cos_cput(CC_QSTY, QS_ACID);
        cos_proj_oct(1); frame16();
        memcpy(keep, trk[2].step, sizeof keep);
        fm1_ms += 5000u; cos_proj_oct(1); frame16();
        CHECK(memcmp(keep, trk[2].step, sizeof keep), "OCT+ again: another acid line");
    }
    /* REC on START: the music before it */
    {
        int16_t m2 = trk[2].p[P_MUTE];
        cos_cput(CC_QSTY, QS_DUB); cos_proj_oct(1); frame16();
        CHECK(trk[2].p[P_MUTE], "DUB TECHNO: T3 muted"); cos_qs_undo();
        frame16();
        CHECK(trk[2].p[P_MUTE] == m2 && (trk[2].eng_req == ENGI_ACID || trk[2].engine == ENGI_ACID), "the ACID start back (T3 as it was: %d %d, %u %u)", trk[2].p[P_MUTE], m2, trk[2].eng_req, trk[2].engine);
    }
    /* DRUMS */
    cos_cput(CC_QDRM, 1); cos_proj_oct(1); frame16();
    CHECK(trk[1].p[P_MUTE] && !trk[0].p[P_MUTE], "DRUMS PERC: the KICK track muted, the kit on");
    cos_cput(CC_QDRM, 2); cos_proj_oct(1); frame16();
    CHECK(trk[1].p[P_MUTE] && trk[0].p[P_MUTE], "DRUMS NONE: both muted");
    qs_drm = 0;
    cos_syntakt(CC_STON, 1);
    CHECK(qs_drm_get() == 1u, "SYNTAKT ON: PERC by default");
    CHECK(favorites.factory[6][17] == 1u, "SYNTAKT ON: AUTO LV off");
    cos_syntakt(CC_STON, 0);
    CHECK(qs_drm_get() == 0u && favorites.factory[6][17] == 0u, "OFF: FULL, AUTO LV on");
    song.g[G_CLOCK] = 0;
    /* GEN's UNDO: the track's steps, the transport on */
    {
        static step_t before[NSTEP];
        track_select(2);
        transport_req = 1; frame16(); for (k = 0; k < 50u; k++) mix_block(o, CTL);
        memcpy(before, trk[2].step, sizeof before);
        cz.mode = CM_GEN; frame16();
        fm1_ms += 5000u;
        ac_oct(1, 0); frame16();
        CHECK(!same_line(before, trk[2].step), "GEN: a new acid line");
        CHECK(!undo_gen(), "REC: undone");
        frame16();
        CHECK(same_line(before, trk[2].step) && song.playing, "the line before, the transport still playing");
        CHECK(!undo_gen() && !same_line(before, trk[2].step), "REC again: forward");
        transport_req = 2; frame16();
    }
    /* SATURATE and the dither: a sine at -12 dBFS through the master */
    {
        static double r[4096];
        double h1, h3;
        uint32_t n = 4096;
        for (s = 0; s < 2u; s++) {
            favorites.factory[8][23] = (uint8_t)(s ? 13u : 0u); ui_polls();
            for (k = 0; k < 2u; k++) {
                for (i = 0; i < n; i++) {
                    int32_t l = (int32_t)(8231.0 * sin(2 * M_PI * 1000.0 * (k * n + i) / FS)), rr = l;
                    master_out(&l, &rr);
                    r[i] = l;
                }
            }
            h1 = bin(r, n, 1000.0); h3 = bin(r, n, 3000.0);
            printf("  SATURATE %s: 3rd %.1f dB\n", s ? "+12" : "OFF", 10 * log10(h3 / h1 + 1e-30));
            if (!s) CHECK(h3 < h1 * 1e-7, "SATURATE OFF: clean (3rd %.1f dB)", 10 * log10(h3 / h1 + 1e-30));
            else CHECK(h3 > h1 * 1e-3, "SATURATE +12 dB: the 3rd up (%.1f dB)", 10 * log10(h3 / h1 + 1e-30));
        }
        favorites.factory[8][23] = 0; ui_polls();
        /* silence: 0; twice the same */
        {
            int32_t a[256], b[256], l, rr;
            song.master_q12 = 1500;
            for (i = 0; i < 60000u; i++) { l = rr = 0; master_out(&l, &rr); }
            for (i = 0; i < 256u; i++) { l = rr = 0; master_out(&l, &rr); a[i] = l; }
            k = 0; for (i = 0; i < 256u; i++) k |= (uint32_t)a[i];
            CHECK(!k, "silence through the master: 0");
            dth_s = 1; for (i = 0; i < 256u; i++) a[i] = mix_mul_d(1000 + (int32_t)i * 37, 1500 << 3, dth());
            dth_s = 1; for (i = 0; i < 256u; i++) b[i] = mix_mul_d(1000 + (int32_t)i * 37, 1500 << 3, dth());
            CHECK(!memcmp(a, b, sizeof a), "from the same start (PLAY): the same output");
            song.master_q12 = 4096;
        }
        /* a fading sine through MASTER 1/8: the error noise, its mean ~0 */
        {
            double e = 0, m = 0;
            for (i = 0; i < 20000u; i++) {
                int32_t x = (int32_t)(40.0 * sin(2 * M_PI * 220.0 * i / FS));
                int32_t y = mix_mul_d(x, 4096, dth());
                double ex = x / 8.0;
                e += (y - ex) * (y - ex); m += y - ex;
            }
            CHECK(fabs(m / 20000.0) < 0.05 && sqrt(e / 20000.0) < 0.75, "MASTER 1/8: the error unbiased (%.3f), about a LSB's TPDF (%.3f)", m / 20000.0, sqrt(e / 20000.0));
        }
    }
    /* the drums' rate conversion: a hat's mirror */
    {
        CHECK(DX_CUBIC(DV_HATC) && DX_CUBIC(DV_HATO) && DX_CUBIC(DV_BELL) && !DX_CUBIC(DV_CLAP), "the hats, the bell: the cubic; the clap: as before");
    }
    printf(fails ? "start: %d FAILED\n" : "start: all ok\n", fails);
    return fails != 0;
}
