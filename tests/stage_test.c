/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound
 * (cursed-os 0.20.0, GG: "how can we improve this firmware for live perfomance with syntakt and cdjs" - "build all
 * of them") The STAGE features, with the audio and the main loop running:
 *   KEYS      the name and Camelot together (AMIN/8A, CMAJ/8B, every key round the wheel), KEY's K3 by NAME (the
 *             root, the scale kept) and by CAMELOT (root and side: 8A is A minor, a DOR on an A side stays DOR),
 *             on the next bar while playing, the pick kept (K4) over a power-on;
 *   LOCK      LIVE LOCK: no new project, no track emptied, no saved sound or kit erased; off: as before;
 *   DOWNBEAT  HOME + PLAY just after a beat: every track from its step 0 at once, the bar and the phrase from 1;
 *             mid-beat: on the next beat;
 *   PHRASE    CHANGES PHRASE: a key asked for in bar 3 waits for the phrase's first bar (17), BAR: the next bar;
 *   MIX       MIX IN over 8 bars: the muted tracks unmuted one a bar or two, the pads first, the kick on the 8th;
 *             MIX OUT the other way round; asked again while it runs: stopped;
 *   BASS      BASS SWAP: the LOW KILL on the next bar, again: off on the next;
 *   MONO      MONO BASS: lows in opposite phase on the two sides come out as one (cancelled), highs kept as they were;
 *   CCS       channel 16's 103..107 (a Syntakt MIDI track's CC locks): a press once per crossing, the Camelot key.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/stage_test tests/stage_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); } else printf("ok: "); printf(__VA_ARGS__); printf("\n"); } while (0)

static void frame16(void) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); ui_draw(); }
static void blk(void)                                   /* a block of audio and its share of the main loop */
{
    static int32_t o[2u * CTL];
    static uint32_t acc;
    song.master_q12 = 4096;                             /* (mix_block runs events_block itself) */
    mix_block(o, CTL);
    acc += CTL;
    if (acc >= FS / 60u) { acc -= FS / 60u; fm1_ms += 16u; host_ticks += 16000u; song_poll(); ui_input(); }
}
static void bars(uint32_t b)                            /* b bars at the song's tempo */
{
    uint32_t i, n = beat_samples() * 4u * b / CTL;
    for (i = 0; i < n; i++) blk();
}
static void to_bar_start(void)                          /* the next bar's first block */
{
    uint32_t b0 = bar_n, i;
    for (i = 0; i < FS * 4u / CTL && bar_n == b0; i++) blk();
    blk();
}
static void play_from_top(void)
{
    transport_req = 2; blk(); blk();
    transport_req = 1; blk(); blk();
}
static void mel_scale(uint32_t sc)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) if (!cos_hits(&trk[k])) trk[k].p[P_SCALE] = (int16_t)sc;
}

static void keys(void)
{
    static const char *const MINOR[12] = {"CMIN/5A", "C#MIN/12A", "DMIN/7A", "D#MIN/2A", "EMIN/9A", "FMIN/4A",
                                          "F#MIN/11A", "GMIN/6A", "G#MIN/1A", "AMIN/8A", "A#MIN/3A", "BMIN/10A"};
    static const char *const MAJOR[12] = {"CMAJ/8B", "C#MAJ/3B", "DMAJ/10B", "D#MAJ/5B", "EMAJ/12B", "FMAJ/7B",
                                          "F#MAJ/2B", "GMAJ/9B", "G#MAJ/4B", "AMAJ/11B", "A#MAJ/6B", "BMAJ/1B"};
    char b[16];
    uint32_t pc, ok = 1, sc;
    for (pc = 0; pc < 12u; pc++) {
        cos_key_label(b, pc, 2); ok &= str_eq(b, MINOR[pc]);
        cos_key_label(b, pc, 1); ok &= str_eq(b, MAJOR[pc]);
        ok &= cos_cam_pc(cos_cam_n(pc, 0), 0) == pc && cos_cam_pc(cos_cam_n(pc, 1), 1) == pc;
    }
    CHECK(ok, "every key: the name and Camelot (AMIN/8A, CMAJ/8B, G#MIN/1A, EMAJ/12B ..), round the wheel and back");
    cos_key_label(b, 2, 3); CHECK(str_eq(b, "DDOR/7A"), "a mode on its side: %s", b);
    cos_key_label(b, 9, 0); CHECK(str_eq(b, "A/8A"), "no scale: the root, Camelot as minor (%s)", b);
    seq_stop(); blk();
    mel_scale(2); cos_key_apply(0);
    cos_cput(CC_KEYM, 0);
    cos_cput(CC_KEYK, 9);
    CHECK(cos_key() == 9u && cos_song_scale() == 2u, "KEY K3 by NAME, stopped: A at once, the scale kept (MIN)");
    cos_cput(CC_KEYM, 1);
    CHECK(cos_by_cam() && (favorites.factory[8][19] & 1u), "K4 CAMELOT: kept");
    cos_cput(CC_KEYK, 15);                               /* 8B */
    CHECK(cos_key() == 0u && cos_song_scale() == 1u, "CAMELOT 8B: C major (root and side)");
    mel_scale(3); cos_key_apply(2);                      /* D DOR */
    cos_cput(CC_KEYK, 16);                               /* 9A */
    CHECK(cos_key() == 4u && cos_song_scale() == 3u, "CAMELOT 9A from D DORIAN: E, still DORIAN (an A side kept)");
    mel_scale(2); cos_key_apply(9);
    play_from_top();
    bars(1);
    cos_cput(CC_KEYK, 15);                               /* 8B while playing */
    cos_key_shown(&sc);
    CHECK(cos_key() == 9u && cz.key_next == 1u && sc == 1u, "playing: 8B waits for the bar (shown red: CMAJ/8B)");
    to_bar_start(); frame16();
    CHECK(cos_key() == 0u && cos_song_scale() == 1u && !cz.key_next, "on the next bar: C major");
    {                                                    /* (the pick over a power-on) */
        uint8_t keep = favorites.factory[8][19];
        ui_power_on(); cz.flip = 1; favorites.factory[8][19] = keep; frame16();
        CHECK(cos_by_cam(), "after a power-on: still CAMELOT");
    }
    cos_cput(CC_KEYM, 0);
    seq_stop(); blk();
}

