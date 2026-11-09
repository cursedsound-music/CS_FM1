/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursedsound) cursed-os's knobs wired: every knob on every page of every mode, on each kind of track, turned a long
 * way (40 detents right, or left at the top) while a pattern plays on all five tracks: the sound must change. The
 * knobs that do not touch the sound by themselves (settings, MIDI, what a generator does next, a depth at 0 ...) are
 * listed with the reason, and what they do is checked in cos_test.c.
 *   cc -O2 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/cos_wire_test tests/cos_wire_test.c -lm
 *   (WIRE_LIST=1: every knob and how much it changed the sound) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int set_knob(uint32_t m, const char *tab, uint32_t k, int32_t target, const char *name);

static int fails, turned, heard, quiet;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define RB (2u * 44100u / CTL)                         /* blocks rendered: 2 s, a bar at 120 */
#define RB_LONG (4u * RB)                              /* .. four bars: what changes every bar (GEN's mutations) */
static int32_t base[RB_LONG][2u * CTL], got[2u * CTL];
static uint32_t rb_n = RB;

/* every random generator from its start: each case renders as the baseline unless the knob changed it */
static void rng_seed_all(void)
{
    dx_mot_rng = 0x9E3779B9u; ap_rng = 0x2545F491u; rng_state = 0x1234567u; mod_seed = 0x2545F491u; pfx_seed = 0x9E3779B9u;
}

/* the device's power-on, then a pattern on each track: the kit four on the floor with snare and hats, the synths a
 * line of six notes (the pad its chords) */
static void boot(void)
{
    uint32_t i, t;
    ui_power_on();
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    load_depth++; fm6_init(); power_on_parts(); load_depth--;
    song.sel = 0;
    song.master_q12 = 4096;                              /* (the master pot at unity) */
    cz.flip = 1;
    ui_polls();
    for (t = 0; t < trk_live; t++) {
        track_t *tr = &trk[t];
        tr->p[P_SLEN] = 16;
        tr->p[P_MUTE] = 0;
        for (i = 0; i < 16u; i++) {
            step_t *s = &tr->step[i];
            memset(s, 0, sizeof *s);
            s->vel = 100;
            if (drum_track(tr)) {                         /* (every lane somewhere, odd steps too: SWING) */
                s->hit = (uint8_t)((i % 4u == 0u ? 1u : 0u) | (i % 8u == 4u ? 2u : 0u) | (i % 2u == 0u ? 8u : 0u) |
                                   (i == 3u ? 4u : 0u) | (i == 7u ? 16u : 0u) | (i == 11u ? 32u : 0u) | (i == 13u ? 64u : 0u) |
                                   (i == 15u ? 128u : 0u) | (i == 9u ? 4u : 0u));
                s->time = s->hit ? ST_NOTE : ST_REST;
            } else if (i % 3u == 0u || i == 7u) {          /* (and an odd step: SWING) */
                static const uint8_t L[6] = {48, 51, 55, 58, 53, 46};
                s->note[0] = (uint8_t)(L[(i / 3u) % 6u] + (t == 3u ? 12u : 0u));
                if (t == 3u) { s->note[1] = (uint8_t)(s->note[0] + 3u); s->note[2] = (uint8_t)(s->note[0] + 7u); s->n = 3; }
                else s->n = 1;
                s->time = ST_NOTE;
                if (i == 3u || i == 9u) s->flags |= SF_ACCENT;   /* (ACID's ACCENT and SLIDE: something to act on) */
                if (i == 6u || i == 12u) s->flags |= SF_SLIDE;
                if (i == 7u) s->note[0] = 50;
            } else
                s->time = ST_REST;
        }
    }
    for (t = 0; t < trk_live; t++) { trk[t].p[P_DLY] = 50; trk[t].p[P_REV] = 50; }   /* (the FX pages: something in) */
    for (t = 0; t < 4u; t++) macro_v[t] = 64;            /* (the macros at their middle: a routing heard) */
    rng_seed_all();
}

/* the macros routed on every track, as a sound's MOD pages would (M1..M4 on MOD 1..4 to the filter, the level ..) */
static void route_macros(void)
{
    static const char *const SRC[4] = {"M1", "M2", "M3", "M4"}, *const DST[4] = {"CUT", "AMP", "DLY", "PAN"};
    uint32_t t, j;
    for (t = 0; t < trk_live; t++) {
        char tab[8] = "MOD 1";
        track_select(t);
        for (j = 0; j < 4u; j++) {
            tab[4] = (char)('1' + j);
            set_knob(CM_SRC, tab, 0, 0, SRC[j]);
            set_knob(CM_SRC, tab, 1, 0, DST[j]);
            set_knob(CM_SRC, tab, 2, 70, 0);
        }
    }
    track_select(0);
}

