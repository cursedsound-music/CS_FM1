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
#define LED_ON(k) ((fm1_led[led_pos[14u + (k)] >> 3] >> (led_pos[14u + (k)] & 7u)) & 1u)
#define LED_BR(k) ((fm1_led_breath[led_pos[14u + (k)] >> 3] >> (led_pos[14u + (k)] & 7u)) & 1u)
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

static void settle(void) { uint32_t i; for (i = 0; i < 12u; i++) frame16(); }   /* (0.12.5: BACK's glides, ~200 ms) */
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
        settle();
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
        settle();
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
        settle();
        CHECK(!(dx_mute & DXM_LANE(3)) && lv_snd[0] == 0 && song.g[G_DJF] != GP[G_DJF].max - 5 && !live_qback,
              "the bar: the HAT, DELAY and DJ FILTER back together (%x %d %d)", dx_mute, lv_snd[0], song.g[G_DJF]);
        CHECK(!moved_early, "the knobs not ahead of the sounds");
        transport_req = 2; frame16();
        set_mq(0);
    }
    {   /* (0.12.5) BACK lets the tails ring: the sends down in ~100 ms, the FEEDBACK down over two bars, the delay heard
         * on after it; a knob turned meanwhile takes over */
        static int32_t o[2 * CTL];
        uint32_t b, f;
        int16_t fb0 = song.g[G_DFDBK];
        double tail_old, tail_new;
        set_mq(0);
        song.g[G_BPM] = 120;
        transport_req = 1; frame16();
        cos_cput(CC_LVDLY, 100); cos_cput(CC_LVFB, 120);
        for (b = 0; b < FS / CTL; b++) mix_block(o, CTL);
        btn(B_OCTDN);
        for (f = 0; f < 4u; f++) frame16();
        CHECK(lv_snd[0] < 100 && song.g[G_DFDBK] > fb0 + 40, "BACK: the send going, the FEEDBACK still high (%d %d)", lv_snd[0], song.g[G_DFDBK]);
        for (f = 0; f < 10u; f++) frame16();
        CHECK(lv_snd[0] == 0, "the send down in ~100 ms (%d)", lv_snd[0]);
        for (f = 0; f < 4000u / 16u + 10u; f++) frame16();   /* (two bars at 120: 4 s) */
        CHECK(song.g[G_DFDBK] == fb0, "the FEEDBACK back after two bars (%d %d)", song.g[G_DFDBK], fb0);
        {   /* the delay after BACK: heard longer than with FEEDBACK let down at once (as before) */
            uint32_t k;
            double *tl[2] = {&tail_old, &tail_new};
            for (k = 0; k < 2u; k++) {
                for (b = 0; b < FS * 3u / CTL; b++) mix_block(o, CTL);   /* (the last one's tail gone) */
                for (b = 0; b < NTRK; b++) trk[b].p[P_MUTE] = (int16_t)(b != 2u);
                trk[2].p[P_DLY] = 0;
                cos_nrm.ok = 0;
                cos_cput(CC_LVDLY, 100); cos_cput(CC_LVFB, 120);
                for (b = 0; b < FS / CTL; b++) {          /* (notes into the delay through LIVE's send) */
                    if (b % 40u == 0u) trk_note_on(&trk[2], 48 + (b / 40u) % 5u, 110);
                    if (b % 40u == 4u) trk_note_off(&trk[2], 48 + (b / 40u) % 5u);
                    mix_block(o, CTL);
                }
                for (b = 0; b < NTRK; b++) trk[b].p[P_MUTE] = 1;   /* (everything quiet: only the tail) */
                if (k == 0u) { cos_back(0); cos_rmp.on = 0; song.g[G_DFDBK] = fb0; lv_snd[0] = 0; }   /* (as 0.12.4) */
                else cos_back(0);
                *tl[k] = 0;
                for (b = 0; b < FS * 2u / CTL; b++) {
                    uint32_t i;
                    mix_block(o, CTL);
                    if (b % 22u == 0u) frame16();
                    if (b > FS / CTL) for (i = 0; i < 2u * CTL; i++) *tl[k] += (double)o[i] * o[i];   /* (1 .. 2 s after) */
                }
            }
            printf("  the delay 1 .. 2 s after BACK: %.3g (FEEDBACK let down at once: %.3g)\n", tail_new, tail_old);
            CHECK(tail_new > tail_old * 4.0, "the echoes ring on after BACK (%.3g, cut at once: %.3g)", tail_new, tail_old);
        }
        cos_cput(CC_LVFB, 120);
        cos_nrm.ok = 0; cos_cput(CC_LVFB, 110); btn(B_OCTDN); frame16();
        cos_cput(CC_LVFB, 90);
        for (f = 0; f < 40u; f++) frame16();
        CHECK(song.g[G_DFDBK] == 90, "FEEDBACK turned meanwhile: its own (%d)", song.g[G_DFDBK]);
        song.g[G_DFDBK] = fb0;
        for (b = 0; b < NTRK; b++) trk[b].p[P_MUTE] = 0;
        transport_req = 2; frame16();
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
        settle();
        CHECK(!lv_snd[0] && !lv_snd[1], "CC 99: BACK TO NORMAL (%d %d)", lv_snd[0], lv_snd[1]);
    }
    {   /* (0.10.4) the keys' lights on LIVE: a track playing, its black key lit; a drum sound in, its white key lit; a
         * flip waiting for the bar, its key breathing */
        uint32_t k;
        set_mq(0);
        cz.mode = CM_HOME; cz.tab[CM_HOME] = 1;
        for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = 0;
        trk[4].p[P_MUTE] = 1;
        dx_mute_set(DXM_LANE(2));
        cos_leds();                                      /* (its first call reads the (zero) map: then ours) */
        {   /* (the host's key map is zero: the hardware's, as ui_test.c test_step_leds) */
            static const int8_t MAP[6][FM1_NCOL] = {
                {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
                {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12}, {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
                { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22}, {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};
            uint32_t id, p, r;
            for (id = 0; id < 41u; id++) {
                led_pos[id] = 0xFF;
                for (p = 0; p < FM1_NCOL; p++)
                    for (r = 1; r < 5u; r++)
                        if (MAP[r][p] == (int8_t)id) led_pos[id] = (uint8_t)((p << 3) | r);
            }
        }
        frame16(); cos_leds();
        if (getenv("DBG")) { uint32_t q; for (q = 0; q < 8; q++) printf("pos %u: %x led %x\n", q, led_pos[14 + q], fm1_led[q]); printf("mode %u tab %u live %d anim %d\n", cz.mode, cz.tab[CM_HOME], cos_on_live(), host_anim); }
        for (k = 0; k < 6u; k++)
            CHECK(LED_ON(BK2[k]) == (k != 4u), "black %u: %s", k + 1u, k != 4u ? "lit (playing)" : "dark (muted)");
        for (k = 0; k < 8u; k++)
            CHECK(LED_ON(WK2[k]) == (k != 2u), "white %u: %s", k + 1u, k != 2u ? "lit (in)" : "dark (CLAP out)");
        set_mq(2);
        transport_req = 1; frame16();
        cos_drum_flip(5);
        key(BK2[1]);                                     /* (black 2: the KICK muted on the bar) */
        cos_leds();
        CHECK(LED_BR(WK2[5]) && !LED_ON(WK2[5]), "TOM's flip waiting: white 6 breathes");
        CHECK(LED_BR(BK2[1]) && !LED_ON(BK2[1]), "T2's mute waiting: black 2 breathes");
        transport_req = 2; frame16();
        set_mq(0);
        dx_mute_set(0);
    }
    printf("cos_back_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
