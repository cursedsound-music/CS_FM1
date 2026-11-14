/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound
 * (cursedsound) cursed-os 0.12.4: a step's LENGTH, steps held + black 3 shorter / black 4 longer: the sixteenths of a
 * step (its own gate), then whole steps (TIEs after it), stopped by a note; heard as shorter and longer.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/step_len_test tests/step_len_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static void frame16(void) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); ui_draw(); }
static uint32_t lenof(uint32_t i) { return cos_slen_get(TSEL, i, (uint32_t)TSEL->p[P_SLEN]); }
/* the track played from step 1 for a bar: blocks its voice is held (attack / decay-sustain, before the release) */
static uint32_t sounding(void)
{
    static int32_t o[2 * CTL];
    uint32_t b, v, nb = FS * 2u / CTL, n = 0;
    transport_req = 1; ui_input();
    for (b = 0; b < nb; b++) {
        uint32_t on = 0;
        mix_block(o, CTL);
        for (v = 0; v < NVOICE; v++) on |= TSEL->v[v].active && (TSEL->v[v].stage == 1u || TSEL->v[v].stage == 2u);
        n += on;
    }
    transport_req = 2; ui_input();
    for (b = 0; b < 400u; b++) mix_block(o, CTL);
    return n;
}

int main(void)
{
    uint32_t i, k, prev, lens[12], d[4];
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1; song.playing = 0; song.master_q12 = 4096; song.g[G_BPM] = 120; frame16();
    track_select(3);                                     /* (T4, the bass: a synth) */
    for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = (int16_t)(k != 3u);
    for (i = 0; i < NSTEP; i++) step_clear(&TSEL->step[i]);
    TSEL->p[P_SLEN] = 16;
    TSEL->p[P_SGATE] = 64;
    TSEL->step[0].time = ST_NOTE; TSEL->step[0].n = 1; TSEL->step[0].note[0] = 36; TSEL->step[0].vel = 100;
    TSEL->step[6].time = ST_NOTE; TSEL->step[6].n = 1; TSEL->step[6].note[0] = 43; TSEL->step[6].vel = 100;
    cz.mode = CM_TRIG; cz.tab[CM_TRIG] = 0; cz.bank = 0;
    cz.tg_down = 1u << WK[0];                            /* (step 1's white key held: cos_input's record of it) */
    CHECK(cos_held() == 1u, "step 1 held (%llx)", (unsigned long long)cos_held());
    CHECK(lenof(0) == 0u, "none set: the track's GATE");
    for (k = 0; k < 12u; k++) { cos_step_len(1); lens[k] = lenof(0); }
    CHECK(lens[0] == 10u, "B4 from GATE 64 (8/16): 10/16 (%u)", lens[0]);
    CHECK(lens[1] == 12u && lens[2] == 15u && lens[3] == 32u, "12/16, 15/16, 2 STEPS (%u %u %u)", lens[1], lens[2], lens[3]);
    CHECK(TSEL->step[1].time == ST_TIE && CT_LEN(&TSEL->step[0]) == 0u, "2 STEPS: step 2 a TIE, its own gate off");
    CHECK(lens[7] == 96u && TSEL->step[5].time == ST_TIE, "6 STEPS: up to the note on step 7 (%u)", lens[7]);
    CHECK(lens[8] == 96u && TSEL->step[6].time == ST_NOTE && TSEL->step[6].note[0] == 43u, "past it: stopped, the note kept (%u)", lens[8]);
    for (k = 0; k < 5u; k++) cos_step_len(0);
    CHECK(lenof(0) == 15u && TSEL->step[1].time == ST_REST, "B3 back: 15/16, the ties gone (%u)", lenof(0));
    for (k = 0; k < 20u; k++) cos_step_len(0);
    CHECK(lenof(0) == 1u, "B3 down to 1/16 (%u)", lenof(0));
    cz.tg_down = 0;
    step_clear(&TSEL->step[6]);
    /* heard: 1/16, 8/16, 15/16, 4 steps */
    {
        static const uint32_t V[4] = {1, 8, 15, 64};
        for (k = 0; k < 4u; k++) {
            cos_slen_set(TSEL, 0, V[k], 16);
            d[k] = sounding();
        }
        printf("  sounding blocks: 1/16 %u, 8/16 %u, 15/16 %u, 4 steps %u\n", d[0], d[1], d[2], d[3]);
        CHECK(d[0] < d[1] && d[1] < d[2] && d[2] < d[3], "longer each (%u %u %u %u)", d[0], d[1], d[2], d[3]);
    }
    {   /* the black keys while the step is held (cos_input) */
        cos_slen_set(TSEL, 0, 8, 16);
        prev = lenof(0);
        fm1_in.notes = host_notes = 1u << WK[0]; cos_input(); host_notes = 0;   /* (white 1 down: held, as cos_test's) */
        host_notes = 1u << BK[3]; fm1_in.notes |= host_notes; cos_input();      /* (black 4, white 1 still down) */
        fm1_in.notes = 0; cos_input();                  /* (let go: no trig toggled) */
        host_notes = 0;
        CHECK(lenof(0) == 10u, "white 1 held + black 4: longer (%u -> %u)", prev, lenof(0));
        CHECK(TSEL->step[0].time == ST_NOTE && TSEL->step[0].n, "let go: the note still there (no trig toggled)");
    }
    printf("step_len_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
