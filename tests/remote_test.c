/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound
 * (cursedsound) the phone remote, the device's side (firmware/src/cos_remote.c):
 *   remote_test   its own checks: the CCs, TEXT, the sampler's frames refused (0.10.0: gone), BLE's frame assembly */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t rt_w0;
static int rt_reply(uint32_t *cc, uint32_t *v)          /* the last channel-16 CC sent since rt_w0 */
{
    uint32_t i, got = 0;
    for (i = rt_w0; i != mo_w; i++) {
        uint32_t pk = midi_out_q[i % MQ];
        if (((pk >> 8) & 0xFFu) == 0xBFu) { *cc = (pk >> 16) & 127u; *v = (pk >> 24) & 127u; got = 1; }
    }
    rt_w0 = mo_w;
    mo_r = mo_w;
    return got;
}
static void rt_feed(const uint8_t *f, uint32_t n)        /* a whole F0 .. F7 */
{
    rt_w0 = mo_w;
    if (n >= 5u && f[0] == 0xF0u && f[n - 1u] == 0xF7u && f[1] == 0x7Du && f[2] == 0x63u) rmt_frame(f + 3, n - 4u);
}
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(int argc, char **argv)
{
    ui_power_on();
    cz.flip = 1;
    song.master_q12 = 4096;                              /* (the master pot at unity: 0 in the tests) */
    usb.config = 1;
    song.playing = 0;
    {   /* channel 16's CCs: PLAY, a mute, the DJ FILTER, a pad key held and let go, a track picked */
        uint32_t m0 = trk[2].p[P_MUTE];
        cos_mq = 0;
        rmt_cc_put(20, 127); rmt_cc_put(20, 0);
        rmt_cc_put(24, 127);
        rmt_cc_put(70, 0);
        rmt_cc_put(47, 127);                             /* (white 8: LPF sweep, perform.c's) */
        rmt_cc_put(87, 127);
        cos_remote_poll();
        CHECK(transport_req == 1u, "CC 20: PLAY asked (%u)", transport_req);
        CHECK(trk[2].p[P_MUTE] != (int16_t)m0, "CC 24: track 3 muted");
        CHECK(song.g[G_DJF] == GP[G_DJF].min, "CC 70 at 0: the DJ FILTER at its bottom (%d)", song.g[G_DJF]);
        CHECK((perf_held >> PF_LPF) & 1u, "CC 47 held: the pad's LPF on");
        CHECK(song.sel == 2u, "CC 87: track 3 picked (%u)", song.sel);
        rmt_cc_put(47, 0);
        cos_remote_poll();
        CHECK(!((perf_held >> PF_LPF) & 1u), "CC 47 let go: off");
        rmt_cc_put(47, 0);
        cos_remote_poll();
        CHECK(!((perf_held >> PF_LPF) & 1u), "a release repeated: nothing");
        transport_req = 0;
        trk[2].p[P_MUTE] = (int16_t)m0;
        {   /* (0.9.6) NUDGE held, let go; lost after 5 s; BPM + 1 */
            int16_t *vp; int32_t b0;
            cos_cdesc(CC_BPM, &vp); b0 = *vp;
            rmt_cc_put(37, 127); ui_input();
            CHECK(bpm_nudge == 1, "CC 37 held: the tempo nudged up (%d)", bpm_nudge);
            rmt_cc_put(37, 0); ui_input();
            CHECK(bpm_nudge == 0, "CC 37 let go: no nudge (%d)", bpm_nudge);
            rmt_cc_put(36, 127); ui_input();
            CHECK(bpm_nudge == -1, "CC 36 held: nudged down (%d)", bpm_nudge);
            fm1_ms += 6000u; ui_input();
            CHECK(bpm_nudge == 0, "a nudge held 6 s (the link lost): over (%d)", bpm_nudge);
            rmt_cc_put(39, 127); ui_input(); cos_cdesc(CC_BPM, &vp);
            CHECK(*vp == b0 + 100, "CC 39: BPM + 1 (%d -> %d)", b0, *vp);
            rmt_cc_put(38, 127); ui_input(); cos_cdesc(CC_BPM, &vp);
            CHECK(*vp == b0, "CC 38: BPM - 1 (%d)", *vp);
            rmt_cc_put(79, 72); ui_input(); cos_cdesc(CC_BPM, &vp);
            CHECK(*vp == 13200, "CC 79 at 72: 132 BPM (%d)", *vp);
            rmt_cc_put(80, 1284 >> 7); rmt_cc_put(81, 1284 & 127); ui_input(); cos_cdesc(CC_BPM, &vp);
            CHECK(*vp == 12840, "CC 80/81: 128.4 BPM (%d)", *vp);
            cos_bpm_set(b0);                             /* (hundredths, 0.10.1) */
            {   /* the faders: each of 70..78 moves its knob to the top and the bottom */
                static const uint8_t ID[9] = {CC_DJF, CC_EQL, CC_EQM, CC_EQH, CC_MAC0, CC_MAC0 + 1u, CC_MAC0 + 2u, CC_MAC3, CC_MOPOS};
                uint32_t c;
                for (c = 0; c < 9u; c++) {
                    const param_desc_t *d;
                    int32_t hi, lo;
                    rmt_cc_put(70u + c, 127); ui_input(); d = cos_cdesc(ID[c], &vp); hi = *vp;
                    rmt_cc_put(70u + c, 0); ui_input(); d = cos_cdesc(ID[c], &vp); lo = *vp;
                    CHECK(hi == d->max && lo == d->min, "CC %u: its knob top %d (%d) bottom %d (%d)", 70u + c, hi, d->max, lo, d->min);
                    rmt_cc_put(70u + c, 64); ui_input();
                }
            }
        }
    }
    {   /* TEXT: the first VOICE track's words; the reply an ACK */
        static const uint8_t F[] = {0xF0, 0x7D, 0x63, 0x01, 0x7F, 'h', 'e', 'l', 'l', 'o', ' ', 'm', 'e', 0xF7};
        uint32_t t, cc = 0, v = 99;
        for (t = 0; t < NTRK && trk_eng(&trk[t]) != ENGI_VOICE; t++) {}
        if (t == NTRK) { set_engine_of(&trk[4], ENGI_VOICE); t = 4; }
        trk[t].p[P_VLANG] = 0;
        rt_feed(F, sizeof F);
        CHECK(rt_reply(&cc, &v) && cc == 118u, "TEXT: ACK (%u %u)", cc, v);
        CHECK(!strcmp(lyr_get(t), "hello me") && trk[t].p[P_VLYR] == 0 && trk[t].p[P_VLANG] == LG_EN,
              "TEXT: track %u sings 'hello me' (%s)", t + 1u, lyr_get(t));
    }
    {   /* (0.10.0) the sampler's frames (0x10..0x13, 0.9.5..0.9.9): gone, NAK 1 */
        static const uint8_t B[] = {0xF0, 0x7D, 0x63, 0x10, 0, 0, 'A', 0xF7};
        uint32_t cc = 0, v = 0;
        rt_feed(B, sizeof B);
        CHECK(rt_reply(&cc, &v) && cc == 119u && v == 1u, "a sampler frame: NAK 1 (%u %u)", cc, v);
    }
    {   /* the ISR's frame assembly: BLE's SysEx packets (CIN 4..7) into one frame; a second while one waits: dropped */
        rmt_fready = 0;
        rmt_sx_pkt(0x4u | 0xF0u << 8 | 0x7Du << 16 | 0x63u << 24);
        rmt_sx_pkt(0x4u | 0x01u << 8 | 0x7Fu << 16 | 'o' << 24);
        rmt_sx_pkt(0x6u | 'k' << 8 | 0xF7u << 16);
        CHECK(rmt_fready && rmt_fn == 6u && rmt_f[4] == 'o' && rmt_f[5] == 'k', "BLE: a frame assembled (%u)", rmt_fn);
        rmt_sx_pkt(0x4u | 0xF0u << 8 | 0x7Du << 16 | 0x63u << 24);
        rmt_sx_pkt(0x6u | 'x' << 8 | 0xF7u << 16);
        CHECK(rmt_fn == 6u && rmt_f[4] == 'o', "BLE: a frame while one waits: dropped");
        rmt_fready = 0;
    }
    printf("remote_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
