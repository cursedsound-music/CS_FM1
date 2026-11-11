/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) cursed-os 0.10.3: HOME > LIVE (the DJ FILTER, the global REVERB and DELAY sends, the delay's FEEDBACK;
 * white 1..8 the drums' sounds, black 1..6 the tracks) and BACK TO NORMAL (OCT-): at once with MUTE Q NOW or stopped,
 * on the bar with BAR, everything together; the sends heard; the phone's CC 99..102.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/cos_back_test tests/cos_back_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static void set_mq(uint32_t v) { favorites.factory[6][18] = (uint8_t)(v + 1u); cos_mq = (uint8_t)v; }   /* (MUTE Q: the settings' byte) */
static void frame16(void) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); ui_draw(); }
static void key(uint32_t k) { host_notes = 1u << k; cos_input(); frame16(); }
static void btn(uint32_t b) { fm1_in.buttons |= 1u << panel.btn[b]; host_pressed |= 1u << panel.btn[b]; frame16();
                              fm1_in.buttons &= ~(1u << panel.btn[b]); host_released |= 1u << panel.btn[b]; frame16(); }
/* the reverb's return over half a second of track k's hits (its own send at 0) */
static double lv_wet(uint32_t k)
{
    static int32_t o[2 * CTL];
    uint32_t b, i;
    double e = 0;
    for (i = 0; i < NTRK; i++) trk[i].p[P_MUTE] = (int16_t)(i != k);
    trk[k].p[P_REV] = 0; trk[k].p[P_DLY] = 0; trk[k].p[P_CHOR] = 0;
    for (b = 0; b < 40u; b++) mix_block(o, CTL);
    for (b = 0; b < FS / CTL; b++) {
        if (b % 40u == 0u) trk_note_on(&trk[k], 48 + (b / 40u) % 5u, 110);
        if (b % 40u == 4u) trk_note_off(&trk[k], 48 + (b / 40u) % 5u);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) e += (double)send_r[i] * send_r[i];
    }
    return e;
}

