/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursedsound) cursed-os's knobs, every one on every page of every mode, on each kind of track (the kit, ACID, the
 * bass, the pad, VOICE):
 *   1. right is more: a detent to the right moves the value up (CHANCE's F_PCTINV and an ordered list: up as shown),
 *      and the dial's pointer with it; a detent left brings it back; at the top it stays
 *   2. at power-on each one of cursed-os's own knobs (CK_COS) and the globals (CK_GLO) shows its default, and every
 *      value is within its range
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/cos_knob_test tests/cos_knob_test.c -lm */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails, checked, at_top;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* where the dial shows v: what cos_dial draws (the pointer's place, lo .. hi) */
static int32_t shown_at(const param_desc_t *d, int32_t v) { return cos_dial_at(d, v); }

static void page_name(char *b, uint32_t m, const cpg_t *p, uint32_t k)
{
    snprintf(b, 64, "%s > %s knob %u (T%u %s)", CM_NAME[m], p->tab, k + 1u, song.sel + 1u, trk_engp(TSEL)->name);
}

/* every knob of the page now: right, then left */
static void turn_all(uint32_t m)
{
    const cpg_t *pp;
    uint32_t n, t, k;
    cos_pages(m, &pp, &n);
    for (t = 0; t < n; t++) {
        for (k = 0; k < 4u; k++) {
            const cpg_t *p;
            const param_desc_t *d;
            int16_t *vp;
            int32_t v0, v1, v2, s0, s1;
            char nm[64];
            cz.mode = (uint8_t)m; cz.tab[m] = (uint8_t)t; cz.hot = 0;
            p = cos_page();
            d = cos_knob(p, k, &vp);
            if (!d || !vp || d->max == d->min) continue;
            page_name(nm, m, p, k);
            if (p->kind == CK_COS && (p->id[k] == CC_SLOT)) continue;   /* (SAVE's slot: white keys too, wraps none) */
#if !COS_SLIM
            if (p->kind == CK_COS && p->id[k] == CC_TRKS) continue;   /* (JIANT's whole UI: its own TRACKS 4 / 6 setting
                                                         * decides, ui.c rp_apply; the firmware and the preview are COS_SLIM) */
#endif
            v0 = *vp;
            CHECK(v0 >= d->min && v0 <= d->max, "%s: %d outside %d .. %d", nm, v0, d->min, d->max);
            host_ticks += 1000000u;                       /* (no acceleration: one detent a step) */
            cos_turn(k, 1);
            d = cos_knob(cos_page(), k, &vp);
            if (!d || !vp) continue;
            v1 = *vp;
            s0 = shown_at(d, v0); s1 = shown_at(d, v1);
            checked++;
            if (s0 >= d->max && v1 == v0) { at_top++; continue; }   /* (at the top: stays) */
            CHECK(cos_page()->tab == p->tab || !strcmp(cos_page()->tab, p->tab), "%s: the page changed under the knob (now %s)", nm, cos_page()->tab);
            CHECK(s1 > s0 || (p->id[k] == ID_BPM && v1 > v0), "%s (%s): right went %d -> %d (shown %d -> %d)", nm, d->label, v0, v1, s0, s1);
            host_ticks += 1000000u;
            cos_turn(k, -1);
            d = cos_knob(cos_page(), k, &vp);
            v2 = vp ? *vp : v0;
            if (d && d->fmt == F_INT && d->names) {       /* (a name a detent, onto its middle: the name back) */
                uint32_t n0, n2, kk;
                n0 = name_of(d, v0, &kk); n2 = name_of(d, v2, &kk);
                CHECK(n0 == n2, "%s (%s): left again, name %u not %u", nm, d->label, n2, n0);
            } else
                CHECK(v2 == v0, "%s (%s): left again %d -> %d, not back to %d", nm, d ? d->label : "?", v1, v2, v0);
        }
    }
}

/* what each knob shows at power-on: cursed-os's own and the globals their defaults (the music's: the sound's own) */
static void defaults_all(uint32_t m)
{
    const cpg_t *pp;
    uint32_t n, t, k;
    cos_pages(m, &pp, &n);
    for (t = 0; t < n; t++)
        for (k = 0; k < 4u; k++) {
            const cpg_t *p;
            const param_desc_t *d;
            int16_t *vp;
            char nm[64];
            cz.mode = (uint8_t)m; cz.tab[m] = (uint8_t)t;
            p = cos_page();
            d = cos_knob(p, k, &vp);
            if (!d || !vp) continue;
            page_name(nm, m, p, k);
            CHECK(*vp >= d->min && *vp <= d->max, "%s (%s) at power-on: %d outside %d .. %d", nm, d->label, *vp, d->min, d->max);
            /* the settings' and the performance's own (HOME, FX, MIX, GEN): their defaults. SRC and AMP show the sound (a
             * kit's KICK, an LFO's rate are the preset's), MIDI's OUT CH the track's own channel (T1 1 .. T6 6) */
            if (p->kind == CK_COS && p->id[k] == CC_OUTCH)
                CHECK(*vp == (int16_t)(song.sel + 1u), "%s (OUT CH) at power-on: %d, the track's own %u", nm, *vp, song.sel + 1u);
            else if ((p->kind == CK_COS || p->kind == CK_GLO) && m != CM_SRC && m != CM_AMP && p->id[k] != ID_BPM &&
                     p->id[k] != CC_SLOT && p->id[k] != CC_TRKS)
                CHECK(*vp == d->def, "%s (%s) at power-on: %d, its default %d", nm, d->label, *vp, d->def);
        }
}

int main(void)
{
    uint32_t m, tr, k;
    ui_power_on();                                        /* (the device's power-on, main.c felucca_init: the globals' defaults,
                                                         * TRK_DEF's tracks) */
    for (m = 0; m < G_COUNT; m++) song.g[m] = GP[m].def;
    load_depth++; fm6_init(); power_on_parts(); load_depth--;
    song.sel = 0;
    cz.flip = 1;                                          /* (COS_DEFAULT 0 in the UI tests: on) */
    ui_polls();
    for (tr = 0; tr < trk_live; tr++) {                   /* power-on: the defaults */
        track_select(tr);
        for (m = 0; m < CM_N; m++) defaults_all(m);
    }
    for (tr = 0; tr < trk_live; tr++) {                   /* each kind of track: every page's knobs */
        track_select(tr);
        for (m = 0; m < CM_N; m++) turn_all(m);
    }
    /* through the panel (panel_enc, PANEL_DEFAULT's directions: clockwise +): KNOB 1 up, SELECT the next page,
     * ALGORITHM the next track, PRESETS the next sound */
    {
        int16_t *vp;
        int32_t v0;
        uint32_t t0, s0, p0, tot;
        track_select(0);
        cz.mode = CM_FX; cz.tab[CM_FX] = 6;               /* (RUMBLE: LEVEL 0) */
        cos_knob(cos_page(), 0, &vp); v0 = *vp;
        host_ticks += 1000000u; host_enc[panel.enc[EN_K1]] = 1; cos_input();
        cos_knob(cos_page(), 0, &vp);
        CHECK(*vp > v0, "KNOB 1 clockwise through the panel: up (%d -> %d)", v0, *vp);
        t0 = cz.tab[CM_FX];
        host_enc[panel.enc[EN_SELECT]] = -1; cos_input();
        CHECK(cz.tab[CM_FX] + 1u == t0, "SELECT anticlockwise: the page before (%u -> %u)", t0, cz.tab[CM_FX]);
        host_enc[panel.enc[EN_SELECT]] = 1; cos_input();
        CHECK(cz.tab[CM_FX] == t0, "SELECT clockwise: the next page");
        s0 = song.sel;
        {   /* (0.10.2) ALGO: the engine; EDIT held + ALGO: the track */
            uint32_t e0 = TSEL->eng_req;
            host_enc[panel.enc[EN_ALGO]] = 1; cos_input();
            CHECK(song.sel == s0 && TSEL->eng_req != e0, "ALGORITHM clockwise: the next engine (%u -> %u)", e0, TSEL->eng_req);
            cz.ed_down = 1;
            host_enc[panel.enc[EN_ALGO]] = 1; cos_input();
            cz.ed_down = 0;
            CHECK(song.sel == s0 + 1u, "EDIT + ALGORITHM clockwise: the next track (%u -> %u)", s0, song.sel);
        }
        cz.mode = CM_SRC;
        p0 = preset_pos(&tot);
        host_enc[panel.enc[EN_PRESET]] = 1; cos_input();
        CHECK(preset_pos(&tot) == (p0 + 1u) % tot, "PRESETS clockwise: the next sound (%u -> %u of %u)", p0, preset_pos(&tot), tot);
        for (k = 0; k < NE; k++) CHECK(panel.dir[k] == 1, "encoder role %u: clockwise is + (PANEL_DEFAULT)", k);
    }
    /* the delay's TIME: right is longer all the way (1/32 .. 1/1, then 5 ms .. the longest), each value once but 2BAR
     * and 4BAR (past the line: cos.c COS_DT) */
    {
        static const char *const SYNCN[11] = {"1/32", "16T", "1/16", "8T", "1/16D", "1/8", "1/8D", "1/4", "1/4D", "1/2", "1/1"};
        int16_t *vp;
        const param_desc_t *d;
        uint32_t i, seen[128] = {0}, ok = 1, n = 0;
        cz.mode = CM_FX; cz.tab[CM_FX] = 3;               /* (DELAY: TIME knob 1) */
        d = cos_knob(cos_page(), 0, &vp);
        for (i = 0; i < 200u; i++) { host_ticks += 1000000u; cos_turn(0, -1); }
        CHECK(!strcmp(d->names[*vp - d->min], "1/32"), "all the way left: 1/32 (%s)", d->names[*vp - d->min]);
        for (i = 0; i < 11u; i++) {
            ok &= !strcmp(d->names[*vp - d->min], SYNCN[i]);
            seen[(*vp - d->min) & 127]++; n++;
            host_ticks += 1000000u; cos_turn(0, 1);
        }
        CHECK(ok, "the divisions shortest to longest, 1/32 .. 1/1");
        CHECK(!strcmp(d->names[*vp - d->min], "5ms"), "then the free times from 5 ms (%s)", d->names[*vp - d->min]);
        for (i = 0; i < 200u; i++) { int32_t a = *vp; seen[(a - d->min) & 127]++; n++; host_ticks += 1000000u; cos_turn(0, 1); if (*vp == a) break; }
        for (i = 0; i <= (uint32_t)(d->max - d->min); i++) ok &= seen[i] == (i == 8u || i == 9u ? 0u : 1u);
        CHECK(ok && n == (uint32_t)(d->max - d->min - 1), "every TIME once on the way (%u of %d)", n, d->max - d->min - 1);
        *vp = (int16_t)(d->min + 8);                      /* (a project's 2BAR: at 1/1's place, a turn on from there) */
        CHECK(cos_dial_at(d, *vp) == cos_dial_at(d, d->min + 7), "2BAR from a project: the pointer at 1/1's place");
        host_ticks += 1000000u; cos_turn(0, 1);
        CHECK(!strcmp(d->names[*vp - d->min], "5ms"), "2BAR, a turn right: 5ms (%s)", d->names[*vp - d->min]);
        *vp = (int16_t)(d->min + 8);
        host_ticks += 1000000u; cos_turn(0, -1);
        CHECK(!strcmp(d->names[*vp - d->min], "1/2"), "2BAR, a turn left: 1/2 (%s)", d->names[*vp - d->min]);
        *vp = (int16_t)d->def;
    }
    printf("cos_knob_test: %d knobs turned (%d at their top), %s (%d failures)\n", checked, at_top, fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
