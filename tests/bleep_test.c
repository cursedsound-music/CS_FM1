/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) cursed-os 0.15.1, GG: hypnotic techno's bleeps and bloops, "random but consistent": GEN > BLEEP
 * (ui_bleep.c) and DUB's BLEEP type (eng_dub.c).
 *   the same SEED: the same line; another: another; every note in the key; the beat lighter than the offbeats; VARY 0:
 *   the bars the same, VARY: some steps their own; OCT+ a new seed, OCT- back; SOUND loads a BLEEP sound.
 *   BLEEP: every preset pings and falls, bounded; RANDOM: a note on a step the same each time, another step another
 *   ping; BLOOP: the pitch glides into the note.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/bleep_test tests/bleep_test.c -lm */
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
static void frame16(void) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); ui_draw(); }
static int32_t o[2 * CTL];
static step_t bkeep[NSTEP];
static void settle(void) { uint32_t b; for (b = 0; b < FS / CTL; b++) mix_block(o, CTL); }
/* a ping of note on step st (the voice's dice), n samples into r */
static void ping(uint32_t note, uint32_t st, int32_t *r, uint32_t n)
{
    uint32_t b, i;
    settle(); settle();
    trk[TK].seq_idx = (uint16_t)st;
    trk_note_on(&trk[TK], note, 100);
    for (b = 0; b < n / CTL; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) r[b * CTL + i] = o[2u * i];
    }
    trk_note_off(&trk[TK], note);
}

