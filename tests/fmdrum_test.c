/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) cursed-os 0.11.0: FMDRUM (engine 9, eng_fmdrum.c): reachable, every TYPE and preset sounding, bounded,
 * falling; the TRANSIENT layer on FMDRUM and KICK; DIST TUBE and RAT; an older project's engine 9 (PHYS) still ANALOG,
 * a new one's FMDRUM; FMDRUM's stars apart from KICK's and the crash log.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/fmdrum_test tests/fmdrum_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void solo(uint32_t k)
{
    uint32_t i;
    for (i = 0; i < NTRK; i++) trk[i].p[P_MUTE] = (int16_t)(i != k);
}
/* a hit on track k over 0.5 s: its energy in the first and the last 100 ms, its peak; the first 2048 samples kept */
static int32_t first[2048];
static double hit(uint32_t k, uint32_t note, double *late, int32_t *peak)
{
    static int32_t o[2 * CTL];
    uint32_t b, i, nb = FS / 2u / CTL;
    double e = 0, l = 0;
    int32_t pk = 0;
    for (b = 0; b < 40u; b++) mix_block(o, CTL);
    trk_note_on(&trk[k], note, 110);
    for (b = 0; b < nb; b++) {
        mix_block(o, CTL);
        if (b == 2u) trk_note_off(&trk[k], note);
        for (i = 0; i < CTL; i++) {
            int32_t s = o[2u * i], a = s < 0 ? -s : s;
            uint32_t at = b * CTL + i;
            if (at < FS / 10u) e += (double)s * s;
            if (at > FS * 4u / 10u) l += (double)s * s;
            if (at < 2048u) first[at] = s;
            if (a > pk) pk = a;
        }
    }
    if (late) *late = l;
    if (peak) *peak = pk;
    return e;
}
static double diff_first(const int32_t *a)
{
    double d = 0;
    uint32_t i;
    for (i = 0; i < 512u; i++) d += fabs((double)a[i] - first[i]);
    return d;
}
/* the energy of 100 ms after s seconds more */
static double quiet(uint32_t s)
{
    static int32_t o[2 * CTL];
    uint32_t b, i, nb = FS * s / CTL, nq = FS / 10u / CTL;
    double e = 0;
    for (b = 0; b < nb; b++) mix_block(o, CTL);
    for (b = 0; b < nq; b++) {
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++) e += (double)o[i] * o[i];
    }
    return e;
}

