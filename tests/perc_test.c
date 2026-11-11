/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) cursed-os 0.14.1, GG 2026-11-11: "i dont need 2 kick drums ... the kick in the kit could be something
 * else". With a KICK track, the kit's first sound is a PERC: a RIDE at power-on, named PERC / PC, its own mute (HOME +
 * white 9), out of the KICK KILL and the DUCK, GEN's percussion line on it; without a KICK track the kick as before.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/perc_test tests/perc_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); } else printf("ok: "); printf(__VA_ARGS__); printf("\n"); } while (0)
static void frame16(void) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); ui_draw(); }

int main(void)
{
    uint32_t i, k, kt = NTRK, dr = NTRK;
    dx_lane_t ride;
    uint8_t lay;
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1; song.playing = 0; song.master_q12 = 4096; frame16(); frame16();
    for (k = 0; k < NTRK; k++) { if (trk[k].engine == ENGI_KICK && kt == NTRK) kt = k; if (trk[k].engine == ENGI_DRUM && dr == NTRK) dr = k; }
    CHECK(kt < NTRK && dr < NTRK && dx_k0p, "power-on: a KICK track, the kit's first a PERC");
    CHECK(cos_dsrc_get(DSRC_FMD + 5u, &ride, &lay) && !memcmp(&dx_kit[0], &ride, sizeof ride) && trk[dr].p[P_LN0] == 90,
          "the PERC a RIDE, LEVEL 90 (%d)", trk[dr].p[P_LN0]);
    CHECK(!strcmp(drum_lane_name(&trk[dr], 0), "PERC") && !strcmp(COS_LN[0], "PC") && !strcmp(COS_DRUM5[0], "PERC"), "named PERC");
    /* the KICK KILL: the KICK track, not the PERC */
    dx_mute_set(DXG_KICK);
    CHECK(dx_kick_muted() && !dx_lane_muted(0), "KICK KILL: the KICK track out, the PERC on");
    dx_mute_set(DXG_PERC);
    CHECK(!dx_kick_muted() && dx_lane_muted(0), "the toms' group: the PERC with them");
    dx_mute_set(0);
    /* HOME + white 9: the PERC's own mute; HOME + white 1: the KICK track's (MUTE Q off: at once) */
    cos_mq = 0;
    cos_drum_flip(8);
    CHECK((dx_mute & DXM_LANE(0)) && !trk[kt].p[P_MUTE] && !dx_kick_muted(), "HOME + white 9: PERC out, the kick on");
    cos_drum_flip(8);
    CHECK(!(dx_mute & DXM_LANE(0)), "again: in");
    cos_drum_flip(0);
    CHECK(trk[kt].p[P_MUTE] && !(dx_mute & DXM_LANE(0)), "HOME + white 1: the KICK track out");
    cos_drum_flip(0);
    /* no DUCK from the PERC */
    {
        static int32_t o[2 * CTL];
        for (k = 0; k < 20u; k++) mix_block(o, CTL);
        duck_hit = 0;
        trk_note_on(&trk[dr], DRUM_LANE_NOTE[0], 100);
        mix_block(o, CTL);
        CHECK(!duck_hit, "a PERC hit: no DUCK");
        trk_note_off(&trk[dr], DRUM_LANE_NOTE[0]);
        trk_note_on(&trk[kt], 36, 100);
        CHECK(duck_hit, "a KICK hit: the DUCK");
        trk_note_off(&trk[kt], 36);
        for (k = 0; k < 20u; k++) mix_block(o, CTL);
    }
    /* GEN: a percussion line on it, off the quarters; the kick's steps kept for the bass */
    {
        uint32_t q = 0, off = 0;
        gseed(GS_DRUMS);
        gs.g = &GENRE[0];
        for (i = 0; i < NSTEP; i++) gclear(&trk[dr].step[i]);
        gen_drums(&trk[dr], 16);
        for (i = 0; i < 16u; i++) {
            if (trk[dr].step[i].hit & 1u) { if (i % 4u == 0u) q++; else off++; }
        }
        CHECK(!q && off >= 2u && gs.kick, "GEN: the PERC off the quarters (%u hits), the kick's steps known (%llx)", off,
              (unsigned long long)gs.kick);
    }
    /* without a KICK track: the kick as before */
    set_engine_of(&trk[kt], ENGI_ANALOG); trk[kt].engine = ENGI_ANALOG;
    frame16();
    CHECK(!dx_k0p && !strcmp(drum_lane_name(&trk[dr], 0), "KICK") && !strcmp(COS_LN[0], "BD"), "no KICK track: KICK, BD");
    dx_mute_set(DXG_KICK);
    CHECK(dx_lane_muted(0), "KICK KILL: the kit's kick");
    dx_mute_set(0);
    printf(fails ? "perc: %d FAILED\n" : "perc: all ok\n", fails);
    return fails != 0;
}
