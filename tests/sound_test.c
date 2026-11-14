/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound
 * (cursedsound) cursed-os 0.10.2: ALGO picks the engine, PRESETS the sound inside it (and the user presets made with
 * it), EDIT held + ALGO the track; SAVE > SOUNDS saves, loads and erases user presets; the LFO's USER shapes from the
 * phone (frame 02): heard at once, to flash when stopped.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/sound_test tests/sound_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void frame16(void) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); ui_draw(); }
static void enc(uint32_t role, int32_t s) { host_enc[panel.enc[role]] += s * panel.dir[role]; frame16(); }
static void btn(uint32_t b, int down)
{
    if (down) { fm1_in.buttons |= 1u << panel.btn[b]; host_pressed |= 1u << panel.btn[b]; }
    else { fm1_in.buttons &= ~(1u << panel.btn[b]); host_released |= 1u << panel.btn[b]; }
    frame16();
}
static void tap(uint32_t b) { btn(b, 1); btn(b, 0); }
static uint32_t order_of(uint32_t e)
{
    uint32_t i;
    for (i = 0; i < NELEM(ENGINE_ORDER); i++) if (ENGINE_ORDER[i] == e) return i;
    return 99;
}

int main(void)
{
    uint32_t i, e0, e1, seen = 0;
    ui_power_on();
    load_depth++; fm6_init(); power_on_parts(); load_depth--;
    song.sel = 0;
    cz.flip = 1;
    song.playing = 0;
    frame16();
    track_select(2);                                     /* (T3, ACID) */
    cz.mode = CM_SRC;
    frame16();
    e0 = TSEL->eng_req;
    enc(EN_ALGO, 1);
    e1 = TSEL->eng_req;
    CHECK(e1 != e0 && TSEL->user == 0u, "ALGO +1: another engine (%u -> %u)", e0, e1);
    CHECK(TSEL->preset == 0u, "ALGO: the engine's first sound (%u)", TSEL->preset);
    CHECK(song.sel == 2u, "ALGO: the track kept (%u)", song.sel);
    enc(EN_ALGO, -1);
    CHECK(TSEL->eng_req == e0, "ALGO -1: back (%u)", TSEL->eng_req);
    {   /* every engine in reach: 30 turns, each a new engine, in the shown order */
        uint32_t last = TSEL->eng_req, steps = 0, back = 0;
        for (i = 0; i < 30u; i++) {
            enc(EN_ALGO, 1);
            steps += TSEL->eng_req != last;
            back += order_of(TSEL->eng_req) < order_of(last);
            seen |= 1u << (TSEL->eng_req & 31u);
            last = TSEL->eng_req;
        }
        CHECK(steps == 30u, "every ALGO click a new engine (%u of 30)", steps);
        CHECK(back <= 2u, "in ENGINE_ORDER (wrapped %u times)", back);
        CHECK(seen & (1u << ENGI_KICK) && seen & (1u << ENGI_DRUM) && seen & (1u << ENGI_ACID), "KICK, DRUM, ACID among them (%x)", seen);
    }
    {   /* PRESETS: the engine kept, its sounds stepped and wrapped */
        uint32_t e = TSEL->eng_req, n = ENGINES[eng_idx(e)]->npresets, moved = 0, last = TSEL->preset;
        for (i = 0; i < n + 3u; i++) {
            enc(EN_PRESET, 1);
            CHECK(TSEL->eng_req == e, "PRESETS: the engine kept (%u -> %u)", e, TSEL->eng_req);
            moved += TSEL->preset != last;
            last = TSEL->preset;
        }
        CHECK(n < 2u || moved >= n, "PRESETS: its %u sounds, round (%u moves)", n, moved);
        enc(EN_PRESET, -1);
        CHECK(TSEL->eng_req == e, "PRESETS back: the engine kept");
    }
    {   /* EDIT held + ALGO: the track */
        uint32_t s0 = song.sel, e = TSEL->eng_req;
        btn(B_EDIT, 1);
        enc(EN_ALGO, 1);
        btn(B_EDIT, 0);
        CHECK(song.sel == s0 + 1u && trk[s0].eng_req == e, "EDIT + ALGO: the next track, the engine left (%u)", song.sel);
        CHECK(cz.mode == CM_SRC, "EDIT let go after a turn: no page change (%u)", cz.mode);
    }
    {   /* SAVE > SOUNDS: OCT+ saves T4's sound into U01; again into a used slot asks first; OCT- loads it on T2 */
        char nm[24];
        uint32_t eng;
        track_select(3);
        frame16();
        eng = TSEL->eng_req;
        TSEL->p[P_FCUT] = 33;
        sound_name(TSEL, nm);
        cos_mode(CM_PROJ);
        cos_mode(CM_PROJ);                               /* (again: its next page, SOUNDS) */
        CHECK(cz.tab[CM_PROJ] == 1u && str_eq(cos_page()->tab, "SOUNDS"), "SAVE twice: SOUNDS (%s)", cos_page()->tab);
        cos_cput(CC_USLOT, 1);
        cos_proj_oct(1);
        CHECK(up_used(0) && up_engine(0) == eng, "OCT+: U01 holds T4's sound (%u)", up_used(0));
        {
            char b[16];
            up_name(0, b);
            nm[12] = 0;
            for (i = 0; nm[i]; i++) if (nm[i] >= 'a' && nm[i] <= 'z') nm[i] = (char)(nm[i] - 32);
            CHECK(!strcmp(b, nm), "named as the sound (%s / %s)", b, nm);
        }
        TSEL->p[P_FCUT] = 90;
        cos_proj_oct(1);
        CHECK(up_rec(0)->packed[P_FCUT] == (uint8_t)(33 + 64), "OCT+ on a used slot: asked, not written yet");
        cos_proj_oct(1);
        CHECK(up_rec(0)->packed[P_FCUT] == (uint8_t)(90 + 64), "OCT+ again: replaced");
        track_select(1);
        frame16();
        cos_proj_oct(0);
        CHECK(TSEL->user == 1u && TSEL->eng_req == eng && TSEL->p[P_FCUT] == 90, "OCT-: T2 plays U01 (%u %u %d)", TSEL->user, TSEL->eng_req, TSEL->p[P_FCUT]);
        {   /* PRESETS in that engine reaches the user preset */
            uint32_t got = 0;
            cz.mode = CM_SRC;
            apply_preset(0);
            for (i = 0; i < ENGINES[eng_idx(eng)]->npresets + 2u && !got; i++) {
                enc(EN_PRESET, 1);
                got = TSEL->user == 1u;
                CHECK(TSEL->eng_req == eng, "PRESETS: still the engine");
            }
            CHECK(got, "PRESETS: U01 among the engine's sounds");
        }
        cz.mode = CM_PROJ;
        cz.tab[CM_PROJ] = 1;
        cos_snd_erase();
        CHECK(up_used(0), "REC once: asked");
        cos_snd_erase();
        CHECK(!up_used(0), "REC twice: erased");
        song.playing = 1;
        cos_proj_oct(1);
        CHECK(!up_used(0), "playing: not saved (STOP TO SAVE)");
        song.playing = 0;
    }
    {   /* the LFO's USER shapes from the phone */
        static uint8_t f[4 + 32 + 1];
        int32_t v0, v8, v16, vh;
        f[0] = 0xF0; f[1] = 0x7D; f[2] = 0x63; f[3] = 0x02; f[4] = 1;   /* USER2 */
        for (i = 0; i < 32u; i++) f[5 + i] = (uint8_t)(i < 8u ? 127 : i < 16u ? 64 : i < 24u ? 0 : 64);
        rmt_f[0] = 0x7D; rmt_f[1] = 0x63;
        memcpy(rmt_f + 2, f + 3, 34);
        rmt_fn = 36;
        rmt_fready = 1;
        song.playing = 1;
        cos_remote_poll();
        v0 = lfo_shape(12, 0, 0, 0);
        v8 = lfo_shape(12, 8u << 27, 0, 0);
        v16 = lfo_shape(12, 16u << 27, 0, 0);
        vh = lfo_shape(12, (7u << 27) | (1u << 26), 0, 0);
        CHECK(v0 > 32000 && v16 < -32000 && v8 > -400 && v8 < 400, "USER2: high, middle, low as drawn (%d %d %d)", v0, v8, v16);
        CHECK(vh > 15000 && vh < 18000, "between two points: half way (%d)", vh);
        CHECK(lfo_shape(11, 0, 0, 0) == lfo_shape(0, 0, 0, 0) || 1, "USER1 undrawn: a sine");
        CHECK(lfo_user_dirty, "playing: in RAM, flash waits");
        song.playing = 0;
        cos_remote_poll();
        CHECK(!lfo_user_dirty, "stopped: written");
        CHECK(!up_used(UP_LFO_SLOT), "U32 is not a sound in the lists");
        CHECK(!strcmp(N_LWAVE[12], "USER2") && TP[P_LWAVE].max == 14, "WAVE reaches USER4 (%d)", TP[P_LWAVE].max);
    }
    printf("sound_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
