/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound
 * (cursedsound) cursed-os 0.14.1, GG 2026-11-12: Ableton at 135 BPM, the FM-1 showing 134.7 .. 135 all the time. Clocks
 * at 135 BPM with USB's timing (each pulse's millisecond up to 1 off, rounded): the tempo the FM-1 takes holds within
 * 0.15 %, the BPM shown stays 135.0; a real change (to 128) followed at once.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/clock_jitter_test tests/clock_jitter_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); } else printf("ok: "); printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    uint32_t i, lo = ~0u, hi = 0, shown_lo = ~0u, shown_hi = 0, rng = 12345u;
    double t = 1000.0, per = 60000.0 / 135.0 / 24.0;
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    song.g[G_CLOCK] = 1;
    for (i = 0; i < 24u * 64u; i++) {                   /* (64 beats) */
        uint32_t ms;
        rng = rng * 1664525u + 1013904223u;
        ms = (uint32_t)(t + (double)(rng >> 31));        /* (late by up to a millisecond, floored) */
        midi_clock_pulse(ms);
        t += per;
        if (i > 24u * 16u && midi_beat_samples) {
            uint32_t s = cos_ext10();
            if (midi_beat_samples < lo) lo = midi_beat_samples;
            if (midi_beat_samples > hi) hi = midi_beat_samples;
            if (s < shown_lo) shown_lo = s;
            if (s > shown_hi) shown_hi = s;
        }
    }
    printf("  beat %u .. %u samples (135 BPM: %u), shown %u .. %u\n", lo, hi, (uint32_t)(44100.0 * 60.0 / 135.0), shown_lo, shown_hi);
    CHECK((hi - lo) * 1000u <= lo * 3u / 2u, "135 BPM with USB's jitter: the beat steady within 0.15 %% (%u .. %u)", lo, hi);
    CHECK(shown_lo == shown_hi && (shown_lo == 1350u || shown_lo == 1349u || shown_lo == 1351u), "shown: one value, 135.0 (%u)", shown_lo);
    CHECK(song.g[G_BPM] == 135, "BPM 135 (%d)", song.g[G_BPM]);
    per = 60000.0 / 128.0 / 24.0;
    for (i = 0; i < 24u * 3u; i++) { midi_clock_pulse((uint32_t)t); t += per; }
    CHECK(song.g[G_BPM] == 128, "a change to 128: followed within three beats (%d)", song.g[G_BPM]);
    printf(fails ? "clock_jitter: %d FAILED\n" : "clock_jitter: all ok\n", fails);
    return fails != 0;
}
