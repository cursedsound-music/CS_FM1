/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) cursed-os (firmware/src/cos.c) against the real UI sources: the song's key (SCL), the trigs.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/t/cos_test tests/cos_test.c -lm && build/t/cos_test */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    uint32_t i, k;
    uint8_t before[NTRK][NSTEP];
    ui_power_on();
    cz.on = 1;
    song.sel = 4; set_engine_of(TSEL, ENGI_ANALOG);
    song.sel = 0; set_engine_of(TSEL, ENGI_DRUM);
    for (k = 0; k < NTRK; k++) trk[k].p[P_ROOT] = 9;                 /* A */
    song.sel = 4;
    ac_put(AK_DENS, 80);                                              /* an ACID line on T5 */
    grid_hit(&trk[0], 0, 0, 1); grid_hit(&trk[0], 4, 0, 1);           /* a kick on T1 */
    CHECK(cos_key() == 9u, "the key read from the tracks: A (%u)", cos_key());
    for (k = 0; k < NTRK; k++) for (i = 0; i < NSTEP; i++) before[k][i] = trk[k].step[i].note[0];

    /* stopped: at once; A -> C is +3 (nearest), every synth note up 3, roots C, the kit untouched */
    cos_key_ask(7);                                                   /* key 7 = C (key 0 is F) */
    CHECK(cos_key() == 0u, "stopped: the key is C at once (%u)", cos_key());
    for (i = 0; i < 16u; i++)
        if (trk[4].step[i].time == ST_NOTE)
            CHECK(trk[4].step[i].note[0] == before[4][i] + 3u, "T5 step %u: %u -> %u, not +3", i, before[4][i], trk[4].step[i].note[0]);
    CHECK(step_lanes(&trk[0].step[0]) & 1u, "the kit's kick kept");
    CHECK(trk[0].step[0].note[0] == before[0][0], "the kit's notes untouched");
    CHECK(trk[4].p[P_ROOT] == 0 && trk[2].p[P_ROOT] == 0, "every synth track's root C");

    /* C -> F#: the nearest is -6 (never more than a tritone away), then back to A: the notes as they were */
    cos_key_ask(1);                                                   /* key 1 = F# */
    CHECK(cos_key() == 6u, "F# (%u)", cos_key());
    cos_key_ask(4);                                                   /* key 4 = A */
    for (i = 0; i < 16u; i++)
        if (trk[4].step[i].time == ST_NOTE) CHECK(trk[4].step[i].note[0] == before[4][i], "back in A, step %u as before", i);

    /* playing: it waits for the next bar */
    song.playing = 1;
    bar_n = 7;
    cos_key_ask(6);                                                   /* key 6 = B */
    cos_key_poll();
    CHECK(cos_key() == 9u && cz.key_next == 12u, "playing: still A until the bar (key %u next %u)", cos_key(), cz.key_next);
    bar_n = 8;
    cos_key_poll();
    CHECK(cos_key() == 11u && !cz.key_next, "the next bar: B (%u)", cos_key());
    song.playing = 0;

    /* a new ACID line after the key change: in the song's key (B), minor without a scale */
    song.sel = 4;
    for (k = 0; k < 6u; k++) {
        ac_oct(1, 0);
        for (i = 0; i < 16u; i++)
            if (trk[4].step[i].time == ST_NOTE)
                CHECK((SCALE_MASK[2] >> ((trk[4].step[i].note[0] + 12u - 11u) % 12u)) & 1u, "ACID in B minor: note %u", trk[4].step[i].note[0]);
    }

    /* TRIG: a white key a trig, again off; past the length refused */
    cz.mode = CM_TRIG; cz.bank = 0;
    for (i = 0; i < NSTEP; i++) step_clear(&trk[4].step[i]);
    cos_trig(3);
    CHECK(trk[4].step[3].time == ST_NOTE && trk[4].step[3].n == 1, "a trig written");
    cos_trig(3);
    CHECK(!(trk[4].step[3].time == ST_NOTE && trk[4].step[3].n), "and cleared");
    song.sel = 0; cz.lane = 1;
    cos_trig(2);
    CHECK(step_lanes(&trk[0].step[2]) & 2u, "a kit: the lane's hit (SD)");

    printf("cos_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
