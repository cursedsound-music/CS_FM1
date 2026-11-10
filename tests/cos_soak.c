/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) cursed-os soak: hours of a random player on every button, key and knob, the sound rendered, the screen
 * drawn, long pauses (the IDLE screen), under the address and undefined-behaviour sanitizers (an out-of-bounds read
 * or write anywhere stops it with the place). After GG's crash (0.8.8, an hour of a jam: a bus error in the UI's draw).
 *   cc -O1 -g -w -fsanitize=address,undefined -fno-sanitize-recover=all -Ibuild/gen -Ifirmware/src -Itests \
 *      -o build/t/cos_soak tests/cos_soak.c -lm && build/t/cos_soak [minutes] [seed] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t sk_seed;
static uint32_t rnd(uint32_t n) { sk_seed ^= sk_seed << 13; sk_seed ^= sk_seed >> 17; sk_seed ^= sk_seed << 5; return n ? sk_seed % n : 0u; }

int main(int argc, char **argv)
{
    uint32_t minutes = argc > 1 ? (uint32_t)atoi(argv[1]) : 60u, f, frames, held = 0, notes = 0, pages = 0;
    static int32_t o[2u * CTL];
    sk_seed = argc > 2 ? (uint32_t)atoi(argv[2]) | 1u : 0x9E3779B9u;
    ui_power_on();
    cz.flip = 1;                                        /* (cursed-os's UI) */
    frames = minutes * 60u * 1000u / 16u;
    for (f = 0; f < frames; f++) {
        uint32_t r = rnd(1000u), b;
        if (r < 30u) {                                  /* a button: a tap, or held over a few frames (combos) */
            b = rnd(NB);
            if (b == B_SAVE && rnd(4u)) b = B_HOME;     /* (fewer saves: each one a flash write) */
            if (held & (1u << b)) { fm1_in.buttons &= ~(1u << panel.btn[b]); host_released |= 1u << panel.btn[b]; held &= ~(1u << b); }
            else { fm1_in.buttons |= 1u << panel.btn[b]; host_pressed |= 1u << panel.btn[b]; held |= 1u << b; }
        } else if (r < 40u && held) {                   /* let everything go */
            uint32_t k;
            for (k = 0; k < NB; k++) if (held & (1u << k)) { fm1_in.buttons &= ~(1u << panel.btn[k]); host_released |= 1u << panel.btn[k]; }
            held = 0;
        } else if (r < 120u) {                          /* a key down or up (chords too) */
            uint32_t k = rnd(27u);
            notes ^= 1u << k;
            if (notes & (1u << k)) host_notes |= 1u << k;
            if (rnd(5u) == 0u) notes = 0;
            fm1_in.notes = notes;
        } else if (r < 220u) {                          /* a knob: KNOB 1..4, SELECT, ALGO, PRESETS; bursts */
            uint32_t role = rnd(10u) < 7u ? EN_K1 + rnd(4u) : rnd(3u);
            int32_t s = (int32_t)rnd(rnd(8u) ? 3u : 40u) + 1;
            host_enc[panel.enc[role]] += (rnd(2u) ? s : -s) * panel.dir[role];
        } else if (r < 222u) {
            transport_req = song.playing ? 2u : 1u;
        } else if (r == 222u && rnd(20u) == 0u) {       /* a long pause: hands off, the IDLE screen comes up */
            uint32_t p = 60000u + rnd(240000u), t;
            fm1_in.buttons = fm1_in.notes = 0; held = notes = 0;
            for (t = 0; t < p; t += 16u) {
                host_ticks += 16000u; fm1_ms += 16u;
                ui_input();
                if (song.playing) { uint32_t q; for (q = 0; q < 11u; q++) mix_block(o, CTL); }
                ui_draw();
            }
            pages++;
        }
        host_ticks += 16000u + rnd(3u) * 4000u;
        fm1_ms += rnd(50u) ? 16u : 1u + rnd(120u);
        ui_input();
        {                                               /* ~16 ms of sound (11 blocks of 64) */
            uint32_t q;
            for (q = 0; q < 11u; q++) mix_block(o, CTL);
        }
        ui_draw();
        if (f % (frames / 20u + 1u) == 0u) { printf("  %u%%  mode %u pg %u playing %u\n", f * 100u / frames, cz.mode, cos_page() ? 1u : 0u, song.playing); fflush(stdout); }
    }
    printf("cos_soak: %u minutes, %u idle pauses: ok\n", minutes, pages);
    return 0;
}
