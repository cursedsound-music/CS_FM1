/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound
 * (cursedsound) cursed-os 0.16.0, GG 2026-11-13: "i still notice noise ... crackle on kicks ... mostly with kicks and
 * bass". The KICK track's hit took the kit's lane 0 LAYER, FEEDBK and AHDSR (dx_lay[0], dx_tone, dx_de[0]): since 0.14.1
 * the PERC, a RIDE from FMDRUM, its FM settings read as LAYER 10, FEEDBK 12 on the kick's sine (a crackling FM kick from
 * the first PLAY on: its tail's top end -6 dB). Now the kick is its own: whatever the kit's lane 0 holds, the same kick
 * (within 1 %: the voice's state from hit to hit), clean (the tail's top end, past 50 ms, under -40 dB: -50); the kit's own lane 0 still takes them.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/kick_own_test tests/kick_own_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); } else printf("ok: "); printf(__VA_ARGS__); printf("\n"); } while (0)
static int32_t o[2 * CTL];
#define NS (FS / 2u)
static int32_t r[NS];
/* track tk's hit (note), alone, 0.5 s of the left output */
static void hit(uint32_t tk, uint32_t note)
{
    uint32_t b, i;
    for (b = 0; b < 300u; b++) { song.master_q12 = 4096; mix_block(o, CTL); }
    trk_note_on(&trk[tk], note, 110);
    for (b = 0; b < NS / CTL; b++) {
        song.master_q12 = 4096;
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) r[b * CTL + i] = o[2u * i];
    }
    trk_note_off(&trk[tk], note);
}
/* the second difference's energy past 50 ms against the signal's: the top end of the tail */
static double top(void)
{
    double e = 0, h = 0;
    uint32_t i;
    for (i = FS / 20u; i < NS; i++) {
        double d = (double)r[i] - 2.0 * r[i - 1] + r[i - 2];
        e += (double)r[i] * r[i];
        h += d * d;
    }
    return 10.0 * log10(h / (e + 1.0) + 1e-30);
}

int main(void)
{
    uint32_t k, kt = NTRK, dr = NTRK;
    static int32_t a[NS];
    double t0, t1, d = 0, e = 0;
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1;
    for (k = 0; k < NTRK; k++) { if (trk[k].engine == ENGI_KICK && kt == NTRK) kt = k; if (trk[k].engine == ENGI_DRUM && dr == NTRK) dr = k; }
    CHECK(kt < NTRK && dr < NTRK, "a KICK track and a kit");
    for (k = 0; k < NTRK; k++) { trk[k].p[P_MUTE] = (int16_t)(k != kt); trk[k].p[P_REV] = trk[k].p[P_DLY] = trk[k].p[P_CHOR] = 0; }
    dx_lay[0] = 0; dx_tone = 0;
    hit(kt, 36); memcpy(a, r, sizeof a); t0 = top();
    dx_lay[0] = 202; dx_tone = 0xF0;                     /* (a RIDE's: what the PERC put there) */
    memset(dx_de[0], 0, DE_N); dx_de[0][DE_SUS] = 90;
    hit(kt, 36); t1 = top();
    for (k = 0; k < NS; k++) { d += ((double)a[k] - r[k]) * ((double)a[k] - r[k]); e += (double)a[k] * a[k]; }
    CHECK(e > 1e12 && d < e * 1e-2, "the kit's lane 0 LAYER / FEEDBK / AHDSR: the KICK track's kick the same (%.3g of %.3g)", d, e);
    CHECK(t0 < -40.0 && t1 < -40.0, "the kick's tail clean (%.1f dB, %.1f dB)", t0, t1);
    /* the kit's own lane 0 (no KICK track: its kick) still takes its LAYER */
    set_engine_of(&trk[kt], ENGI_ANALOG); trk[kt].engine = ENGI_ANALOG;
    trk[kt].p[P_MUTE] = 1; trk[dr].p[P_MUTE] = 0;
    for (k = 0; k < 4u; k++) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); ui_draw(); }
    memset(dx_de[0], 0, DE_N);
    dx_lay[0] = 0; dx_tone = 0; hit(dr, DRUM_LANE_NOTE[0]); memcpy(a, r, sizeof a);
    dx_lay[0] = 202; dx_tone = 0xF0; hit(dr, DRUM_LANE_NOTE[0]);
    for (k = 0, d = e = 0; k < NS; k++) { d += ((double)a[k] - r[k]) * ((double)a[k] - r[k]); e += (double)a[k] * a[k]; }
    CHECK(e > 1e10 && d > e * 1e-3, "the kit's lane 0: its LAYER heard (%.3g of %.3g)", d, e);
    printf(fails ? "kick_own: %d FAILED\n" : "kick_own: all ok\n", fails);
    return fails != 0;
}
