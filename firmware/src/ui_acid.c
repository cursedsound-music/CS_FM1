/* SPDX-License-Identifier: GPL-3.0-only
 * cursedsound edition: SEQ > ACID, a controlled-random 303 line generator. Included by ui_graph.c after ui_euclid.c
 * (it uses EUCLID's key finder eu_key and degree helper eu_deg).
 *
 * A line is three independent parts, each with its own seed, so each can be kept while the others are thrown again:
 *   RHYTHM  which sixteenths play, which are held over (TIE)
 *   NOTES   a small pool of scale degrees (NOTES: 1..7, the root always in it) and which plays where, octave jumps
 *   FLAGS   accents and slides
 * ACID    KNOB 1 DENSITY, 2 SLIDE, 3 ACCENT, 4 OCTAVE        (amounts 0..100)
 * ACID 2  KNOB 1 STEPS (4..64), 2 NOTES (pool size), 3 TIE, 4 LOCK (OFF, RHYTHM, NOTES, R+N: what OCT+ keeps)
 * ACID 3  KNOB 1 EVERY (OFF, 1..8 bars), 2 AMOUNT, 3 ROOT (AUTO, C..B), 4 SCALE (AUTO, MIN, PHRY, DOR, HARM, MAJ, PEN)
 * ACID 4  (cursed-os) KNOB 1 LOCK, 2 ROTATE (0 .. STEPS - 1: the line's start that many steps later, the steps as they
 *         are now, hand edits and step locks with them; a new throw or a mutation keeps the offset)
 * OCT+ on ACID / ACID 2: a new throw of the parts LOCK does not keep; OCT- walks back through the last eight throws.
 * OCT+ on ACID 3: a mutation now (AMOUNT % of the steps thrown again from a mutation seed); OCT- back to the anchor.
 * EVERY > OFF: a mutation every EVERY bars, on the bar (turn EVERY back to OFF to freeze the line where it is).
 * The key: ROOT / SCALE when set; else the track's root (the song's key) in its SCL scale, or minor without one.
 * Every knob turn writes the whole line again (the same seeds: the same line for the same settings); the track's
 * automation stays. The settings are the session's (RAM), the steps are the track's as any other (saved with it) */
#define AC_HIST 8u
typedef struct { uint16_t r, n, f; } acs_t;            /* the three parts' seeds */
typedef struct {
    uint8_t on, dens, slide, acc, oct, steps, notes, tie, lock, every, amt, root, scale, ctr, bar, rot;
    uint16_t ms;                                        /* the mutation seed (0: the anchor, as thrown) */
    acs_t s, hist[AC_HIST];
    uint8_t nh;                                         /* throws in hist */
} ac_t;
static ac_t ac[NTRK] COS_RAM;
static uint32_t ac_ms COS_RAM;                                  /* (the last hand gesture: an UNDO point after 2 s) */
enum { AK_DENS, AK_SLIDE, AK_ACC, AK_OCT, AK_STEPS, AK_NOTES, AK_TIE, AK_LOCK, AK_EVERY, AK_AMT, AK_ROOT, AK_SCALE, AK_ROT, AK_N };
enum { AL_OFF, AL_RHY, AL_NOT, AL_RN };
static const uint8_t AC_SCL[7] = {0, 2, 8, 3, 7, 1, 6};   /* SCALE's choices: AUTO, then N_SCALE / SCALE_MASK indices */
static const uint8_t AC_EVERY[6] = {0, 1, 2, 4, 8, 16};

/* a hash of (seed, step, salt): the same line for the same seeds, whatever is asked in what order */
static uint32_t ac_h(uint32_t s, uint32_t i, uint32_t salt)
{
    uint32_t x = s * 2654435761u ^ (i + 1u) * 2246822519u ^ salt * 3266489917u;
    x ^= x >> 15; x *= 2246822519u; x ^= x >> 13; x *= 3266489917u; x ^= x >> 16;
    return x;
}
static uint32_t ac_ch(uint32_t s, uint32_t i, uint32_t salt, int32_t pct) { return (int32_t)(ac_h(s, i, salt) % 100u) < pct; }