static void render(int32_t (*into)[2u * CTL], double *diff)
{
    uint32_t b, i;
    transport_req = 1;
    ui_input();
    if (diff) *diff = 0;
    for (b = 0; b < rb_n; b++) {
        memset(got, 0, sizeof got);
        mix_block(got, CTL);
        if ((b & 15u) == 0u) ui_input();               /* (the main loop now and then: MORPH, the polls) */
        if (into) memcpy(into[b], got, sizeof got);
        if (diff)
            for (i = 0; i < 2u * CTL; i++) {
                double d = (double)got[i] - (double)base[b][i];
                *diff += d * d;
            }
    }
    transport_req = 2;
    ui_input();
}

/* the knobs that do not change the sound by themselves, and why (their effect: cos_test.c) */
static const char *why_quiet(uint32_t m, const cpg_t *p, uint32_t k)
{
    uint32_t id = p->id[k];
    if (p->kind == CK_COS) {
        if (id == CC_SLOT) return "SAVE: the slot a press saves to";
        if (id == CC_NAME) return "SAVE: the name the next save takes";
        if (id == CC_IDLE) return "SYSTEM: when the IDLE screen comes up";
        if (id == CC_MCCH || id == CC_MCDIR || id == CC_MCMAP || (id >= CC_MC0 && id <= CC_MC3) || id == CC_MOCC)
            return "LINK 2/3: the macros' and MORPH's MIDI CCs";
        if (id == CC_PCCH || id == CC_PCOUT || id == CC_VJCH) return "LINK: program changes and the VJ notes";
        if (id == CC_MUTEQ) return "MUTE Q: when a mute lands";
        if (id == CC_MOUT || id == CC_CLOCK || id == CC_BT || id == CC_LOAD) return "MIDI / SYSTEM settings";
        if (id == CC_MOBARS) return "MORPH BARS: a glide's length";
        if (id == CC_MOPOS) return "MORPH A-B: nothing stored to morph between";
        if (id == CC_OUTCH) return "MIDI OUT CH";
        if (id == CC_TRKS) return "TRACKS: how many play (5 already)";
        if (id == CC_DSEL) return "DRUM: which drum the knobs edit (cos_test.c)";
        if (id == CC_DE0 + DE_SUS) return "SUSTAIN: only while a step or key holds the drum (the test's steps are short)";
        if (id == CC_DSNP && dx_kit[dx_ui_lane & 7u].a[DXP_NOISE] < 16u)
            return "SNAP: the noise's envelope, and this drum has little noise";
    }
    if (p->kind == CK_GLO && (id == G_CLOCK || id == G_CLOCK)) return "MIDI: the clock and MIDI out";
    if (m == CM_DRONE) return "DRONE: plays while keys hold notes";
    if (id == ID_MUTEQ) return "MUTE Q: when a mute lands";
    if (m == CM_SRC && str_eq(p->tab, "CHORD")) return "CHORD: what a key plays (live, recorded)";
    if (m == CM_GEN && (str_eq(p->tab, "ACID 3") || str_eq(p->tab, "ACID 4") || str_eq(p->tab, "DUB 3") ||
                        str_eq(p->tab, "MUTATE")) && rb_n == RB) return "GEN: the mutations, every N bars (WIRE_LONG=1)";
    if ((m == CM_GEN || m == CM_TRIG) && p->kind == CK_EU && str_eq(p->tab, "EUCLID") && k == 3u && cos_kit())
        return "EUCLID LANE: the lane the next one writes";
    if (m == CM_TRIG && p->kind == CK_EU && str_eq(p->tab, "EUCLID") && k == 3u && rb_n == RB)
        return "EUCLID VARY: the bars after the first, which answer it (past this run)";
    if (m == CM_SRC && trk_engp(TSEL) == &ENG_ACID && id == P_DEC && !TSEL->p[P_E6])
        return "ACID DRV TYPE: DRIVE at 0, no drive to shape";
    if (m == CM_GEN && p->kind == CK_DB && id == DK_MOVE && rb_n == RB)
        return "DUB MOVE: the loop's later chords, past this run (WIRE_LONG=1: heard)";
    if (m == CM_GEN && p->kind == CK_AC && id == AK_LOCK) return "ACID 4 LOCK: what the next OCT+ keeps (tests/acid_test.c)";
    if (m == CM_GEN && p->kind == CK_EU && id == EK_U0 + 1u && !eu_of(song.sel)->mu[3])
        return "MUTATE AMOUNT: WHAT GROOVE is a new seed each time, the amount moves RHYTHM and FEEL";
    if (trk_engp(TSEL) == &ENG_FORMANT && (id == P_ED_FLT || id == P_LD_FLT || (str_eq(p->tab, "MOD 1") && k != 1u)))
        return "VOICE singing: its vowels are the words', no filter to move (MOD 1: to CUT)";
    if (trk_engp(TSEL) == &ENG_FORMANT && id == P_E7) return "VOICE's LOOP: the words' loop";
    if (cos_kit() && str_eq(p->tab, "MOD 1") && k != 1u) return "a kit: CUT moved only with an LFO or ENV on it (voice.c)";
    (void)m;
    return 0;
}

