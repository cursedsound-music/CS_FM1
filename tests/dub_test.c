/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound
 * (cursedsound) cursed-os 0.15.0: DUB (engine 6, eng_dub.c), GG's dub chords and atmospheres: reachable, every preset
 * sounding and bounded; a STAB falls, an ATMOS holds and moves; ATMOS in the key (any key struck: the chord on ROOT);
 * COLOR brighter, DENSITY denser; THROW lifts the delay send; its presets keep their sends (others load dry); a project
 * of before 0.15 (a 6: TRIO) ANALOG, of now DUB.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/dub_test tests/dub_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); } else printf("ok: "); printf(__VA_ARGS__); printf("\n"); } while (0)
#define TK 3u
static int32_t o[2 * CTL];
static void settle(void) { uint32_t b; for (b = 0; b < FS / CTL; b++) mix_block(o, CTL); }
/* note on track TK, held for hold_s seconds: the energy of each 100 ms window (w[0..nw)), the peak; the high part's
 * energy (the first difference) in *hf */
static double win[40];
static int32_t peak;
static double play(uint32_t note, double hold_s, uint32_t nw, double *hf)
{
    uint32_t b, i, nb = (uint32_t)(nw * FS / 10u / CTL), off = (uint32_t)(hold_s * FS / CTL);
    double e = 0, h = 0;
    int32_t prev = 0;
    memset(win, 0, sizeof win);
    peak = 0;
    trk_note_on(&trk[TK], note, 100);
    for (b = 0; b < nb; b++) {
        if (b == off) trk_note_off(&trk[TK], note);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t s = o[2u * i], a = s < 0 ? -s : s;
            uint32_t w = (b * CTL + i) / (FS / 10u);
            if (w < 40u) win[w] += (double)s * s;
            e += (double)s * s;
            h += (double)(s - prev) * (s - prev);
            prev = s;
            if (a > peak) peak = a;
        }
    }
    trk_note_off(&trk[TK], note);
    settle(); settle(); settle();
    if (hf) *hf = h / (e + 1);
    return e;
}

