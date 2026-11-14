/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound
 * (cursedsound) cursed-os 0.13.0: LFO > DRUMS: a drum held rolls at the RATE (stopped: from the press; playing: on the
 * grid), the kick through the KICK track; black 1..4 held repeat the drums' last steps; each drum's CHANCE; kept in the
 * project.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/drm_test tests/drm_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static void frame16(void) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); ui_draw(); }
/* the hits of drum lane l (the kit's; 0 with a KICK track: the KICK's) over ms */
static uint32_t hits(uint32_t l, uint32_t ms, uint32_t keys)
{
    static int32_t o[2 * CTL];
    uint32_t b, nb = FS / 1000u * ms / CTL, n = 0, last = 255;
    drum_lane_t *K = l == 0u ? drum_kit_of(&trk[1]) : drum_kit_of(&trk[0]);
    for (b = 0; b < nb; b++) {
        fm1_in.notes = keys;
        if (b % 22u == 0u) frame16();
        mix_block(o, CTL);
        if (K[l == 0u ? DV_KICK : l].age < last) n++;
        last = K[l == 0u ? DV_KICK : l].age;
    }
    fm1_in.notes = 0;
    frame16();
    return n;
}
static void grid(uint32_t l, uint32_t every)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++) step_clear(&trk[0].step[i]);
    for (i = 0; i < 16u; i += every) { trk[0].step[i].time = ST_NOTE; trk[0].step[i].hit = (uint8_t)(1u << l); trk[0].step[i].vel = 100; }
    trk[0].p[P_SLEN] = 16;
}

int main(void)
{
    uint32_t i, n;
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1; song.playing = 0; song.master_q12 = 4096; song.g[G_BPM] = 120; frame16();
    for (i = 0; i < NTRK; i++) trk[i].p[P_MUTE] = (int16_t)(i > 1u);
    for (i = 0; i < NSTEP; i++) { step_clear(&trk[0].step[i]); step_clear(&trk[1].step[i]); }
    trk[0].p[P_LN0 + DV_SNARE] = trk[0].p[P_LN0 + DV_HATC] = 100;
    cz.mode = CM_DRUMS;
    hits(DV_SNARE, 300, 0);
    n = hits(DV_SNARE, 1000, 0);
    CHECK(n <= 1u, "nothing held, stopped: no snare (%u)", n);
    cos_cput(CC_RRATE, 1);                               /* 1/16 at 120: 8 a second */
    n = hits(DV_SNARE, 1000, 1u << WK[1]);
    CHECK(n >= 7u && n <= 9u, "white 2 held 1 s, 1/16: ~8 snare hits (%u)", n);
    cos_cput(CC_RRATE, 2);
    n = hits(DV_SNARE, 1000, 1u << WK[1]);
    CHECK(n >= 15u && n <= 17u, "1/32: ~16 (%u)", n);
    cos_cput(CC_RRATE, 3);
    n = hits(DV_SNARE, 1000, 1u << WK[1]);
    CHECK(n >= 11u && n <= 13u, "1/16T: ~12 (%u)", n);
    cos_cput(CC_RRATE, 1);
    n = hits(DV_KICK, 1000, 1u << WK[0]);
    CHECK(n >= 7u && n <= 9u, "white 1: the KICK track rolls (%u)", n);
    cz.mode = CM_HOME;
    n = hits(DV_SNARE, 1000, 1u << WK[1]);
    CHECK(n <= 1u, "outside DRUMS: white 2 rolls nothing (%u)", n);
    cz.mode = CM_DRUMS;
    {   /* playing: hats on 1, 5, 9, 13; REPEAT 1 step held from step 1: a hat every step */
        uint32_t plain, rep;
        grid(DV_HATC, 4);
        transport_req = 1; frame16();
        plain = hits(DV_HATC, 2000, 0);
        transport_req = 2; frame16();
        transport_req = 1; frame16();
        hits(DV_HATC, 20, 0);                            /* (into step 1: its hat played) */
        rep = hits(DV_HATC, 2000, 1u << BK[3]);
        transport_req = 2; frame16();
        CHECK(plain >= 3u && plain <= 5u, "a bar, hats on the beats: 4 (%u)", plain);
        CHECK(rep >= 14u, "black 4 held from step 1: the step again and again (%u)", rep);
        transport_req = 1; frame16();
        hits(DV_HATC, 20, 0);
        rep = hits(DV_HATC, 2000, 1u << BK[0]);
        transport_req = 2; frame16();
        CHECK(rep >= 3u && rep <= 5u, "black 1 (8 steps): the beats as they were (%u)", rep);
    }
    {   /* CHANCE */
        uint32_t full, half, none;
        grid(DV_HATC, 1);
        transport_req = 1; frame16();
        full = hits(DV_HATC, 4000, 0);
        cos_cput(CC_DCH0 + DV_HATC, 50);
        half = hits(DV_HATC, 4000, 0);
        cos_cput(CC_DCH0 + DV_HATC, 0);
        none = hits(DV_HATC, 2000, 0);
        cos_cput(CC_DCH0 + DV_HATC, 100);
        transport_req = 2; frame16();
        CHECK(full >= 30u && half > full / 4u && half < full * 3u / 4u && none <= 1u, "CHANCE 100 / 50 / 0: %u / %u / %u hats", full, half, none);
    }
    {   /* kept in the project */
        static project_t q;
        uint8_t b[64]; uint32_t len;
        dx_skip[3] = 40;
        memset(&q, 0, sizeof q);
        memcpy(q.dx_skip, dx_skip, 8);
        len = proj_dc(&q, b, 0, 0);
        memset(q.dx_skip, 0, 8);
        CHECK(len == 10u && proj_dc(&q, b, 0, sizeof b) == 10u && q.dx_skip[3] == 40u, "'DC': written, read back (%u %u)", len, q.dx_skip[3]);
        dx_skip[3] = 0;
    }
    printf("drm_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
