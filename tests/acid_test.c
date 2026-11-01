/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) SEQ > ACID (firmware/src/ui_acid.c): the 303 line generator against the real UI sources.
 *   cc -w -Ibuild/gen -Ifirmware/src -Itests -o build/t/acid_test tests/acid_test.c -lm && build/t/acid_test */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { uint8_t time[64], note[64], flags[64]; uint32_t len; } snap_t;
static void snap(const track_t *t, snap_t *s)
{
    uint32_t i;
    memset(s, 0, sizeof *s);
    s->len = (uint32_t)t->p[P_SLEN];
    for (i = 0; i < 64u; i++) {
        s->time[i] = t->step[i].time;
        s->note[i] = t->step[i].time == ST_NOTE ? t->step[i].note[0] : 0;
        s->flags[i] = t->step[i].time == ST_NOTE ? (uint8_t)(t->step[i].flags & (SF_ACCENT | SF_SLIDE)) : 0;
    }
}
static uint32_t rhythm_eq(const snap_t *a, const snap_t *b)
{
    uint32_t i;
    for (i = 0; i < a->len; i++) if (a->time[i] != b->time[i]) return 0;
    return a->len == b->len;
}
static uint32_t notes_eq(const snap_t *a, const snap_t *b)   /* (on the steps both play) */
{
    uint32_t i, same = 0, both = 0;
    for (i = 0; i < a->len; i++)
        if (a->time[i] == ST_NOTE && b->time[i] == ST_NOTE) { both++; same += a->note[i] == b->note[i]; }
    return both && same == both;
}
static uint32_t count(const snap_t *s, uint32_t time) { uint32_t i, n = 0; for (i = 0; i < s->len; i++) n += s->time[i] == time; return n; }
static uint32_t distinct_pc(const snap_t *s)
{
    uint32_t i, m = 0, n = 0;
    for (i = 0; i < s->len; i++) if (s->time[i] == ST_NOTE) m |= 1u << (s->note[i] % 12u);
    for (; m; m &= m - 1u) n++;
    return n;
}

