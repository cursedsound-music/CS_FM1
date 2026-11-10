/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) the phone remote, the device's side (firmware/src/cos_remote.c, cos_sample.c, eng_sampler.c), fed the
 * frames tools/cursed_remote_proto.py makes (tests/remote_proto_test.py runs it):
 *   remote_test <frames file> <slot>   each frame (F0 .. F7) into rmt_frame, its reply printed (ACK n / NAK c);
 *                                      then "DEC" and the slot's samples as the engine decodes them (a line each),
 *                                      then "PLAY <rms> <crossings at C4> <crossings at C5>": the SAMPLER playing it
 *   remote_test                        its own checks: the CCs, TEXT, a NAK while playing, a sound out of order */
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

static void play_stats(uint32_t slot, uint32_t note, double *rms, uint32_t *cross)
{
    static int32_t o[2 * CTL];
    track_t *t = &trk[1];
    uint32_t b, i, nb = FS / 2u / CTL, x = 0;
    double e = 0;
    int32_t last = 0;
    set_engine_of(t, getenv("RT_ENG") ? (uint32_t)atoi(getenv("RT_ENG")) : ENGI_SAMPLE);
    t->engine = t->eng_req;
    t->p[P_E0] = (int16_t)slot; t->p[P_E1] = 0; t->p[P_E2] = 127; t->p[P_E3] = 0; t->p[P_E4] = 0; t->p[P_E5] = 0; t->p[P_E6] = 100;
    t->p[P_MUTE] = 0; t->p[P_LEVEL] = 100; t->p[P_DLY] = t->p[P_REV] = t->p[P_CHOR] = t->p[P_DIST] = 0;
    t->p[P_ATK] = 0; t->p[P_SUS] = 127; t->p[P_DEC] = 64;
    for (i = 0; i < NTRK; i++) if (i != 1u) trk[i].p[P_MUTE] = 1;
    for (b = 0; b < 40u; b++) mix_block(o, CTL);          /* (the engine's switch faded through first) */
    trk_note_on(t, note, 100);
    if (getenv("RT_DBG")) { int32_t pk = 0, q; mix_block(o, CTL); for (q = 0; q < 2 * CTL; q++) if (abs(o[q]) > pk) pk = abs(o[q]); printf("dbg peak %d mute %d lvl %d live %u solo %u\n", pk, t->p[P_MUTE], t->p[P_LEVEL], trk_live, perf_solo); printf("dbg eng %u req %u active %d s6 %d ph0 %u end %u\n", t->engine, t->eng_req, t->v[0].active, t->v[0].s[6], t->v[0].ph[0], t->v[0].ph[2]); }
    for (b = 0; b < nb; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t s = o[2u * i];
            e += (double)s * s;
            if ((s >= 0) != (last >= 0) && b * CTL < FS * 3u / 10u) x++;   /* (crossings: the first 0.3 s) */
            last = s;
        }
    }
    trk_note_off(t, note);
    for (b = 0; b < 40u; b++) mix_block(o, CTL);
    *rms = sqrt(e / (nb * CTL));
    *cross = x;
}

int main(int argc, char **argv)
{
    ui_power_on();
    cz.flip = 1;
    song.master_q12 = 4096;                              /* (the master pot at unity: 0 in the tests) */
    usb.config = 1;
    song.playing = 0;
    if (argc >= 3) {                                     /* frames from the Python side */
        FILE *fp = fopen(argv[1], "rb");
        static uint8_t buf[1 << 20];
        uint32_t n, i, s = 0, slot = (uint32_t)atoi(argv[2]), cc, v;
        const smp_ent_t *e;
        if (!fp) return 2;
        n = (uint32_t)fread(buf, 1, sizeof buf, fp);
        fclose(fp);
        for (i = 0; i < n; i++) {
            if (buf[i] == 0xF0u) s = i;
            if (buf[i] == 0xF7u) {
                rt_feed(buf + s, i - s + 1u);
                if (rt_reply(&cc, &v)) printf("%s %u\n", cc == 118u ? "ACK" : "NAK", v);
                else printf("NONE\n");
            }
        }
        e = smp_ent(slot);
        printf("DEC %u %u\n", e ? e->len : 0u, e ? e->rate : 0u);
        if (e) {
            int32_t pr = 0, ix = 0;
            uint32_t k;
            for (k = 0; k < e->len; k++) printf("%d\n", smp_next(e, k, &pr, &ix));
            {
                double r1, r2;
                uint32_t c1, c2;
                play_stats(slot, 60, &r1, &c1);
                play_stats(slot, 72, &r2, &c2);
                printf("PLAY %.0f %u %u\n", r1, c1, c2);
            }
        }
        return 0;
    }
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
            CHECK(*vp == b0 + 10, "CC 39: BPM + 1 (%d -> %d)", b0, *vp);
            rmt_cc_put(38, 127); ui_input(); cos_cdesc(CC_BPM, &vp);
            CHECK(*vp == b0, "CC 38: BPM - 1 (%d)", *vp);
            rmt_cc_put(79, 72); ui_input(); cos_cdesc(CC_BPM, &vp);
            CHECK(*vp == 1320, "CC 79 at 72: 132 BPM (%d)", *vp);
            rmt_bpm(b0);
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
    {   /* the sampler: playing, a BEGIN is refused (NAK 2); DATA before a BEGIN: NAK 6 */
        static const uint8_t B[] = {0xF0, 0x7D, 0x63, 0x10, 0, 0, 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 0x22, 0x2C, 0x01,
                                    0x00, 0x02, 0x00, 0x04, 0x01, 0x00, 0xF7};   /* (22050 Hz, 256 samples, 132 bytes) */
        static const uint8_t D[] = {0xF0, 0x7D, 0x63, 0x11, 5, 0, 1, 2, 3, 0xF7};
        uint32_t cc = 0, v = 0;
        song.playing = 1;
        rt_feed(B, sizeof B);
        CHECK(rt_reply(&cc, &v) && cc == 119u && v == 2u, "BEGIN while playing: NAK 2 (%u %u)", cc, v);
        song.playing = 0;
        su.on = 0;
        rt_feed(D, sizeof D);
        CHECK(rt_reply(&cc, &v) && cc == 119u && v == 6u, "DATA with no BEGIN: NAK 6 (%u %u)", cc, v);
        rt_feed(B, sizeof B);
        CHECK(rt_reply(&cc, &v) && cc == 118u && v == 0u, "BEGIN stopped: ACK 0 (%u %u)", cc, v);
        rt_feed(D, sizeof D);
        CHECK(rt_reply(&cc, &v) && cc == 119u && v == 6u, "DATA out of order (5 after 0): NAK 6 (%u %u)", cc, v);
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