#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <math.h>

static void run(pid_t c);
/* shared with the forked cases: the baseline, the counts */
static int32_t (*sbase)[2u * CTL];
static struct { int turned, heard, quiet, notheard, stuck; } *cnt;

/* a knob of mode m's page `tab` to a value by turning it (a turn's side effects as on the device) */
static int set_knob(uint32_t m, const char *tab, uint32_t k, int32_t target, const char *name)
{
    const cpg_t *pp;
    const param_desc_t *d;
    int16_t *vp;
    uint32_t n, t, i;
    cos_pages(m, &pp, &n);
    for (t = 0; t < n && !str_eq(pp[t].tab, tab); t++) {}
    if (t >= n) return 0;
    cz.mode = (uint8_t)m; cz.tab[m] = (uint8_t)t;
    d = cos_knob(cos_page(), k, &vp);
    if (!d || !vp) return 0;
    for (i = 0; i < 400u && *vp != d->min; i++) { host_ticks += 1000000u; cos_turn(k, -1); d = cos_knob(cos_page(), k, &vp); }
    for (i = 0; i < 400u; i++) {
        char v[16]; const char *u = 0;
        param_format(d, *vp, v, &u);
        if (name ? str_eq(v, name) : *vp >= target) return 1;
        host_ticks += 1000000u; cos_turn(k, 1); d = cos_knob(cos_page(), k, &vp);
    }
    return 0;
}

/* a page's group: the pages that work together (LFO 1, LFO 1 TO, LFO 1+; COMP, COMP 2, COMP 3; DELAY, DELAY 2 ..) */
static void group_of(char *g, const char *tab)
{
    uint32_t l;
    str_cpy(g, tab, 16);
    l = str_len(g);
    if (l && g[l - 1u] == '+') g[--l] = 0;
    if (l > 3u && str_eq(g + l - 3u, " TO")) g[l -= 3u] = 0;
    if (l > 2u && g[l - 2u] == ' ' && (g[l - 1u] == '2' || g[l - 1u] == '3') && !(g[0] == 'L' && g[1] == 'F') &&
        !(g[0] == 'F' && g[1] == 'I') && !(g[0] == 'M' && g[1] == 'O'))
        g[l -= 2u] = 0;
}