int main(void)
{
    track_t *t;
    snap_t a, b, c;
    uint32_t i, k, n;
    ui_power_on();
    song.sel = 4;
    t = TSEL;
    set_engine_of(t, ENGI_ANALOG);
    go_title("ACID");

    /* determinism: the same settings, the same line */
    ac_put(AK_DENS, 60); snap(t, &a);
    ac_put(AK_DENS, 61); ac_put(AK_DENS, 60); snap(t, &b);
    CHECK(rhythm_eq(&a, &b) && notes_eq(&a, &b) && !memcmp(a.flags, b.flags, 64), "a knob turned back: the same line");
    CHECK(a.len == 16u, "16 steps by default (%u)", a.len);
    CHECK(a.time[0] == ST_NOTE, "step 1 always plays");

    /* density extremes */
    ac_put(AK_DENS, 0); snap(t, &c);
    CHECK(count(&c, ST_NOTE) == 0u, "DENSITY 0: silence (%u notes)", count(&c, ST_NOTE));
    ac_put(AK_DENS, 100); ac_put(AK_TIE, 0); snap(t, &c);
    CHECK(count(&c, ST_NOTE) >= 14u, "DENSITY 100: nearly every step (%u)", count(&c, ST_NOTE));
    ac_put(AK_DENS, 60); ac_put(AK_TIE, 15);

    /* STEPS */
    ac_put(AK_STEPS, 64); snap(t, &c);
    CHECK(c.len == 64u, "STEPS 64 (%u)", c.len);
    ac_put(AK_STEPS, 16);

    /* NOTES 1: the root alone, octaves allowed */
    ac_put(AK_OCT, 0);
    ac_put(AK_NOTES, 1); snap(t, &c);
    CHECK(distinct_pc(&c) == 1u, "NOTES 1: one pitch class (%u)", distinct_pc(&c));
    for (n = 2; n <= 7u; n++) {
        ac_put(AK_NOTES, (int32_t)n); snap(t, &c);
        CHECK(distinct_pc(&c) <= n, "NOTES %u: at most %u pitch classes (%u)", n, n, distinct_pc(&c));
    }
    ac_put(AK_NOTES, 4); ac_put(AK_OCT, 20);

    /* ROOT / SCALE: every note in the key */
    ac_put(AK_ROOT, 10); ac_put(AK_SCALE, 2);            /* A PHRY */
    for (k = 0; k < 20u; k++) {
        ac_oct(1, 0); snap(t, &c);
        for (i = 0; i < c.len; i++)
            if (c.time[i] == ST_NOTE) {
                uint32_t pc = (c.note[i] + 12u - 9u) % 12u;
                CHECK((SCALE_MASK[8] >> pc) & 1u, "A PHRY: note %u out of the key", c.note[i]);
            }
    }

    /* LOCK RHYTHM: OCT+ keeps the rhythm, changes the notes */
    ac_put(AK_LOCK, AL_RHY); snap(t, &a);
    for (k = 0, n = 0; k < 8u; k++) { ac_oct(1, 0); snap(t, &b); CHECK(rhythm_eq(&a, &b), "LOCK RHYTHM keeps the rhythm"); n += !notes_eq(&a, &b); }
    CHECK(n >= 6u, "LOCK RHYTHM: the notes change (%u of 8)", n);
    /* LOCK NOTES: the pool and the seed kept: the rhythm changes */
    ac_put(AK_LOCK, AL_NOT); snap(t, &a);
    for (k = 0, n = 0; k < 8u; k++) { ac_oct(1, 0); snap(t, &b); n += !rhythm_eq(&a, &b); }
    CHECK(n >= 6u, "LOCK NOTES: the rhythm changes (%u of 8)", n);
    /* LOCK R+N: only accents and slides */
    ac_put(AK_LOCK, AL_RN); snap(t, &a);
    for (k = 0; k < 8u; k++) { ac_oct(1, 0); snap(t, &b); CHECK(rhythm_eq(&a, &b) && notes_eq(&a, &b), "LOCK R+N keeps rhythm and notes"); }

    /* OCT-: the throws before, in order */
    ac_put(AK_LOCK, AL_OFF);
    snap(t, &a); ac_oct(1, 0); snap(t, &b); ac_oct(1, 0); ac_oct(0, 0); snap(t, &c);
    CHECK(rhythm_eq(&b, &c) && notes_eq(&b, &c), "OCT- one back");
    ac_oct(0, 0); snap(t, &c);
    CHECK(rhythm_eq(&a, &c) && notes_eq(&a, &c), "OCT- two back");

    /* mutation: AMOUNT 0 changes nothing; a mutation then ANCHOR returns */
    snap(t, &a);
    ac_put(AK_AMT, 0); ac_oct(1, 1); snap(t, &b);
    CHECK(rhythm_eq(&a, &b) && notes_eq(&a, &b), "AMOUNT 0: a mutation changes nothing");
    ac_put(AK_AMT, 30);
    for (k = 0, n = 0; k < 8u; k++) { ac_oct(1, 1); snap(t, &b); n += !(rhythm_eq(&a, &b) && notes_eq(&a, &b)); }
    CHECK(n >= 6u, "AMOUNT 30: mutations change the line (%u of 8)", n);
    ac_oct(0, 1); snap(t, &c);
    CHECK(rhythm_eq(&a, &c) && notes_eq(&a, &c), "OCT- on ACID 3: back to the anchor");

    /* slides only into a note; accents louder */
    for (k = 0; k < 30u; k++) {
        ac_put(AK_SLIDE, 100); ac_put(AK_ACC, 50); ac_oct(1, 0);
        for (i = 0; i < t->p[P_SLEN]; i++) {
            const step_t *s = &t->step[i];
            if (s->time != ST_NOTE) continue;
            if (s->flags & SF_SLIDE) CHECK(i + 1u < (uint32_t)t->p[P_SLEN] && t->step[i + 1u].time == ST_NOTE, "a slide into a note (step %u)", i);
            CHECK(!(s->flags & SF_ACCENT) || s->vel > 100u, "an accent louder");
        }
    }

    /* a drum track: refused, its steps untouched */
    song.sel = 0; set_engine_of(TSEL, ENGI_DRUM); go_title("ACID");
    snap(TSEL, &a); ac_put(AK_DENS, 90); ac_oct(1, 0); snap(TSEL, &b);
    CHECK(!memcmp(&a, &b, sizeof a), "a drum track: ACID leaves it alone");

    printf("acid_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
