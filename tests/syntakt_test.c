/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound
 * (cursedsound) cursed-os 0.17.0, GG 2026-11-13: "complete the syntakt integration. It should be easy to setup and
 * allow for easy song and jam creation", the FM-1 as additional voices to the Syntakt in DJ sets, its audio into the
 * Syntakt's INPUT, the Syntakt's tempo set by hand to the CDJs. GLO > SYNTAKT:
 *   ON: CLOCK TRS, its patterns followed on channel 15 (or the one set), nothing sent back; kept over a power-off
 *   (the settings byte read at power-on); OFF: the FM-1's own clock, no patterns.
 *   PATTERNS OFF / FOLLOW; PAT LEN: a pattern change of the Syntakt's (PRG CH SEND, sent when the next pattern is
 *   picked) comes in at the end of its pattern (the bars since PLAY a multiple of PAT LEN), one of the FM-1's own
 *   on the next bar as before.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/syntakt_test tests/syntakt_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); } else printf("ok: "); printf(__VA_ARGS__); printf("\n"); } while (0)
static void frame16(void) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); ui_draw(); }
static void play_n(uint32_t samples)                    /* the audio and the main loop (ui_test.c's sl_play) */
{
    uint32_t done;
    for (done = 0; done < samples; done += 128u) {
        events_block(128u);
        if (!(done & 1023u)) song_poll();
    }
    song_poll();
}
static void steps_at(uint32_t base)                     /* every track: step 1 holds note base + track */
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) {
        track_defaults_steps(&trk[i]);
        trk[i].p[P_SLEN] = 16;
        trk[i].step[0] = (step_t){{(uint8_t)(base + i), 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
    }
}

int main(void)
{
    uint32_t i, bar, n;
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1;
    /* the page: GLO's, one of its everyday pages */
    {
        const cpg_t *p;
        cos_pages(CM_MIX, &p, &n);
        for (i = 0; i < n && !str_eq(p[i].tab, "SYNTAKT"); i++) {}
        CHECK(i < n && p[i].id[0] == CC_STON && cos_ess(CM_MIX, p, i, n), "GLO > SYNTAKT: an everyday page (a GLO tap)");
        cz.mode = CM_MIX; cz.tab[CM_MIX] = (uint8_t)i; frame16(); frame16();
    }
    /* ON / OFF */
    cos_cput(CC_PCCH, 0); song.g[G_CLOCK] = 0; cos_cput(CC_PCOUT, 1);
    cos_syntakt(CC_STON, 1);
    CHECK(song.g[G_CLOCK] == 2 && cos_pc_ch == 15u && !cos_pc_out && (favorites.factory[8][31] & 1u) && favorites.factory[6][19] == 15u,
          "SYNTAKT ON: CLOCK TRS, its patterns on channel 15, PC OUT off, kept");
    frame16();
    cos_cput(CC_PCCH, 11);
    cos_syntakt(CC_STON, 1);
    CHECK(cos_pc_ch == 11u, "a channel set (PC CH 11) kept by ON");
    cos_syntakt(CC_STPAT, 0);
    CHECK(!cos_pc_ch, "PATTERNS OFF");
    cos_syntakt(CC_STPAT, 1);
    CHECK(cos_pc_ch == 15u, "PATTERNS FOLLOW: channel 15");
    cos_syntakt(CC_STLEN, 2);
    CHECK(cos_pc_bars == 4u && (favorites.factory[8][31] >> 1) == 2u, "PAT LEN 4 BARS");
    /* a power-on reads it back */
    {                                                    /* (the settings as the flash gives them back at power-on) */
        uint8_t keep = favorites.factory[8][31];
        song.g[G_CLOCK] = 0; cos_pc_bars = 1; cos_st_boot = 0;
        ui_power_on(); cz.flip = 1; favorites.factory[8][31] = keep; frame16();
    }
    CHECK(song.g[G_CLOCK] == 2 && cos_pc_bars == 4u, "after a power-on: CLOCK TRS, PAT LEN 4 (%d %u)", song.g[G_CLOCK], cos_pc_bars);
    cos_syntakt(CC_STON, 0);
    CHECK(song.g[G_CLOCK] == 0 && !cos_pc_ch && !(favorites.factory[8][31] & 1u), "SYNTAKT OFF: the own clock, no patterns");
    {
        uint8_t keep = favorites.factory[8][31];
        song.g[G_CLOCK] = 0; cos_st_boot = 0; ui_power_on(); cz.flip = 1; favorites.factory[8][31] = keep; frame16();
    }
    CHECK(song.g[G_CLOCK] == 0, "after a power-on: still the own clock");
    /* the page draws (on and off) */
    frame16();
    cos_syntakt(CC_STON, 1); cos_rx_ms = fm1_ms | 1u; cos_pc_last = 19; frame16();
    CHECK(1, "the page drawn, on and off");
    song.g[G_CLOCK] = 0;                                 /* (the bars below on the FM-1's own clock) */
    cos_syntakt(CC_STLEN, 2);
    /* the Syntakt's pattern changes: at the end of its pattern (PAT LEN 4) */
    if (song_cur) (void)song_select(0);
    steps_at(60); project_save(0);
    steps_at(70); project_save(1);
    steps_at(80); project_save(2);
    (void)section_jump(0);
    transport_req = 1; play_n(256);
    bar = chain_bar();
    play_n(bar);                                        /* (bar 2) */
    midi_event(0xC0u, 14u, 1u, 0);                       /* (its A02: section B) */
    midi_pc_poll();
    CHECK(live.req == 1 && cos_pc_wait, "PC 1 on channel 15: section B asked for, the Syntakt's");
    play_n(bar);
    CHECK(live.cur == 0 && live.req == 1, "the next bar: still A (the Syntakt's pattern runs on; bar %u)", chain_nbar);
    play_n(bar * 2u);
    CHECK(live.cur == 1 && live.req < 0 && !cos_pc_wait, "bar 5, the end of its 4-bar pattern: B (%d, bars %u)", live.cur, chain_nbar);
    /* one of the FM-1's own: the next bar, as before */
    (void)section_jump(2);
    play_n(bar);
    CHECK(live.cur == 2, "a section asked for here: the next bar (%d)", live.cur);
    /* PAT LEN 1: the next bar */
    cos_syntakt(CC_STLEN, 0);
    midi_event(0xC0u, 14u, 4u, 0); midi_pc_poll();
    play_n(bar);
    CHECK(live.cur == 0, "PAT LEN 1 BAR: its A05 (A) on the next bar (%d)", live.cur);
    cos_syntakt(CC_STON, 0);
    printf(fails ? "syntakt: %d FAILED\n" : "syntakt: all ok\n", fails);
    return fails != 0;
}