/* the page's group engaged: what is at its least (OFF, 0, a bipolar's middle) to 60 %, so its other knobs are heard */
static void engage(uint32_t m, const char *tab)
{
    const cpg_t *pp;
    uint32_t n, t, k;
    char g[16], h[16];
    group_of(g, tab);
    cos_pages(m, &pp, &n);
    for (t = 0; t < n; t++) {
        group_of(h, pp[t].tab);
        if (!str_eq(g, h)) continue;
        for (k = 0; k < 4u; k++) {
            const param_desc_t *d;
            int16_t *vp;
            cz.mode = (uint8_t)m; cz.tab[m] = (uint8_t)t;
            d = cos_knob(cos_page(), k, &vp);
            if (!d || !vp || pp[t].id[k] == ID_OCT || pp[t].id[k] == ID_BPM || (pp[t].kind == CK_TRK && pp[t].id[k] == P_MUTE))
                continue;
            if (d->fmt == F_ENUM && !(d->names && d->names[0] && str_eq(d->names[0], "OFF")) && d->max - d->min > 1)
                continue;                                 /* (a list: only an OFF is switched on; a type is left as it is) */
            if (*vp == d->min || (d->min < 0 && d->max > 0 && *vp == 0))
                set_knob(m, pp[t].tab, k, d->min < 0 && d->max > 0 ? (d->max * 6 / 10 > 0 ? d->max * 6 / 10 : 1)
                                                                    : d->min + ((d->max - d->min) * 6 / 10 > 0 ? (d->max - d->min) * 6 / 10 : 1), 0);   /* (an ON / OFF: ON) */
        }
    }
    if (str_eq(g, "CHORD")) set_knob(m, "VOICE", 0, 0, "POLY");   /* (a chord: POLY) */
    if (str_eq(g, "VOICE")) set_knob(m, "VOICE", 0, 0, "UNI");    /* (DETUNE: the unison's) */
    if (str_eq(g, "LFO 2")) engage(m, "FILTER 2");        /* (LFO 2 moves FILTER 2: on) */
    if (str_eq(g, "DIST")) set_knob(m, "SEND", 0, 60, 0); /* (the DIST page: the send in) */
    if (str_eq(g, "AMP")) {                              /* (DEC, SUS: mid; the notes held long: the envelope heard) */
        set_knob(m, "AMP", 1, 30, 0); set_knob(m, "AMP", 2, 64, 0);   /* (a short DECAY: the SUSTAIN reached in a step) */
        TSEL->p[P_SGATE] = (int16_t)track_desc(TSEL, P_SGATE)->max;
    }
    if (m == CM_GEN && (str_eq(g, "ACID") || str_eq(g, "DUB") || str_eq(g, "MUTATE"))) {   /* (a mutation every bar, all of it) */
        const cpg_t *q; uint32_t nn, tt, kk;
        cos_pages(m, &q, &nn);
        for (tt = 0; tt < nn; tt++)
            if (str_eq(q[tt].tab, str_eq(g, "MUTATE") ? "MUTATE" : str_eq(g, "DUB") ? "DUB 3" : "ACID 3"))
                for (kk = 0; kk < 4u; kk++) {
                    const param_desc_t *d; int16_t *vp;
                    cz.mode = (uint8_t)m; cz.tab[m] = (uint8_t)tt;
                    d = cos_knob(cos_page(), kk, &vp);
                    if (!d) continue;
                    if (str_eq(d->label, "EVERY")) set_knob(m, q[tt].tab, kk, 0, d->names && str_eq(d->names[1], "1 BAR") ? "1 BAR" : 0),
                                                    d->names && str_eq(d->names[1], "1 BAR") ? (void)0 : (void)set_knob(m, q[tt].tab, kk, 1, 0);
                    else if (str_eq(d->label, "AMOUNT") || str_eq(d->label, "AMT")) set_knob(m, q[tt].tab, kk, d->max, 0);
                }
    }
    if (str_eq(g, "KEY")) {                              /* (SEQ: the sequencer's notes in the scale; one outside it) */
        set_knob(m, "KEY", 0, 0, "MIN");                 /* (CHR, the power-on's, quantizes nothing) */
        set_knob(m, "KEY", 1, 0, "SEQ");
        TSEL->step[12].note[0] = 49;
    }
}

/* one knob, in a copy (fork) of the page's engaged state: turned (each of the turns in its own copy, the first heard
 * counts), the sound rendered, compared */
static void one(uint32_t tr, uint32_t m, uint32_t t, uint32_t k, int list)
{
    static const int32_t TURN[6] = {40, -40, 5, -5, 1, -1};
    const cpg_t *p;
    const param_desc_t *d;
    int16_t *vp;
    int32_t v0;
    uint32_t j, moved = 0, ok = 0;
    double best = 0;
    const char *q;
    char nm[96];
    cz.mode = (uint8_t)m; cz.tab[m] = (uint8_t)t;
    p = cos_page();
    d = cos_knob(p, k, &vp);
    if (!d || !vp || d->max == d->min) return;
    snprintf(nm, sizeof nm, "%s > %s K%u %s (T%u %s)", CM_NAME[m], p->tab, k + 1u, d->label, tr + 1u, trk_engp(TSEL)->name);
    v0 = *vp;
    for (j = 0; j < 6u && !ok; j++) {
        int fd[2];
        pid_t c;
        if (pipe(fd)) return;
        c = fork();
        if (!c) {
            uint32_t i;
            int32_t v1;
            double diff;
            for (i = 0; i < (uint32_t)(TURN[j] < 0 ? -TURN[j] : TURN[j]); i++) { host_ticks += 1000000u; cos_turn(k, TURN[j] < 0 ? -1 : 1); }
            cz.mode = (uint8_t)m; cz.tab[m] = (uint8_t)t;
            d = cos_knob(cos_page(), k, &vp);
            v1 = vp ? *vp : v0;
            memcpy(base, sbase, sizeof base);
            cz.mode = CM_HOME;                            /* (HOME while it plays, as on a stage) */
            render(0, &diff);
            if (v1 == v0) diff = -1;                      /* (did not move this way) */
            if (write(fd[1], &diff, sizeof diff) != sizeof diff) {}
            _exit(0);
        }
        close(fd[1]);
        {
            double diff = -1;
            if (read(fd[0], &diff, sizeof diff) != sizeof diff) diff = -1;
            close(fd[0]);
            run(c);
            if (diff >= 0) { moved = 1; if (diff > best) best = diff; if (diff > 1e6) ok = 1; }
        }
    }
    __sync_fetch_and_add(&cnt->turned, 1);
    q = why_quiet(m, p, k);
    if (list) printf("%-64s %s %g\n", nm, ok ? "HEARD" : moved ? "quiet" : "STUCK", best);
    if (!moved) { __sync_fetch_and_add(&cnt->stuck, 1); printf("STUCK %s: either way, still %d\n", nm, v0); }
    else if (ok) __sync_fetch_and_add(&cnt->heard, 1);
    else if (q) __sync_fetch_and_add(&cnt->quiet, 1);
    else { __sync_fetch_and_add(&cnt->notheard, 1); printf("NOT HEARD %-64s from %d (difference at most %g)\n", nm, v0, best); }
    fflush(stdout);
}

