/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound
 * (cursedsound) cursed-os 0.13.0: SAVE > KITS: a kit made (its sounds, levels, AHDSR, sends, CHANCE) saved into K01,
 * changed, loaded back; a used slot asks first; playing: loads, does not save; REC twice erases.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/kit_test tests/kit_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static void frame16(void) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); ui_draw(); }

int main(void)
{
    static dx_lane_t made[8];
    static uint8_t de[8][DE_N];
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1; song.playing = 0; frame16();
    cos_mode(CM_PROJ); cos_mode(CM_PROJ); cos_mode(CM_PROJ);
    CHECK(cz.tab[CM_PROJ] == 2u && str_eq(cos_page()->tab, "KITS"), "SAVE three times: KITS (%s)", cos_page()->tab);
    frame16();
    /* a kit made: the snare from the 909 kit on FM, levels, an AHDSR, a send, a chance */
    dx_kit[DV_SNARE] = DX_KITS[1].lane[DV_HATC];
    dx_kit[DV_SNARE].mode = (uint8_t)((dx_kit[DV_SNARE].mode & ~0x0Cu) | DXF_FM << 2);
    dx_kit[DV_TOM].a[DXP_PITCH] = 99;
    dx_lay[DV_RIM] = 0x31;
    dx_de[DV_CLAP][DE_REL] = 77;
    dx_snd[DV_HATO] = 0x55;
    dx_skip[DV_HATC] = 30;
    trk[0].p[P_LN0 + DV_BELL] = 11;
    memcpy(made, dx_kit, sizeof made);
    memcpy(de, dx_de, sizeof de);
    cos_cput(CC_KSLOT, 1);
    cos_proj_oct(1);
    CHECK(kit_is_used(0), "OCT+: K01 saved (%s)", cz.msg);
    CHECK(kit_name(0)[0] != 0, "named (%s)", kit_name(0));
    /* changed */
    memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
    memset(dx_de, 0, sizeof dx_de);
    dx_lay[DV_RIM] = 0; dx_snd[DV_HATO] = 0; dx_skip[DV_HATC] = 0; trk[0].p[P_LN0 + DV_BELL] = 100;
    cos_proj_oct(0);
    CHECK(!memcmp(dx_kit, made, sizeof made) && !memcmp(dx_de, de, sizeof de) && dx_lay[DV_RIM] == 0x31 &&
          dx_snd[DV_HATO] == 0x55 && dx_skip[DV_HATC] == 30u && trk[0].p[P_LN0 + DV_BELL] == 11,
          "OCT-: K01 back, all of it (%s)", cz.msg);
    {   /* a used slot: asked first */
        dx_kit[DV_TOM].a[DXP_PITCH] = 10;
        cos_proj_oct(1);
        CHECK(!strcmp(cz.msg, "OCT+ AGAIN: REPLACE"), "used: asks (%s)", cz.msg);
        cos_proj_oct(1);
        dx_kit[DV_TOM].a[DXP_PITCH] = 55;
        cos_proj_oct(0);
        CHECK(dx_kit[DV_TOM].a[DXP_PITCH] == 10, "again: replaced (%d)", dx_kit[DV_TOM].a[DXP_PITCH]);
    }
    {   /* playing: loads, does not save */
        song.playing = 1;
        dx_kit[DV_TOM].a[DXP_PITCH] = 70;
        cos_cput(CC_KSLOT, 2);
        cos_proj_oct(1);
        CHECK(!kit_is_used(1) && !strcmp(cz.msg, "STOP TO SAVE"), "playing: K02 not saved (%s)", cz.msg);
        cos_cput(CC_KSLOT, 1);
        cos_proj_oct(0);
        CHECK(dx_kit[DV_TOM].a[DXP_PITCH] == 10, "playing: K01 loads (%d)", dx_kit[DV_TOM].a[DXP_PITCH]);
        song.playing = 0;
    }
    {   /* REC twice: erased */
        cos_kit_erase();
        CHECK(kit_is_used(0), "REC once: asked");
        cos_kit_erase();
        CHECK(!kit_is_used(0), "REC twice: erased");
        cos_proj_oct(0);
        CHECK(!strcmp(cz.msg, "EMPTY SLOT"), "an empty slot: nothing (%s)", cz.msg);
    }
    memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
    printf("kit_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
