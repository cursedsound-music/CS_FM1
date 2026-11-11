/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) cursed-os 0.14.0, GG 2026-11-11: TRIG > COND (Elektron-style, written again):
 *   CONDITION  a step plays on 1ST, !1ST, FILL, !FILL or the pass N of every M (1:2 .. 8:8) of its track's pattern;
 *   SLIDE      a step's locks glide in from the values before, across the step;
 *   SOUND      a kit step's lane plays another drum sound (a sound lock), its own back at its next hit;
 * set by holding steps on TRIG > COND; kept by a save (proj_pack / proj_import).
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/cond_test tests/cond_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); } else printf("ok: "); printf(__VA_ARGS__); printf("\n"); } while (0)
static void frame16(void) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); ui_draw(); }
static int32_t o[2 * CTL];
static void play_samples(uint32_t n) { uint32_t b; for (b = 0; b < n / CTL; b++) mix_block(o, CTL); }
static uint32_t kit_track(void) { uint32_t k; for (k = 0; k < NTRK && trk[k].engine != ENGI_DRUM; k++) {} return k; }

int main(void)
{
    uint32_t i, k, c, n, m, ok;
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1; song.playing = 0; song.master_q12 = 4096; song.g[G_BPM] = 120; frame16();

    /* the conditions' table */
    ok = 1;
    for (c = 4; c < TC_N; c++) {
        char b[8];
        tc_nm(c, &n, &m);
        b[0] = (char)('0' + n); b[1] = ':'; b[2] = (char)('0' + m); b[3] = 0;
        ok &= n >= 1u && n <= m && !strcmp(TC_NAME[c + 1u], b);
    }
    CHECK(ok && TC_MAX < 128u && !TC_NAME[TC_N + 1u], "conditions: 1ST !1ST FILL !FILL, then N:M (M 2 3 4 8), named");

    /* a step's condition, pass by pass */
    {
        track_t *t = &trk[3];
        step_t s;
        static const struct { uint32_t c; uint8_t want[8]; } T[] = {
            {0, {1, 0, 0, 0, 0, 0, 0, 0}}, {1, {0, 1, 1, 1, 1, 1, 1, 1}}, {4, {1, 0, 1, 0, 1, 0, 1, 0}},
            {5, {0, 1, 0, 1, 0, 1, 0, 1}}, {8, {0, 0, 1, 0, 0, 1, 0, 0}}, {12, {0, 0, 0, 1, 0, 0, 0, 1}},
            {13, {1, 0, 0, 0, 0, 0, 0, 0}}, {20, {0, 0, 0, 0, 0, 0, 0, 1}}};
        memset(&s, 0, sizeof s);
        s.time = ST_NOTE; s.n = 1; s.note[0] = 48; s.vel = 100;
        ok = 1;
        for (i = 0; i < NELEM(T); i++) {
            s.probability = (uint8_t)(TC_BASE + T[i].c);
            for (k = 0; k < 8u; k++) {
                t->loops = (uint16_t)k;
                if (step_rolls(t, &s) != T[i].want[k]) { ok = 0; printf("  %s pass %u: %d\n", TC_NAME[T[i].c + 1u], k, step_rolls(t, &s)); }
            }
        }
        CHECK(ok, "1ST, !1ST, 1:2, 2:2, 2:3, 4:4, 1:8, 8:8 play on their passes");
        s.probability = (uint8_t)(TC_BASE + 2u);
        seq_fill = 1; k = (uint32_t)step_rolls(t, &s); seq_fill = 0;
        CHECK(k && !step_rolls(t, &s), "FILL: only while a FILL plays");
        CHECK(step_chance(&s) == 100u, "a condition's chance: 100 %%");
        t->loops = 0;
    }

    /* heard: a synth's step on 1:2 sounds every other pass */
    {
        track_t *t;
        uint32_t ons[4] = {0}, b, nb, pass;
        track_select(3); t = TSEL;
        for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = (int16_t)(k != 3u);
        for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
        t->p[P_SLEN] = 4;
        t->step[0].time = ST_NOTE; t->step[0].n = 1; t->step[0].note[0] = 48; t->step[0].vel = 100;
        t->step[0].probability = (uint8_t)(TC_BASE + 5u);   /* (2:2) */
        transport_req = 1; ui_input();
        nb = (uint32_t)(4u * beat_samples() / 4u / CTL);  /* (a pass: 4 sixteenths) */
        for (pass = 0; pass < 4u; pass++) {
            uint32_t seen = 0;
            for (b = 0; b < nb; b++) {
                uint32_t v;
                mix_block(o, CTL);
                for (v = 0; v < NVOICE; v++) seen |= t->v[v].active && (t->v[v].stage == 1u || t->v[v].stage == 2u);
            }
            ons[pass] = seen;
        }
        transport_req = 2; ui_input(); play_samples(FS / 4u);
        CHECK(!ons[0] && ons[1] && !ons[2] && ons[3], "2:2 heard on passes 2 and 4 (%u %u %u %u)", ons[0], ons[1], ons[2], ons[3]);
        step_clear(&t->step[0]);
    }

    /* SLIDE: a lock glides in over its step */
    {
        track_t *t = TSEL;
        int16_t base, mid = -1000, end = -1000, first = -1000;
        uint32_t b, nb, step_n = (uint32_t)(beat_samples() / 4u);
        for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
        t->p[P_SLEN] = 4;
        for (i = 0; i < 4u; i++) { t->step[i].time = ST_NOTE; t->step[i].n = 1; t->step[i].note[0] = 48; t->step[i].vel = 100; }
        motion_clear(t);
        base = t->p[P_FCUT] = 20;
        CHECK(!motion_set_lock(t, 1, P_FCUT, 100), "a lock: CUT 100 on step 2");
        t->step[1].flags |= SF_SLIDE;
        transport_req = 1; ui_input();
        nb = step_n / CTL;
        for (b = 0; b < nb; b++) mix_block(o, CTL);       /* (step 1) */
        mix_block(o, CTL); first = t->p[P_FCUT];
        for (b = 1; b < nb / 2u; b++) mix_block(o, CTL);
        mid = t->p[P_FCUT];
        for (; b < nb - 1u; b++) mix_block(o, CTL);
        end = t->p[P_FCUT];
        transport_req = 2; ui_input(); play_samples(FS / 4u);
        CHECK(first < 40 && mid > 45 && mid < 75 && end > 90, "SLIDE: CUT 20 -> 100 across step 2 (%d, %d, %d)", first, mid, end);
        t->step[1].flags &= (uint8_t)~SF_SLIDE;
        transport_req = 1; ui_input();
        for (b = 0; b < nb + 4u; b++) mix_block(o, CTL);
        mid = t->p[P_FCUT];
        transport_req = 2; ui_input(); play_samples(FS / 4u);
        CHECK(mid == 100, "no SLIDE: at once (%d)", mid);
        CHECK(t->p[P_FCUT] == base, "stopped: the sound's own value back (%d)", t->p[P_FCUT]);
        motion_clear(t);
    }

    /* SOUND: a kit's lane plays another sound on a step */
    {
        uint32_t kt = kit_track(), b, nb, lane = 3u, src = 30u;
        track_t *t;
        dx_lane_t own, want, seen1, seen2;
        uint8_t lay, wl;
        project_t q, r;
        static project_store_t st;
        track_select(kt); t = TSEL;
        for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = (int16_t)(k != kt);
        for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
        t->p[P_SLEN] = 4;
        t->step[0].hit = (uint8_t)(1u << lane); t->step[2].hit = (uint8_t)(1u << lane);
        t->step[0].time = t->step[2].time = ST_NOTE;
        own = dx_kit[lane];
        CHECK(cos_dsrc_get(src, &want, &wl) && memcmp(&want, &own, sizeof own), "source %u: another sound than the lane's", src);
        cz.mode = CM_TRIG; cz.tab[CM_TRIG] = 1; cz.lane = (uint8_t)lane; frame16();
        CHECK(cos_on_cnd(), "a kit's TRIG > COND (its second page)");
        cz.tg_down = 1u << WK[0]; fm1_in.notes = 1u << WK[0]; cz.lk_used = 0;
        cos_turn(2, 30);                                 /* (SOUND: the knob turned to it) */
        cos_cnd_put(CC_TSND, (int32_t)src);
        CHECK(cos_snd_of(t, 0, lane) == src && motion_count(t) == 1u, "held step 1, SOUND: its sound lock (%u)", cos_snd_of(t, 0, lane));
        CHECK(cz.lk_used, "the key let go: no trig toggled");
        cz.tg_down = 0; fm1_in.notes = 0;
        nb = (uint32_t)(beat_samples() / 4u / CTL);
        transport_req = 1; ui_input();
        for (b = 0; b < nb / 2u; b++) mix_block(o, CTL);
        seen1 = dx_kit[lane];
        for (; b < nb * 2u + nb / 2u; b++) mix_block(o, CTL);
        seen2 = dx_kit[lane];
        {
            dx_lane_t kk[8]; uint8_t ll[8];
            for (b = 0; b < nb * 2u; b++) mix_block(o, CTL);   /* (pass 2's step 1: swapped again) */
            dx_kit_own(kk, ll);
            CHECK(!memcmp(&kk[lane], &own, sizeof own) && !memcmp(&dx_kit[lane], &want, sizeof want),
                  "swapped: what a save reads is the lane's own");
        }
        transport_req = 2; ui_input(); play_samples(FS / 4u);
        CHECK(!memcmp(&seen1, &want, sizeof want), "step 1: the locked sound");
        CHECK(!memcmp(&seen2, &own, sizeof own), "step 3 (no lock): its own back");
        CHECK(!memcmp(&dx_kit[lane], &own, sizeof own), "stopped: its own");
        /* kept by a save */
        project_capture(&q);
        q.t[kt].step[2].probability = (uint8_t)(TC_BASE + 4u);
        q.t[kt].step[2].flags |= SF_SLIDE;
        q.sum = proj_sum(&q);
        CHECK(proj_pack(&st, &q), "packed");
        CHECK(proj_import(&r, &st, sizeof st), "loaded back");
        CHECK(r.t[kt].step[2].probability == TC_BASE + 4u && (r.t[kt].step[2].flags & SF_SLIDE), "its CONDITION and SLIDE kept");
        {
            uint32_t found = 0;
            for (i = 0; i < r.motion.count; i++)
                found |= MOTION_ID(&r.motion.event[i]) == MOT_SND(lane) && (r.motion.event[i].param & MOTION_LOCK) &&
                         MOTION_VAL(&r.motion.event[i]) == MOT_SNDV(lane, src);
            CHECK(found, "its sound lock kept");
        }
        /* cleared with the step's locks */
        CHECK(motion_clear_locks(t, 0) == 1u && !cos_snd_of(t, 0, lane), "REC + step: the sound lock goes with its locks");
    }

    /* the knobs on COND with steps held: no parameter lock */
    {
        track_t *t = &trk[3];
        track_select(3);
        motion_clear(t);
        cz.mode = CM_TRIG; cz.tab[CM_TRIG] = 2; frame16();
        CHECK(cos_on_cnd(), "a synth's TRIG > COND (its third page)");
        cz.tg_down = 1u << WK[1]; fm1_in.notes = 1u << WK[1]; cz.lk_used = 0;
        cos_turn(0, 3);
        CHECK(t->step[1].probability == TC_BASE + 2u && !motion_count(t), "knob 1 held on step 2: COND FILL (%u), no lock",
              t->step[1].probability);
        cos_turn(1, 1);
        CHECK(t->step[1].flags & SF_SLIDE, "knob 2: SLIDE on");
        cos_turn(0, -10);
        CHECK(!t->step[1].probability, "COND back to --: plays always");
        cz.tg_down = 0; fm1_in.notes = 0;
        cos_cnd_put(CC_TCOND, 3);
        CHECK(!strcmp(cz.msg, "HOLD A STEP"), "nothing held: HOLD A STEP");
    }
    printf(fails ? "cond: %d FAILED\n" : "cond: all ok\n", fails);
    return fails != 0;
}
