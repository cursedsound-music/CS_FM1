/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) cursed-os 0.10.0: the KICK engine (eng_kick.c) and the power-on layout (T1 DRUM without its kick, T2
 * KICK), every KIND sounding, the keys tuning it, one voice, the KICK KILL, GEN's PATTERN.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/kick_test tests/kick_test.c -lm && build/t/kick_test */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void solo(uint32_t k)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) trk[i].p[P_MUTE] = (int16_t)(i != k);
}
/* a hit on track k: its energy, its zero crossings over the first 150 ms (its pitch) */
static double hit(uint32_t k, uint32_t note, uint32_t *cross)
{
    static int32_t o[2 * CTL];
    uint32_t b, i, x = 0, nb = FS / 2u / CTL;
    int32_t last = 0;
    double e = 0;
    for (b = 0; b < 30u; b++) mix_block(o, CTL);
    trk_note_on(&trk[k], note, 110);
    for (b = 0; b < nb; b++) {
        mix_block(o, CTL);
        if (b == 3u) trk_note_off(&trk[k], note);
        for (i = 0; i < CTL; i++) {
            int32_t s = o[2u * i];
            e += (double)s * s;
            if (b * CTL + i > FS / 200u && b * CTL < FS * 15u / 100u && (s >= 0) != (last >= 0)) x++;
            last = s;
        }
    }
    if (cross) *cross = x;
    return e;
}