static ac_t *ac_of(uint32_t k)
{
    ac_t *a = &ac[k % NTRK];
    if (!a->on) {
        a->on = 1;
        a->dens = 65; a->slide = 25; a->acc = 30; a->oct = 20;
        a->steps = 16; a->notes = 4; a->tie = 15;
        a->amt = 25;
        a->s.r = (uint16_t)(0x3031u + k * 7u); a->s.n = (uint16_t)(0x1303u + k * 13u); a->s.f = (uint16_t)(0x0303u + k * 17u);
    }
    return a;
}

/* the key: e->root, e->mask (and e->src as EUCLID's: 0 SCL, 1 + n the track the notes fit, 7 the seed's, 8 ACID's own) */
static void ac_key(uint32_t k, const ac_t *a, eu_t *e)
{
    memset(e, 0, sizeof *e);
    e->seed = a->s.n;
    /* (cursed-os) AUTO: the song's key, the track's root (SCL, or the key pressed on cursed-os's KEY), in the track's
     * scale or minor without one; ROOT / SCALE set: theirs */
    e->root = (uint8_t)((trk[k % NTRK].p[P_ROOT] % 12 + 12) % 12);
    e->mask = (uint16_t)SCALE_MASK[trk[k % NTRK].p[P_SCALE] > 0 ? trk[k % NTRK].p[P_SCALE] : 2];
    if (a->root) e->root = (uint8_t)((a->root - 1u) % 12u);
    if (a->scale) e->mask = (uint16_t)SCALE_MASK[AC_SCL[a->scale % 7u]];
    e->src = 8;
}

/* the NOTES pool: the root and up to six degrees more, drawn by weight (the octave, the seventh, the fifth, the third
 * first: the acid line's usual notes), from seed sn; out: degrees, their number */
static uint32_t ac_pool(uint32_t sn, uint32_t want, uint32_t np, int32_t *deg)
{
    int32_t cand[7] = {(int32_t)np, 6, 4, 2, 1, 3, 5};   /* (in a 7-note scale: 8ve, 7th, 5th, 3rd, 2nd, 4th, 6th) */
    uint32_t w[7] = {30, 18, 18, 14, 9, 7, 5}, n = 1, i, tot, r, j;
    deg[0] = 0;
    if (np < 7u)                                         /* (a pentatonic or another short scale: its own steps) */
        for (i = 1; i < 7u; i++) cand[i] = (int32_t)(i < np ? i : np + i % np);
    for (j = 0; n < want && j < 7u; j++) {
        for (tot = 0, i = 0; i < 7u; i++) tot += w[i];
        if (!tot) break;
        r = ac_h(sn, j, 0x900Du) % tot;
        for (i = 0; i < 7u && r >= w[i]; i++) r -= w[i];
        if (i >= 7u) break;
        deg[n++] = cand[i];
        w[i] = 0;                                        /* (each degree once) */
    }
    return n;
}

/* (cursed-os) steps a..b-1 of s in reverse (the rotation's three reversals: no second copy of the steps) */
static void ac_rev(step_t *s, uint32_t a, uint32_t b)
{
    while (a + 1u < b) {
        step_t x = s[a];
        s[a] = s[--b];
        s[b] = x;
        a++;
    }
}
/* track t's first n steps turned r later (step i to (i + r) mod n); lk: the step locks on them too (automation stays) */
static void ac_turn(track_t *t, uint32_t n, uint32_t r, uint32_t lk)
{
    uint32_t k = trk_index(t), i, f;
    if (n < 2u || !(r %= n)) return;
    f = motion_guard();                                  /* (the steps and their locks turned between two samples) */
    ac_rev(t->step, 0, n);
    ac_rev(t->step, 0, r);
    ac_rev(t->step, r, n);
    for (i = 0; lk && i < motion.count; i++) {
        motion_event_t *e = &motion.event[i];
        if (MOTION_TRK(e) == k && (e->param & MOTION_LOCK) && MOTION_STEP(e) < n)
            e->place = (uint8_t)((e->place & 0xC0u) | (MOTION_STEP(e) + r) % n);
    }
    motion_unguard(f);
}

