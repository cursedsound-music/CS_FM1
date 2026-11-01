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
    cz.flip = 1;                                                     /* (COS_DEFAULT 0 in the UI tests: on) */
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

    /* DRONE: a key holds its note on the selected track, again lets it go; four at most; another track moves it; the audio
     * side (seq.c cos_drone_block) sounds what is asked, STOP leaves it; a key change moves it with the song */
    song.sel = 4; cz.mode = CM_DRONE; cz.dr_oct = 0;
    cos_drone_key(0); cos_drone_key(7);
    CHECK(cz.dr_n == 2 && cz.dr_note[0] == 41 && cz.dr_note[1] == 48 && cz.dr_trk == 4, "two notes held on T5 (%u)", cz.dr_n);
    cos_drone_key(7);
    CHECK(cz.dr_n == 1 && cz.dr_note[0] == 41, "a key again lets its note go");
    cos_drone_key(1); cos_drone_key(2); cos_drone_key(3); cos_drone_key(4);
    CHECK(cz.dr_n == 4 && cz.dr_note[0] == 42, "four at most, the oldest goes (%u: %u)", cz.dr_n, cz.dr_note[0]);
    cos_drone_block();
    CHECK(cos_dr_n == 4 && cos_dr_trk == 4 && cos_dr_gen == cos_dr.gen, "the audio side took the request");
    seq_stop();
    cos_drone_block();
    CHECK(cos_dr_n == 4, "STOP leaves the drone");
    {
        uint8_t n0 = cz.dr_note[0];
        cos_key_ask((uint32_t)((cos_key() + 2u + 7u) % 12u));   /* (a key two semitones up: key = pc - 5) */
        CHECK(cz.dr_note[0] == n0 + 2u, "the key change moves the drone (%u -> %u)", n0, cz.dr_note[0]);
    }
    song.sel = 2; cos_drone_key(0);
    CHECK(cz.dr_n == 1 && cz.dr_trk == 2, "a key on another track moves the drone there");
    cos_drone_clear(); cos_drone_block();
    CHECK(cz.dr_n == 0 && cos_dr_n == 0, "REC lets it go");

    /* GLO > MIDI: T3's MIDI OUT channel 10, kept as 10 (its own: 0); MIDI OUT ALL; the sequencer's channel follows */
    song.sel = 2; cz.mode = CM_MIX; cz.tab[CM_MIX] = 1;
    cos_cput(CC_OUTCH, 10);
    CHECK(favorites.factory[15][8 + 2] == 10u, "T3 -> channel 10 kept in the settings (%u)", favorites.factory[15][10]);
    ui_polls();
    CHECK(trk_midi_ch(2) == 9u && trk_midi_ch(1) == 1u, "the sequencer sends T3 on 10, T2 on its own 2");
    cos_cput(CC_OUTCH, 3);
    CHECK(favorites.factory[15][10] == 0u, "its own channel: 0, as JIANT");
    cos_cput(CC_MOUT, 2);
    CHECK((midi_out_mode & 3u) == 2u, "MIDI OUT ALL");

    printf("cos_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
