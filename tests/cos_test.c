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

/* a panel button pressed (down 1) or let go, through cos_input */
static void cos_test_btn(uint32_t b, int down)
{
    uint32_t m = 1u << panel.btn[b];
    if (down) { fm1_in.buttons |= m; host_pressed = m; }
    else { fm1_in.buttons &= ~m; host_released = m; }
    cos_input();
}

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
        CHECK(mc_amt == 0 && lim_t == 8412, "COMP 0 off; LIMIT byte 6: CEILING -12 dBFS (%d)", lim_t);
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
        {
            project_t *q = &proj_scratch;
            project_capture(q);
            CHECK(q->tf == 2u && proj_tf(q, blk, 0, 0) == 3u && blk[0] == 'T' && blk[2] == 2u, "captured and written: TF 2");
            q->tf = 0; bpm_frac = 0;
            CHECK(proj_tf(q, blk, 0, 8) == 3u && q->tf == 2u && bpm_frac == 0u, "read back into the project, not the music");
            blk[0] = 'X';
            proj_tf(q, blk, 0, 8);
            CHECK(q->tf == 0u, "a project without it: a whole BPM");
        }
    }


    /* HOME held: PERFORM (K1 BPM) wherever HOME was; let go: back. A tap on HOME: its next page. MACROS: M1..M4 */
    {
        int16_t *vp;
        cz.mode = CM_HOME; cz.tab[CM_HOME] = 3;          /* (MACROS) */
        cos_cput(CC_MAC0 + 1u, 90);
        CHECK(macro_v[1] == 90 && cos_cdesc(CC_MAC0 + 1u, &vp) && *vp == 90, "MACROS: M2 90");
        cos_cput(CC_MAC0, 300);
        CHECK(macro_v[0] == 127, "M1 kept to 127 (%u)", macro_v[0]);
        macro_v[0] = macro_v[1] = 0;
        cos_test_btn(B_HOME, 1);
        CHECK(cz.tab[CM_HOME] == 0u, "held: PERFORM");
        cos_turn(0, 1);
        cos_test_btn(B_HOME, 0);
        CHECK(cz.tab[CM_HOME] == 3u, "let go after a knob: MACROS again (%u)", cz.tab[CM_HOME]);
        cos_test_btn(B_HOME, 1); cos_test_btn(B_HOME, 0);
        CHECK(cz.tab[CM_HOME] == 4u, "a tap: the next page, MORPH (%u)", cz.tab[CM_HOME]);
        cz.mode = CM_FX; cz.tab[CM_HOME] = 2;
        cos_test_btn(B_HOME, 1);
        CHECK(cz.mode == CM_HOME && cz.tab[CM_HOME] == 0u, "from FX: HOME, PERFORM while held");
        cos_test_btn(B_HOME, 0);
        CHECK(cz.tab[CM_HOME] == 2u, "let go: the HOME page it was on (%u)", cz.tab[CM_HOME]);
        song.g[G_BPM] = 128; bpm_frac = 0;
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
        set_engine_of(t3, ENGI_ANALOG); t3->p[P_ROOT] = 9; t3->p[P_VOICE] = V_POLY;   /* (A; a POLY track, the pad) */
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


    /* the isolator: MID KILL (a notch at ~1 kHz), HIGH KILL (24 dB an octave at ~2.9 kHz); the knobs on the master EQ */
    {
        int32_t L[64], R[64], W[64], SD[64], SR[64], pk;
        uint32_t b, i2, f, ph = 0;
        int16_t *vp;
        static const uint32_t HZ[3] = {1000, 8000, 100};
        for (f = 0; f < 3u; f++) {
            cfx_home = f == 0u ? CX_BIT(CX_MIDK) : CX_BIT(CX_HIK); pk = 0;
            if (f == 2u) cfx_home = CX_BIT(CX_HIK);
            for (b = 0; b < 400u; b++) {
                for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = og_sin(ph >> 16) / 3; ph += HZ[f] * 97391u; W[i2] = SD[i2] = SR[i2] = 0; }
                cfx_block(L, R, W, SD, SR, 64);
                if (b > 200u) for (i2 = 0; i2 < 64u; i2++) if (L[i2] > pk) pk = L[i2];
            }
            if (f == 0u) CHECK(pk < 1500, "MID KILL: 1 kHz down (%d of ~10900)", pk);
            if (f == 1u) CHECK(pk < 800, "HIGH KILL: 8 kHz down (%d)", pk);
            if (f == 2u) CHECK(pk > 9500, "HIGH KILL: 100 Hz through (%d)", pk);
        }
        cfx_home = 0;
        for (b = 0; b < 40u; b++) { for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 0; W[i2] = 0; } cfx_block(L, R, W, SD, SR, 64); }
        CHECK(!cx.busy, "let go: out");
        cos_cput(CC_EQM, -6); cos_cput(CC_EQH, 4); cos_cput(CC_EQL, -12);
        CHECK(iso_v[0] == -12 && iso_v[1] == -6 && iso_v[2] == 4 && cos_cdesc(CC_EQM, &vp) && *vp == -6 &&
              !(eq_v[0] | eq_v[1] | eq_v[2] | eq_v[3]), "ISOLATOR: LOW MID HIGH its own (cos_iso.c), JIANT's EQ untouched");
        {                                                 /* a project keeps it ('IS'); one without it opens flat */
            project_t *q = &proj_scratch;
            cos_cput(CC_EQL, ISO_KILL);
            project_capture(q);
            CHECK(proj_pack(&proj_wire, q), "packed");
            iso_v[0] = iso_v[1] = iso_v[2] = 0;
            CHECK(proj_import(q, &proj_wire, PROJ_STORE_SIZE) && q->iso[0] == ISO_KILL && q->iso[1] == -6 && q->iso[2] == 4,
                  "ISOLATOR: read back in the project (%d %d %d)", q->iso[0], q->iso[1], q->iso[2]);
            project_restore_runtime(q);
            CHECK(iso_v[0] == ISO_KILL && iso_v[1] == -6 && iso_v[2] == 4, "ISOLATOR: a load applies it");
            iso_v[0] = iso_v[1] = iso_v[2] = 0;
            project_capture(q);
            CHECK(proj_pack(&proj_wire, q) && proj_import(q, &proj_wire, PROJ_STORE_SIZE) && !(q->iso[0] | q->iso[1] | q->iso[2]),
                  "ISOLATOR flat: no block, flat back");
#if COS_SLIM
            q->iso[0] = q->iso[1] = q->iso[2] = 0;            /* (a 0.5 project: its ISOLATOR was JIANT's EQ) */
            q->eqv[0] = -6; q->eqv[1] = q->eqv[2] = -3; q->eqv[3] = 2;
            q->sum = proj_sum(q);
            project_restore_runtime(q);
            CHECK(iso_v[0] == -6 && iso_v[1] == -3 && iso_v[2] == 2 && !(eq_v[0] | eq_v[1] | eq_v[2] | eq_v[3]),
                  "a cursed-os 0.5 project: its EQ becomes the ISOLATOR (%d %d %d)", iso_v[0], iso_v[1], iso_v[2]);
            iso_v[0] = iso_v[1] = iso_v[2] = 0;
#endif
        }
        iso_v[0] = iso_v[1] = iso_v[2] = 0;
    }


    /* TAP TEMPO (HOME + white 16): four taps 469 ms apart, 127.9 BPM; a pause over 2 s starts again */
    {
        uint32_t t0 = 100000u;
        cz_tap.n = 0;
        cos_tap(t0); cos_tap(t0 + 469u); cos_tap(t0 + 938u); cos_tap(t0 + 1407u);
        CHECK(song.g[G_BPM] == 127 && bpm_frac == 9u, "TAP: 127.9 (%d.%u)", song.g[G_BPM], bpm_frac);
        cos_tap(t0 + 5000u);
        CHECK(cz_tap.n == 1u, "a pause: from the start");
        cos_tap(t0 + 5500u);
        CHECK(song.g[G_BPM] == 120 && bpm_frac == 0u, "two taps 500 ms apart: 120.0");
        song.g[G_BPM] = 120; bpm_frac = 0;
    }


    /* VJ CH: the live layer's events out as notes (60 BREAK .. 62 DROP, 63 FILL, 64.. a section), CC 20 the build */
    {
        uint32_t b = beat_samples(), w0, i2, got60 = 0, got61 = 0, got62 = 0, cc = 0;
        cos_cput(CC_VJCH, 3);
        usb.config = 1; w0 = mo_w;
        song.playing = 1; bd_state = BD_OFF; bd_req = 1; beat_n = 0; beat_pos = 0; live_tick(64);
        beat_n = 3; beat_pos = b - 5u; live_tick(64);          /* (BREAK) */
        bd_req = 1; live_tick(64);                             /* (BUILD on this bar line too) */
        for (i2 = 0; i2 < 4u; i2++) live_tick(64);             /* (the four bars: the DROP) */
        for (i2 = w0; i2 != mo_w; i2++) {
            uint32_t pk = midi_out_q[i2 % MQ], st = (pk >> 8) & 0xFFu, d1 = (pk >> 16) & 0xFFu;
            if (st == 0x92u && d1 == 60u) got60++;
            if (st == 0x92u && d1 == 61u) got61++;
            if (st == 0x92u && d1 == 62u) got62++;
            if (st == 0xB2u && d1 == 20u) cc++;
        }
        CHECK(got60 == 1u && got61 == 1u && got62 == 1u && cc >= 3u, "VJ: BREAK, BUILD, DROP on channel 3, the build's CC (%u %u %u %u)", got60, got61, got62, cc);
        mo_r = mo_w; usb.config = 0; song.playing = 0; bd_state = BD_OFF; fill_req = 0;
        cos_cput(CC_VJCH, 0);
    }


    /* dub tools: FX > DELAY 2 (MIX DRIVE TYPE MOD); DUB 3's DELAY sets the echo to the stabs (1/8D), FEEDBK to 70 at least */
    {
        const cpg_t *pp; uint32_t n;
        cz.mode = CM_HOME; song.sel = 3;
        cos_pages(CM_FX, &pp, &n);
        CHECK(n == 6u && !strcmp(pp[3].tab, "DELAY 2") && !strcmp(pp[5].tab, "RUMBLE") && pp[3].id[0] == G_DMIX && pp[3].id[3] == G_DMOD, "FX: DELAY 2 after DELAY");
        song.g[G_DTIME] = 1; song.g[G_DFDBK] = 40;
        db_put(DK_DLY, 1);
        CHECK(song.g[G_DTIME] == (int16_t)DT_DOT && song.g[G_DFDBK] == 70, "DUB 3 DELAY 1/8D (%d, %d)", song.g[G_DTIME], song.g[G_DFDBK]);
        db_put(DK_DLY, 0);
        CHECK(song.g[G_DTIME] == (int16_t)DT_DOT, "KEEP leaves it");
        song.g[G_DTIME] = 1; song.g[G_DFDBK] = 60;
    }


    /* the set list: stopped, a song at once; playing, not with sections stored but unwritten; the song's A on the bar */
    {
        uint32_t s0 = song_cur;
        song.playing = 1; live.dirty = 1;
        CHECK(song_go_live((s0 + 1u) % NSONG) == 1 && song_cur == s0 && live.dirty, "playing with an unsaved section: asks first");
        CHECK(song_go_live((s0 + 2u) % NSONG) == 1 && song_cur == s0, "another song pressed: asks again");
        CHECK(song_go_live((s0 + 1u) % NSONG) == 1 && song_cur == s0, ".. for that one");
        CHECK(song_go_live((s0 + 1u) % NSONG) == 0 && song_cur == (s0 + 1u) % NSONG && !live.dirty, "again: the unsaved dropped, the next song's in (%u)", song_cur);
        song.playing = 0; song_go_live(s0); song.playing = 1;
        CHECK(song_go_live((s0 + 1u) % NSONG) == 0 && song_cur == (s0 + 1u) % NSONG, "playing, all saved: at once (%u)", song_cur);
        CHECK(live.req < 0, "(an empty song: nothing staged, the music plays on)");
        song.playing = 0;
        song_go_live(s0);
        CHECK(song_cur == s0, "stopped: back to the first");
    }


    /* performance recording: REC armed, playing, a knob turn on EDIT becomes the track's motion at its step; OCT- clears */
    {
        uint32_t i2, n0;
        track_t *t4 = &trk[3];
        int16_t *vp;
        const cpg_t *pg;
        track_select(3); set_engine_of(t4, ENGI_ANALOG);
        motion_clear(t4); song.rec = 0;
        cz.mode = CM_SRC; cz.tab[CM_SRC] = 0;
        fm1_in.buttons = 0;
        song.rec = 1u << 3; song.playing = 1; t4->seq_idx = 5; t4->seq_pos = 0;
        pg = cos_page();
        n0 = motion_count(t4);
        cos_turn(0, 3);
        CHECK(motion_count(t4) == n0 + 1u && motion_enabled(t4), "REC: the knob's move recorded (%u)", motion_count(t4));
        for (i2 = 0; i2 < motion.count; i2++)
            if (MOTION_TRK(&motion.event[i2]) == 3u) CHECK(MOTION_STEP(&motion.event[i2]) == 5u, "at the step playing");
        (void)vp; (void)pg;
        song.rec = 0; cos_turn(0, 1);
        CHECK(motion_count(t4) == n0 + 1u, "not armed: no record");
        motion_clear(t4); song.playing = 0; cz.mode = CM_HOME;
    }


    /* the FX lane: REC armed + FX held writes the pad's effects a sixteenth at a time; played back after; saved */
    {
        uint32_t P = div_samples(2), b;
        int32_t L[64], R[64], W[64], SD[64], SR[64];
        memset((void *)cfx_lane, 0, sizeof cfx_lane); cfx_lane_on = 0;
        song.playing = 1; ms_clock = 3u * P + 64u;           /* (the 4th sixteenth) */
        cfx_rec = 1; cfx_held = CX_BIT(CX_DRIVE);
        CHECK(cfx_lane_bits(64) == 0u && cfx_lane[3] == CX_BIT(CX_DRIVE) && cfx_lane_on, "recorded at sixteenth 4");
        cfx_rec = 0; cfx_held = 0;
        CHECK(cfx_lane_bits(64) == CX_BIT(CX_DRIVE), "played back there");
        ms_clock = 5u * P + 64u;
        CHECK(cfx_lane_bits(64) == 0u, ".. and not elsewhere");
        for (b = 0; b < 4u; b++) { uint32_t i2; for (i2 = 0; i2 < 64u; i2++) { L[i2] = R[i2] = 0; W[i2] = 0; } cfx_block(L, R, W, SD, SR, 64); }
        /* a project with LFO 2 / FILTER 2, the lane: packed and read back into the project only; a load applies it */
        {
            project_t *q = &proj_scratch;
            trk_x2[2][X2_RATE] = 77; bpm_frac = 3;
            project_capture(q);
            CHECK(proj_pack(&proj_wire, q), "packed");
            trk_x2[2][X2_RATE] = 5; bpm_frac = 0; cfx_lane[3] = 0; cfx_lane_on = 0;
            CHECK(proj_import(q, &proj_wire, PROJ_STORE_SIZE) && q->x2[2][X2_RATE] == 77 && q->tf == 3u && q->fxl_on &&
                  q->fxl[3] == CX_BIT(CX_DRIVE), "read back: in the project");
            CHECK(trk_x2[2][X2_RATE] == 5 && bpm_frac == 0u && !cfx_lane_on, "the music untouched by a read");
            project_restore_runtime(q);
            CHECK(trk_x2[2][X2_RATE] == 77 && bpm_frac == 3u && cfx_lane_on && cfx_lane[3] == CX_BIT(CX_DRIVE), "a load: applied");
            x2_defaults(2); bpm_frac = 0; memset((void *)cfx_lane, 0, sizeof cfx_lane); cfx_lane_on = 0;
        }
        song.playing = 0;
    }

    /* RUMBLE: a four-on-the-floor kick alone; the kick's tail between the kicks, ducked at each, off out of the chain */
    {
        static int32_t o[2u * CTL];
        uint32_t k, i, b, bs;
        int64_t lo_off[2] = {0, 0}, lo_on[2] = {0, 0}, early = 0, late = 0;
        int32_t pk_on = 0;
        int run;
        ui_power_on();
        song.master_q12 = 4096;
        trk_live = 5;
        set_engine_of(&trk[0], ENGI_DRUM); trk[0].engine = trk[0].eng_req;
        for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = k != 0u;
        CHECK(ENGINES[trk[0].engine] == &ENG_DRUM, "track 1 a kit");
        for (i = 0; i < 16u; i++) {
            step_t *s = &trk[0].step[i];
            memset(s, 0, sizeof *s);
            s->time = i % 4u ? ST_REST : ST_NOTE;
            s->vel = 110;
            s->hit = i % 4u ? 0u : 1u;
        }
        trk[0].p[P_SLEN] = 16; trk[0].p[P_DLY] = 0; trk[0].p[P_REV] = 0; trk[0].p[P_CHOR] = 0;
        song.g[G_BPM] = 128; bpm_frac = 0; song.g[G_DUCK] = 0;
        bs = beat_samples();
        for (run = 0; run < 2; run++) {
            rmb_v[0] = run ? 100 : 0; rmb_v[1] = 80; rmb_v[2] = 50; rmb_v[3] = 0;   /* (DUCK off here: the tail whole) */
            transport_req = 1; ui_input();
            for (b = 0; b < 8u * bs / CTL; b++) {              /* (two bars) */
                memset(o, 0, sizeof o);
                mix_block(o, CTL);
                if (b * CTL < 4u * bs) continue;               /* (the second bar: the tail built up) */
                for (i = 0; i < CTL; i++) {
                    uint32_t at = (b * CTL + i) % bs;          /* where in the beat */
                    int64_t v = o[2u * i];
                    int64_t *e = run ? lo_on : lo_off;
                    e[at >= bs / 2u] += v * v;                 /* (the beat's halves: the kick's, the offbeat's) */
                    if (run && (o[2u * i] > pk_on || -o[2u * i] > pk_on)) pk_on = o[2u * i] < 0 ? -o[2u * i] : o[2u * i];
                }
            }
            transport_req = 2; ui_input();
            for (b = 0; b < 200u; b++) { memset(o, 0, sizeof o); mix_block(o, CTL); }
        }
        printf("  (RUMBLE: the offbeat's energy %lld off, %lld on; the kick's half %lld -> %lld; peak %d)\n",
               (long long)lo_off[1], (long long)lo_on[1], (long long)lo_off[0], (long long)lo_on[0], pk_on);
        CHECK(lo_on[1] > 2 * lo_off[1] && lo_on[0] < 2 * lo_off[0], "RUMBLE: the kick's tail fills the offbeat, the kick itself as it was");
        CHECK(pk_on < 30000, "RUMBLE: under full scale (%d)", pk_on);
        /* STEMS: the rumble in the kit's stem (track 1's), not lost */
        {
            int64_t st[2] = {0, 0};
            for (run = 0; run < 2; run++) {
                rmb_v[0] = run ? 100 : 0;
                fx_stems = 1;
                transport_req = 1; ui_input();
                for (b = 0; b < 8u * bs / CTL; b++) {
                    memset(o, 0, sizeof o);
                    fx_stems = 1;                         /* (as stems_test: ui_input follows the menu) */
                    mix_block(o, CTL);
                    if (b * CTL < 4u * bs) continue;
                    for (i = 0; i < CTL; i++)
                        if ((b * CTL + i) % bs >= bs / 2u) st[run] += (int64_t)stem_buf[0][i] * stem_buf[0][i];
                }
                transport_req = 2; ui_input();
                fx_stems = 0;
                for (b = 0; b < 200u; b++) { memset(o, 0, sizeof o); mix_block(o, CTL); }
            }
            CHECK(st[1] > 2 * st[0], "STEMS: the rumble in the kit's stem (%lld -> %lld)", (long long)st[0], (long long)st[1]);
            rmb_v[0] = 100;
        }
        /* DUCK 100: just after the kick far less of it than at the offbeat */
        rmb_v[3] = 100;
        transport_req = 1; ui_input();
        for (b = 0; b < 8u * bs / CTL; b++) {
            memset(o, 0, sizeof o);
            mix_block(o, CTL);
            if (b * CTL < 4u * bs) continue;
            for (i = 0; i < CTL; i++) {
                uint32_t at = (b * CTL + i) % bs;
                int64_t r = rb.oz2;                            /* (the return itself, the kick not in it) */
                (void)r;
                if (at >= bs / 8u && at < bs / 4u) early += (int64_t)rb.pk * rb.pk;
                if (at >= bs * 5u / 8u && at < bs * 3u / 4u) late += (int64_t)rb.pk * rb.pk;
            }
        }
        transport_req = 2; ui_input();
        printf("  (RUMBLE DUCK 100: just after the kick %lld, at the offbeat %lld)\n", (long long)early, (long long)late);
        CHECK(late > 4 * early, "DUCK: the rumble pulled down at the kick, up into the offbeat");
        rmb_v[0] = 0;
        for (b = 0; b < 4u; b++) { memset(o, 0, sizeof o); mix_block(o, CTL); }
        CHECK(!rb.on, "LEVEL 0: out of the chain");
        /* FX > RUMBLE's knobs; a project keeps them, a read leaves the music's */
        {
            project_t *q = &proj_scratch;
            int16_t *vp;
            cos_cput(CC_RB0, 60); cos_cput(CC_RB0 + 1u, 90); cos_cput(CC_RB0 + 2u, 30); cos_cput(CC_RB3, 250);
            CHECK(rmb_v[0] == 60 && rmb_v[1] == 90 && rmb_v[2] == 30 && rmb_v[3] == 100, "the knobs (DUCK kept to 100)");
            CHECK(cos_cdesc(CC_RB0 + 1u, &vp) && *vp == 90, "read back");
            project_capture(q);
            CHECK(proj_pack(&proj_wire, q), "packed");
            rmb_v[0] = 0; rmb_v[1] = 70;
            CHECK(proj_import(q, &proj_wire, PROJ_STORE_SIZE) && q->rmb[0] == 60 && q->rmb[1] == 90 && q->rmb[3] == 100, "read: in the project");
            CHECK(rmb_v[0] == 0, "the music untouched by a read");
            project_restore_runtime(q);
            CHECK(rmb_v[0] == 60 && rmb_v[1] == 90 && rmb_v[2] == 30, "a load: applied");
            memcpy(rmb_v, RB_DEF, sizeof rmb_v);
            project_capture(q);
            CHECK(proj_rb(q, 0, 0, 0) == 0u, "at its defaults: no block written");
        }
    }

    /* MORPH: A and B stored by HOME's white 13 / 14, a glide over BARS from the next bar, KNOB 1 by hand; only what
     * differs moves; a project keeps A and B */
    {
        static int32_t o[2u * CTL];
        uint32_t b, bar, k;
        int16_t *vp;
        ui_power_on();
        memset(&mph, 0, sizeof mph); mph.bars = 3;      /* (4 bars) */
        song.g[G_BPM] = 120; bpm_frac = 0;
        bar = 4u * beat_samples();
        cz.mode = CM_HOME;
        trk[1].p[P_LEVEL] = 100; trk[1].p[P_REV] = 0; trk[2].p[P_LEVEL] = 77; macro_v[1] = 0; song.g[G_DJF] = 0;
        host_notes = 1u << WK[12]; cos_input();          /* (white 13: A stored) */
        CHECK(mph.set == 1u && mph.s[0][1 * MO_TP + 0] == 100, "white 13: A stored");
        trk[1].p[P_LEVEL] = 40; trk[1].p[P_REV] = 90; macro_v[1] = 127; song.g[G_DJF] = -30;
        host_notes = 1u << WK[13]; cos_input();          /* (white 14: B stored) */
        CHECK(mph.set == 3u && mph.pos == 1000, "white 14: B stored, at B");
        trk[2].p[P_LEVEL] = 50;                           /* (moved after both: not the morph's, A and B hold it alike) */
        host_notes = 1u << WK[12]; cos_input();          /* (A: a glide over 4 bars from the next bar) */
        CHECK(mph.gl && mph.to == 0, "white 13 again: a glide to A");
        transport_req = 1; ui_input();
        for (b = 0; b < (bar / 2u) / CTL; b++) { memset(o, 0, sizeof o); mix_block(o, CTL); }
        ui_input();
        CHECK(trk[1].p[P_LEVEL] == 40 && mph.pos == 1000, "the bar not in yet: still B (%d)", trk[1].p[P_LEVEL]);
        for (b = 0; b < (bar / 2u + 2u * bar) / CTL; b++) { memset(o, 0, sizeof o); mix_block(o, CTL); }
        ui_input();
        CHECK(mph.pos > 400 && mph.pos < 600 && trk[1].p[P_LEVEL] > 60 && trk[1].p[P_LEVEL] < 80, "half way after 2 of 4 bars (%d, LEVEL %d)", mph.pos, trk[1].p[P_LEVEL]);
        CHECK(macro_v[1] > 40 && macro_v[1] < 90 && song.g[G_DJF] < -8 && song.g[G_DJF] > -22, "the macros and the DJ FILTER with it (%u, %d)", macro_v[1], song.g[G_DJF]);
        CHECK(trk[2].p[P_LEVEL] == 50, "what A and B hold alike: left as set");
        for (b = 0; b < (3u * bar) / CTL; b++) { memset(o, 0, sizeof o); mix_block(o, CTL); ui_input(); }
        CHECK(!mph.gl && mph.pos == 0 && trk[1].p[P_LEVEL] == 100 && trk[1].p[P_REV] == 0 && macro_v[1] == 0 && song.g[G_DJF] == 0, "at A when it ends");
        transport_req = 2; ui_input();
        for (b = 0; b < 8u; b++) { memset(o, 0, sizeof o); mix_block(o, CTL); ui_input(); }
        CHECK(!song.playing, "stopped");
        cos_cput(CC_MOPOS, 25);
        CHECK(trk[1].p[P_LEVEL] == 85 && cos_cdesc(CC_MOPOS, &vp) && *vp == 25, "KNOB 1 by hand: a quarter to B (%d)", trk[1].p[P_LEVEL]);
        cos_cput(CC_MOBARS, 0);                          /* (CUT, stopped: at once) */
        host_notes = 1u << WK[13]; cos_input(); ui_input();
        CHECK(mph.pos == 1000 && trk[1].p[P_LEVEL] == 40, "CUT, stopped: at B at once (%d %d gl %u run %u playing %u)", mph.pos, trk[1].p[P_LEVEL], mph.gl, mph.run, song.playing);
        fm1_in.buttons |= 1u << panel.btn[B_HOME];
        trk[1].p[P_LEVEL] = 10;
        host_notes = 1u << WK[13]; cos_input();          /* (HOME + white 14: B stored again) */
        fm1_in.buttons &= ~(1u << panel.btn[B_HOME]);
        CHECK(mph.s[1][1 * MO_TP] == 10, "HOME + white 14: B stored again");
        {
            project_t *q = &proj_scratch;
            project_capture(q);
            CHECK(proj_pack(&proj_wire, q), "packed");
            memset(&mph, 0, sizeof mph);
            CHECK(proj_import(q, &proj_wire, PROJ_STORE_SIZE) && mph.set == 0u, "a read: the music's MORPH untouched");
            project_restore_runtime(q);
            CHECK(mph.set == 3u && mph.s[1][1 * MO_TP] == 10 && mph.s[0][1 * MO_TP] == 100 && mph.bars == 0u, "a load: A and B back");
            memset(&mph, 0, sizeof mph); mph.bars = 3;
            project_capture(q);
            CHECK(proj_mo(q, 0, 0, 0) == 0u, "nothing stored: no block written");
        }
        for (k = 0; k < NTRK; k++) (void)k;
    }

    /* HOME > LOOP: JIANT's LIVE layer on the track picked: LEN 3 loops its first three steps, RATE, DENS, SHIFT;
     * white 15: back in phase on the beat */
    {
        static int32_t o[2u * CTL];
        uint32_t b, mx = 0, beat;
        int16_t *vp;
        ui_power_on();
        song.g[G_BPM] = 120; bpm_frac = 0;
        beat = beat_samples();
        cz.mode = CM_HOME; cz.tab[CM_HOME] = 5; song.sel = 0;
        trk[0].p[P_SLEN] = 16; trk[1].p[P_SLEN] = 16; trk[1].p[P_SDIV] = trk[0].p[P_SDIV];   /* (track 2: the time kept) */
        cos_cput(CC_LVLEN, 3);
        CHECK(lv.cut[0] == 103u && lv.want[0].cut == 3u && cos_cdesc(CC_LVLEN, &vp) && *vp == 3, "LEN 3 asked for");
        transport_req = 1; ui_input();
        for (b = 0; b < 8u * beat / CTL; b++) {          /* (two bars) */
            memset(o, 0, sizeof o); mix_block(o, CTL);
            if (b * CTL > beat && trk[0].seq_idx > mx) mx = trk[0].seq_idx;
        }
        CHECK(mx < 3u && LV_ON(0), "LEN 3: the first three steps, round and round (the highest %u)", mx);
        cos_cput(CC_LVRATE, 6); cos_cput(CC_LVDN, 4); cos_cput(CC_LVSH, 2);
        CHECK(lv.want[0].rate == 2 && lv.want[0].drop == 50u && lv.want[0].rot == 2, "RATE x2, DENS -50, SHIFT 2");
        cos_cput(CC_LVDN, 11);
        CHECK(lv.want[0].rat == 3u && !lv.want[0].drop, "DENS x3: ratchets");
        host_notes = 1u << WK[14]; cos_input();          /* (white 15: back) */
        CHECK(!lv.cut[0] && !lv.rate[0] && !lv.dn[0] && !lv.rot[0], "white 15: the knobs back");
        for (b = 0; b < 2u * beat / CTL; b++) { memset(o, 0, sizeof o); mix_block(o, CTL); }
        CHECK(!LV_ON(0) && trk[0].seq_idx == trk[1].seq_idx, "on the beat: the track as written, in phase (%u, track 2 %u)", trk[0].seq_idx, trk[1].seq_idx);
        for (b = 0; b < 4u * beat / CTL; b++) { memset(o, 0, sizeof o); mix_block(o, CTL); if (trk[0].seq_idx > mx) mx = trk[0].seq_idx; }
        CHECK(mx > 8u, "all sixteen again (%u)", mx);
        transport_req = 2; ui_input();
        /* SHIFT set while stopped: from the first step on, two ahead of track 2 (seq_live.c: was lost at the first
         * 1/16, the track in phase) */
        host_notes = 1u << WK[14]; cos_input(); host_notes = 0; cos_input();
        for (b = 0; b < 2u * beat / CTL; b++) { memset(o, 0, sizeof o); mix_block(o, CTL); }
        cos_cput(CC_LVSH, 2);
        transport_req = 1; ui_input();
        for (b = 0; b < (4u * beat + beat / 2u) / CTL; b++) { memset(o, 0, sizeof o); mix_block(o, CTL); }
        CHECK(LV_ON(0) && trk[0].seq_idx % 16u == (trk[1].seq_idx + 2u) % 16u, "SHIFT 2 set while stopped: two ahead after PLAY (%u, track 2 %u)",
              trk[0].seq_idx, trk[1].seq_idx);
        transport_req = 2; ui_input();
        host_notes = 1u << WK[14]; cos_input(); host_notes = 0; cos_input();
    }

    /* the FX lane: perform.c's effects too (REPEAT, the sweeps, the mutes, GLITCH, the LOOPs), played a block ahead so
     * they go in on the sixteenth as a key held would */
    {
        uint32_t P = div_samples(2);
        memset((void *)cfx_lane, 0, sizeof cfx_lane); memset((void *)cfx_plane, 0, sizeof cfx_plane); cfx_lane_on = 0;
        song.playing = 1; ms_clock = 5u * P + 64u;
        cfx_rec = 1; perf_held = PF_BIT(PF_HPF) | PF_BIT(PF_R16) | PF_BIT(PF_OCTD);   /* (OCT-: not the pad's, not kept) */
        (void)cfx_lane_bits(64);
        CHECK(cfx_plane[5] == (PF_BIT(PF_HPF) | PF_BIT(PF_R16)) && cfx_lane_on, "HP sweep and REPEAT 1/16 recorded at sixteenth 6");
        cfx_rec = 0; perf_held = 0;
        ms_clock = 5u * P - 64u + 64u;                    /* (a block ending where sixteenth 6 begins) */
        (void)cfx_lane_bits(64);
        CHECK(perf_lane == (PF_BIT(PF_HPF) | PF_BIT(PF_R16)), "played from the block before it: on the 1/16");
        ms_clock = 7u * P + 64u; (void)cfx_lane_bits(64);
        CHECK(perf_lane == 0u, ".. and not elsewhere");
        {
            project_t *q = &proj_scratch;
            project_capture(q);
            CHECK(proj_pack(&proj_wire, q), "packed");
            memset((void *)cfx_plane, 0, sizeof cfx_plane);
            CHECK(proj_import(q, &proj_wire, PROJ_STORE_SIZE) && q->fxp[5] == (PF_BIT(PF_HPF) | PF_BIT(PF_R16)), "kept with the project ('FP')");
            project_restore_runtime(q);
            CHECK(cfx_plane[5] == (PF_BIT(PF_HPF) | PF_BIT(PF_R16)), "a load: back");
        }
        memset((void *)cfx_lane, 0, sizeof cfx_lane); memset((void *)cfx_plane, 0, sizeof cfx_plane); cfx_lane_on = 0;
        song.playing = 0; (void)cfx_lane_bits(64);
        CHECK(perf_lane == 0u, "stopped: nothing played");
    }

    /* GLO > LINK 2: the macros as CCs: out when they move (one a block), in from the other machine (not sent back) */
    {
        uint32_t w0, i2, got = 0, gv = 0, back = 0;
        int16_t *vp;
        memset(favorites.factory[8] + 24, 0, 6); ui_polls();
        CHECK(!cos_mc_ch && !cos_mc_dir && cos_mc_cc[0] == 74u && cos_mc_cc[3] == 85u && cos_cdesc(CC_MCMAP, &vp) && *vp == 1,
              "0 there (older settings): OFF, BOTH, the ST TRACK map (74 75 84 85)");
        cos_cput(CC_MCCH, 2); cos_cput(CC_MCDIR, 0); cos_cput(CC_MCMAP, 2);
        CHECK(cos_mc_cc[0] == 70u && cos_mc_cc[1] == 71u && cos_mc_cc[2] == 24u && cos_mc_cc[3] == 30u, "MAP ST FX: 70 71 24 30");
        cos_cput(CC_MC0, 70); cos_cput(CC_MC0 + 1u, 71); cos_cput(CC_MC0 + 2u, 72); cos_cput(CC_MC3, 73);   /* (LINK 3: each its own) */
        CHECK(cos_mc_cc[2] == 72u && cos_cdesc(CC_MCMAP, &vp) && *vp == 0 && cos_cdesc(CC_MC0 + 2u, &vp) && *vp == 72, "LINK 3: M3 CC 72, MAP CUSTOM");
        CHECK(favorites.factory[8][24] == 2u && favorites.factory[8][25] == 71u && favorites.factory[8][28] == 73u, "kept in the settings");
        ui_polls();
        CHECK(cos_mc_ch == 2u && cos_mc_cc[0] == 70u && cos_mc_cc[2] == 72u && cos_mc_cc[3] == 73u, "read back from them");
        usb.config = 1;
        macro_v[0] = macro_v[1] = macro_v[2] = macro_v[3] = 0;
        for (i2 = 0; i2 < 8u; i2++) live_tick(64);            /* (settled) */
        w0 = mo_w;
        macro_v[2] = 99;
        for (i2 = 0; i2 < 5u; i2++) live_tick(64);           /* (one CC a block, round the four macros and MORPH) */
        for (i2 = w0; i2 != mo_w; i2++) {
            uint32_t pk = midi_out_q[i2 % MQ], st = (pk >> 8) & 0xFFu, d1 = (pk >> 16) & 0xFFu, d2 = pk >> 24;
            if (st == 0xB1u && d1 == 72u) { got++; gv = d2; }
        }
        CHECK(got == 1u && gv == 99u, "M3 99 out as CC 72 on channel 2, once (%u, %u)", got, gv);
        midi_event(0xB0u, 1u, 71u, 64u);                      /* (the other machine's CC 71: M2) */
        CHECK(macro_v[1] == 64u, "CC 71 in: M2 64");
        w0 = mo_w;
        for (i2 = 0; i2 < 8u; i2++) live_tick(64);
        for (i2 = w0; i2 != mo_w; i2++) if ((((midi_out_q[i2 % MQ]) >> 8) & 0xFFu) == 0xB1u) back++;
        CHECK(back == 0u, "not sent back (%u)", back);
        cos_cput(CC_MCDIR, 1);
        midi_event(0xB0u, 1u, 70u, 5u);
        CHECK(macro_v[0] == 0u, "OUT only: the other machine's CC not taken");
        /* MORPH's A-B on CC 30: in from the other machine's crossfader, out from a glide; never echoed */
        cos_cput(CC_MCDIR, 0);
        cos_cput(CC_MOCC, 30);
        CHECK(cos_mo_cc == 31u && favorites.factory[8][30] == 31u && cos_cdesc(CC_MOCC, &vp) && *vp == 30, "MO CC 30, kept");
        memset(&mph, 0, sizeof mph); mph.bars = 3;
        trk[1].p[P_LEVEL] = 100; mph_read(mph.s[0]); trk[1].p[P_LEVEL] = 0; mph_read(mph.s[1]); mph.set = 3;
        midi_event(0xB0u, 1u, 30u, 127u);                     /* (the crossfader to the right: B) */
        ui_input();
        CHECK(mph.pos == 1000 && trk[1].p[P_LEVEL] == 0, "CC 30 127 in: at B (%d)", mph.pos);
        midi_event(0xB0u, 1u, 30u, 32u); ui_input();
        CHECK(mph.pos > 240 && mph.pos < 270 && trk[1].p[P_LEVEL] > 70 && trk[1].p[P_LEVEL] < 80, "CC 30 32: a quarter (%d, LEVEL %d)", mph.pos, trk[1].p[P_LEVEL]);
        w0 = mo_w; back = 0;
        for (i2 = 0; i2 < 10u; i2++) live_tick(64);
        for (i2 = w0; i2 != mo_w; i2++) if ((((midi_out_q[i2 % MQ]) >> 16) & 0xFFu) == 30u) back++;
        CHECK(back == 0u, "the crossfader's value not sent back (%u)", back);
        mph_hand(75);
        w0 = mo_w; got = gv = 0;
        for (i2 = 0; i2 < 10u; i2++) live_tick(64);
        for (i2 = w0; i2 != mo_w; i2++) {
            uint32_t pk = midi_out_q[i2 % MQ];
            if (((pk >> 8) & 0xFFu) == 0xB1u && ((pk >> 16) & 0xFFu) == 30u) { got++; gv = pk >> 24; }
        }
        CHECK(got == 1u && gv == 95u, "KNOB 1 at 75: out as CC 30 95, once (%u, %u)", got, gv);
        cos_cput(CC_MOCC, -1);
        CHECK(!cos_mo_cc && !favorites.factory[8][30], "MO CC OFF");
        memset(&mph, 0, sizeof mph); mph.bars = 3;
        mo_r = mo_w; usb.config = 0;
        cos_cput(CC_MCCH, 0); cos_cput(CC_MCDIR, 0);
        macro_v[0] = macro_v[1] = macro_v[2] = macro_v[3] = 0;
    }

    /* an external clock: the header's tempo steady (a quarter beat measured in ms: +-1 % each reading), its tempo
     * jumps taken at once; HOME + OCT and TAP do nothing then */
    {
        uint32_t f, lo = 9999, hi = 0, ch = 0, last = 0, s = 12345;
        song.g[G_CLOCK] = 1;
        for (f = 0; f < 400u; f++) {                          /* (128 BPM: a quarter beat 117.2 ms, read as 116 .. 118) */
            uint32_t dt, v;
            s = s * 1103515245u + 12345u;
            dt = 116u + (s >> 16) % 3u;
            midi_beat_samples = (uint32_t)FS * dt / 250u;
            v = cos_ext10();
            if (f > 100u) { if (v < lo) lo = v; if (v > hi) hi = v; if (last && v != last) ch++; }
            last = v;
        }
        CHECK(lo >= 1270u && hi <= 1290u, "128 BPM shown 127.0 .. 129.0 at worst (%u .. %u)", lo, hi);
        CHECK(ch < 30u, "and steady: %u changes in 300 frames", ch);
        midi_beat_samples = (uint32_t)FS * 100u / 250u;        /* (150 BPM: a jump) */
        CHECK(cos_ext10() > 1480u, "a tempo change taken at once (%u)", cos_ext10());
        {
            int16_t b0 = song.g[G_BPM];
            cz.msg[0] = 0;
            cos_tap(1000); cos_tap(1500);
            CHECK(!strcmp(cz.msg, "EXT CLOCK LEADS") && song.g[G_BPM] == b0, "TAP: the clock leads (%s)", cz.msg);
        }
        midi_beat_samples = 0; song.g[G_CLOCK] = 0;
        CHECK(cos_ext10() == 0u && !cos_follows(), "its own tempo again");
    }

    {   /* SEQ held + REC: the track emptied on the second press within 3 s; the first only asks; GEN's REC undoes it */
        uint32_t tr, n0, n1, rec0 = song.rec;
        for (tr = 0; tr < 2u; tr++) {                     /* (the kit and ACID) */
            track_select(tr);
            cz.mode = CM_TRIG;
            for (i = 0; i < 8u; i++) {                    /* (some steps, a lock) */
                step_t *st = &TSEL->step[i * 2u];
                if (drum_track(TSEL)) st->hit |= 1u; else { st->time = ST_NOTE; st->n = 1; st->note[0] = 48; }
            }
            for (i = n0 = 0; i < NSTEP; i++) n0 += TSEL->step[i].time != ST_REST || TSEL->step[i].hit;
            cos_test_btn(B_SEQ, 1);
            cos_test_btn(B_REC, 1); cos_test_btn(B_REC, 0);
            for (i = n1 = 0; i < NSTEP; i++) n1 += TSEL->step[i].time != ST_REST || TSEL->step[i].hit;
            CHECK(n0 >= 8u && n1 == n0 && song.rec == rec0, "SEQ + REC once (T%u): it asks, nothing cleared, REC not armed (%u %u)", tr + 1u, n0, n1);
            fm1_ms += 4000u;                              /* (too late: it asks again) */
            cos_test_btn(B_REC, 1); cos_test_btn(B_REC, 0);
            for (i = n1 = 0; i < NSTEP; i++) n1 += TSEL->step[i].time != ST_REST || TSEL->step[i].hit;
            CHECK(n1 == n0, "SEQ + REC after 4 s: asks again (%u)", n1);
            fm1_ms += 1000u;
            cos_test_btn(B_REC, 1); cos_test_btn(B_REC, 0);
            cos_test_btn(B_SEQ, 0);
            for (i = n1 = 0; i < NSTEP; i++) n1 += TSEL->step[i].time != ST_REST || TSEL->step[i].hit;
            CHECK(n1 == 0u && song.rec == rec0, "SEQ + REC twice (T%u): the track empty (%u steps left)", tr + 1u, n1);
            CHECK(undo_do() == 0, "undo after a clear");
            for (i = n1 = 0; i < NSTEP; i++) n1 += TSEL->step[i].time != ST_REST || TSEL->step[i].hit;
            CHECK(n1 == n0, "the clear undone (%u of %u)", n1, n0);
        }
        track_select(0);
    }
    {   /* a synth on SEQ: a step held + PRESETS its note up the scale (a rest: the root first), + B1 ACCENT, + B2 SLIDE;
         * the key let go: no trig (the step stays) */
        step_t *st;
        int32_t root;
        track_select(1);                                  /* (ACID) */
        cz.mode = CM_TRIG; cz.bank = 0;
        st = &TSEL->step[2];
        step_clear(st);
        root = 36 + (TSEL->p[P_ROOT] % 12 + 12) % 12;
        fm1_in.notes = host_notes = 1u << WK[2]; cos_input(); host_notes = 0;   /* (step 3 held) */
        host_ticks += 1000000u; host_enc[panel.enc[EN_PRESET]] = 1; cos_input();
        CHECK(st->time == ST_NOTE && st->n == 1u && st->note[0] == root, "a rest + PRESETS: the root (%u, %d)", st->note[0], root);
        host_ticks += 1000000u; host_enc[panel.enc[EN_PRESET]] = 1; cos_input();
        host_ticks += 1000000u; host_enc[panel.enc[EN_PRESET]] = 1; cos_input();
        CHECK(st->note[0] == scale_steps(TSEL, root, 2), "+ PRESETS twice: two up the scale (%u)", st->note[0]);
        host_ticks += 1000000u; host_enc[panel.enc[EN_PRESET]] = -1; cos_input();
        CHECK(st->note[0] == scale_steps(TSEL, root, 1), "PRESETS back: one down (%u)", st->note[0]);
        fm1_in.notes |= 1u << BK[0]; host_notes = 1u << BK[0]; cos_input(); host_notes = 0;
        fm1_in.notes &= ~(1u << BK[0]); cos_input();
        CHECK(st->flags & SF_ACCENT, "+ B1: ACCENT on");
        fm1_in.notes |= 1u << BK[1]; host_notes = 1u << BK[1]; cos_input(); host_notes = 0;
        fm1_in.notes &= ~(1u << BK[1]); cos_input();
        CHECK((st->flags & (SF_ACCENT | SF_SLIDE)) == (SF_ACCENT | SF_SLIDE), "+ B2: SLIDE on (%u)", st->flags);
        fm1_in.notes |= 1u << BK[0]; host_notes = 1u << BK[0]; cos_input(); host_notes = 0;
        fm1_in.notes &= ~(1u << BK[0]); cos_input();
        CHECK(!(st->flags & SF_ACCENT) && (st->flags & SF_SLIDE), "+ B1 again: ACCENT off");
        fm1_in.notes = 0; cos_input();                    /* (the step let go) */
        CHECK(st->time == ST_NOTE && st->note[0] == scale_steps(TSEL, root, 1), "let go: the step stays, no trig (%u)", st->time);
        fm1_in.notes = host_notes = 1u << WK[2]; cos_input(); host_notes = 0;
        fm1_in.notes = 0; cos_input();
        CHECK(st->time != ST_NOTE, "a tap without PRESETS: the step off, as before");
        host_ticks += 1000000u; host_enc[panel.enc[EN_PRESET]] = 1; cos_input();
        CHECK(cz.bank == 1u || cos_banks() == 1u, "nothing held: PRESETS the bank of 16, as before (%u)", cz.bank);
        cz.bank = 0;
        track_select(0);
    }
    printf("cos_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