int main(void)
{
    uint32_t i, k;
    double e, l;
    int32_t pk;
    ui_power_on();
    load_depth++; fm6_init(); power_on_parts(); load_depth--;
    song.sel = 0;
    cz.flip = 1;
    ui_input();
    song.master_q12 = 4096;
    song.playing = 0;
    CHECK(eng_ok(ENGI_FMDRUM) && ENGINES[ENGI_FMDRUM] == &ENG_FMDRUM, "engine 9 is FMDRUM, pickable");
    {
        uint32_t seen = 0;
        for (i = 0; i < NENG_SHOWN; i++) seen += ENGINE_ORDER[i] == ENGI_FMDRUM;
        CHECK(seen == 1u, "FMDRUM in ENGINE_ORDER once (%u)", seen);
        CHECK(eng_step(ENGI_KICK, 1) == ENGI_FMDRUM, "ALGO: after KICK (%u)", eng_step(ENGI_KICK, 1));
        for (i = seen = 0; i < NENG_SHOWN; i++) seen += ENGINE_PC_ORDER[i] == ENGI_FMDRUM;
        CHECK(seen == 1u, "and in ENGINE_PC_ORDER (%u)", seen);
    }
    set_engine_of(&trk[2], ENGI_FMDRUM);
    solo(2);
    for (k = 0; k < NELEM(FMD_PRESETS); k++) {
        apply_preset_to(&trk[2], k);
        e = hit(2, 48, &l, &pk);
        CHECK(e > 1e9 || (e > 2e8 && pk > 8000), "preset %s sounds: %.3g (peak %d)", FMD_PRESETS[k].name, e, pk);
        CHECK(l < e / 4, "preset %s falls (%.3g -> %.3g)", FMD_PRESETS[k].name, e, l);
        CHECK(quiet(2) < 1e6, "preset %s: silence after (%.3g)", FMD_PRESETS[k].name, quiet(0));
        CHECK(pk < 30000, "preset %s bounded (peak %d)", FMD_PRESETS[k].name, pk);
        printf("  %-10s peak %6d  energy %.3g\n", FMD_PRESETS[k].name, pk, e);
        CHECK(trk[2].p[P_DXIMG] == (FMD_PRESETS[k].xp & 15u), "preset %s: its TRANSIENT (%d)", FMD_PRESETS[k].name, trk[2].p[P_DXIMG]);
    }
    apply_preset_to(&trk[2], 0);
    for (k = 0; k < 4u; k++) {
        trk[2].p[P_E0] = (int16_t)k;
        trk[2].p[P_E6] = 64;
        trk[2].p[P_E4] = 127;
        e = hit(2, 60, 0, &pk);
        CHECK(e > 1e9 && pk < 32000, "TYPE %s sounds, bounded (%.3g %d)", N_FMD_TYPE[k], e, pk);
    }
    {   /* every wave, every ratio: a sound, no wild level */
        apply_preset_to(&trk[2], 0);
        for (k = 0; k < 4u; k++) {
            trk[2].p[P_E1] = trk[2].p[P_E7] = (int16_t)k;
            trk[2].p[P_E4] = 127;
            for (i = 0; i < FMD_NRATIO; i += 3u) {
                trk[2].p[P_E5] = (int16_t)i;
                e = hit(2, 36, 0, &pk);
                CHECK(e > 1e9 && pk < 32000, "WAVE %s RATIO %s (%.3g %d)", N_FMD_WAVE[k], N_FMD_RATIO[i], e, pk);
            }
        }
    }
    {   /* the keys tune it: an octave up, about twice the crossings (a long DRUM, no sweep, no FM) */
        uint32_t c[2], j;
        apply_preset_to(&trk[2], 0);
        trk[2].p[P_E2] = 0; trk[2].p[P_E4] = 0; trk[2].p[P_DXIMG] = 0;
        trk[2].p[P_DEC] = trk[2].p[P_REL] = 110;
        for (j = 0; j < 2u; j++) {
            hit(2, 48 + 12 * j, 0, 0);
            for (i = 400, c[j] = 0; i < 2047u; i++) c[j] += (first[i] >= 0) != (first[i - 1] >= 0);
        }
        CHECK(c[1] > c[0] * 3 / 2 && c[1] < c[0] * 5 / 2, "an octave up: twice the crossings (%u %u)", c[0], c[1]);
    }
    {   /* the TRANSIENT: each kind changes the hit's start; OFF leaves it */
        static int32_t base[2048];
        apply_preset_to(&trk[2], 0);
        trk[2].p[P_DXIMG] = 0;
        trk[2].p[P_MORPH] = 100;
        hit(2, 48, 0, 0);
        memcpy(base, first, sizeof base);
        hit(2, 48, 0, 0);
        l = diff_first(base);
        for (k = 1; k < TRN_N; k++) {
            trk[2].p[P_DXIMG] = (int16_t)k;
            hit(2, 48, 0, &pk);
            CHECK(diff_first(base) > l * 3 + 512.0 * 50, "FMDRUM TRANSIENT %s heard (%.3g, OFF %.3g)", N_TRN[k], diff_first(base), l);
            CHECK(pk < 32000, "bounded (%d)", pk);
        }
        {
            const param_desc_t *d = track_desc(&trk[2], P_DXIMG);
            CHECK(!strcmp(d->label, "TRANS") && d->max == TRN_N - 1, "P_DXIMG on FMDRUM: TRANS (%s)", d->label);
        }
    }
    {   /* the KICK's TRANSIENT (T2) */
        static int32_t base[2048];
        solo(1);
        apply_preset_to(&trk[1], 1);                     /* 909: none */
        CHECK(trk[1].p[P_DXIMG] == 0, "909: no transient");
        trk[1].p[P_MORPH] = 127;
        hit(1, 36, 0, 0);
        memcpy(base, first, sizeof base);
        for (k = 1; k < TRN_N; k++) {
            trk[1].p[P_DXIMG] = (int16_t)k;
            hit(1, 36, 0, &pk);
            CHECK(diff_first(base) > 512.0 * 200, "KICK TRANSIENT %s heard (%.3g)", N_TRN[k], diff_first(base));
        }
        apply_preset_to(&trk[1], 0);
        CHECK(trk[1].p[P_DXIMG] == 2 && trk[1].p[P_MORPH] > 30, "TECHNO: CLICK (%d %d)", trk[1].p[P_DXIMG], trk[1].p[P_MORPH]);
    }
    {   /* DIST TUBE and RAT: the sound changed, bounded, silence stays silence */
        static int32_t base[2048];
        uint32_t ty;
        solo(2);
        apply_preset_to(&trk[2], 1);
        trk[2].p[P_DIST] = 0;
        hit(2, 48, 0, 0);
        memcpy(base, first, sizeof base);
        for (ty = DT_TUBE; ty <= DT_RAT; ty++) {
            trk[2].p[P_DTYPE] = (int16_t)ty;
            trk[2].p[P_DIST] = 90;
            e = hit(2, 48, &l, &pk);
            CHECK(diff_first(base) > 512.0 * 300 && pk < 32700, "DIST %s: shaped, bounded (%.3g %d)", N_DTYPE[ty], diff_first(base), pk);
            l = quiet(2);
            CHECK(l < 1e6, "DIST %s: silence after the hit, no DC (%.3g)", N_DTYPE[ty], l);
        }
        trk[2].p[P_DIST] = 0;
        trk[2].p[P_DTYPE] = 0;
    }
    {   /* (0.12.0) a kit's sound on FMDRUM's voice: FILTER FM on the SNARE (DRUM's TONE page), every TYPE and TRANSIENT */
        static int32_t base[2048];
        uint32_t ty, tr;
        int32_t *cv;
        const param_desc_t *d;
        solo(0);
        trk[0].p[P_LN0 + DV_SNARE] = 100;
        dx_kit[DV_SNARE] = DX_KIT_DEF[DV_SNARE];
        e = hit(0, 38, 0, &pk);
        printf("  DRUM-X snare: energy %.3g peak %d\n", e, pk);
        memcpy(base, first, sizeof base);
        track_select(0);
        dx_ui_lane = DV_SNARE;
        cos_cput(CC_DFLT, 3);
        CHECK(((dx_kit[DV_SNARE].mode >> 2) & 3u) == DXF_FM && dx_kit_ok(dx_kit), "FILTER FM set, the kit still valid");
        d = cos_ddesc(CC_DWAV, (int16_t **)&cv);
        CHECK(!strcmp(d->label, "TYPE") && !strcmp(d->names[1], "SNARE"), "WAVE shows TYPE (%s)", d->label);
        d = cos_ddesc(CC_DSNP, (int16_t **)&cv);
        CHECK(!strcmp(d->label, "TRANS") && d->max == 7, "SNAP shows TRANS (%s)", d->label);
        for (ty = 0; ty < 4u; ty++)
            for (tr = 0; tr < 8u; tr++) {
                cos_cput(CC_DWAV, (int32_t)ty);
                cos_cput(CC_DSNP, (int32_t)tr);
                e = hit(0, 38, &l, &pk);
                CHECK(e > 2e8 && pk < 32700, "FM lane TYPE %u TRANS %u: sounds, bounded (%.3g %d)", ty, tr, e, pk);
                CHECK(l < e / 4, "FM lane TYPE %u TRANS %u: falls (%.3g -> %.3g)", ty, tr, e, l);
                if (tr == 0u || tr == 4u) printf("  FM lane %s TRANS %u: energy %.3g peak %d\n", N_FMD_TYPE[ty], tr, e, pk);
                if (!ty && !tr) CHECK(diff_first(base) > 512.0 * 300, "FM: not DRUM-X's snare (%.3g)", diff_first(base));
            }
        CHECK(quiet(2) < 1e6, "FM lane: silence after");
        cos_cput(CC_DSNP, 3);
        CHECK(((dx_kit[DV_SNARE].mode >> 4) & 7u) == 3u && ((dx_kit[DV_SNARE].mode >> 2) & 3u) == DXF_FM, "TRANS in bits 4..6, FM kept");
        cos_cput(CC_DFLT, 1);
        e = hit(0, 38, 0, 0);
        CHECK(e > 1e8, "back to BP: DRUM-X's again (%.3g)", e);
        {   /* the hats FM: the closed one chokes the open one */
            static int32_t o[2 * CTL];
            uint32_t b;
            dx_ui_lane = DV_HATO; cos_cput(CC_DFLT, 3); cos_cput(CC_DWAV, 3);
            dx_ui_lane = DV_HATC; cos_cput(CC_DFLT, 3); cos_cput(CC_DWAV, 3);
            trk[0].p[P_LN0 + DV_HATO] = trk[0].p[P_LN0 + DV_HATC] = 100;
            trk_note_on(&trk[0], 46, 110);
            for (b = 0; b < 20u; b++) mix_block(o, CTL);
            CHECK(drum_kit_of(&trk[0])[DV_HATO].x.live, "open hat (FM) rings");
            trk_note_on(&trk[0], 42, 110);
            for (b = 0; b < 20u; b++) mix_block(o, CTL);
            CHECK(!drum_kit_of(&trk[0])[DV_HATO].x.live, "the closed one chokes it");
            trk_note_off(&trk[0], 46); trk_note_off(&trk[0], 42);
        }
        memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
    }
    {   /* (0.12.1) SOUND > SOURCE: every drum sound the firmware has, into the SNARE; each sounds, bounded, named */
        uint32_t i, n = 0;
        char nm[24];
        solo(0);
        track_select(0);
        dx_ui_lane = DV_SNARE;
        trk[0].p[P_LN0 + DV_SNARE] = 100;
        for (i = 1; i <= DSRC_N; i++) {
            cos_cput(CC_DSRC, (int32_t)i);
            cos_dsrc_name(i, nm);
            CHECK(str_len(nm) <= 7u && str_len(nm) >= 3u, "source %u named (%s)", i, nm);
            CHECK(cos_dsrc_same(DV_SNARE), "source %s loaded as it is", nm);
            e = hit(0, 38, 0, &pk);
            CHECK(e > 1e8 && pk < 32700, "source %s in the SNARE: sounds, bounded (%.3g %d)", nm, e, pk);
            n += e > 1e8;
        }
        CHECK(n == DSRC_N, "all %u sources sound (%u)", DSRC_N, n);
        cos_cput(CC_DSRC, 1 + 3 * DX_NKIT + 1);           /* 909's closed hat */
        cos_dsrc_name(1 + 3 * DX_NKIT + 1, nm);
        CHECK(!strcmp(nm, "909 HHC") && !memcmp(&dx_kit[DV_SNARE], &DX_KITS[1].lane[DV_HATC], sizeof(dx_lane_t)), "909 HHC into the snare (%s)", nm);
        cos_cput(CC_KICK + DXP_DECAY, 90);
        CHECK(!cos_dsrc_same(DV_SNARE), "edited: OWN shown");
        memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
    }
    {   /* (0.13.0) T.PITCH (P_WRDEP) on FMDRUM and KICK; an FM kit sound's RATIO, WAVE, S.TIME */
        static int32_t base[2048];
        int16_t *vp;
        CHECK(trn_rate(0) == 16u && trn_rate(-12) == 8u && trn_rate(12) == 32u && trn_rate(127) == 16u,
              "T.PITCH: 0 as recorded, -12 half speed, +12 double, 127 (WHEEL's) as recorded (%u %u %u)", trn_rate(0), trn_rate(-12), trn_rate(12));
        CHECK(!strcmp(track_desc(&trk[1], P_WRDEP)->label, "T.PITCH"), "KICK: P_WRDEP is T.PITCH");
        apply_preset_to(&trk[1], 0);
        CHECK(trk[1].p[P_WRDEP] == 0, "a preset: T.PITCH 0");
        solo(1);
        hit(1, 36, 0, 0);
        memcpy(base, first, sizeof base);
        trk[1].p[P_WRDEP] = -12;
        hit(1, 36, 0, 0);
        CHECK(diff_first(base) > 512.0 * 50, "T.PITCH -12: the click lower (%.3g)", diff_first(base));
        trk[1].p[P_WRDEP] = 0;
        solo(0);
        track_select(0);
        dx_ui_lane = DV_SNARE;
        trk[0].p[P_LN0 + DV_SNARE] = 100;
        cos_cput(CC_DFLT, 3);
        CHECK(!strcmp(cos_ddesc(CC_DLAY, &vp)->label, "RATIO") && !strcmp(cos_ddesc(CC_DFB, &vp)->label, "WAVE") &&
              !strcmp(cos_ddesc(CC_DBT, &vp)->label, "S.TIME"), "FM: LAYER RATIO, FEEDBK WAVE, BENDTYP S.TIME");
        cos_cput(CC_DWAV, 1);
        hit(0, 38, 0, 0);
        memcpy(base, first, sizeof base);
        cos_cput(CC_DLAY, 8);                            /* (RATIO 8) */
        hit(0, 38, 0, 0);
        CHECK(DXF_LR(dx_lay[DV_SNARE]) == 8u && diff_first(base) > 512.0 * 200, "RATIO 8: another sound (%.3g)", diff_first(base));
        memcpy(base, first, sizeof base);
        cos_cput(CC_DFB, 3);                             /* (WAVE SAW) */
        hit(0, 38, 0, &pk);
        CHECK(DXF_LW(dx_lay[DV_SNARE]) == 3u && diff_first(base) > 512.0 * 200 && pk < 32700, "WAVE SAW: another, bounded (%.3g %d)", diff_first(base), pk);
        cos_cput(CC_DBT, 1);
        CHECK(DXF_LT(dx_lay[DV_SNARE]) == 1u && ((dx_kit[DV_SNARE].mode >> 4) & 7u) == ((dx_kit[DV_SNARE].mode >> 4) & 7u), "S.TIME LONG");
        cos_cput(CC_DFLT, 1);
        CHECK(dx_lay[DV_SNARE] == 0u, "back to BP: its LAYER byte its own again (%x)", dx_lay[DV_SNARE]);
        memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
        memcpy(dx_lay, DX_LAY_DEF, sizeof dx_lay);
    }
    {   /* projects: engine 9 of before 0.11 (phys 2) is PHYS's, ANALOG now; written since (PROJ_FMD), FMDRUM */
        static project_t q;
        memset(&q, 0, sizeof q);
        q.phys = PROJ_PHYS;
        q.t[0].engine = ENGI_PHYS;
        q.t[1].engine = ENGI_DRUM;
        proj_phys_gone(&q);
        CHECK(q.t[0].engine == ENGI_PHYS_TO && q.t[1].engine == ENGI_DRUM && q.phys == PROJ_FMD, "old 9: ANALOG (%u %u)", q.t[0].engine, q.phys);
        q.t[0].engine = ENGI_FMDRUM;
        proj_phys_gone(&q);
        CHECK(q.t[0].engine == ENGI_FMDRUM, "new 9: FMDRUM (%u)", q.t[0].engine);
    }
    {   /* the stars: FMDRUM's in row 6 bytes 4..7, KICK's 0..3, the crash log (row 9) untouched */
        uint8_t row9[32], row6[32];
        memcpy(row9, favorites.factory[9], 32);
        memset(favorites.factory[6], 0, 8);
        memcpy(row6, favorites.factory[6], 32);
        CHECK(favorite_set(ENGI_FMDRUM, 3, 1) && favorite_has(ENGI_FMDRUM, 3) && !favorite_has(ENGI_KICK, 3), "FMDRUM star 4");
        CHECK(favorites.factory[6][4] == 8u, "in row 6 byte 4 (%x)", favorites.factory[6][4]);
        CHECK(favorite_set(ENGI_KICK, 3, 1) && favorite_has(ENGI_FMDRUM, 3) && favorites.factory[6][0] == 8u, "KICK's apart");
        CHECK(!memcmp(row9, favorites.factory[9], 32), "the crash log untouched");
        CHECK(!memcmp(row6 + 8, favorites.factory[6] + 8, 24), "row 6's settings untouched");
        CHECK(!favorite_set(ENGI_FMDRUM, 40, 1), "past 32: no star");
        favorite_set(ENGI_FMDRUM, 3, 0); favorite_set(ENGI_KICK, 3, 0);
    }
    printf("fmdrum_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
