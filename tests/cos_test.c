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

    /* LOCKS: TRIG, step 3 held (its white key down), KNOB 2 of the sound page used last (SRC 1: E1) turned: a lock on step 3
     * only, from the sound's own value; the step's trig not toggled when let go; held + REC: the lock goes */
    song.sel = 4; set_engine_of(TSEL, ENGI_ANALOG);
    for (i = 0; i < NSTEP; i++) step_clear(&trk[4].step[i]);
    motion_clear_locks(&trk[4], NSTEP);
    cz.mode = CM_SRC; cz.tab[CM_SRC] = 0; cz.lk_mode = 0;
    cos_turn(1, 0);                                     /* (a turn of nothing: no page remembered yet beyond SRC 1) */
    cz.mode = CM_TRIG; cz.bank = 0;
    trk[4].step[3].time = ST_NOTE; trk[4].step[3].n = 1; trk[4].step[3].note[0] = 48;
    cz.tg_down = 1u << WK[3]; fm1_in.notes = 1u << WK[3]; cz.lk_used = 0;
    CHECK(cos_held() == (1ull << 3), "step 3 held (%llx)", (unsigned long long)cos_held());
    {
        int16_t v, base = motion_base_value(&trk[4], P_E1);
        cos_lock_turn(1, 5, cos_held());
        CHECK(motion_lock_get(&trk[4], 3, P_E1, &v) && v != base, "a lock of E1 on step 3 (%d, the sound's %d)", v, base);
        CHECK(!motion_lock_get(&trk[4], 4, P_E1, &v), "no lock on step 4");
        CHECK(motion_lock_steps(4) == (1ull << 3), "the foot's dot: step 3 only");
    }
    fm1_in.notes = 0; cos_input();                      /* (let go: no trig toggled, a lock was set) */
    CHECK(trk[4].step[3].time == ST_NOTE && trk[4].step[3].n == 1, "the locked step keeps its trig");
    CHECK(motion_clear_locks(&trk[4], 3) == 1u, "its lock clears");
    cz.tg_down = 1u << WK[5]; fm1_in.notes = 1u << WK[5]; cz.lk_used = 0;
    fm1_in.notes = 0; cos_input();                      /* (a tap: the trig) */
    CHECK(trk[4].step[5].time == ST_NOTE && trk[4].step[5].n, "a tap on step 6 writes its trig on release");

    /* WORDS: a VOICE track; OCT+ a phrase W1..W16 (a language set: EN), another each time; OCT- the one before */
    song.sel = 1; set_engine_of(TSEL, ENGI_VOICE);
    trk[1].p[P_VLANG] = 0; trk[1].p[P_VLYR] = 0;
    cos_words(1);
    CHECK(trk[1].p[P_VLANG] == LG_EN && trk[1].p[P_VLYR] >= 1 && trk[1].p[P_VLYR] <= (int)LYR_NW, "OCT+: EN, words W%d", trk[1].p[P_VLYR]);
    {
        int16_t w1 = trk[1].p[P_VLYR];
        cos_words(1);
        CHECK(trk[1].p[P_VLYR] != w1, "another phrase");
        cos_words(0);
        CHECK(trk[1].p[P_VLYR] == w1, "OCT-: the one before");
    }
    {
        const cpg_t *pp; uint32_t n;
        cos_pages(CM_SRC, &pp, &n);
        CHECK(n == 8u && !strcmp(pp[2].tab, "WORDS"), "a VOICE track's SRC has WORDS");
        song.sel = 4; cos_pages(CM_SRC, &pp, &n);
        CHECK(n == 7u && strcmp(pp[2].tab, "WORDS"), "an ANALOG track's does not");
    }

    /* ACID made easy: GEN from a kit goes over to an ANALOG track (one made from the last melodic one if none), an
     * empty one gets a line at once */
    {
        uint32_t j, na = 0, cnt = 0;
        for (j = 0; j < trk_live; j++) if (trk_eng(&trk[j]) == ENGI_ACID) set_engine_of(&trk[j], ENGI_PHASE);
        for (j = 0; j < trk_live; j++) { uint32_t i2; for (i2 = 0; i2 < NSTEP; i2++) step_clear(&trk[j].step[i2]); }
        set_engine_of(&trk[0], ENGI_DRUM); track_select(0);
        cz.mode = CM_HOME; cos_mode(CM_GEN);
        CHECK(!cos_kit() && trk_eng(TSEL) == ENGI_ACID, "GEN from the kit: on an ACID track (T%u)", song.sel + 1u);
        for (j = 0; j < NSTEP; j++) if (TSEL->step[j].time == ST_NOTE) na++;
        CHECK(na > 0u, "a line thrown at once (%u notes)", na);
        for (j = 0; j < trk_live; j++) cnt += trk_eng(&trk[j]) == ENGI_ACID;
        CHECK(cnt == 1u, "one ACID track");
        track_select(0); cz.mode = CM_HOME; cos_mode(CM_GEN);
        cnt = 0; for (j = 0; j < trk_live; j++) cnt += trk_eng(&trk[j]) == ENGI_ACID;
        CHECK(cnt == 1u && trk_eng(TSEL) == ENGI_ACID, "again: the same track, no other made");
    }

    /* SAVE: OCT+ the music into the slot picked, its name read back; OCT- loads it */
    {
        uint32_t j, i2;
        song.playing = 0;
        cz.mode = CM_HOME; cos_mode(CM_PROJ);
        CHECK(cz.mode == CM_PROJ, "SAVE: its page");
        set_engine_of(&trk[0], ENGI_DRUM); grid_hit(&trk[0], 0, 0, 1);
        cz.slot = 2; cos_proj_oct(1);
        CHECK(cz.pn[2][0] != 0, "slot C saved (%s)", cz.pn[2]);
        CHECK(proj_cur_slot() == 2u, "C the slot open");
        for (j = 0; j < trk_live; j++) for (i2 = 0; i2 < NSTEP; i2++) step_clear(&trk[j].step[i2]);
        cos_proj_oct(0);
        CHECK(!cos_empty(&trk[0]), "OCT-: the music back from C");
        cz.slot = 3; cos_proj_oct(0);
        CHECK(!strcmp(cz.msg, "EMPTY SLOT") || cz.pn[3][0], "an empty slot: said so");
    }

    printf("cos_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
