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

/* (cursed-os 0.19.0, GG: "a true extra voice for syntakt ... sounds that we cant do on syntakt") GLO > SYNTAKT 2:
 *   the VOICE SET: every sound found by name (engines.c), each once;
 *   SYNTAKT ON with SYNTAKT 2 never set: PROGRAMS VOICES, FOLLOW, HOLD, TOUCH LOW; set by hand: left as it is;
 *   PROGRAMS VOICES / ENGINE / LIST: what a PROGRAM CHANGE on a track's channel loads;
 *   the note after a PROGRAM CHANGE waits for the sound (the new one plays, never the old), at most ~100 ms; its
 *   note-off before then: it never sounds;
 *   FOLLOW: a MIDI note on a track's channel quiets its own steps until STOP (OFF: both play);
 *   HOLD: a track's MIDI-held chord keeps its voices while another part wants more than the budget;
 *   TOUCH: velocity and aftertouch open the brightness (the block's value; the stored one back after it), OFF nothing */
static uint32_t sounding(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].stage != 4u;
    return n;
}
static int has_note(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].stage != 4u && t->v[i].note == note) return 1;
    return 0;
}
static void blk(void)                                   /* a block: the events, then the sound (the voices start) */
{
    static int32_t o[2u * CTL];
    events_block(CTL);
    song.master_q12 = 4096;
    mix_block(o, CTL);
}
static void play_r(uint32_t samples)
{
    uint32_t done;
    for (done = 0; done < samples; done += CTL) {
        blk();
        if (!(done & 1023u)) song_poll();
    }
    song_poll();
}
static void syntakt2(void)
{
    uint32_t i, j, k, ok = 1;
    track_t *t3 = &trk[2], *t5 = &trk[4];
    for (i = 0; i < NVOICE_SET; i++) {
        uint32_t e = voice_set_at(i, &k);
        if (e >= NENGINES) { ok = 0; printf("  not found: %s\n", VOICE_SET[i].name); }
        for (j = 0; j < i; j++) ok &= !(VOICE_SET[j].e == VOICE_SET[i].e && str_eq(VOICE_SET[j].name, VOICE_SET[i].name));
    }
    CHECK(ok && NVOICE_SET == 32u, "the VOICE SET: %u sounds, each found by name, each once", (unsigned)NVOICE_SET);
    favorites.factory[8][20] = 0; cos_st2 = 0;
    cos_syntakt(CC_STON, 1);
    CHECK(cos_st2 == (1u | ST2_FOLLOW | ST2_HOLD | 1u << 4) && (favorites.factory[8][20] & 0x80u) == 0u,
          "SYNTAKT ON, SYNTAKT 2 never set: VOICES, FOLLOW, HOLD, TOUCH LOW (%02x)", cos_st2);
    cos_syntakt2(CC_STTCH, 0); cos_syntakt2(CC_STFOL, 0);
    cos_syntakt(CC_STON, 0); cos_syntakt(CC_STON, 1);
    CHECK(cos_st2 == (1u | ST2_HOLD) && (favorites.factory[8][20] & 0x80u), "set by hand: SYNTAKT ON leaves it (%02x)", cos_st2);
    {                                                    /* (a power-on reads it back) */
        uint8_t keep = favorites.factory[8][20];
        cos_st2 = 0; ui_power_on(); cz.flip = 1; favorites.factory[8][20] = keep; frame16();
        CHECK(cos_st2 == (1u | ST2_HOLD), "after a power-on: SYNTAKT 2 as set (%02x)", cos_st2);
    }
    {                                                    /* the page and its knobs draw */
        const cpg_t *p;
        uint32_t n;
        cos_pages(CM_MIX, &p, &n);
        for (i = 0; i < n && !str_eq(p[i].tab, "SYNTAKT 2"); i++) {}
        CHECK(i < n && p[i].id[0] == CC_STPC && cos_ess(CM_MIX, p, i, n), "GLO > SYNTAKT 2: an everyday page");
        cz.mode = CM_MIX; cz.tab[CM_MIX] = (uint8_t)i; frame16(); frame16();
    }
    seq_stop(); song.g[G_CLOCK] = 0;
    /* PROGRAMS VOICES: PC 9 on channel 3 (T3): KALIMBA; the note right after it waits, then plays it */
    cos_syntakt2(CC_STPC, 1);
    midi_event(0xC0u, 2u, 9u, 0);
    midi_event(0x90u, 2u, 64u, 100u);
    blk();
    CHECK(!sounding(t3) && midi_park_n == 1u, "VOICES: the note after PC waits for the sound (%u parked)", midi_park_n);
    midi_pc_poll();
    for (i = 0; i < 16u; i++) blk();
    CHECK(t3->eng_req == ENGI_ADD && str_eq(ENGINES[ENGI_ADD]->presets[t3->preset].name, "KALIMBA") && has_note(t3, 64),
          "VOICES: PC 9 loads KALIMBA, then the note plays it");
    midi_event(0x80u, 2u, 64u, 0); blk();
    /* its note-off before the sound is in: never sounds */
    midi_event(0xC0u, 2u, 12u, 0); midi_event(0x90u, 2u, 65u, 100u); midi_event(0x80u, 2u, 65u, 0);
    midi_pc_poll(); blk();
    CHECK(!has_note(t3, 65) && !midi_park_n && str_eq(ENGINES[t3->eng_req]->presets[t3->preset].name, "NYLON"),
          "a note let go before its sound came: never sounds (PC 12: NYLON)");
    /* the UI stuck: played after ~100 ms anyway */
    midi_event(0xC0u, 2u, 0u, 0); midi_event(0x90u, 2u, 66u, 100u);
    for (i = 0; i < PARK_MAX + 2u; i++) blk();
    CHECK(has_note(t3, 66) && !midi_park_n, "no sound load for ~100 ms: the note plays anyway");
    midi_pc_poll(); midi_event(0x80u, 2u, 66u, 0); blk();
    CHECK(str_eq(ENGINES[t3->eng_req]->presets[t3->preset].name, "TINE EP"), "the late load still lands (PC 0: TINE EP)");
    midi_event(0xC0u, 2u, 40u, 0); midi_pc_poll();
    CHECK(str_eq(ENGINES[t3->eng_req]->presets[t3->preset].name, "TINE EP"), "past the set (PC 40): nothing loaded");
    /* PROGRAMS ENGINE: the track's engine's own */
    cos_syntakt2(CC_STPC, 2);
    midi_event(0xC0u, 2u, 3u, 0); midi_pc_poll(); blk();
    CHECK(t3->eng_req == ENGI_FM6 && t3->preset == preset_nth(ENGINES[ENGI_FM6], 3u), "ENGINE: PC 3, FM6's 4th sound");
    /* PROGRAMS LIST: as ever (0.9.8's numbers) */
    cos_syntakt2(CC_STPC, 0);
    midi_event(0xC0u, 2u, 0u, 0); midi_pc_poll(); blk();
    CHECK(t3->eng_req == ENGI_ANALOG && t3->preset == 0u, "LIST: PC 0, the list's first (ANALOG's)");
    /* FOLLOW: a step pattern on T3; a MIDI note quiets it until STOP */
    cos_syntakt2(CC_STFOL, 1);
    track_defaults_steps(t3); t3->p[P_SLEN] = 16; t3->p[P_MUTE] = 0; t3->p[P_VOICE] = V_POLY;
    midi_live = 0;                                       /* (the notes above, stopped, made T3 the MIDI's already) */
    for (i = 0; i < 16u; i++) t3->step[i] = (step_t){{50, 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
    transport_req = 1; play_r(FS / 4u);
    CHECK(has_note(t3, 50), "FOLLOW: before any MIDI, T3's own steps play");
    midi_event(0x90u, 2u, 70u, 100u); play_r(FS / 2u);
    CHECK(has_note(t3, 70) && !has_note(t3, 50) && (midi_live & 4u), "FOLLOW: a MIDI note on its channel, its own steps quiet");
    midi_event(0x80u, 2u, 70u, 0); play_r(FS / 2u);
    CHECK(!has_note(t3, 50), "FOLLOW: still quiet between the MIDI's notes");
    transport_req = 2; play_r(CTL * 4u); transport_req = 1; play_r(FS / 4u);
    CHECK(has_note(t3, 50) && !(midi_live & 4u), "FOLLOW: after STOP its own steps again");
    cos_syntakt2(CC_STFOL, 0);
    midi_event(0x90u, 2u, 70u, 100u); play_r(FS / 4u);
    CHECK(has_note(t3, 70) && has_note(t3, 50), "FOLLOW OFF: both play");
    midi_event(0x80u, 2u, 70u, 0); transport_req = 2; play_r(CTL * 4u);
    for (i = 0; i < 16u; i++) t3->step[i].time = 0;
    track_defaults_steps(t3);
    /* HOLD: T5's MIDI chord keeps its voices while T3 wants every voice */
    for (k = 0; k < 2u; k++) {
        uint32_t held;
        cos_syntakt2(CC_STHLD, (int32_t)!k);
        for (i = 0; i < NTRK; i++) midi_silence_track(i);
        for (i = 0; i < 64u; i++) blk();
        t5->p[P_VOICE] = V_POLY; t3->p[P_VOICE] = V_POLY;
        for (i = 0; i < 4u; i++) midi_event(0x90u, 4u, 60u + 3u * i, 100u);
        blk();
        for (i = 0; i < 8u; i++) { trk_note_on(t3, 40u + i, 100u); blk(); }
        for (i = 0; i < 8u; i++) blk();
        held = sounding(t5);
        CHECK(k ? held < 4u : held == 4u, "HOLD %s: T5's 4-note MIDI chord, T3 plays 8 notes: %u of T5's sound", k ? "OFF" : "ON", held);
        for (i = 0; i < 8u; i++) trk_note_off(t3, 40u + i);
        for (i = 0; i < 4u; i++) midi_event(0x80u, 4u, 60u + 3u * i, 0);
        for (i = 0; i < 64u; i++) blk();
    }
    /* TOUCH: the brightness for the block, the stored value back after it */
    {
        uint32_t id;
        int16_t keep;
        cos_syntakt2(CC_STTCH, 3);
        midi_event(0x90u, 2u, 60u, 127u); blk();
        id = touch_id(t3);
        keep = t3->p[id];
        CHECK(id && mod_begin(t3) && t3->p[id] > keep, "TOUCH HIGH, velocity 127: the brightness up (%d -> %d)", keep, t3->p[id]);
        mod_end(t3);
        CHECK(t3->p[id] == keep, "TOUCH: the stored value back after the block");
        cos_syntakt2(CC_STTCH, 0);
        CHECK(!touch_id(t3) && !mod_begin(t3), "TOUCH OFF: nothing added");
        midi_event(0x80u, 2u, 60u, 0); blk();
        cos_syntakt2(CC_STTCH, 3);
        midi_live = 0;
        CHECK(!touch_id(t3), "TOUCH: a track the MIDI does not play, nothing");
        cos_syntakt2(CC_STTCH, 0);
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
    syntakt2();
    printf(fails ? "syntakt: %d FAILED\n" : "syntakt: all ok\n", fails);
    return fails != 0;
}