/* track k's line written again from its settings */
static void ac_gen(uint32_t k, uint32_t hand)
{
    track_t *t = &trk[k % NTRK];
    ac_t *a = ac_of(k);
    eu_t key;
    uint8_t pc[12], on[64];
    int32_t pool[7], base, ctr, sum = 0;
    uint32_t n = clamp(a->steps, 4, 64), np = 0, npool, i, c, g = 0;
    if (hand && fm1_ms - ac_ms > 2000u) undo_mark();
    ac_ms = hand ? fm1_ms : ac_ms;
    for (i = 0; !a->ctr && i < (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP); i++)   /* the register: the notes it had (none: C2) */
        if (t->step[i].time == ST_NOTE && t->step[i].n) { sum += t->step[i].note[0]; g++; }
    if (!a->ctr) a->ctr = (uint8_t)clamp(g ? sum / (int32_t)g : 40, 28, 72);
    ctr = a->ctr;
    ac_key(k, a, &key);
    for (c = 0; c < 12u; c++)
        if ((key.mask >> c) & 1u) pc[np++] = (uint8_t)c;
    if (!np) { pc[0] = 0; np = 1; }
    base = ctr - ((ctr - key.root + 120) % 12);          /* the root at or below the register */
    npool = ac_pool(a->s.n, clamp(a->notes, 1, 7), np, pool);
    load_begin(t, LOAD_PAT);
    for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
    t->p[P_SLEN] = (int16_t)n;
    for (i = 0; i < n; i++) {                            /* RHYTHM: 1 a note (the downbeats likelier, the offbeat
                                                         * sixteenths less), 2 the note before held over (TIE; a second
                                                         * step held: half as likely), 0 a rest */
        uint32_t m = a->ms && ac_ch(a->ms, i, 0x3u, a->amt), sr = m ? a->s.r ^ a->ms : a->s.r;
        int32_t p = (int32_t)a->dens + (!(i & 3u) ? 25 : (i & 1u) ? -10 : 0);
        on[i] = (uint8_t)(a->dens && (i == 0u || ac_ch(sr, i, 0x1u, p)));
        if (!on[i] && i && on[i - 1u] && ac_ch(sr, i, 0x2u, on[i - 1u] == 1u ? a->tie : a->tie / 2))
            on[i] = 2;
    }
    for (i = 0; i < n; i++) {
        step_t *s = &t->step[i];
        uint32_t m = a->ms && ac_ch(a->ms, i, 0x3u, a->amt);
        uint32_t sn = m ? a->s.n ^ a->ms : a->s.n, sf = m ? a->s.f ^ a->ms : a->s.f;
        int32_t d, note, sh = 0;
        uint32_t nx = i + 1u < n ? i + 1u : n;            /* (the last step slides into nothing: the loop restarts) */
        if (on[i] == 2u) { s->time = ST_TIE; continue; }
        if (!on[i]) continue;
        if (!(i & 7u) && ac_ch(sn, i, 0x10u, 60)) d = 0;  /* (the root likelier on the half bars) */
        else {
            uint32_t r = ac_h(sn, i, 0x11u) % 100u;
            d = r < 40u || npool < 2u ? pool[0] : pool[1u + r % (npool - 1u)];
        }
        if (ac_ch(sn, i, 0x12u, a->oct)) sh = ac_ch(sn, i, 0x13u, 75) ? 12 : -12;
        note = clamp(eu_deg(pc, np, base, d) + sh, 24, 96);
        s->time = ST_NOTE;
        s->n = 1;
        s->note[0] = (uint8_t)note;
        s->vel = 90;
        if (ac_ch(sf, i, 0x20u, a->acc)) { s->flags |= SF_ACCENT; s->vel = 120; }
        if (nx < n && on[nx] == 1u && ac_ch(sf, i, 0x21u, a->slide)) s->flags |= SF_SLIDE;   /* (into the next note) */
    }
    ac_turn(t, n, a->rot % n, 0);                           /* (ROTATE: the start where it was put) */
    load_end(t);
    pat_sig[k % NTRK] = ~steps_sig(t);                   /* (the user's steps now) */
    ui.force = 1;
}