static void run(pid_t c) { int st; if (c > 0) waitpid(c, &st, 0); }

/* the page's group engaged and its baseline rendered (in a copy), then each knob of the page */
static void page(uint32_t tr, uint32_t m, uint32_t t, int list)
{
    const cpg_t *pp;
    uint32_t n, k;
    pid_t b;
    char tab[16];
    cos_pages(m, &pp, &n);
    str_cpy(tab, pp[t].tab, sizeof tab);
    engage(m, tab);
    cos_pages(m, &pp, &n);                               /* (engaging may change the list: DUB's stab makes a bass POLY, the
                                                         * pages in another order; the page by its name, as cos_turn keeps it) */
    for (t = 0; t < n && !str_eq(pp[t].tab, tab); t++) {}
    if (t >= n) return;
    b = fork();
    if (!b) { cz.mode = CM_HOME; render(base, 0); memcpy(sbase, base, sizeof base); _exit(0); }
    run(b);
    for (k = 0; k < 4u; k++) one(tr, m, t, k, list);
}

int main(void)
{
    uint32_t m, tr, t, n, k;
    int list = getenv("WIRE_LIST") != 0;
    sbase = mmap(0, sizeof base, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (getenv("WIRE_LONG")) rb_n = RB_LONG;
    cnt = mmap(0, sizeof *cnt, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    boot();
    route_macros();
    fflush(stdout);
    for (tr = 0; tr < 5u; tr++) {
        pid_t a = fork();
        if (a) { run(a); continue; }
        track_select(tr);                                 /* (each track picked: its own clean copy) */
        {
            pid_t b = fork();
            if (!b) { render(base, 0); memcpy(sbase, base, sizeof base); _exit(0); }
            run(b);
        }
        if (tr == 0u) {                                   /* (the premise: the copy renders as the baseline; the tracks play) */
            double d, e = 0;
            uint32_t b2, i2;
            pid_t b = fork();
            if (!b) { memcpy(base, sbase, sizeof base); render(0, &d); _exit(d != 0.0); }
            { int st; waitpid(b, &st, 0); CHECK(WEXITSTATUS(st) == 0, "a second copy renders as the baseline"); }
            for (b2 = RB / 2u; b2 < RB; b2++) for (i2 = 0; i2 < 2u * CTL; i2++) e += (double)sbase[b2][i2] * sbase[b2][i2];
            printf("  (the baseline: %.0f RMS)\n", sqrt(e / (RB / 2u * 2u * CTL)));
            CHECK(e > 1e9, "the five tracks play");
            if (fails) _exit(1);
        }
        for (m = 0; m < CM_N; m++) {
            const cpg_t *pp;
            if (rb_n == RB_LONG && m != CM_GEN && m != CM_TRIG) continue;   /* (WIRE_LONG: GEN's and SEQ's pages, four bars each) */
            cos_pages(m, &pp, &n);
            for (t = 0; t < n; t++) {
                pid_t c = fork();                     /* (each page: its own copy, its group engaged) */
                if (!c) { page(tr, m, t, list); _exit(0); }
                run(c);
            }
        }
        fflush(stdout);
        _exit(fails != 0);
    }
    printf("cos_wire_test: %d knobs turned, %d changed the sound, %d settings that do not (%d unexplained), %d stuck, %s\n",
           cnt->turned, cnt->heard, cnt->quiet, cnt->notheard, cnt->stuck,
           fails || cnt->notheard || cnt->stuck ? "FAILED" : "ok");
    return fails || cnt->notheard || cnt->stuck;
}