int main(void)
{
    uint32_t k, c1, c2;
    double e;
    ui_power_on();                                       /* (as ui_test.c ui_power_on_099: the power-on parts) */
    load_depth++;
    fm6_init();
    power_on_parts();
    load_depth--;
    song.sel = 0;
    cz.flip = 1;
    ui_input();
    song.master_q12 = 4096;
    song.playing = 0;
    CHECK(trk_live == NTRK, "six tracks play at power-on (%u)", trk_live);
    CHECK(trk_eng(&trk[0]) == ENGI_DRUM && trk_eng(&trk[1]) == ENGI_KICK && trk_eng(&trk[2]) == ENGI_ACID &&
          trk_eng(&trk[5]) == ENGI_VOICE, "the layout: DRUM KICK ACID .. VOICE (%u %u %u %u)",
          trk_eng(&trk[0]), trk_eng(&trk[1]), trk_eng(&trk[2]), trk_eng(&trk[5]));
    CHECK(trk[0].p[P_LN0] == 0 && trk[0].p[P_LN0 + 1] > 0, "the kit's kick lane at 0, its snare not (%d %d)", trk[0].p[P_LN0], trk[0].p[P_LN0 + 1]);
    {
        char b[8];
        cos_trk_label(&trk[1], b);
        CHECK(!strcmp(b, "KICK"), "T2's label: KICK (%s)", b);
    }
    solo(0);
    e = hit(0, 36, 0);
    CHECK(e < 1e6, "the kit's C (its kick) silent: %.0f", e);
    e = hit(0, 38, 0);
    CHECK(e > 1e9, "the kit's snare sounds: %.3g", e);
    solo(1);
    for (k = 0; k < 12u; k++) {
        trk[1].p[P_E0] = (int16_t)k;
        e = hit(1, 36, 0);
        CHECK(e > 1e10, "KIND %u (%s) sounds: %.3g", k, KICK_N[k], e);
    }
    for (k = 0; k < NELEM(KICK_PRESETS); k++) {
        apply_preset_to(&trk[1], k);
        e = hit(1, 36, 0);
        CHECK(e > 1e10, "preset %s sounds: %.3g", KICK_PRESETS[k].name, e);
    }
    apply_preset_to(&trk[1], 0);
    trk[1].p[P_E3] = 127;                                /* (DECAY long: a pitch to count) */
    hit(1, 36, &c1);
    hit(1, 48, &c2);
    CHECK(c2 > c1 * 3 / 2 && c2 < c1 * 5 / 2, "an octave up: about twice the crossings (%u %u)", c1, c2);
    {   /* one voice: a second hit cuts the first */
        static int32_t o[2 * CTL];
        uint32_t b, i, n = 0;
        trk_note_on(&trk[1], 36, 110);
        mix_block(o, CTL);
        trk_note_on(&trk[1], 40, 110);
        for (b = 0; b < 4u; b++) mix_block(o, CTL);
        for (i = 0; i < NVOICE; i++) n += trk[1].v[i].active;
        CHECK(n == 1u, "a kick is mono: one voice sounding (%u)", n);
        trk_note_off(&trk[1], 36); trk_note_off(&trk[1], 40);
    }
    {   /* KICK KILL (DXG_KICK) silences it */
        dx_mute_set(DXG_KICK);
        e = hit(1, 36, 0);
        CHECK(e < 1e9, "KICK KILL: the KICK track silent (the last hit's fade only) (%.3g)", e);
        dx_mute_set(0);
    }
    {   /* GEN on the KICK: an empty track gets FOUR ON THE FLOOR, PATTERN on KNOB 1 */
        uint32_t i, n = 0;
        for (i = 0; i < NSTEP; i++) trk[1].step[i].n = 0, trk[1].step[i].time = ST_REST;
        track_select(1);
        cos_mode(CM_GEN);
        for (i = 0; i < 16u; i++) n += trk[1].step[i].n && trk[1].step[i].note[0] == 36u;
        CHECK(n == 4u && trk[1].step[0].n && trk[1].step[4].n && trk[1].step[12].n, "GEN: four on the floor (%u)", n);
        cos_cput(CC_KPAT, 4);
        for (i = 0, n = 0; i < 16u; i++) n += trk[1].step[i].n;
        CHECK(n == 2u && trk[1].step[8].n, "PATTERN HALF: beats 1 and 3 (%u)", n);
    }
    {   /* (0.10.0) the drums' sounds in and out: NOW at once; the phone's CC 91..98 (explicit); BAR waits for the bar */
        static int32_t o[2 * CTL];
        uint32_t b;
        dx_mute_set(0);
        trk[1].p[P_MUTE] = 0;
        cos_mq = 0;
        cos_drum_flip(1);
        CHECK((dx_mute & DXM_LANE(1)) && cos_drum_out(1), "NOW: SNARE out at once (%x)", dx_mute);
        solo(0);
        e = hit(0, 38, 0);
        CHECK(e < 1e6, "the snare out: silent (%.3g)", e);
        cos_drum_flip(1);
        CHECK(!(dx_mute & DXM_LANE(1)), "SNARE back in (%x)", dx_mute);
        trk[1].p[P_MUTE] = 0;                            /* (solo() muted it) */
        cos_drum_flip(0);
        CHECK(trk[1].p[P_MUTE] && cos_drum_out(0), "white 1: the KICK track muted");
        cos_drum_flip(0);
        CHECK(!trk[1].p[P_MUTE], "and back");
        rmt_cc_put(93, 127); rmt_cc_put(94, 127); rmt_cc_put(94, 127); cos_remote_poll();
        CHECK((dx_mute & (DXM_LANE(2) | DXM_LANE(3))) == (DXM_LANE(2) | DXM_LANE(3)), "CC 93 94: CLAP and HAT out, a repeat no flip (%x)", dx_mute);
        rmt_cc_put(93, 0); rmt_cc_put(91, 127); cos_remote_poll();
        CHECK(!(dx_mute & DXM_LANE(2)) && trk[1].p[P_MUTE], "CC 93 0: CLAP in; CC 91: the KICK out (%x)", dx_mute);
        rmt_cc_put(91, 0); rmt_cc_put(94, 0); cos_remote_poll();
        cos_mq = 2;                                      /* BAR: waits for the bar while playing */
        song.g[G_BPM] = 120;
        transport_req = 1; ui_input();
        for (b = 0; b < 10u; b++) mix_block(o, CTL);
        cos_drum_flip(4);
        CHECK(!(dx_mute & DXM_LANE(4)) && cos_drum_out(4), "BAR: OPEN HAT out pending (%x %x)", dx_mute, live_qlane);
        for (b = 0; b < FS * 2u / CTL + 4u; b++) mix_block(o, CTL);   /* (a bar at 120: 2 s) */
        CHECK((dx_mute & DXM_LANE(4)) && !live_qlane, "BAR: in on the bar (%x %x)", dx_mute, live_qlane);
        transport_req = 2; ui_input();
        dx_mute_set(0);
        cos_mq = 0;
    }
    printf("kick_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
