/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursedsound) the FX pad, every key: held from a playing mix, the sound is another within the first half second
 * (each effect heard at once: 0.7's FREEZE held nothing with the delay empty, the risers took a bar to be heard: found
 * on a device), and FREEZE with no delay sends at all.
 *   cc -O2 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/cos_pad_test tests/cos_pad_test.c -lm */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"


static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)


/* every random generator from its start: each case renders as the baseline unless the knob changed it */
static void rng_seed_all(void)
{
    dx_mot_rng = 0x9E3779B9u; ap_rng = 0x2545F491u; rng_state = 0x1234567u; mod_seed = 0x2545F491u; pfx_seed = 0x9E3779B9u;
}

/* the device's power-on, then a pattern on each track: the kit four on the floor with snare and hats, the synths a
 * line of six notes (the pad its chords) */
static void boot(void)
{
    uint32_t i, t;
    ui_power_on();
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    load_depth++; fm6_init(); power_on_parts(); load_depth--;
    song.sel = 0;
    song.master_q12 = 4096;                              /* (the master pot at unity) */
    cz.flip = 1;
    ui_polls();
    for (t = 0; t < trk_live; t++) {
        track_t *tr = &trk[t];
        tr->p[P_SLEN] = 16;
        tr->p[P_MUTE] = 0;
        for (i = 0; i < 16u; i++) {
            step_t *s = &tr->step[i];
            memset(s, 0, sizeof *s);
            s->vel = 100;
            if (drum_track(tr)) {                         /* (every lane somewhere, odd steps too: SWING) */
                s->hit = (uint8_t)((i % 4u == 0u ? 1u : 0u) | (i % 8u == 4u ? 2u : 0u) | (i % 2u == 0u ? 8u : 0u) |
                                   (i == 3u ? 4u : 0u) | (i == 7u ? 16u : 0u) | (i == 11u ? 32u : 0u) | (i == 13u ? 64u : 0u) |
                                   (i == 15u ? 128u : 0u) | (i == 9u ? 4u : 0u));
                s->time = s->hit ? ST_NOTE : ST_REST;
            } else if (i % 3u == 0u || i == 7u) {          /* (and an odd step: SWING) */
                static const uint8_t L[6] = {48, 51, 55, 58, 53, 46};
                s->note[0] = (uint8_t)(L[(i / 3u) % 6u] + (t == 3u ? 12u : 0u));
                if (t == 3u) { s->note[1] = (uint8_t)(s->note[0] + 3u); s->note[2] = (uint8_t)(s->note[0] + 7u); s->n = 3; }
                else s->n = 1;
                s->time = ST_NOTE;
                if (i == 3u || i == 9u) s->flags |= SF_ACCENT;   /* (ACID's ACCENT and SLIDE: something to act on) */
                if (i == 6u || i == 12u) s->flags |= SF_SLIDE;
                if (i == 7u) s->note[0] = 50;
            } else
                s->time = ST_REST;
        }
    }
    for (t = 0; t < trk_live; t++) { trk[t].p[P_DLY] = 50; trk[t].p[P_REV] = 50; }   /* (the FX pages: something in) */
    for (t = 0; t < 4u; t++) macro_v[t] = 64;            /* (the macros at their middle: a routing heard) */
    rng_seed_all();
}

/* the macros routed on every track, as a sound's MOD pages would (M1..M4 on MOD 1..4 to the filter, the level ..) */

static void blocks(uint32_t nb, int32_t *dst)
{
    uint32_t b, i;
    int32_t o[2u * CTL];
    for (b = 0; b < nb; b++) {
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) dst[b * CTL + i] = o[2u * i];
    }
}
#define PAD_S (44100u / 2u)                               /* (the first half second) */
static int32_t A[PAD_S + CTL], Bv[PAD_S + CTL];
/* the difference a key makes over the first half second, dB under the mix (0: all of it); -200 none */
static double pad_diff(uint32_t key, int sends)
{
    int fd[2], gd[2];
    pid_t ch;
    double r = -200;
    if (pipe(gd)) return -200;
    ch = fork();
    if (!ch) {
        uint32_t nb = PAD_S / CTL, t, i;
        size_t got = 0, want = (size_t)nb * CTL * 4u;
        pid_t g;
        double e = 0, eb = 0, d;
        boot();
        if (!sends) for (t = 0; t < trk_live; t++) trk[t].p[P_DLY] = 0;
        transport_req = 1; ui_input();
        blocks(PAD_S / CTL, A);                          /* (half a second in) */
        if (pipe(fd)) _exit(1);
        g = fork();
        if (!g) { blocks(nb, A); if (write(fd[1], A, want) < 0) {} _exit(0); }
        while (got < want) { ssize_t n = read(fd[0], (char *)A + got, want - got); if (n <= 0) break; got += (size_t)n; }
        waitpid(g, 0, 0);
        cfx_press(cfx_key(key), 1);
        blocks(nb, Bv);
        for (i = 0; i < nb * CTL; i++) { d = (double)Bv[i] - A[i]; e += d * d; eb += (double)A[i] * A[i]; }
        d = e > 0 ? 10 * log10(e / (eb + 1)) : -200;
        if (write(gd[1], &d, sizeof d) < 0) {}
        _exit(0);
    }
    if (read(gd[0], &r, sizeof r) != sizeof r) r = -200;
    waitpid(ch, 0, 0);
    close(gd[0]); close(gd[1]);
    return r;
}
int main(void)
{
    static const char *const WN[16] = {"RPT 1/8", "RPT 1/16", "RPT 1/32", "GATE 1/16", "GATE 1/8", "TAPE STOP", "GLITCH", "LPF",
                                       "HPF", "DRIVE", "CRUSH", "DLY THROW", "REV WASH", "FREEZE", "HP RISER", "NOISE RISER"};
    static const char *const BN[11] = {"MUTE 1", "MUTE 2", "MUTE 3", "MUTE 4", "MUTE 5", "MUTE 6", "KICK KILL", "LOW KILL",
                                       "DRUMS ONLY", "LOOP 3", "LOOP 7"};
    uint32_t k, n = 0;
    double d;
    boot();
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        int bl = key_black(k);
        if (bl && p < NTRK && p >= trk_live) continue;   /* (a track not playing: nothing to mute) */
        d = pad_diff(k, 1);
        n++;
        if (getenv("PAD_LIST")) printf("  %-11s %6.1f dB\n", bl ? BN[p] : WN[p], d);
        CHECK(d > -30.0, "FX pad %s %u (%s): %.1f dB of change in its first half second", bl ? "black" : "white", p + 1u,
              bl ? BN[p] : WN[p], d);
    }
    d = pad_diff(23, 0);
    CHECK(d > -30.0, "FREEZE with no delay sends: it holds what plays (%.1f dB)", d);
    printf("cos_pad_test: %u keys, %s (%d failures)\n", n + 1u, fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