/* the knob views (params.c page_desc: ids ID_AC0 + AK_*, ID_AC1 + AK_* - 8) */
static int16_t ac_v[AK_N] COS_RAM;
static param_desc_t ac_d COS_RAM;
static __attribute__((noinline)) const param_desc_t *ac_desc(uint32_t k, int16_t **valp)
{
    static const char *const LB[AK_N] = {"DENSITY", "SLIDE", "ACCENT", "JUMPS", "STEPS", "NOTES", "TIE", "LOCK", "EVERY",
                                         "AMOUNT", "ROOT", "SCALE", "ROTATE"};
    static const char *const LK[] = {"OFF", "RHYTHM", "NOTES", "R+N", 0};
    static const char *const EV[] = {"OFF", "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS", 0};
    static const char *const RT[] = {"AUTO", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", 0};
    static const char *const SC[] = {"AUTO", "MIN", "PHRY", "DOR", "HARM", "MAJ", "MPEN", 0};
    ac_t *a = ac_of(song.sel);
    k %= AK_N;
    ac_d.label = LB[k]; ac_d.fmt = F_INT; ac_d.min = 0; ac_d.max = 100; ac_d.def = 0; ac_d.names = 0; ac_d.unit = 0;
    switch (k) {
    case AK_DENS: ac_v[k] = a->dens; break;
    case AK_SLIDE: ac_v[k] = a->slide; break;
    case AK_ACC: ac_v[k] = a->acc; break;
    case AK_OCT: ac_v[k] = a->oct; break;
    case AK_STEPS: ac_d.min = 4; ac_d.max = 64; ac_v[k] = a->steps; break;
    case AK_NOTES: ac_d.min = 1; ac_d.max = 7; ac_v[k] = a->notes; break;
    case AK_TIE: ac_v[k] = a->tie; break;
    case AK_LOCK: ac_d.fmt = F_ENUM; ac_d.max = 3; ac_d.names = LK; ac_v[k] = a->lock; break;
    case AK_EVERY: ac_d.fmt = F_ENUM; ac_d.max = 5; ac_d.names = EV; ac_v[k] = a->every; break;
    case AK_AMT: ac_v[k] = a->amt; break;
    case AK_ROOT: ac_d.fmt = F_ENUM; ac_d.max = 12; ac_d.names = RT; ac_v[k] = a->root; break;
    case AK_ROT: ac_d.max = a->steps > 1u ? a->steps - 1 : 0; ac_v[k] = (int16_t)(a->rot % (a->steps ? a->steps : 1u)); break;
    default: ac_d.fmt = F_ENUM; ac_d.max = 6; ac_d.names = SC; ac_v[k] = a->scale; break;
    }
    *valp = &ac_v[k];
    return &ac_d;
}

/* knob k turned to v: the setting, then (not LOCK, EVERY, AMOUNT) the line again */
static __attribute__((noinline)) void ac_put(uint32_t k, int32_t v)
{
    ac_t *a = ac_of(song.sel);
    uint8_t *f[AK_N] = {&a->dens, &a->slide, &a->acc, &a->oct, &a->steps, &a->notes, &a->tie, &a->lock, &a->every,
                        &a->amt, &a->root, &a->scale, &a->rot};
    static const uint8_t LO[AK_N] = {0, 0, 0, 0, 4, 1, 0, 0, 0, 0, 0, 0, 0},
                         HI[AK_N] = {100, 100, 100, 100, 64, 7, 100, 3, 5, 100, 12, 6, 63};
    uint32_t was = a->rot, n;
    k %= AK_N;
    if (k == AK_ROT) {                                   /* (cursed-os) ROTATE: the steps there now turned, not thrown again */
        track_t *t = TSEL;
        if (drum_track(t)) { ui_message("ACID: PICK A MELODIC TRACK"); return; }
        if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
        a->rot = (uint8_t)clamp(v, 0, a->steps > 1u ? a->steps - 1 : 0);
        n = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
        ui.force = 1;
        if (a->rot == was) return;
        if (fm1_ms - ac_ms > 2000u) undo_mark();
        ac_ms = fm1_ms;
        ac_turn(t, n, (a->rot + n * 64u - was) % n, 1);   /* (not a pattern load: that would drop the track's motion) */
        pat_sig[song.sel % NTRK] = ~steps_sig(t);
        return;
    }
    *f[k] = (uint8_t)clamp(v, LO[k], HI[k]);
    if (k == AK_STEPS && a->rot >= a->steps) a->rot = (uint8_t)(a->steps - 1u);
    ui.force = 1;
    if (k == AK_LOCK || k == AK_EVERY || (k == AK_AMT && !a->ms))
        return;
    if (drum_track(TSEL)) { ui_message("ACID: PICK A MELODIC TRACK"); return; }
    if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
    ac_gen(song.sel, 1);
}

/* OCT+: a new throw (the parts LOCK keeps stay), the throw before onto the history; OCT-: back one throw */
static void ac_throw(uint32_t k, int up)
{
    ac_t *a = ac_of(k);
    uint32_t i;
    if (up) {
        uint32_t r = fm1_ms * 2654435761u ^ ac_h(a->s.r ^ a->s.n, a->nh, 0xACu);
        for (i = AC_HIST - 1u; i > 0u; i--) a->hist[i] = a->hist[i - 1u];   /* (the firmware's libc has no memmove) */
        a->hist[0] = a->s;
        a->nh = (uint8_t)(a->nh < AC_HIST ? a->nh + 1u : AC_HIST);
        if (a->lock != AL_RHY && a->lock != AL_RN) a->s.r = (uint16_t)(ac_h(r, 1, 1) | 1u);
        if (a->lock != AL_NOT && a->lock != AL_RN) a->s.n = (uint16_t)(ac_h(r, 2, 2) | 1u);
        a->s.f = (uint16_t)(ac_h(r, 3, 3) | 1u);
        ui_message(a->lock == AL_OFF ? "NEW LINE" : a->lock == AL_RHY ? "NEW NOTES" : a->lock == AL_NOT ? "NEW RHYTHM" : "NEW ACCENTS");
    } else {
        if (!a->nh) { ui_message("NO LINE BEFORE"); return; }
        a->s = a->hist[0];
        for (i = 0; i + 1u < AC_HIST; i++) a->hist[i] = a->hist[i + 1u];
        a->nh--;
        ui_message("LINE BEFORE");
    }
    a->ms = 0;
    ac_gen(k, 1);
}
static void ac_mutate(uint32_t k, uint32_t hand)
{
    ac_t *a = ac_of(k);
    a->ms = (uint16_t)(ac_h(fm1_ms, k, a->ms) | 1u);
    ac_gen(k, hand);
}
static __attribute__((noinline)) void ac_oct(int up, int mu)
{
    ac_t *a = ac_of(song.sel);
    if (drum_track(TSEL)) { ui_message("ACID: PICK A MELODIC TRACK"); return; }
    if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
    if (!mu) { ac_throw(song.sel, up); return; }
    if (up) { ac_mutate(song.sel, 1); ui_message("MUTATED"); }
    else { a->ms = 0; ac_gen(song.sel, 1); ui_message("ANCHOR"); }
}
/* every UI frame: playing (not a song's chain), each track with EVERY on mutates in the last beat of every EVERY bars */
static __attribute__((noinline)) void ac_tick(void)
{
    uint32_t k, b = bar_n;
    for (k = 0; !song.playing && k < NTRK; k++) ac[k].bar = 0;
    if (!song.playing || chain_busy() || beat_n != 3u || beat_pos < beat_samples() / 2u) return;
    for (k = 0; k < NTRK; k++) {
        ac_t *a = &ac[k];
        uint32_t ev = AC_EVERY[a->every % 6u];
        if (!a->on || !ev || !a->amt || drum_track(&trk[k]) || a->bar == (uint8_t)(b + 1u) || (b + 1u) % ev) continue;
        a->bar = (uint8_t)(b + 1u);
        ac_mutate(k, 0);
    }
}

/* ------------------------------------------------------------- the graph --- */
/* the line as a 303 would show it: a lane of steps (a note: its name and octave; an accent bright, a slide joined to the
 * next, a tie a bar on), the key, LOCK and the mutation on top */
static __attribute__((noinline)) void graph_acid(const track_t *t)
{
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char *const LK[4] = {"", "LOCK R", "LOCK N", "LOCK R+N"};
    const ac_t *a = ac_of(song.sel);
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, cols = len < 16u ? len : 16u, rows = (len + 15u) / 16u;
    int32_t h = (int32_t)graph_h(), x0 = 6, y0 = 16, w = 228, cw = w / (int32_t)cols, rh, lo = 127, hi = 0;
    uint32_t now = song.playing ? t->seq_idx % len : ~0u;
    eu_t key;
    char b[16];
    ac_key(song.sel, a, &key);
    str_cpy(b, NN[key.root % 12u], sizeof b);
    for (i = 1; i < sizeof SCALE_MASK / sizeof SCALE_MASK[0] && SCALE_MASK[i] != key.mask; i++) {}
    if (i < sizeof SCALE_MASK / sizeof SCALE_MASK[0]) { str_cpy(b + str_len(b), " ", 2); str_cpy(b + str_len(b), N_SCALE[i], 8); }
    cv_text(x0, 2, &AF_X, b, T_TEXT);
    if (a->lock) cv_text(x0 + 70, 2, &AF_X, LK[a->lock & 3u], T_MID);
    if (a->rot % len) { str_cpy(b, "ROT ", sizeof b); fmt_int(b + 4, (int32_t)(a->rot % len)); cv_text(x0 + 140, 2, &AF_X, b, T_MID); }
    if (a->ms) cv_text_r(x0 + w, 2, &AF_X, "MUT", T_ACCENT, T_SURF);
    if (rows > 4u) rows = 4u;
    rh = (h - y0 - 2) / (int32_t)rows;
    for (i = 0; i < len; i++) {
        const step_t *s = &t->step[i];
        if (s->time == ST_NOTE && s->n) { lo = s->note[0] < lo ? s->note[0] : lo; hi = s->note[0] > hi ? s->note[0] : hi; }
    }
    if (hi < lo) { lo = 36; hi = 48; }
    if (hi - lo < 12) hi = lo + 12;
    for (i = 0; i < len && i < 64u; i++) {
        const step_t *s = &t->step[i];
        int32_t x = x0 + (int32_t)(i % 16u) * cw, ry = y0 + (int32_t)(i / 16u) * rh, by = ry + rh - 4;
        int32_t y = by - (s->time == ST_NOTE ? (s->note[0] - lo) * (rh - 16) / (hi - lo) : 0) - 6;
        if (s->time == ST_NOTE && s->n) {
            uint32_t ac2 = (s->flags & SF_ACCENT) != 0u;
            cv_rect(x + 1, y, cw - 2 > 1 ? cw - 2 : 1, ac2 ? 5 : 3, ac2 ? T_ACCENT : T_THEME);
            if (s->flags & SF_SLIDE) cv_rect(x + cw - 2, y + 1, 3, 1, T_TEXT);
        } else if (s->time == ST_TIE) {
            cv_rect(x, y0 + (int32_t)(i / 16u) * rh + rh - 10, cw, 1, T_MID);
        }
        cv_rect(x + 1, by, cw - 2 > 1 ? cw - 2 : 1, 2, i == now ? T_TEXT : !(i & 3u) ? T_MID : T_DIM);
    }
}
