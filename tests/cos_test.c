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
        CHECK(n == 9u && !strcmp(pp[2].tab, "WORDS") && !strcmp(pp[4].tab, "CHORD"), "a VOICE track's SRC has WORDS, CHORD");
        song.sel = 4; cos_pages(CM_SRC, &pp, &n);
        CHECK(n == 8u && strcmp(pp[2].tab, "WORDS") && !strcmp(pp[3].tab, "CHORD"), "an ANALOG track's: no WORDS, CHORD after VOICE");
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

    /* ACID's two 303s: the first two ACID tracks get one each, a third none; one whose track left ACID is taken */
    {
        set_engine_of(&trk[1], ENGI_ACID); set_engine_of(&trk[2], ENGI_ACID); set_engine_of(&trk[3], ENGI_ACID);
        trk[1].engine = trk[2].engine = trk[3].engine = ENGI_ACID;
        acid_own[0] = acid_own[1] = 0;
        CHECK(acid_slot(&trk[1]) == 0 && acid_slot(&trk[2]) == 1, "two 303s");
        CHECK(acid_slot(&trk[3]) < 0, "a third ACID track: none");
        trk[1].engine = ENGI_PHASE;
        CHECK(acid_slot(&trk[3]) == 0, "T2 left ACID: its 303 to T4");
        CHECK(acid_slot(&trk[2]) == 1, "T3 keeps its own");
    }

    /* MASTER: COMP pulls a loud mix down (the reduction shown), leaves a quiet one about as it was; LIMIT's ceiling */
    {
        uint32_t i, b2;
        int32_t pk_in = 0, pk_out = 0;
        mc_amt = 80; mc_atk = 5; mc_rel = 40; comp_env = 0; comp_g = 32768;
        for (b2 = 0; b2 < 40u; b2++) {                  /* (40 blocks of a loud square: the envelope settles) */
            for (i = 0; i < CTL; i++) { mix_l[i] = mix_r[i] = ((i / 16u) & 1u) ? 120000 : -120000; wet[i] = 0; }
            comp_block(CTL);
        }
        for (i = 0; i < CTL; i++) { pk_in = 120000; pk_out = mix_l[i] > pk_out ? mix_l[i] : pk_out; }
        CHECK(comp_gr > 256, "loud: over an octave (6 dB) of reduction (%d/256)", comp_gr);
        CHECK(pk_out < pk_in / 2, "the loud mix brought down (%d of %d)", pk_out, pk_in);
        for (b2 = 0; b2 < 900u; b2++) {                  /* (~1.3 s: RELEASE 40 is ~260 ms a step of e) */
            for (i = 0; i < CTL; i++) { mix_l[i] = mix_r[i] = ((i / 16u) & 1u) ? 800 : -800; wet[i] = 0; }
            comp_block(CTL);
        }
        CHECK(comp_gr == 0, "quiet: no reduction once released (%d)", comp_gr);
        CHECK(mix_l[20] >= 800 && mix_l[20] <= 1700, "quiet: about as it was, the makeup on it (%d)", mix_l[20]);
        favorites.factory[15][4] = 0; favorites.factory[15][7] = 6; ui_polls();
        CHECK(mc_amt == 0 && lim_t == 9021, "COMP 0 off; LIMIT -6 dB: the ceiling halved (%d)", lim_t);
        favorites.factory[15][7] = 0; ui_polls();
    }

    /* the FX pad: FX held, its page the macros (CRUSH, DEPTH as the level), the keys the effects' (seq.c's mask) */
    {
        cz.mode = CM_HOME; cz.fx_down = 1; cz.fx_used = 0;
        CHECK(cos_page() == CZP_PERF, "FX held: the pad's knobs");
        cos_cput(CC_PF0 + 1u, 40); cos_cput(CC_PF3, 70);
        CHECK(perf_k[1] == 40 && perf_k[3] == 30 && cz.fx_used, "CRUSH 40, DEPTH 70 (cut 30), the hold used");
        host_notes = 1u; cos_input();
        CHECK(kb_mask == (1u << panel.btn[B_FX]) && perf_mask == kb_mask, "the keys go to the effects while FX is down");
        CHECK(cz.mode == CM_HOME, "a key on the pad: no mode change");
        cz.fx_down = 0; perf_k[1] = perf_k[3] = 0;
    }

    /* LFO shapes: every one within Q15 over a cycle; LFO 2 runs at its RATE, the matrix reads it (LFO2) */
    {
        uint32_t w, q, bad = 0;
        int32_t lo = 0, hi = 0, v;
        for (w = 0; w < 11u; w++)
            for (q = 0; q < 4096u; q++) {
                v = lfo_shape(w, q << 20, 0x12345678u, 0x9ABCDEF0u);
                bad += v < -32768 || v > 32767;
                if (w == 9u) { lo = v < lo ? v : lo; hi = v > hi ? v : hi; }
            }
        CHECK(!bad, "the eleven shapes stay within Q15 (%u out)", bad);
        CHECK(hi > 30000 && lo < -30000, "EXP spans the range (%d .. %d)", lo, hi);
        trk_x2[2][X2_RATE] = 100; trk_x2[2][X2_WAVE] = 2; trk[2].lfo2_ph = 0;
        for (q = 0; q < 10u; q++) lfo2_tick(&trk[2]);
        CHECK(trk[2].lfo2_ph != 0 && trk[2].lfo2_val > -32768, "LFO 2 moves (%u)", trk[2].lfo2_ph);
        CHECK(mod_tsrc(&trk[2], MS_LFO2, 0) == trk[2].lfo2_val, "the matrix's LFO2 is LFO 2");
        x2_defaults(2);
    }
    /* FILTER 2: LP at a low CUT takes a fast square down; OFF leaves it as it was; HP the other way */
    {
        int32_t buf[CTL], b2;
        uint32_t i, j;
        int64_t e_dry = 0, e_lp = 0;
        track_t *t2 = &trk[2];
        for (i = 0; i < NVOICE; i++) t2->v[i].active = 0;
        trk_x2[2][X2_F2T] = 1; trk_x2[2][X2_F2C] = 20; trk_x2[2][X2_F2R] = 0; f2_on[2] = 0;
        for (j = 0; j < 8u; j++) {
            for (i = 0; i < CTL; i++) buf[i] = (i & 2u) ? 8000 : -8000;
            track_f2(t2, buf, CTL);
        }
        for (i = 0; i < CTL; i++) { e_dry += 8000LL * 8000LL; e_lp += (int64_t)buf[i] * buf[i]; }
        CHECK(e_lp * 20 < e_dry, "LP at CUT 20: the fast square down by more than 13 dB");
        trk_x2[2][X2_F2T] = 0;
        for (j = 0; j < 2u; j++) { for (i = 0; i < CTL; i++) buf[i] = (i & 2u) ? 8000 : -8000; track_f2(t2, buf, CTL); }
        for (i = 0, b2 = 0; i < CTL; i++) b2 |= buf[i] != ((i & 2u) ? 8000 : -8000);
        CHECK(!b2 && !f2_on[2], "OFF: faded out, then the input untouched");
        x2_defaults(2);
    }
    /* projects keep LFO 2 and FILTER 2 ('X2'), and a slot read for its name leaves the live ones alone */
    {
        char nm[16];
        song.playing = 0;
        trk_x2[3][X2_F2T] = 3; trk_x2[3][X2_F2C] = 90; trk_x2[3][X2_RATE] = -5; trk_x2[3][X2_F2E] = -20;
        set_engine_of(&trk[0], ENGI_DRUM); grid_hit(&trk[0], 0, 0, 1);
        project_save(1);
        x2_defaults(3);
        proj_slot_name(1, nm);
        CHECK(trk_x2[3][X2_F2T] == 0, "reading slot B's name: the live FILTER 2 kept (OFF)");
        project_load(1);
        CHECK(trk_x2[3][X2_F2T] == 3 && trk_x2[3][X2_F2C] == 90 && trk_x2[3][X2_RATE] == -5 && trk_x2[3][X2_F2E] == -20,
              "loaded: T4's BP at 90, LFO 2 in time, ENV -20 (%d %d %d %d)", trk_x2[3][X2_F2T], trk_x2[3][X2_F2C], trk_x2[3][X2_RATE], trk_x2[3][X2_F2E]);
        x2_defaults(3);
    }

    /* OCTAVE: TRANS in octaves (its semitones kept); a kit has none. A kit's EDIT: KICK (both MORPH ends), LEVELS */
    {
        const cpg_t *pp; uint32_t n;
        int16_t *vp;
        song.sel = 2; set_engine_of(TSEL, ENGI_ANALOG); TSEL->p[P_TRANS] = 3;
        cos_cput(CC_OCT, -2);
        CHECK(TSEL->p[P_TRANS] == -21, "OCTAVE -2: TRANS 3 -> -21 (%d)", TSEL->p[P_TRANS]);
        CHECK(cos_cdesc(CC_OCT, &vp) && *vp == -2, "OCTAVE reads -2");
        TSEL->p[P_TRANS] = 0;
        song.sel = 0; set_engine_of(TSEL, ENGI_DRUM);
        CHECK(!cos_cdesc(CC_OCT, &vp), "a kit: no OCTAVE");
        cos_pages(CM_SRC, &pp, &n);
        CHECK(!strcmp(pp[0].tab, "KICK") && !strcmp(pp[2].tab, "LEVELS"), "a kit's EDIT: KICK first, LEVELS");
        {
            int32_t a0 = dx_kit[0].a[DXP_DECAY], b0 = dx_kit[0].b[DXP_DECAY];
            const param_desc_t *d = cos_cdesc(CC_KICK + DXP_DECAY, &vp);
            CHECK(d && !strcmp(d->label, "DECAY"), "KICK 3: DECAY");
            cos_cput(CC_KICK + DXP_DECAY, *vp + 10);
            CHECK(dx_kit[0].a[DXP_DECAY] == clamp(a0 + 10, 0, 127) && dx_kit[0].b[DXP_DECAY] == clamp(b0 + 10, 0, 127), "both ends +10");
            dx_kit[0].a[DXP_DECAY] = (uint8_t)a0; dx_kit[0].b[DXP_DECAY] = (uint8_t)b0;
        }
        cos_cput(CC_LN0, 90);
        CHECK(TSEL->p[P_LN0] == 90, "the KICK's level (LEVELS 1)");
        TSEL->p[P_LN0] = 127;
    }

    /* the LFOs' SYNC, RATE, MODE: HZ <-> BPM, ONE held after its cycle, TRIG from PHASE at a fresh note */
    {
        int16_t *vp;
        uint32_t q;
        track_t *t3 = &trk[3];
        song.sel = 3; set_engine_of(t3, ENGI_ANALOG); t3->p[P_LRATE] = 60;
        cos_cput(CC_L1SYNC, 1);
        CHECK(t3->p[P_LRATE] == -5 && cos_cdesc(CC_L1RATE, &vp)->fmt == F_ENUM && *vp == 7, "LFO 1 BPM: 1/4 (after the slow three)");
        cos_cput(CC_L1RATE, 0);
        CHECK(t3->p[P_LRATE] == -15 && cos_cdesc(CC_L1RATE, &vp) && *vp == 0, "32BAR first on the knob (%d)", t3->p[P_LRATE]);
        cos_cput(CC_L1RATE, 2);
        CHECK(t3->p[P_LRATE] == -13, "8BAR (%d)", t3->p[P_LRATE]);
        {   /* 32 bars at 120 BPM: a cycle of 32 x 4 x 22050 samples */
            uint32_t ph0, q;
            uint64_t want = ((uint64_t)1000u * CTL << 32) / 2822400u, got;
            song.playing = 0; song.g[G_BPM] = 120; bpm_frac = 0; bpm_nudge = 0; ph0 = t3->lfo_ph; t3->p[P_LRATE] = -15;
            for (q = 0; q < 1000u; q++) track_lfo_tick(t3);
            got = (uint32_t)(t3->lfo_ph - ph0);
            CHECK(got > want * 98u / 100u && got < want * 102u / 100u, "32BAR: a cycle of 32 bars (%llu of %llu)", (unsigned long long)got, (unsigned long long)want);
        }
        cos_cput(CC_L1RATE, 12);
        CHECK(t3->p[P_LRATE] == -10, "1/16 (%d)", t3->p[P_LRATE]);
        cos_cput(CC_L1SYNC, 0);
        CHECK(t3->p[P_LRATE] == 60, "back to HZ");
        cos_cput(CC_L1MODE, LM_FREE);
        CHECK(t3->p[P_LPHASE] < 0, "FREE: PHASE -1");
        cos_cput(CC_L1MODE, LM_ONE);
        CHECK(t3->p[P_LPHASE] >= 0 && trk_x2[3][X2_L1ONE] == 1, "ONE: TRIG with ONE");
        trk_x2[3][X2_RATE] = 127; trk_x2[3][X2_WAVE] = 2; trk_x2[3][X2_L2MODE] = LM_ONE; trk_x2[3][X2_L2PH] = 0;
        lfo_note(t3);
        for (q = 0; q < 4000u; q++) lfo2_tick(t3);
        CHECK(t3->lfo2_done && t3->lfo2_val > 30000, "LFO 2 ONE: one SAW cycle, held at its top (%d)", t3->lfo2_val);
        lfo_note(t3);
        CHECK(!t3->lfo2_done && t3->lfo2_ph == 0, "a fresh note: from PHASE again");
        trk_x2[3][X2_L2MODE] = LM_FREE; lfo2_tick(t3); { uint32_t p0 = t3->lfo2_ph; lfo_note(t3); CHECK(t3->lfo2_ph == p0, "FREE: a note does not restart it"); }
        x2_defaults(3); t3->p[P_LPHASE] = 0;
    }


    /* (the FX pad) techno: GATE, LOW KILL, TAPE STOP, the risers, THROW / FREEZE / WASH's offsets, KICK, DRUMS; HOLD */
    {
        int32_t L[64], R[64], W[64], SD[64], SR[64];
        uint32_t b, i2, P;
        int32_t pk, lo, hi;
        song.playing = 0; song.g[G_BPM] = 120; bpm_frac = 0; bpm_nudge = 0; song.g[G_CLOCK] = 0;
        CHECK(beat_samples() == 22050u, "120 BPM: 22050 a beat (%u)", beat_samples());
        bpm_frac = 5;
        CHECK(beat_samples() == 44100u * 600u / 1205u, "120.5 BPM (%u)", beat_samples());
        bpm_nudge = 1;
        CHECK(beat_samples() == 44100u * 600u / 1205u * 1000u / 1030u, "nudged 3 %% faster (%u)", beat_samples());
        bpm_nudge = -1;
        CHECK(beat_samples() > 44100u * 600u / 1205u, "nudged slower (%u)", beat_samples());
        bpm_nudge = 0; bpm_frac = 0;
        perf_latch_on = 0; cfx_held = cfx_latched = 0;
        cfx_press(CX_CODE + CX_GATE16, 1);
        CHECK(cfx_held == CX_BIT(CX_GATE16), "HOLD: the key holds GATE 1/16");
        cfx_press(CX_CODE + CX_GATE16, 0);
        CHECK(!cfx_held, "let go: off");
        perf_latch_on = 1;
        cfx_press(CX_CODE + CX_DRIVE, 1); cfx_press(CX_CODE + CX_DRIVE, 0);
        CHECK(cfx_latched == CX_BIT(CX_DRIVE), "LATCH: a press turns it on");
        cfx_press(CX_CODE + CX_DRIVE, 1);
        CHECK(!cfx_latched, ".. the next off");
        perf_latch_on = 0;
        /* GATE 1/16 on a steady input: half of each 1/16 open, half shut */
        P = div_samples(2);
        cfx_held = CX_BIT(CX_GATE16);
        lo = 1 << 30; hi = -1;
        for (b = 0; b < 2u * P / 64u + 200u; b++) {
            for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 10000; W[i2] = SD[i2] = SR[i2] = 0; }
            cfx_block(L, R, W, SD, SR, 64);
            if (b > 100u) for (i2 = 0; i2 < 64u; i2++) { if (L[i2] < lo) lo = L[i2]; if (L[i2] > hi) hi = L[i2]; }
        }
        CHECK(lo < 200 && hi > 9800, "GATE 1/16: shut and open (%d .. %d)", lo, hi);
        cfx_held = 0;
        for (b = 0; b < 40u; b++) { for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 10000; W[i2] = 0; } cfx_block(L, R, W, SD, SR, 64); }
        CHECK(L[63] == 10000 && !cx.gs, "let go: open again, the gate out (%d)", L[63]);
        /* LOW KILL: 50 Hz gone, 3 kHz through */
        {
            uint32_t ph = 0, f;
            for (f = 0; f < 2u; f++) {
                uint32_t inc = f ? 3000u * 97391u : 50u * 97391u;   /* (2^32 / 44100 a Hz) */
                cfx_held = CX_BIT(CX_LOWK); pk = 0;
                for (b = 0; b < 400u; b++) {
                    for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = og_sin(ph >> 16) / 3; ph += inc; W[i2] = 0; }
                    cfx_block(L, R, W, SD, SR, 64);
                    if (b > 200u) for (i2 = 0; i2 < 64u; i2++) if (L[i2] > pk) pk = L[i2];
                }
                if (!f) CHECK(pk < 900, "LOW KILL: 50 Hz down (%d of ~10900)", pk);
                else CHECK(pk > 9000, "LOW KILL: 3 kHz through (%d)", pk);
            }
            cfx_held = 0;
            for (b = 0; b < 40u; b++) { for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 0; W[i2] = 0; } cfx_block(L, R, W, SD, SR, 64); }
        }
        /* TAPE STOP: the buffer borrowed, quiet within a beat, given back after */
        {
            uint32_t ph = 0;
            cfx_held = CX_BIT(CX_TAPE);
            for (b = 0; b < 22050u / 64u + 40u; b++) {
                for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = og_sin(ph >> 16) / 3; ph += 440u * 97391u; W[i2] = 0; }
                cfx_block(L, R, W, SD, SR, 64);
                if (b == 2u) CHECK(sl_lent && cx.tape == 1u, "TAPE STOP has the buffer");
                if (b == 4u) { pk = 0; for (i2 = 0; i2 < 64u; i2++) if (L[i2] > pk) pk = L[i2]; CHECK(pk > 5000, "it starts at speed (%d)", pk); }
            }
            pk = 0; for (i2 = 0; i2 < 64u; i2++) if ((L[i2] < 0 ? -L[i2] : L[i2]) > pk) pk = L[i2] < 0 ? -L[i2] : L[i2];
            CHECK(pk < 300, "stopped: quiet (%d)", pk);
            cfx_press(PF_R16, 1);
            CHECK(!(perf_held & PF_BIT(PF_R16)), "REPEAT waits while TAPE STOP has the buffer");
            cfx_press(PF_R16, 0);
            cfx_held = 0;
            for (b = 0; b < 20u; b++) { for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 7000; W[i2] = 0; } cfx_block(L, R, W, SD, SR, 64); }
            CHECK(!sl_lent && !cx.tape && L[63] == 7000, "let go: live again, the buffer back (%d)", L[63]);
        }
        /* NOISE RISER: up over 4 bars, loud but not past full scale */
        {
            int64_t e0 = 0, e1 = 0;
            uint32_t nb = 64u * P / 64u;
            cfx_held = CX_BIT(CX_NOISE); pk = 0;
            for (b = 0; b < nb + 10u; b++) {
                for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 0; W[i2] = SR[i2] = 0; }
                cfx_block(L, R, W, SD, SR, 64);
                for (i2 = 0; i2 < 64u; i2++) {
                    if (b < nb / 4u) e0 += (int64_t)L[i2] * L[i2]; else if (b > nb) e1 += (int64_t)L[i2] * L[i2];
                    if ((L[i2] < 0 ? -L[i2] : L[i2]) > pk) pk = L[i2] < 0 ? -L[i2] : L[i2];
                }
            }
            e1 /= 640; e0 /= (int64_t)(nb / 4u) * 64;
            printf("  (NOISE RISER: the first bar's mean square %lld, the end's %lld, peak %d)\n", (long long)e0, (long long)e1, pk);
            CHECK(e1 > 16 * e0 && e1 > 2000000 && pk < 40000, "NOISE RISER swells (%lld -> %lld, peak %d)", (long long)e0, (long long)e1, pk);
            cfx_held = 0;
            for (b = 0; b < 40u; b++) { for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 0; W[i2] = 0; } cfx_block(L, R, W, SD, SR, 64); }
            CHECK(L[63] == 0, "let go: gone");
        }
        /* HP RISER: after 4 bars 100 Hz is gone; let go, the drop: back at once */
        {
            uint32_t ph = 0, nb = 64u * P / 64u;
            cfx_held = CX_BIT(CX_HPR); pk = 0;
            for (b = 0; b < nb + 20u; b++) {
                for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = og_sin(ph >> 16) / 3; ph += 100u * 97391u; W[i2] = 0; }
                cfx_block(L, R, W, SD, SR, 64);
                if (b > nb) for (i2 = 0; i2 < 64u; i2++) if (L[i2] > pk) pk = L[i2];
            }
            CHECK(pk < 600, "HP RISER at its top: 100 Hz out (%d)", pk);
            cfx_held = 0;
            for (b = 0; b < 4u; b++) { for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 5000; W[i2] = 0; } cfx_block(L, R, W, SD, SR, 64); }
            CHECK(L[63] == 5000, "the drop: the mix whole again (%d)", L[63]);
        }
        /* THROW: the dry into the delay's send, its feedback up, still a bar after; FREEZE: nothing in, feedback 100 */
        song.g[G_DFDBK] = 40;
        cfx_held = CX_BIT(CX_THROW);
        for (b = 0; b < 4u; b++) { for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 8000; W[i2] = SD[i2] = SR[i2] = 0; } cfx_block(L, R, W, SD, SR, 64); }
        CHECK(SD[63] > 7000 && cfx_goff[GF_DFDBK] == 48, "THROW: into the delay, feedback to 88 (%d, %d)", SD[63], cfx_goff[GF_DFDBK]);
        cfx_held = 0;
        for (b = 0; b < 8u; b++) { for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 8000; W[i2] = SD[i2] = SR[i2] = 0; } cfx_block(L, R, W, SD, SR, 64); }
        CHECK(SD[63] == 0 && cfx_goff[GF_DFDBK] == 48, "let go: nothing more in, the echoes ring on");
        for (b = 0; b < 16u * P / 64u + 2u; b++) { for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 0; W[i2] = SD[i2] = SR[i2] = 0; } cfx_block(L, R, W, SD, SR, 64); }
        CHECK(cfx_goff[GF_DFDBK] == 0 && !cx.busy, "a bar on: the delay as it was");
        cfx_held = CX_BIT(CX_FREEZE);
        for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 8000; W[i2] = 0; SD[i2] = 500; }
        cfx_block(L, R, W, SD, SR, 64);
        CHECK(SD[10] == 0 && cfx_goff[GF_DFDBK] == 60, "FREEZE: the send shut, feedback at 100");
        cfx_held = CX_BIT(CX_WASH);
        for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 8000; W[i2] = SR[i2] = 0; }
        for (b = 0; b < 4u; b++) cfx_block(L, R, W, SD, SR, 64);
        CHECK(SR[63] > 0 && cfx_goff[GF_RSIZE] > 0 && L[63] < 8000, "WASH: into the reverb, larger, the dry back");
        /* KICK KILL: the kit's kicks muted while held, as they were after; DRUMS ONLY: the kits soloed */
        dx_mute = 0;
        cfx_held = CX_BIT(CX_KICK) | CX_BIT(CX_DRUMS);
        cfx_block(L, R, W, SD, SR, 64);
        CHECK((dx_mute & DXG_KICK) && perf_solo, "KICK KILL, DRUMS ONLY (%x %x)", dx_mute, perf_solo);
        {
            uint32_t k2, ok = 1;
            for (k2 = 0; k2 < NTRK; k2++)
                ok &= ((perf_solo >> k2) & 1u) == (ENGINES[eng_idx(trk[k2].eng_req)] == &ENG_DRUM);
            CHECK(ok, "DRUMS ONLY: the kits only (%x)", perf_solo);
        }
        cfx_held = 0;
        cfx_block(L, R, W, SD, SR, 64);
        CHECK(!(dx_mute & DXG_KICK) && !perf_solo, "let go: the kicks and every track back");
        for (b = 0; b < 16u * P / 64u + 40u; b++) { for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 0; W[i2] = 0; } cfx_block(L, R, W, SD, SR, 64); }
        CHECK(!cx.busy, "every share out");
        /* the pad's keys: white 4 GATE 1/16, black 7 KICK KILL */
        CHECK(CFX_W[3] == CX_CODE + CX_GATE16 && CFX_B[6] == CX_CODE + CX_KICK && CFX_W[0] == PF_R8, "the key map");
    }

    /* BPM in tenths (HOME held: a knob's step a tenth), the 'TF' block */
    {
        int16_t *vp;
        uint8_t blk[8];
        song.g[G_BPM] = 128; bpm_frac = 0;
        cz.mode = CM_HOME; cz.tab[CM_HOME] = 0;
        fm1_in.buttons |= 1u << panel.btn[B_HOME];
        cos_turn(0, 1); cos_turn(0, 1);
        CHECK(song.g[G_BPM] == 128 && bpm_frac == 2, "HOME held: 128.2 (%d.%u)", song.g[G_BPM], bpm_frac);
        fm1_in.buttons &= ~(1u << panel.btn[B_HOME]);
        cos_turn(0, 1);
        CHECK(song.g[G_BPM] >= 129 && bpm_frac == 2, "a whole one, the tenth kept (%d.%u)", song.g[G_BPM], bpm_frac);
        CHECK(cos_cdesc(CC_BPM, &vp) && *vp == song.g[G_BPM] * 10 + 2, "the knob's value in tenths");
        song.g[G_BPM] = 128;
        CHECK(proj_tf(blk, 0, 0) == 3u && blk[0] == 'T' && blk[2] == 2u, "written: TF 2");
        bpm_frac = 0;
        CHECK(proj_tf(blk, 0, 8) == 3u && bpm_frac == 2u, "read back");
        blk[0] = 'X';
        proj_tf(blk, 0, 8);
        CHECK(bpm_frac == 0u, "a project without it: a whole BPM");
    }


    /* the master COMP (THRESH RATIO KNEE MAKEUP INPUT MIX SC HPF) and LIMIT (DRIVE RELEASE AUTO LV), from the knobs */
    {
        uint32_t b, i2, ph = 0;
        int32_t gr_full, gr_sc, pk;
        int16_t *vp;
        memset(&favorites.factory[6][8], 0, 10);
        cos_cput(CC_COMP, 1); cos_cput(CC_CTHR, -20); cos_cput(CC_CRAT, 7); cos_cput(CC_CKNEE, 0);
        ui_polls();
        CHECK(mc_amt && mc.thr == -20 && mc.rat == 7 && mc.knee == 0 && mc.mix == 100 && mc.in == 0, "COMP ON, -20 dB, 20:1, hard knee");
        CHECK(cos_cdesc(CC_CRAT, &vp) && *vp == 7 && cos_cdesc(CC_CTHR, &vp) && *vp == -20, "the knobs read back");
        comp_env = 0; comp_g = 32768;
        for (b = 0; b < 400u; b++) {                     /* a 1 kHz sine at -6 dBFS: 14 dB over, at 20:1 ~13 dB down */
            for (i2 = 0; i2 < 64u; i2++) { mix_l[i2] = mix_r[i2] = og_sin(ph >> 16) / 2; ph += 1000u * 97391u; wet[i2] = 0; }
            comp_block(64);
        }
        gr_full = comp_gr;
        CHECK(gr_full * 6 / 256 >= 11 && gr_full * 6 / 256 <= 15, "20:1 at -20: about 13 dB (%d)", gr_full * 6 / 256);
        pk = 0; for (i2 = 0; i2 < 64u; i2++) if (mix_l[i2] > pk) pk = mix_l[i2];
        CHECK(pk < 5000, "the peak down to near the threshold (%d)", pk);
        cos_cput(CC_CMK, 12); ui_polls();
        for (b = 0; b < 400u; b++) { for (i2 = 0; i2 < 64u; i2++) { mix_l[i2] = mix_r[i2] = og_sin(ph >> 16) / 2; ph += 1000u * 97391u; wet[i2] = 0; } comp_block(64); }
        { int32_t p2 = 0; for (i2 = 0; i2 < 64u; i2++) if (mix_l[i2] > p2) p2 = mix_l[i2]; CHECK(p2 > 3 * pk, "MAKEUP +12 dB (%d -> %d)", pk, p2); }
        cos_cput(CC_CMK, 0); cos_cput(CC_CMIX, 50); ui_polls();
        for (b = 0; b < 400u; b++) { for (i2 = 0; i2 < 64u; i2++) { mix_l[i2] = mix_r[i2] = og_sin(ph >> 16) / 2; ph += 1000u * 97391u; wet[i2] = 0; } comp_block(64); }
        { int32_t p2 = 0; for (i2 = 0; i2 < 64u; i2++) if (mix_l[i2] > p2) p2 = mix_l[i2]; CHECK(p2 > 2 * pk && p2 < 16384, "MIX 50: the dry under it (%d)", p2); }
        cos_cput(CC_CMIX, 100); cos_cput(CC_CSC, 4); ui_polls();   /* SC HPF 120 Hz: a 50 Hz kick hardly reaches the detector */
        comp_env = 0;
        for (b = 0; b < 600u; b++) { for (i2 = 0; i2 < 64u; i2++) { mix_l[i2] = mix_r[i2] = og_sin(ph >> 16) / 2; ph += 50u * 97391u; wet[i2] = 0; } comp_block(64); }
        gr_sc = comp_gr;
        cos_cput(CC_CSC, 0); ui_polls(); comp_env = 0;
        for (b = 0; b < 600u; b++) { for (i2 = 0; i2 < 64u; i2++) { mix_l[i2] = mix_r[i2] = og_sin(ph >> 16) / 2; ph += 50u * 97391u; wet[i2] = 0; } comp_block(64); }
        CHECK(gr_sc + 256 < comp_gr, "SC HPF: 50 Hz squeezed less (%d dB vs %d)", gr_sc * 6 / 256, comp_gr * 6 / 256);
        cos_cput(CC_CIN, -12); cos_cput(CC_CRAT, 0); ui_polls(); comp_env = 0;
        for (b = 0; b < 400u; b++) { for (i2 = 0; i2 < 64u; i2++) { mix_l[i2] = mix_r[i2] = og_sin(ph >> 16) / 2; ph += 1000u * 97391u; wet[i2] = 0; } comp_block(64); }
        CHECK(comp_gr * 6 / 256 <= 2, "INPUT -12: under the threshold, little reduction (%d)", comp_gr * 6 / 256);
        cos_cput(CC_LIN, 6); cos_cput(CC_LREL, 0); cos_cput(CC_LVL, 1); ui_polls();
        CHECK(lim_in > 60000 && lim_in < 70000 && lim_rs == 8u && lev_off, "LIMIT: DRIVE +6 dB, RELEASE 5 MS, AUTO LV OFF (%d)", lim_in);
        memset(&favorites.factory[6][8], 0, 10); favorites.factory[15][4] = 0; ui_polls();
        CHECK(!mc_amt && lim_in == 32768 && lim_rs == 11u && !lev_off && mc.thr == -12 && mc.rat == 3, "0s: the defaults");
    }


    /* DUB: GEN on an empty synth track: DUB STAB and offbeat four-note stabs in the key; MOVE, VOICING, a new groove */
    {
        uint32_t j, ns = 0, off = 0, bar[4] = {0, 0, 0, 0}, ok = 1;
        const cpg_t *pp; uint32_t n;
        track_t *t3 = &trk[3];
        for (j = 0; j < NSTEP; j++) step_clear(&t3->step[j]);
        set_engine_of(t3, ENGI_ANALOG); t3->p[P_ROOT] = 9;   /* (A) */
        track_select(3); cz.mode = CM_HOME; cos_mode(CM_GEN);
        cos_pages(CM_GEN, &pp, &n);
        CHECK(n == 7u && pp[0].kind == CK_DB && pp[3].kind == CK_AC, "a synth track's GEN: DUB, then ACID");
        CHECK(trk_eng(t3) == ENGI_ANALOG && t3->p[P_VOICE] == V_POLY && t3->p[P_DLY] > 0, "DUB STAB, poly, echo on it");
        for (j = 0; j < (uint32_t)t3->p[P_SLEN]; j++) {
            const step_t *st = &t3->step[j];
            if (st->time != ST_NOTE || !st->n) continue;
            ns++; off += (j & 3u) == 2u; bar[j / 16u]++;
            ok &= st->n == 4u && st->note[0] % 12u == 9u && st->note[1] == st->note[0] + 3u && st->note[2] == st->note[0] + 7u
                  && st->note[3] == st->note[0] + 10u && st->note[0] >= 36u && st->note[0] <= 55u;
        }
        CHECK(t3->p[P_SLEN] == 32 && bar[0] && bar[1], "2 bars, stabs in each (%d)", t3->p[P_SLEN]);
        CHECK(ok, "A MIN7 every stab, root low (C3 .. G3 or an octave below)");
        CHECK(ns >= 4u && off * 2u >= ns, "mostly on the offbeat eighths (%u of %u)", off, ns);
        db_put(DK_MOVE, 1);
        {
            int32_t r0 = -1, r1 = -1;
            for (j = 0; j < 32u; j++) if (t3->step[j].time == ST_NOTE && t3->step[j].n) { if (j < 16u && r0 < 0) r0 = t3->step[j].note[0]; if (j >= 16u && r1 < 0) r1 = t3->step[j].note[0]; }
            CHECK(r0 >= 0 && r1 >= 0 && r0 != r1, "MOVE 2 CHORDS: the second bar moved (%d -> %d)", r0, r1);
        }
        db_put(DK_SHAPE, 2); db_put(DK_VOIC, 3);
        for (j = 0; j < 32u && !(t3->step[j].time == ST_NOTE && t3->step[j].n); j++) {}
        CHECK(j < 32u && t3->step[j].note[3] - t3->step[j].note[0] > 15u, "MIN11 OPEN: spread wider than the shape");
        {
            uint8_t before[32];
            for (j = 0; j < 32u; j++) before[j] = t3->step[j].time == ST_NOTE && t3->step[j].n;
            fm1_ms += 3000u; db_oct(1, 0);
            for (ns = 0, j = 0; j < 32u; j++) ns += before[j] != (t3->step[j].time == ST_NOTE && t3->step[j].n);
            CHECK(ns > 0u, "OCT+: a new groove (%u steps changed)", ns);
            db_oct(0, 0);
            for (ns = 0, j = 0; j < 32u; j++) ns += before[j] != (t3->step[j].time == ST_NOTE && t3->step[j].n);
            CHECK(ns == 0u, "OCT-: the one before");
        }
        {   /* EVERY: a mutation on the bar's last beat, AMOUNT of the stabs thrown again; OCT- on DUB 3 back to the anchor */
            uint8_t before[32];
            uint32_t ch;
            db_put(DK_AMT, 60); db_put(DK_EVERY, 1);
            for (j = 0; j < 32u; j++) before[j] = t3->step[j].time == ST_NOTE && t3->step[j].n;
            song.playing = 1; bar_n = 0; beat_n = 3; beat_pos = beat_samples() - 10u;
            db_tick();
            for (ch = 0, j = 0; j < 32u; j++) ch += before[j] != (t3->step[j].time == ST_NOTE && t3->step[j].n);
            CHECK(db_of(3)->ms && ch > 0u, "EVERY 1 BAR: mutated on the bar (%u steps)", ch);
            db_tick();
            CHECK(db_of(3)->bar == 1u, "once a bar");
            song.playing = 0;
            db_oct(0, 1);
            for (ch = 0, j = 0; j < 32u; j++) ch += before[j] != (t3->step[j].time == ST_NOTE && t3->step[j].n);
            CHECK(!db_of(3)->ms && ch == 0u, "OCT- on DUB 3: the anchor again");
            db_put(DK_EVERY, 0);
        }
        db_put(DK_DENS, 0);
        for (ns = 0, j = 0; j < 32u; j++) ns += t3->step[j].time == ST_NOTE && t3->step[j].n;
        CHECK(ns == 0u, "DENSITY 0: silence");
        cz.mode = CM_HOME;
    }


    /* the live layer (cos_live.c): mutes and solo on the bar, FILL in the bar's second half, BREAKDOWN -> BUILD -> DROP */
    {
        uint32_t b = beat_samples(), j, sd = 0;
        track_t *t0 = &trk[0];
        song.playing = 1; cos_mq = 2; beat_n = 0; beat_pos = 0;
        trk[2].p[P_MUTE] = 0; live_qmute = 1u << 2;
        live_tick(64);
        CHECK(!trk[2].p[P_MUTE] && live_qmute, "MUTE Q BAR: not yet in the bar");
        beat_n = 3; beat_pos = b - 10u; live_tick(64);
        CHECK(trk[2].p[P_MUTE] && !live_qmute, "the next block starts the bar: muted");
        cos_mq = 1; beat_n = 1; beat_pos = b - 10u; live_qmute = 1u << 2; live_tick(64);
        CHECK(!trk[2].p[P_MUTE], "MUTE Q BEAT: on the next beat");
        perf_solo = 0; live_qsolo = 1u; live_qsolo_on = 1; beat_pos = 0; live_tick(64);
        CHECK(!perf_solo, "SOLO waits for the beat");
        beat_pos = b - 1u; live_tick(64);
        CHECK(perf_solo == 1u, "SOLO T1 on the beat");
        perf_solo = 0; cos_mq = 2;
        /* BREAKDOWN */
        bd_state = BD_OFF; bd_req = 1; beat_n = 0; beat_pos = 0; live_tick(64);
        CHECK(bd_state == BD_ARM_BREAK && !cfx_auto, "BREAK asked: on the next bar");
        beat_n = 3; beat_pos = b - 5u; live_tick(64);
        CHECK(bd_state == BD_BREAK && cfx_auto == (CX_BIT(CX_KICK) | CX_BIT(CX_LOWK)), "the bar: BREAK (kick and lows out)");
        bd_req = 1; beat_n = 1; beat_pos = 0; live_tick(64);
        CHECK(bd_state == BD_ARM_BUILD && (cfx_auto & CX_BIT(CX_LOWK)), "BUILD asked: the break holds till the bar");
        beat_n = 3; beat_pos = b - 5u; live_tick(64);
        CHECK(bd_state == BD_BUILD && bd_bars == 4u && (cfx_auto & CX_BIT(CX_HPR)) && (cfx_auto & CX_BIT(CX_NOISE)), "BUILD: four bars of risers");
        fill_req = 0;
        for (j = 0; j < 3u; j++) live_tick(64);           /* (three bar lines) */
        CHECK(bd_state == BD_BUILD && bd_bars == 1u && fill_req, "the last bar of the build: a fill asked for");
        live_tick(64);
        CHECK(bd_state == BD_OFF && !cfx_auto, "the DROP: all back on the bar");
        fill_req = 0;
        song.playing = 0; bd_req = 1; live_tick(64);
        CHECK(bd_state == BD_BREAK, "stopped: at once");
        bd_kill = 1; live_tick(64);
        CHECK(bd_state == BD_OFF && !cfx_auto, "FX + OCT-: off");
        song.playing = 1;
        /* FILL: T1 a kit on 1/16, step 7 playing; the second half rolled, the bar after as it was */
        set_engine_of(t0, ENGI_DRUM);
        for (j = 0; j < NSTEP; j++) step_clear(&t0->step[j]);
        t0->p[P_SLEN] = 16; t0->p[P_SDIV] = 2; t0->p[P_MUTE] = 0; t0->p[P_SOFS] = 0; t0->seq_idx = 7;
        t0->step[8].time = ST_NOTE; t0->step[8].hit = 1u;   /* (the kick on the third beat, kept) */
        fill_req = 1; beat_n = 1; beat_pos = b - 5u; live_tick(64);
        for (j = 8; j < 16u; j++) sd += (t0->step[j].hit >> 1) & 1u;
        CHECK(fill_on && seq_fill && sd == 6u && (t0->step[8].hit & 1u) && (t0->step[15].hit & 4u), "FILL: the snare roll (%u), the kick kept, a clap last", sd);
        CHECK(t0->step[0].time != ST_NOTE, "the bar's first half untouched");
        beat_n = 3; beat_pos = b - 5u; live_tick(64);
        CHECK(!fill_on && !seq_fill && t0->step[8].hit == 1u && t0->step[15].time != ST_NOTE, "the bar after: back as it was");
        song.playing = 0;
    }


    /* GLO > LINK: a PROGRAM CHANGE on the link's channel picks the section (% 4); PC OUT sends a section asked for here */
    {
        cos_cput(CC_PCCH, 10); cos_cput(CC_PCOUT, 1);
        CHECK(cos_pc_ch == 10u && cos_pc_out && favorites.factory[6][19] == 10u, "LINK on channel 10, PC OUT on");
        midi_song_req = 0;
        midi_event(0xC0u, 9u, 5u, 0);
        CHECK(midi_song_req == 2u && cos_pc_in, "PC 5 (a Syntakt's A06): section B (%u)", midi_song_req);
        midi_song_req = 0; cos_pc_in = 0;
        midi_event(0xC0u, 8u, 1u, 0);
        CHECK(midi_song_req == 0u, "another channel: not the link's");
        cos_pc_out_req = 3; live_tick(64);
        CHECK(!cos_pc_out_req, "PC OUT: sent with the next block");
        cos_cput(CC_PCCH, 0); cos_cput(CC_PCOUT, 0); midi_song_req = 0;
    }

    printf("cos_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
