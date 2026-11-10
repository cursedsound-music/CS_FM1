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

static void sk_wdt(void) { fm1_ms++; host_ticks += 1000u; }   /* (a busy loop on the device, PANEL's setup waiting for
                                                                 * a press: time runs, as SysTick's would: it gives up) */
int main(int argc, char **argv)
{
    uint32_t minutes = argc > 1 ? (uint32_t)atoi(argv[1]) : 60u, f, frames, held = 0, notes = 0, pages = 0;
    static int32_t o[2u * CTL];
    sk_seed = argc > 2 ? (uint32_t)atoi(argv[2]) | 1u : 0x9E3779B9u;
    ui_power_on();
    host_wdt_hook = sk_wdt;
    cz.flip = 1;                                        /* (cursed-os's UI) */
    {                                                   /* (0.9.5) the sampler's memory: three sounds of random bytes
                                                         * (any byte is ADPCM), one short, one long, one across sectors */
        static smp_dir_t d;
        uint32_t k, s2 = SMP_DATA0;
        for (k = SMP_DATA0 * 4096u; k < SMP_SECS * 4096u; k++) smp_ram[k] = (uint8_t)rnd(256u);
        memset(&d, 0, sizeof d);
        for (k = 0; k < 3u; k++) {
            static const uint32_t LEN[3] = {300u, 40000u, 90000u};
            uint32_t nb = (LEN[k] + SMP_BLK - 1u) / SMP_BLK * SMP_BLKB, ns = (nb + 4095u) / 4096u, j;
            memcpy(d.e[k * 5u].name, "SOAKTEST", 8);
            d.e[k * 5u].len = LEN[k]; d.e[k * 5u].rate = (uint16_t)(k ? 22050u : 8000u); d.e[k * 5u].nsec = (uint8_t)ns;
            for (j = 0; j < ns; j++) d.e[k * 5u].sec[j] = (uint8_t)(s2++);
            d.e[k * 5u].valid = 1;
        }
        d.magic = SMP_MAGIC; d.n = SMP_NSLOT; d.seq = 1; d.crc = smp_crc(d.e, sizeof d.e);
        memcpy(smp_ram, &d, sizeof d);
        smp_boot();
    }
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
        } else if (r >= 223u && r < 233u) {           /* (0.9.5) the phone remote: a channel-16 CC, now and then a frame
                                                         * of random bytes (a TEXT, a sampler's, garbage) */
            rmt_cc_put(rnd(128u), rnd(128u));
            if (r == 223u && !rmt_fready) {
                uint32_t k, n = 3u + rnd(300u);
                rmt_f[0] = 0x7Du; rmt_f[1] = 0x63u;
                for (k = 2; k < n; k++) rmt_f[k] = (uint8_t)rnd(128u);
                if (rnd(2u)) rmt_f[2] = (uint8_t)(rnd(2u) ? 0x01u : 0x10u + rnd(4u));
                rmt_fn = (uint16_t)n;
                rmt_fready = 1;
            }
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