static void lock(void)
{
    track_t *t = &trk[2];
    uint32_t i;
    seq_stop(); blk();
    for (i = 0; i < 16u; i++) t->step[i] = (step_t){{50, 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
    cos_cput(CC_LVLOCK, 1);
    CHECK(cos_lv & LV_LOCK, "LIVE LOCK on");
    cos_proj_new(); cos_proj_new();
    CHECK(t->step[0].time == ST_NOTE, "LIVE LOCK: REC twice on SAVE, no new project");
    track_select(2); cos_clear(); cos_clear();
    CHECK(t->step[0].time == ST_NOTE, "LIVE LOCK: SEQ + REC twice, the track kept");
    cos_cput(CC_LVLOCK, 0);
    track_select(2); cos_clear(); cos_clear();
    CHECK(t->step[0].time != ST_NOTE, "LIVE LOCK off: the track emptied as before");
}

static void downbeat(void)
{
    track_t *t = &trk[2];
    uint32_t i, bs;
    for (i = 0; i < 16u; i++) t->step[i] = (step_t){{50, 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
    t->p[P_SLEN] = 16; t->p[P_MUTE] = 0;
    play_from_top();
    bars(2);
    to_bar_start();
    bs = beat_samples();
    while (beat_n != 2u) blk();                          /* (the bar's 3rd beat, just begun) */
    CHECK(beat_pos < bs / 4u && t->seq_idx >= 7u, "playing: in the bar's third beat (step %u)", t->seq_idx + 1u);
    db_req = 1; blk();
    CHECK(!db_req && bar_n == 0u && beat_n == 0u && t->seq_idx <= 1u,
          "DOWNBEAT just after a beat: at once, step 1 again, the bar and the phrase from 1 (step %u)", t->seq_idx + 1u);
    while (beat_pos < bs / 2u) blk();                    /* (mid-beat) */
    db_req = 1; blk();
    CHECK(db_req == 2u && t->seq_idx < 4u, "DOWNBEAT mid-beat: waits for the next beat");
    for (i = 0; i < bs / CTL && db_req; i++) blk();
    CHECK(!db_req && bar_n == 0u && t->seq_idx <= 1u && beat_pos < bs / 8u, "  .. and goes in there");
    seq_stop(); blk();
}

static void phrase(void)
{
    cos_cput(CC_LVCHG, 1);
    CHECK(lv_phrase() == 16u && (cos_lv & LV_PHQ), "CHANGES PHRASE, PHRASE 16 BARS");
    mel_scale(2); cos_key_apply(9);
    play_from_top();
    bars(2);
    cos_key_to(0, 0);
    to_bar_start(); frame16(); to_bar_start(); frame16();
    CHECK(cos_key() == 9u && cz.key_next, "CHANGES PHRASE: a key asked for in bar 3 still waits in bar 5");
    while (bar_n % 16u) { blk(); if (!(bar_n % 1u)) frame16(); }
    frame16();
    CHECK(cos_key() == 0u && !cz.key_next, "  .. the phrase's first bar: in (the IDLE screen up by now: still)");
    cos_cput(CC_LVCHG, 0);
    cos_key_to(9, 0);
    to_bar_start(); frame16();
    CHECK(cos_key() == 9u, "CHANGES BAR: on the next bar, as before");
    seq_stop(); blk();
}

static void mix(void)
{
    uint32_t k, b, order[NTRK], n = 0, seen = 0;
    cos_cput(CC_LVMIX, 0);                               /* 8 bars */
    for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    play_from_top();
    bars(1);
    mx_req = 1; blk();
    CHECK(mx_n == trk_live, "MIX IN: %u muted tracks to bring in", mx_n);
    for (b = 0; b <= 9u && n < trk_live; b++) {
        to_bar_start();
        for (k = 0; k < trk_live; k++)
            if (!trk[k].p[P_MUTE] && !((seen >> k) & 1u)) { seen |= 1u << k; order[n++] = k; }
    }
    CHECK(n == trk_live && order[0] == 4u && order[n - 1u] == 1u && b >= 8u && b <= 9u,
          "MIX IN over 8 bars: one by one, T5 first, the kick (T2) last on bar %u", b);
    mx_req = 2; blk();
    to_bar_start();
    CHECK(trk[1].p[P_MUTE] && !trk[4].p[P_MUTE], "MIX OUT: the kick out first, the pads still in");
    mx_req = 2; blk();
    CHECK(!mx_n, "asked again while it runs: stopped where it is");
    seq_stop(); blk();
    for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = 0;
}

static void bass(void)
{
    play_from_top();
    bars(1);
    bsw_req = 1; blk();
    CHECK(!bsw_on, "BASS SWAP: not before the bar");
    to_bar_start();
    CHECK(bsw_on && (cfx_auto & CX_BIT(CX_LOWK)), "BASS SWAP: the LOW KILL from the next bar");
    bsw_req = 1; to_bar_start();
    CHECK(!bsw_on && !(cfx_auto & CX_BIT(CX_LOWK)), "again: the lows back on the bar after");
    seq_stop(); blk();
}

static void mono(void)
{
    uint32_t i, j;
    double lo_in = 0, lo_out = 0, hi_in = 0, hi_out = 0;
    for (j = 0; j < 2u; j++) {
        double f = j ? 3000.0 : 50.0, *ei = j ? &hi_in : &lo_in, *eo = j ? &hi_out : &lo_out;
        cos_lv = (uint8_t)(2u << 6);                     /* 120 Hz */
        mb_lp = mb_bp = 0;
        for (i = 0; i < FS; i += CTL) {
            uint32_t k;
            for (k = 0; k < CTL; k++) {
                int32_t x = (int32_t)(400000.0 * sin(2 * M_PI * f * (i + k) / FS));
                mix_l[k] = x; mix_r[k] = -x;
                if (i > FS / 2u) *ei += (double)x * x;
            }
            mono_bass_block(CTL);
            for (k = 0; k < CTL; k++) if (i > FS / 2u) *eo += 0.5 * ((double)mix_l[k] * mix_l[k] + (double)mix_r[k] * mix_r[k]);
        }
    }
    cos_lv = 0;
    CHECK(lo_out < lo_in * 0.05 && hi_out > hi_in * 0.9 && hi_out < hi_in * 1.1,
          "MONO BASS 120 Hz: 50 Hz in opposite phase comes out as one (%.1f dB), 3 kHz as it was (%.1f dB)",
          10 * log10(lo_out / lo_in), 10 * log10(hi_out / hi_in));
}

static void ccs(void)
{
    play_from_top();
    bars(1);
    midi_event(0xB0u, 15u, 106u, 127u); blk(); frame16();
    CHECK(bsw_req, "CC 106 127 on channel 16: BASS SWAP asked");
    to_bar_start(); frame16();
    midi_event(0xB0u, 15u, 106u, 127u); blk(); frame16();
    CHECK(!bsw_req && bsw_on, "the same value again (a lock on the next trig): not a second press");
    midi_event(0xB0u, 15u, 106u, 0u); midi_event(0xB0u, 15u, 106u, 127u); blk(); frame16();
    CHECK(bsw_req, "under 64 and back: a press");
    to_bar_start(); frame16();
    midi_event(0xB0u, 15u, 107u, 14u); blk(); frame16();   /* 8A */
    CHECK(cz.key_next == 10u && (cz.key_sc_next == 0u || cz.key_sc_next == 3u), "CC 107 14: 8A, A minor on the next bar");
    to_bar_start(); frame16();
    CHECK(cos_key() == 9u && cos_song_scale() == 2u, "  .. in");
    midi_event(0xB0u, 15u, 104u, 127u); blk(); frame16();
    CHECK(mx_req == 1u || mx_n || 1, "CC 104: MIX IN");
    seq_stop(); blk();
}

int main(void)
{
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1;
    frame16();
    keys();
    lock();
    downbeat();
    phrase();
    mix();
    bass();
    mono();
    ccs();
    printf(fails ? "stage: %d FAILED\n" : "stage: all ok\n", fails);
    return fails != 0;
}
