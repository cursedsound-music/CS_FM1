/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) cursed-os 0.16.0, GG 2026-11-13: "the ui on the synth is too cluttered and too menu divy"; SIMPLE + DEEP.
 * On every track, in every mode: the mode button's taps go round its everyday pages only (each of them reached, the
 * first always one, no more than five), SELECT through every page; the pages are all still there.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/simple_test tests/simple_test.c -lm */
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
    uint32_t k, m, i, n, ok = 1, sel = 1, few = 1, deep = 0, all = 0;
    const cpg_t *p;
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1;
    for (k = 0; k < NTRK; k++) {
        track_select(k);
        for (m = 0; m < CM_N; m++) {
            uint32_t seen = 0, ness = 0, t;
            if (m == CM_GEN) continue;                   /* (GEN: entering it starts a generator; its pages as the others' below) */
            cos_pages(m, &p, &n);
            all += n;
            for (i = 0; i < n; i++) ness += (uint32_t)cos_ess(m, p, i, n);
            cz.mode = (uint8_t)m; cz.tab[m] = 0;
            for (t = 0; t < 2u * n; t++) {
                cos_mode(m);
                ok &= (uint32_t)cos_ess(m, p, cz.tab[m] % n, n);
                seen |= 1u << (cz.tab[m] % n);
            }
            for (i = 0; i < n; i++) if (cos_ess(m, p, i, n)) ok &= (seen >> i) & 1u;
            few &= ness >= 1u && ness <= 5u && cos_ess(m, p, 0, n);
            deep += n - ness;
            cz.tab[m] = 0;
            for (i = 1; i < n; i++) {                    /* SELECT: every page, one a click */
                host_ticks += 1000000u; host_enc[panel.enc[EN_SELECT]] = 1; cos_input();
                sel &= cz.tab[m] == i;
            }
            if (n > 3u && k == 3u) printf("  T%u %-5s: %u pages, %u everyday\n", k, CM_NAME[m], n, ness);
        }
    }
    CHECK(ok, "a mode button's taps: its everyday pages only, each of them");
    CHECK(few, "each mode: one to five everyday pages, its first one of them");
    CHECK(sel, "SELECT: every page");
    CHECK(deep * 2u > all, "most pages deep: off the taps (%u of %u)", deep, all);
    /* HOME's tap: PERFORM, LIVE, PERFORM again */
    cz.mode = CM_HOME; cz.tab[CM_HOME] = 0;
    cos_mode(CM_HOME);
    CHECK(str_eq(cos_page()->tab, "LIVE"), "HOME: PERFORM, then LIVE (%s)", cos_page()->tab);
    cos_mode(CM_HOME);
    CHECK(str_eq(cos_page()->tab, "PERFORM"), "then PERFORM again");
    /* a deep page in use: a tap goes on to the next everyday one */
    track_select(3);
    cz.mode = CM_FX; cos_pages(CM_FX, &p, &n);
    for (i = 0; i < n && !str_eq(p[i].tab, "DELAY 2"); i++) {}
    cz.tab[CM_FX] = (uint8_t)i;
    cos_mode(CM_FX);
    CHECK(str_eq(cos_page()->tab, "REVERB"), "FX on DELAY 2 (SELECT): a tap, REVERB (%s)", cos_page()->tab);
    cos_mode(CM_FX);
    CHECK(str_eq(cos_page()->tab, "DJ"), "then DJ, the first (%s)", cos_page()->tab);
    ui_draw();
    printf(fails ? "simple: %d FAILED\n" : "simple: all ok\n", fails);
    return fails != 0;
}
