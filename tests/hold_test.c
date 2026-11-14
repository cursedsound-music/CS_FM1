/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound
 * (cursedsound) cursed-os 0.18.3, GG 2026-11-14: "I haven't found a way to make atmospheric long chords".
 *   GEN > DUB LENGTH past 100 %: 1/2, 1, 2, 4 BARS: a chord on each stretch's first step, tied through it; the
 *   sound held (no gap), not struck again. The quick starts' atmospheres: chords held a bar.
 *   KEY LATCH (SCL + OCT+): a tapped key's chord rings on after its release; another key alone: that one instead; the
 *   held key tapped again: off; LATCH off: off.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/hold_test tests/hold_test.c -lm */
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
static void btn(uint32_t b, int down)
{
    uint32_t m = 1u << panel.btn[b];
    if (down) { fm1_in.buttons |= m; host_pressed = m; } else { fm1_in.buttons &= ~m; host_released = m; }
    cos_input();
    host_pressed = host_released = 0;
}
static void keys(uint32_t m) { host_notes = m; fm1_in.notes = m; }
static int32_t o[2 * CTL];
/* the RMS of the left channel over ms milliseconds */
static double rms(uint32_t ms)
{
    uint32_t b, i, nb = ms * (FS / 1000u) / CTL;
    double e = 0;
    for (b = 0; b < nb; b++) {
        song.master_q12 = 4096; mix_block(o, CTL);
        for (i = 0; i < CTL; i++) e += (double)o[2 * i] * o[2 * i];
    }
    return sqrt(e / (nb * CTL));
}
static uint32_t voices(uint32_t k)
{
    uint32_t v, c = 0;
    for (v = 0; v < NVOICE; v++) c += trk[k].v[v].active && trk[k].v[v].gate;
    return c;
}

int main(void)
{
    uint32_t i, k, ties = 0, notes = 0;
    double lo = 1e30, hi = 0;
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1; song.g[G_BPM] = 120; frame16();
    for (k = 0; k < NTRK; k++) { trk[k].p[P_MUTE] = k != 4u; trk[k].p[P_REV] = trk[k].p[P_DLY] = 0; }
    /* the knob: 100 % then the holds */
    track_select(4);
    set_engine_of(&trk[4], ENGI_DUB); trk[4].engine = ENGI_DUB;
    for (k = 0; k < ENG_DUB.npresets && !str_eq(ENG_DUB.presets[k].name, "ATMOS DARK"); k++) {}
    apply_preset_to(&trk[4], k);
    cz.mode = CM_GEN; frame16();
    db_put(DK_BARS, 1); db_put(DK_MOVE, 1); db_put(DK_DENS, 100);
    db_put(DK_LEN, 19);
    CHECK(db_of(4)->len == 100u, "LENGTH 19: 100 %%");
    db_put(DK_LEN, 21);
    CHECK(db_of(4)->len == 102u && str_eq(DB_LENN[db_len_i(db_of(4)->len)], "1 BAR"), "LENGTH 21: 1 BAR");
    for (i = 0; i < 32u; i++) { notes += trk[4].step[i].time == ST_NOTE; ties += trk[4].step[i].time == ST_TIE; }
    CHECK(trk[4].p[P_SLEN] == 32 && notes == 2u && ties == 30u && trk[4].step[0].time == ST_NOTE && trk[4].step[16].time == ST_NOTE,
          "2 bars, 1 BAR: two chords, each tied through its bar (%u notes, %u ties)", notes, ties);
    CHECK(trk[4].step[0].note[0] != trk[4].step[16].note[0], "MOVE: the second another chord");
    db_put(DK_LEN, 23);
    for (i = 0, notes = 0; i < 32u; i++) notes += trk[4].step[i].time == ST_NOTE;
    CHECK(notes == 1u, "4 BARS on a 2-bar line: one chord, held through");
    /* held: playing, the sound never drops (after its swell) */
    db_put(DK_LEN, 21);
    transport_req = 1; frame16();
    (void)rms(1500);
    for (i = 0; i < 12u; i++) { double r = rms(250); if (r < lo) lo = r; if (r > hi) hi = r; }
    CHECK(lo > hi * 0.35 && lo > 500.0, "1 BAR chords held: no gap over 3 s (%.0f .. %.0f)", lo, hi);
    transport_req = 2; frame16();
    for (i = 0; i < 20u; i++) frame16();
    (void)rms(3000);
    /* the quick start's atmosphere: held a bar */
    cz.mode = CM_PROJ; cz.tab[CM_PROJ] = 3; frame16();
    cos_cput(CC_QSTY, QS_HYPNO); cos_cput(CC_QELM, 0); cos_cput(CC_QBAR, 1);
    cos_proj_oct(1); frame16();
    CHECK(db_of(4)->len == 102u && trk[4].step[1].time == ST_TIE, "HYPNOTIC: T5's atmosphere held a bar (len %u)", db_of(4)->len);
    /* KEY LATCH */
    for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = k != 4u;
    trk[4].engine = trk[4].eng_req;
    track_select(4);
    cz.mode = CM_SRC; frame16();
    btn(B_SCL, 1); btn(B_OCTUP, 1); btn(B_OCTUP, 0); btn(B_SCL, 0);
    CHECK(kb_latch && cz.mode == CM_SRC, "SCL + OCT+: KEY LATCH on (and no KEY page)");
    keys(1u << 5); rms(100); keys(0); rms(400);
    CHECK(voices(4) > 0u, "a key tapped: its chord rings on (%u voices)", voices(4));
    keys(1u << 9); rms(100); keys(0); rms(100);
    k = 0; for (i = 0; i < NVOICE; i++) k += trk[4].v[i].active && trk[4].v[i].gate && trk[4].v[i].note == kb_note[9];
    CHECK(kb_lmask == 1u << 9 && k > 0u, "another key alone: that one held instead");
    keys(1u << 9); rms(100); keys(0); rms(100);
    CHECK(!kb_lmask && !voices(4), "the held key again: off");
    keys(1u << 5); rms(100); keys(0); rms(100);
    btn(B_SCL, 1); btn(B_OCTUP, 1); btn(B_OCTUP, 0); btn(B_SCL, 0);
    rms(50);
    CHECK(!kb_latch && !kb_lmask && !voices(4), "LATCH off: the held chord off");
    keys(1u << 5); rms(100); keys(0); rms(100);
    CHECK(!voices(4), "LATCH off: a key as before (off at its release)");
    printf(fails ? "hold: %d FAILED\n" : "hold: all ok\n", fails);
    return fails != 0;
}