int main(void)
{
    uint32_t i, k, ok, beat = 0, off = 0;
    bl_t *a;
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1; song.playing = 0; song.master_q12 = 4096;
    for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = (int16_t)(k != TK);
    track_select(TK);
    set_engine_of(&trk[TK], ENGI_ANALOG);
    trk[TK].p[P_ROOT] = 9; trk[TK].p[P_SCALE] = 2;    /* (A minor) */
    a = bl_of(TK);
    /* the same seed, the same line */
    bl_put(BK_SEED, 321);
    memcpy(bkeep, trk[TK].step, sizeof bkeep);
    bl_put(BK_SEED, 322);
    k = (uint32_t)memcmp(bkeep, trk[TK].step, sizeof bkeep);
    bl_put(BK_SEED, 321);
    for (i = 0; i < NSTEP && !memcmp(&bkeep[i], &trk[TK].step[i], sizeof(step_t)); i++) {}
    if (i < NSTEP) printf("  step %u: %u %u %u / %u %u %u\n", i, bkeep[i].note[0], bkeep[i].vel, bkeep[i].flags, trk[TK].step[i].note[0], trk[TK].step[i].vel, trk[TK].step[i].flags);
    CHECK(!memcmp(bkeep, trk[TK].step, sizeof bkeep) && k, "SEED 321 again: the same line; 322 another");
    /* in the key, the offbeats first */
    ok = 1;
    for (k = 1; k < 60u; k++) {
        bl_put(BK_SEED, (int32_t)(k * 13u));
        for (i = 0; i < (uint32_t)trk[TK].p[P_SLEN]; i++) {
            const step_t *s = &trk[TK].step[i];
            if (s->time != ST_NOTE || !s->n) continue;
            ok &= (SCALE_MASK[2] >> ((s->note[0] + 12u - 9u) % 12u)) & 1u;
            if (i % 4u == 0u) beat++; else off++;
        }
    }
    CHECK(ok, "every ping in A minor");
    CHECK(off > beat * 4u, "the offbeats far likelier than the beat (%u / %u)", off, beat);
    /* VARY */
    bl_put(BK_LEN, 2); bl_put(BK_VARY, 0); bl_put(BK_SEED, 77);
    ok = trk[TK].p[P_SLEN] == 64;
    for (i = 16; i < 64u; i++) ok &= !memcmp(&trk[TK].step[i], &trk[TK].step[i % 16u], sizeof(step_t));
    CHECK(ok, "4 BARS, VARY 0: each bar the first");
    bl_put(BK_VARY, 60);
    for (i = 16, k = 0, ok = 0; i < 64u; i++) {
        int same = !memcmp(&trk[TK].step[i], &trk[TK].step[i % 16u], sizeof(step_t));
        k += !same; ok += same;
    }
    CHECK(k > 4u && ok > 10u, "VARY 60: some steps their own (%u), most kept (%u)", k, ok);
    /* OCT+ / OCT- */
    {
        uint32_t s0 = a->seed;
        cz.mode = CM_GEN; frame16();
        bl_oct(1, 0);
        CHECK(a->seed != s0, "OCT+: a new seed (%u -> %u)", s0, a->seed);
        bl_oct(0, 0);
        CHECK(a->seed == s0, "OCT-: back (%u)", a->seed);
    }
    /* SOUND */
    bl_put(BK_SND, 1);
    CHECK(trk[TK].eng_req == ENGI_DUB && trk[TK].p[P_E0] == DUB_BLEEP, "SOUND 1: DUB's first BLEEP sound (%u %d)", trk[TK].eng_req, trk[TK].p[P_E0]);
    trk[TK].engine = ENGI_DUB;
    settle();
    /* the BLEEP sounds */
    for (k = 0; k < ENG_DUB.npresets; k++) {
        static int32_t r[FS];
        double e1 = 0, e2 = 0;
        int32_t pk = 0;
        if (ENG_DUB.presets[k].e[0] != DUB_BLEEP) continue;
        apply_preset_to(&trk[TK], k);
        trk[TK].p[P_DLY] = trk[TK].p[P_REV] = 0;
        ping(69, 0, r, FS / 2u);
        for (i = 0; i < FS / 2u; i++) {
            int32_t v = abs(r[i]);
            if (i < FS / 20u) e1 += (double)r[i] * r[i]; else if (i > FS * 4u / 10u) e2 += (double)r[i] * r[i];
            if (v > pk) pk = v;
        }
        printf("  %-13s first 50 ms %.3g, last 100 ms %.3g, peak %d\n", ENG_DUB.presets[k].name, e1, e2, pk);
        CHECK(e1 > 1e8 && e2 < e1 / 4.0 && pk < 65536, "%s: pings and falls, bounded", ENG_DUB.presets[k].name);
    }
    /* RANDOM: the same note on the same step the same ping; another step another */
    {
        static int32_t r1[8192], r2[8192], r3[8192];
        double d12 = 0, d13 = 0, e = 0;
        for (k = 0; k < ENG_DUB.npresets && strcmp(ENG_DUB.presets[k].name, "RANDOM PINGS"); k++) {}
        apply_preset_to(&trk[TK], k);
        trk[TK].p[P_DLY] = trk[TK].p[P_REV] = 0;
        ping(69, 5, r1, 8192); ping(69, 5, r2, 8192); ping(69, 6, r3, 8192);
        for (i = 0; i < 8192u; i++) {
            e += (double)r1[i] * r1[i];
            d12 += (double)(r1[i] - r2[i]) * (r1[i] - r2[i]);
            d13 += (double)(r1[i] - r3[i]) * (r1[i] - r3[i]);
        }
        CHECK(d12 < e / 1000.0 && d13 > e / 20.0, "RANDOM: step 6 twice the same ping (%.3g), step 7 another (%.3g of %.3g)", d12, d13, e);
    }
    /* BLOOP: from above, the pitch falls into the note (the zero crossings: faster at first) */
    {
        static int32_t r[16384];
        uint32_t z0 = 0, z1 = 0;
        for (k = 0; k < ENG_DUB.npresets && strcmp(ENG_DUB.presets[k].name, "BLEEP SINE"); k++) {}
        apply_preset_to(&trk[TK], k);
        trk[TK].p[P_DLY] = trk[TK].p[P_REV] = 0; trk[TK].p[P_E6] = 60; trk[TK].p[P_E7] = 0; trk[TK].p[P_E3] = 0;
        trk[TK].p[P_E4] = 100;
        ping(57, 0, r, 16384);
        for (i = 1; i < 1024u; i++) z0 += 2u * ((r[i - 1] < 0) != (r[i] < 0));   /* (the first 23 ms, x 2) */
        for (i = 12289; i < 14336u; i++) z1 += (r[i - 1] < 0) != (r[i] < 0);
        CHECK(z0 * 2u > z1 * 3u && z1 > 0u, "BLOOP 60: the pitch from above (%u crossings first, %u later)", z0, z1);
    }
    printf(fails ? "bleep: %d FAILED\n" : "bleep: all ok\n", fails);
    return fails != 0;
}