int main(void)
{
    uint32_t k, i;
    double e, hf0, hf1, e0, e1;
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1; song.playing = 0; song.master_q12 = 4096;
    for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = (int16_t)(k != TK);
    CHECK(eng_ok(ENGI_DUB) && ENGINES[ENGI_DUB] == &ENG_DUB && !eng_gone(ENGI_DUB), "engine 6 is DUB, pickable");
    {
        uint32_t seen = 0;
        for (i = 0; i < NENG_SHOWN; i++) seen += ENGINE_ORDER[i] == ENGI_DUB;
        CHECK(seen == 1u && NENG_SHOWN == 15u, "DUB in ENGINE_ORDER once, 15 engines shown (%u)", seen);
    }
    set_engine_of(&trk[TK], ENGI_DUB); trk[TK].engine = ENGI_DUB;
    settle();
    for (k = 0; k < ENG_DUB.npresets; k++) {
        apply_preset_to(&trk[TK], k);
        trk[TK].p[P_DLY] = trk[TK].p[P_REV] = trk[TK].p[P_CHOR] = 0;
        settle();
        e = play(48, 1.0, 20, 0);
        printf("  %-12s energy %.3g peak %d\n", ENG_DUB.presets[k].name, e, peak);
        CHECK(e > 1e9 && peak < 32768 * 2, "%s sounds, bounded", ENG_DUB.presets[k].name);
    }
    /* STAB falls; ATMOS holds and moves */
    apply_preset_to(&trk[TK], 0); trk[TK].p[P_E6] = 0; settle();
    play(48, 2.0, 20, 0);
    CHECK(win[1] > 4.0 * win[8], "STAB: falls (%.3g -> %.3g)", win[1], win[8]);
    apply_preset_to(&trk[TK], 5); trk[TK].p[P_DLY] = trk[TK].p[P_REV] = 0; settle();
    play(48, 4.0, 40, 0);
    {
        double lo = 1e30, hi = 0;
        for (i = 15; i < 38u; i++) { if (win[i] < lo) lo = win[i]; if (win[i] > hi) hi = win[i]; }
        CHECK(lo > win[30] / 20.0 && hi > lo * 1.15, "ATMOS: holds, and moves (%.3g .. %.3g)", lo, hi);
    }
    /* ATMOS in the key: ROOT D, keys D4 and F4 play the same chord */
    trk[TK].p[P_ROOT] = 2; trk[TK].p[P_ATK] = 0; trk[TK].p[P_E7] = 0;
    {
        static int32_t a[4096], b2[4096];
        uint32_t n, j, bl;
        for (j = 0; j < 2u; j++) {
            uint32_t note = j ? 65u : 62u;
            voice_t *v;
            settle();
            trk_note_on(&trk[TK], note, 100);
            for (v = trk[TK].v, n = 0; n < NVOICE && !(v->active && v->note == note); v++, n++) {}
            if (n < NVOICE) { v->ph[0] = v->ph[1] = v->ph[2] = 0; v->s[0] = 0; v->s[1] = v->s[2] = 0; v->s[5] = v->s[6] = 0; }
            for (bl = 0; bl < 4096u / CTL; bl++) {
                mix_block(o, CTL);
                for (i = 0; i < CTL; i++) (j ? b2 : a)[bl * CTL + i] = o[2u * i];
            }
            trk_note_off(&trk[TK], note);
            settle(); settle(); settle();
        }
        e0 = e1 = 0;
        for (i = 1024; i < 4096u; i++) { e0 += (double)a[i] * a[i]; e1 += (double)(a[i] - b2[i]) * (a[i] - b2[i]); }
        CHECK(e0 > 1e8 && e1 < e0 / 50.0, "ATMOS, ROOT D: D4 and F4 the same chord (%.3g of %.3g)", e1, e0);
    }
    trk[TK].p[P_ROOT] = 0;
    /* COLOR brighter, DENSITY denser */
    apply_preset_to(&trk[TK], 5); trk[TK].p[P_DLY] = trk[TK].p[P_REV] = 0; trk[TK].p[P_E7] = 0; trk[TK].p[P_ATK] = 0;
    trk[TK].p[P_E2] = 20; settle(); play(48, 1.0, 10, &hf0);
    trk[TK].p[P_E2] = 110; settle(); play(48, 1.0, 10, &hf1);
    CHECK(hf1 > hf0 * 2.0, "COLOR 20 -> 110: brighter (%.3g -> %.3g)", hf0, hf1);
    trk[TK].p[P_E2] = 60;
    trk[TK].p[P_E3] = 0; settle(); memset(trk[TK].v, 0, sizeof trk[TK].v); e0 = play(48, 1.0, 10, 0);   /* (0.16.1: the voices
                                                          * from the top: the tones' phases a BLEEP's left them) */
    trk[TK].p[P_E3] = 127; settle(); memset(trk[TK].v, 0, sizeof trk[TK].v); e1 = play(48, 1.0, 10, 0);
    CHECK(e1 > e0 * 1.5, "DENSITY 0 -> 127: denser (%.3g -> %.3g)", e0, e1);
    /* THROW */
    apply_preset_to(&trk[TK], 0); settle();
    trk[TK].p[P_E6] = 127; trk_throw[TK] = 0;
    trk_note_on(&trk[TK], 48, 100); mix_block(o, CTL);
    k = trk_throw[TK];
    trk_note_off(&trk[TK], 48); settle();
    CHECK(k >= 120u && !trk_throw[TK], "THROW 127: the stab thrown (%u), falling to 0", k);
    trk[TK].p[P_E6] = 0;
    trk_note_on(&trk[TK], 48, 100); mix_block(o, CTL);
    CHECK(!trk_throw[TK], "THROW 0: none");
    trk_note_off(&trk[TK], 48); settle();
    /* sends: DUB's presets keep theirs; another engine's loads dry */
    apply_preset_to(&trk[TK], 5);
    CHECK(trk[TK].p[P_REV] > 50, "ATMOS DARK: its REVERB send (%d)", trk[TK].p[P_REV]);
    set_engine_of(&trk[TK], ENGI_ANALOG); trk[TK].p[P_REV] = 0;
    apply_preset_to(&trk[TK], 3);
    CHECK(trk[TK].p[P_REV] == 0, "an ANALOG preset: dry (%d)", trk[TK].p[P_REV]);
    /* projects */
    {
        static project_t q;
        memset(&q, 0, sizeof q);
        q.phys = PROJ_FMD;
        q.t[0].engine = ENGI_TRIO;
        proj_phys_gone(&q);
        CHECK(q.t[0].engine == ENGI_PHYS_TO && q.phys == PROJ_DUB, "a 6 of before 0.15: TRIO's, ANALOG (%u %u)", q.t[0].engine, q.phys);
        q.t[0].engine = ENGI_DUB;
        proj_phys_gone(&q);
        CHECK(q.t[0].engine == ENGI_DUB, "a 6 now: DUB");
    }
    /* stars: DUB's in DIGITAL's row, KICK's and FMDRUM's apart */
    memset(favorites.factory[1], 0, 32);
    memset(favorites.factory[6], 0, 8);
    CHECK(favorite_set(ENGI_DUB, 2, 1) && favorite_has(ENGI_DUB, 2) && !favorite_has(ENGI_KICK, 2) && favorites.factory[1][0] == 4u,
          "a DUB star in row 1, KICK's row untouched");
    printf(fails ? "dub: %d FAILED\n" : "dub: all ok\n", fails);
    return fails != 0;
}