int main(void)
{
    static const uint8_t WK2[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
    static const uint8_t BK2[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
    int16_t *vp;
    ui_power_on();
    load_depth++; fm6_init(); power_on_parts(); load_depth--;
    song.sel = 0; cz.flip = 1; song.playing = 0; song.master_q12 = 4096;
    frame16();
    cz.mode = CM_HOME; cz.tab[CM_HOME] = 1;
    frame16();
    CHECK(cos_on_live() && cos_page()->id[0] == CC_DJF && cos_page()->id[1] == CC_LVREV && cos_page()->id[2] == CC_LVDLY,
          "HOME's second page: LIVE, DJ FILTER REVERB DELAY FEEDBACK (%s)", cos_page()->tab);
    {   /* stopped / NOW: the moves, then OCT- puts them all back at once */
        int16_t djf0 = song.g[G_DJF], fb0 = song.g[G_DFDBK];
        set_mq(0);
        key(WK2[1]);                                     /* (white 2: SNARE out, no HOME held) */
        CHECK(dx_mute & DXM_LANE(1), "LIVE: white 2 takes the SNARE out (%x)", dx_mute);
        CHECK(song.sel == 0u, "LIVE: white 2 not a track pick (%u)", song.sel);
        key(BK2[3]);                                     /* (black 4: T4 muted) */
        if (getenv("DBG")) printf("dbg mute %d %d %d %d msg %s mq %u qm %x live %u\n", trk[0].p[P_MUTE], trk[1].p[P_MUTE], trk[2].p[P_MUTE], trk[3].p[P_MUTE], cz.msg, cos_mq, live_qmute, trk_live);
        CHECK(trk[3].p[P_MUTE], "black 4: T4 muted");
        cos_cput(CC_LVREV, 90); cos_cput(CC_LVDLY, 40); cos_cput(CC_LVFB, 100); cos_cput(CC_DJF, GP[G_DJF].min + 10);
        CHECK(lv_snd[1] == 90 && lv_snd[0] == 40 && song.g[G_DFDBK] == 100, "the sends and FEEDBACK moved");
        CHECK(cos_nrm.ok && cos_nrm.djf == djf0 && cos_nrm.snd[1] == 0 && !(cos_nrm.lane & 2u), "NORMAL: before the first move");
        btn(B_OCTDN);
        CHECK(!(dx_mute & DXM_LANE(1)) && !trk[3].p[P_MUTE] && !lv_snd[0] && !lv_snd[1] && song.g[G_DFDBK] == fb0 &&
              song.g[G_DJF] == djf0 && !cos_nrm.ok, "OCT-: all back (%x %d %d %d %d)", dx_mute, trk[3].p[P_MUTE], lv_snd[1], song.g[G_DFDBK], song.g[G_DJF]);
        btn(B_OCTDN);
        CHECK(!strcmp(cz.msg, "ALREADY NORMAL"), "OCT- again: ALREADY NORMAL (%s)", cz.msg);
    }
    {   /* OCT+: this is normal */
        cos_cput(CC_LVREV, 30);
        btn(B_OCTUP);
        cos_cput(CC_LVREV, 100);
        btn(B_OCTDN);
        CHECK(lv_snd[1] == 30, "OCT+ made 30 the normal: back to 30 (%d)", lv_snd[1]);
        btn(B_OCTUP);
        cos_cput(CC_LVREV, 0); btn(B_OCTUP);
    }
    {   /* MUTE Q BAR, playing: nothing moves till the bar, then all of it together */
        static int32_t o[2 * CTL];
        uint32_t b, moved_early = 0;
        set_mq(2);
        song.g[G_BPM] = 120;
        transport_req = 1; frame16();
        for (b = 0; b < 30u; b++) mix_block(o, CTL);
        cos_drum_flip(3);
        for (b = 0; b < FS * 2u / CTL + 4u; b++) mix_block(o, CTL);   /* (HAT out on the bar) */
        frame16();
        CHECK(dx_mute & DXM_LANE(3), "BAR: HAT out on its bar");
        cos_cput(CC_LVDLY, 80);
        cos_cput(CC_DJF, GP[G_DJF].max - 5);
        btn(B_OCTDN);
        CHECK(live_qback == 1u && (dx_mute & DXM_LANE(3)) && lv_snd[0] == 80, "BAR: BACK asked, nothing yet (%u %x %d)", live_qback, dx_mute, lv_snd[0]);
        for (b = 0; b < FS * 2u / CTL + 4u; b++) {
            mix_block(o, CTL);
            if (b % 11u == 0u) {
                frame16();
                if (lv_snd[0] != 80 && (dx_mute & DXM_LANE(3))) moved_early = 1;   /* (the knobs before the sounds) */
            }
        }
        frame16();
        CHECK(!(dx_mute & DXM_LANE(3)) && lv_snd[0] == 0 && song.g[G_DJF] != GP[G_DJF].max - 5 && !live_qback,
              "the bar: the HAT, DELAY and DJ FILTER back together (%x %d %d)", dx_mute, lv_snd[0], song.g[G_DJF]);
        CHECK(!moved_early, "the knobs not ahead of the sounds");
        transport_req = 2; frame16();
        set_mq(0);
    }
    {   /* the global sends heard: a track with its own sends at 0 feeds the reverb through LIVE's */
        double e0, e1;
        lv_snd[1] = 0;
        e0 = lv_wet(2);
        lv_snd[1] = 100;
        e1 = lv_wet(2);
        CHECK(e0 < 1e6 && e1 > 1e9, "REVERB 0: nothing sent (%.3g); 100: sent (%.3g)", e0, e1);
        lv_snd[1] = 100;
        e1 = lv_wet(0);
        CHECK(e1 > 1e9, "the kit's hits too (%.3g)", e1);
        lv_snd[1] = 0;
    }
    {   /* the phone: CC 101 REVERB, 99 BACK */
        cos_nrm.ok = 0;
        rmt_cc_put(101, 127); rmt_cc_put(100, 64); cos_remote_poll();
        cos_cdesc(CC_LVREV, &vp);
        CHECK(lv_snd[1] == 127 && lv_snd[0] == 64, "CC 101 / 100: REVERB / DELAY (%d %d)", lv_snd[1], lv_snd[0]);
        rmt_cc_put(99, 127); cos_remote_poll();
        CHECK(!lv_snd[0] && !lv_snd[1], "CC 99: BACK TO NORMAL (%d %d)", lv_snd[0], lv_snd[1]);
    }
    printf("cos_back_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
