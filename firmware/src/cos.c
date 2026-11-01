/* SPDX-License-Identifier: GPL-3.0-only
 * cursed-os: cursedsound's interface for the FM-1, on JIANT FM's engines and sequencer (JIANT: Juan Jiant; Felucca:
 * Leo Kuroshita, Hügelton Instruments). The layout follows the groovebox model of charlesvestal's X0X (a home grid of
 * the parts, one row of four knobs per page, the steps along the foot) in Elektron's manner: one button a mode, its
 * pages as tabs, the white keys the sixteen trigs. Black and white; red is the one live thing (the playhead, the knob
 * turned, REC, an accent). Included by felucca.c / felucca_web.c after ui_input.c; ui_input / ui_draw / ui_leds hand
 * over to cos_input / cos_draw / cos_leds while cos_active(). HOME + GLO switches to JIANT's own interface and back.
 *
 *   HOME  the six tracks' steps          white keys 1-6: pick a track   black keys 1-6: mute
 *   SEQ   TRIG: the track's steps        white keys: trigs (on release)   OCT-/+: the bank of 16   (a kit: black keys
 *         pick the lane). A step held + a knob: a parameter lock on that step, of the sound page used last (SRC AMP FLTR
 *         LFO FX); held + REC: its locks go. Locked steps: a dot under them
 *   EDIT  SRC 1 / SRC 2: the engine (VOICE: and WORDS, the talking synth: LANG LYRIC THEME ARTIC, OCT+ new words)
 *   ENV   AMP / FLTR / LFO / LFO 2      FX  SEND / DIST
 *   LFO   DRONE: the keys hold notes on the track (up to four), sounding whatever the transport does; REC lets them go
 *   GLO   MIX / MIDI: the track's MIDI OUT channel (the Syntakt's track), MIDI OUT, the clock (TRS: the Syntakt's)
 *   ARP   GEN: ACID (a synth) or EUCLID (a kit); OCT+ a new line, OCT- the one before (ACID 3 / MUTATE: mutate, anchor)
 *   SCL   KEY: a key pressed is the song's new key (HYP-1's way): every synth track's notes and root move by the nearest
 *         interval, on the next bar while playing (the kits stay); SCL held + a key does it from any page
 *   SELECT the page   ALGORITHM the track   KNOB 1-4 the page's four   PLAY start / stop
 * Outside HOME, SEQ and GEN's kit pages the keys play the track. */
/* (cursedsound) cursed-os's and ACID's RAM: in the pool (app.ld POOL: zeroed at boot, ~9 KB spare), in a section of its
 * own after JIANT's .pool, so none of the sound's variables in .data / .bss / .sbss moves (the first links on GitHub: a
 * variable placed among them took ten of the sound's functions 34 bytes past their pinned run; in .sbss, the small data,
 * 220). The host builds (hostsim.c) drop attributes */
#ifndef COS_RAM
#define COS_RAM __attribute__((section(".pool.zcos")))
#endif
#include "cos_font.h"                           /* COS_FONT: Press Start 2P, 8 x 8, 1 bit (tools/gen_cos_font.py) */

#define C_BG 0x0000u
#define C_FG 0xFFFFu
#define C_RED 0xF924u                           /* (255, 36, 36) */
#define C_MID 0xAD55u                           /* (170, 170, 170): labels */
#define C_DIM 0x6B6Du                           /* (110, 110, 110): idle */
#define C_DARK 0x2965u                          /* (45, 45, 45): empty cells, the arcs' tracks */

enum { CM_HOME, CM_TRIG, CM_SRC, CM_AMP, CM_DRONE, CM_FX, CM_GEN, CM_MIX, CM_KEY, CM_PROJ, CM_N };
enum { CK_NONE, CK_TRK, CK_GLO, CK_AC, CK_EU, CK_COS };
enum { CC_OUTCH, CC_MOUT, CC_CLOCK, CC_SLOT };   /* (CK_COS: cursed-os's own knobs) */
typedef struct { const char *tab; uint8_t kind, id[4]; } cpg_t;
#define X 0xFFu
static const cpg_t CZP_HOME[] = {{"PERFORM", CK_GLO, {G_BPM, G_SWING, G_DJF, X}}};
static const cpg_t CZP_TRIG[] = {{"TRIG", CK_TRK, {P_SLEN, P_SDIV, P_SSWING, P_SGATE}}};
static const cpg_t CZP_SRC[] = {{"SRC 1", CK_TRK, {P_E0, P_E1, P_E2, P_E3}}, {"SRC 2", CK_TRK, {P_E4, P_E5, P_E6, P_E7}},
                               {"WORDS", CK_TRK, {P_VLANG, P_VLYR, P_E1, P_E2}}};   /* (WORDS: VOICE only) */
static const cpg_t CZP_AMP[] = {{"AMP", CK_TRK, {P_ATK, P_DEC, P_SUS, P_REL}}, {"FLTR", CK_TRK, {P_FCUT, P_FRES, P_FTYPE, P_ED_FLT}},
                               {"LFO", CK_TRK, {P_LRATE, P_LWAVE, P_LD_PIT, P_LD_FLT}}, {"LFO 2", CK_TRK, {P_LFADE, P_LD_SHP, P_LD_AMP, X}}};
static const cpg_t CZP_DRONE[] = {{"DRONE", CK_TRK, {P_LEVEL, P_FCUT, P_LRATE, P_LD_FLT}}, {"SPACE", CK_TRK, {P_ATK, P_REL, P_DLY, P_REV}}};
static const cpg_t CZP_FX[] = {{"SEND", CK_TRK, {P_DIST, P_CHOR, P_DLY, P_REV}}, {"DIST", CK_TRK, {P_DTYPE, P_DTONE, X, X}}};
static const cpg_t CZP_GENA[] = {{"ACID", CK_AC, {AK_DENS, AK_SLIDE, AK_ACC, AK_OCT}}, {"ACID 2", CK_AC, {AK_STEPS, AK_NOTES, AK_TIE, AK_LOCK}},
                                {"ACID 3", CK_AC, {AK_EVERY, AK_AMT, AK_ROOT, AK_SCALE}}};
static const cpg_t CZP_GENE[] = {{"EUCLID", CK_EU, {EK_HITS, EK_STEPS, EK_ROT, EK_K4}}, {"MODES", CK_EU, {EK_M0, EK_M0 + 1u, EK_M0 + 2u, EK_M0 + 3u}},
                                {"MUTATE", CK_EU, {EK_U0, EK_U0 + 1u, EK_U0 + 2u, EK_U0 + 3u}}};
static const cpg_t CZP_MIX[] = {{"MIX", CK_TRK, {P_LEVEL, P_PAN, P_GLIDE, P_MUTE}}, {"MIDI", CK_COS, {CC_OUTCH, CC_MOUT, CC_CLOCK, X}}};
static const cpg_t CZP_KEY[] = {{"KEY", CK_TRK, {P_SCALE, P_QUANT, 0xFFu, 0xFFu}}};   /* (the root: the keys) */
static const cpg_t CZP_PROJ[] = {{"PROJECT", CK_COS, {CC_SLOT, X, X, X}}};   /* (the slot: KNOB 1 or white keys 1..4) */
#undef X
static const char *const CM_NAME[CM_N] = {"HOME", "TRIG", "SRC", "AMP", "DRONE", "FX", "GEN", "MIX", "KEY", "SAVE"};
static const uint8_t CM_BTN[CM_N] = {B_HOME, B_SEQ, B_EDIT, B_ENV, B_LFO, B_FX, B_ARP, B_GLO, B_SCL, B_SAVE};
static const uint8_t WK[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};   /* the white keys */
static const uint8_t BK[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};                    /* the black keys */
static const char *const COS_LN[NLANE] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};

static struct {
    uint8_t flip, mode, tab[CM_N], bank, lane, hot, combo, ready;   /* flip: HOME + GLO turned it from COS_DEFAULT */
    uint32_t hot_ms, msg_ms, sig[6];
    char msg[24];
    uint8_t key_next, scl_down, scl_used;              /* key_next: 1 + the key asked for (0 C .. 11 B) */
    uint32_t key_bar;                                   /* (bar_n when it was asked for) */
    uint8_t dr_trk, dr_n, dr_note[4];                   /* DRONE: its track, its notes (seq.c cos_dr sounds them) */
    uint8_t lk_mode, lk_tab, lk_used;                   /* LOCK: the sound page used last; a knob turned while steps held */
    uint32_t tg_down, notes0;                           /* TRIG: the white keys down since their press; the keys a frame ago */
    uint8_t ly_prev;                                    /* WORDS: the lyric before OCT+ */
    int8_t dr_oct;
    int16_t cc_v[4];                                    /* (CK_COS: the knobs' views) */
    uint8_t slot, pn_ok;                                /* SAVE: the slot picked (0..3); the names below read */
    char pn[4][13];                     /* .. each slot's name ("": empty) */
} cz COS_RAM;
static int cos_active(void) { return (COS_DEFAULT ^ cz.flip) != 0; }

static int cos_kit(void) { return drum_track(TSEL) != 0; }
static void cos_pages(uint32_t m, const cpg_t **p, uint32_t *n)
{
    static const cpg_t *const P[CM_N] = {CZP_HOME, CZP_TRIG, CZP_SRC, CZP_AMP, CZP_DRONE, CZP_FX, CZP_GENA, CZP_MIX, CZP_KEY, CZP_PROJ};
    static const uint8_t N[CM_N] = {1, 1, 2, 4, 2, 2, 3, 2, 1, 1};
    *p = m == CM_GEN && cos_kit() ? CZP_GENE : P[m % CM_N];
    *n = N[m % CM_N] + (m == CM_SRC && trk_engp(TSEL) == &ENG_FORMANT);   /* (VOICE: WORDS too) */
}
static const cpg_t *cos_page(void)
{
    const cpg_t *p;
    uint32_t n;
    cos_pages(cz.mode, &p, &n);
    return &p[cz.tab[cz.mode] % n];
}
static void cos_say(const char *s) { str_cpy(cz.msg, s, sizeof cz.msg); cz.msg_ms = fm1_ms; }
static const char *const COS_NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

/* the song's key: the first synth track's root (C with none) */
static uint32_t cos_key(void)
{
    uint32_t k;
    for (k = 0; k < NTRK && drum_track(&trk[k]); k++) {}
    return k < NTRK ? (uint32_t)((trk[k].p[P_ROOT] % 12 + 12) % 12) : 0u;
}
/* WORDS (VOICE): OCT+ a new phrase of words (W1 .. W16 at random, a language set if none: EN), OCT- the one before */
static void cos_words(int up)
{
    track_t *t = TSEL;
    if (trk_engp(t) != &ENG_FORMANT) return;
    if (!t->p[P_VLANG]) t->p[P_VLANG] = LG_EN;
    if (up) {
        uint32_t r = fm1_ms * 2654435761u ^ (uint32_t)t->p[P_VLYR] * 40503u, w;
        cz.ly_prev = (uint8_t)t->p[P_VLYR];
        do { r ^= r << 13; r ^= r >> 17; r ^= r << 5; w = 1u + r % LYR_NW; } while (w == (uint32_t)t->p[P_VLYR]);
        t->p[P_VLYR] = (int16_t)w;
        cos_say("NEW WORDS");
    } else {
        int16_t x = t->p[P_VLYR];
        t->p[P_VLYR] = cz.ly_prev; cz.ly_prev = (uint8_t)x;
        cos_say("WORDS BEFORE");
    }
}

/* DRONE: the request to the audio side (seq.c cos_dr): its notes first, then gen, so a block in between sees the old */
static void cos_drone_send(void)
{
    uint32_t i;
    cos_dr.trk = cz.dr_trk;
    cos_dr.n = cz.dr_n;
    for (i = 0; i < 4u; i++) cos_dr.note[i] = cz.dr_note[i];
    cos_dr.gen = (uint8_t)(cos_dr.gen + 1u);
}
/* a key on DRONE: its note held on the selected track, or let go when held already (four at most: the oldest goes);
 * a key on another track moves the drone there */
static void cos_drone_key(uint32_t key)
{
    uint32_t note = (uint32_t)clamp(41 + (int32_t)key + 12 * cz.dr_oct, 12, 108), i, j;   /* (from F2: a drone sits low) */
    if (cz.dr_n && cz.dr_trk != song.sel) cz.dr_n = 0;
    cz.dr_trk = song.sel;
    for (i = 0; i < cz.dr_n && cz.dr_note[i] != note; i++) {}
    if (i < cz.dr_n) {                                    /* held: let it go */
        for (j = i; j + 1u < cz.dr_n; j++) cz.dr_note[j] = cz.dr_note[j + 1u];
        cz.dr_n--;
    } else {
        if (cz.dr_n == 4u) { for (j = 0; j < 3u; j++) cz.dr_note[j] = cz.dr_note[j + 1u]; cz.dr_n = 3; }
        cz.dr_note[cz.dr_n++] = (uint8_t)note;
    }
    cos_drone_send();
}
static void cos_drone_clear(void) { cz.dr_n = 0; cos_drone_send(); }

/* the song into key pc (0..11): the nearest interval (-6 .. +5), every synth track's notes and root moved by it (the
 * notes as stored: MIDI OUT, ACID and the scale all agree; a kit's lanes stay) */
static void cos_key_apply(uint32_t pc)
{
    int32_t d = ((int32_t)pc - (int32_t)cos_key() + 18) % 12 - 6;
    uint32_t k, i, c;
    for (k = 0; d && k < NTRK; k++) {
        track_t *t = &trk[k];
        if (drum_track(t)) continue;
        for (i = 0; i < NSTEP; i++) {
            step_t *s = &t->step[i];
            if (s->time != ST_NOTE) continue;
            for (c = 0; c < s->n && c < 4u; c++) s->note[c] = (uint8_t)clamp((int32_t)s->note[c] + d, 0, 127);
        }
        t->p[P_ROOT] = (int16_t)((t->p[P_ROOT] + d + 120) % 12);
    }
    if (d && cz.dr_n && !drum_track(&trk[cz.dr_trk % NTRK])) {   /* (the drone moves with the song) */
        for (k = 0; k < cz.dr_n; k++) cz.dr_note[k] = (uint8_t)clamp((int32_t)cz.dr_note[k] + d, 12, 108);
        cos_drone_send();
    }
    cz.key_next = 0;
    cos_say(COS_NN[pc % 12u]);
}
/* a key asked for: now when stopped, on the next bar while playing */
static void cos_key_ask(uint32_t key)
{
    uint32_t pc = (5u + key) % 12u;                      /* (key 0 is F) */
    if (!song.playing) { cos_key_apply(pc); return; }
    cz.key_next = (uint8_t)(pc + 1u);
    cz.key_bar = bar_n;
}
static void cos_key_poll(void)
{
    if (cz.key_next && (!song.playing || bar_n != cz.key_bar))
        cos_key_apply(cz.key_next - 1u);
}

/* GLO > MIDI's knobs: the selected track's MIDI OUT channel (the settings: bytes 8 .. 13 of engine 15's, 0 its own),
 * MIDI OUT (KEYS CLOCK ALL OFF: the menu's), the clock the music follows (the project's G_CLOCK: INT USB TRS) */
static const param_desc_t *cos_cdesc(uint32_t id, int16_t **vp)
{
    static const char *const MO[] = {"KEYS", "CLOCK", "ALL", "OFF", 0};
    static const param_desc_t D[2] = {{"OUT CH", F_INT, 1, 16, 1, 0, 0}, {"SENDS", F_ENUM, 0, 3, 0, MO, 0}};
    static const param_desc_t DS = {"SLOT", F_INT, 1, 4, 1, 0, 0};
    uint32_t t = song.sel % NTRK, v = favorites.factory[15][8u + t];
    if (id == CC_SLOT) { cz.cc_v[3] = (int16_t)(cz.slot + 1u); *vp = &cz.cc_v[3]; return &DS; }
    id %= 3u;
    *vp = &cz.cc_v[id];
    if (id == CC_OUTCH) { cz.cc_v[id] = (int16_t)(v && v <= 16u ? v : t + 1u); return &D[0]; }
    if (id == CC_MOUT) { cz.cc_v[id] = (int16_t)(midi_out_mode & 3u); return &D[1]; }
    *vp = &song.g[G_CLOCK];
    return &GP[G_CLOCK];
}
static void cos_cput(uint32_t id, int32_t v)
{
    uint32_t t = song.sel % NTRK;
    if (id == CC_OUTCH) {
        v = clamp(v, 1, 16);
        favorites.factory[15][8u + t] = (uint8_t)((uint32_t)v == t + 1u ? 0u : (uint32_t)v);   /* (its own: 0, as JIANT) */
        settings_save();
    } else if (id == CC_MOUT) {
        midi_out_mode = (uint8_t)(clamp(v, 0, 3));
        settings_save();
    } else if (id == CC_SLOT) {
        cz.slot = (uint8_t)(clamp(v, 1, 4) - 1);
    } else
        song.g[G_CLOCK] = (int16_t)clamp(v, GP[G_CLOCK].min, GP[G_CLOCK].max);
}

/* knob k of the page: its description and its value (0: none) */
static const param_desc_t *cos_knob(const cpg_t *p, uint32_t k, int16_t **vp)
{
    uint32_t id = p->id[k & 3u];
    *vp = 0;
    if (id == 0xFFu) return 0;
    switch (p->kind) {
    case CK_TRK: *vp = &TSEL->p[id]; return track_desc(TSEL, id);
    case CK_GLO: *vp = &song.g[id]; return &GP[id];
    case CK_AC: return ac_desc(id, vp);
    case CK_EU: return eu_desc(id, vp);
    case CK_COS: return cos_cdesc(id, vp);
    }
    return 0;
}
/* LOCK: the page whose knobs a held step locks: the sound page used last (SRC AMP FLTR LFO FX), SRC 1 to start */
static const cpg_t *cos_lk_page(void)
{
    const cpg_t *p;
    uint32_t n, m = cz.lk_mode ? cz.lk_mode : CM_SRC;
    cos_pages(m, &p, &n);
    return &p[cz.lk_tab % n];
}
static uint64_t cos_held(void)                          /* the steps of the bank whose white keys are held (TRIG) */
{
    uint64_t h = 0;
    uint32_t w, len = (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP);
    for (w = 0; w < 16u; w++)
        if (((cz.tg_down >> WK[w]) & 1u) && cz.bank * 16u + w < len) h |= 1ull << (cz.bank * 16u + w);
    return h;
}
/* knob k turned s with steps held: each held step's lock of the lock page's knob k, from its lock or the sound's value */
static void cos_lock_turn(uint32_t k, int32_t s, uint64_t held)
{
    const cpg_t *p = cos_lk_page();
    track_t *t = TSEL;
    uint32_t id = p->id[k & 3u], i;
    const param_desc_t *d;
    if (chain_busy()) { cos_say("SONG PLAYS: STOP IT"); return; }
    if (p->kind != CK_TRK || id == 0xFFu || !motion_param(id)) { cos_say("NO LOCK THERE"); return; }
    d = track_desc(t, id);
    if (!d) return;
    s = accel(EN_K1 + k, s, d->fmt == F_ENUM ? 0 : d->max - d->min);
    for (i = 0; i < NSTEP; i++) {
        int16_t v;
        if (!((held >> i) & 1u)) continue;
        if (!motion_lock_get(t, i, id, &v)) v = motion_base_value(t, id);
        if (motion_set_lock(t, i, id, (int16_t)param_turn(d, v, s)) == 2) { cos_say("LOCKS FULL (64)"); break; }
    }
    cz.lk_used = 1;
    cz.hot = (uint8_t)(k + 1u);
    cz.hot_ms = fm1_ms;
}
static void cos_turn(uint32_t k, int32_t s)
{
    const cpg_t *p = cos_page();
    int16_t *vp;
    const param_desc_t *d = cos_knob(p, k, &vp);
    int32_t v;
    if (!d || !vp || d->max == d->min) return;
    if (cz.mode == CM_SRC || cz.mode == CM_AMP || cz.mode == CM_FX) { cz.lk_mode = cz.mode; cz.lk_tab = cz.tab[cz.mode]; }
    v = param_turn(d, *vp, accel(EN_K1 + k, s, d->fmt == F_ENUM ? 0 : d->max - d->min));
    cz.hot = (uint8_t)(k + 1u);
    cz.hot_ms = fm1_ms;
    if (p->kind == CK_AC) ac_put(p->id[k], v);
    else if (p->kind == CK_EU) eu_put(p->id[k], v);
    else if (p->kind == CK_COS) cos_cput(p->id[k], v);
    else *vp = (int16_t)v;
}

/* the white key w (0..15) on TRIG: its step of the bank on or off (a kit: the lane's hit; a synth: a note, the
 * track's root in its register, or the step emptied) */
static void cos_trig(uint32_t w)
{
    track_t *t = TSEL;
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i = cz.bank * 16u + w;
    step_t *s;
    if (i >= len) { cos_say("PAST THE LENGTH"); return; }
    s = &t->step[i];
    if (cos_kit()) { grid_hit(t, i, cz.lane % NLANE, 2); return; }
    if (s->time == ST_NOTE && s->n) { step_clear(s); return; }
    step_clear(s);
    s->time = ST_NOTE;
    s->n = 1;
    s->note[0] = (uint8_t)(36 + (t->p[P_ROOT] % 12 + 12) % 12);
    s->vel = 100;
}
static uint32_t cos_banks(void) { return ((uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP) + 15u) / 16u; }

/* ACID's track: the first ANALOG one; none: the last live one that is neither a kit nor VOICE becomes ANALOG's ACID
 * sound (an UNDO point first; REC on GEN undoes it) */
static __attribute__((noinline)) uint32_t cos_acid_trk(uint32_t *made)
{
    uint32_t i, k = trk_live;
    char b[24];
    for (i = 0; i < trk_live; i++)
        if (trk_eng(&trk[i]) == ENGI_ANALOG) return i;
    for (i = trk_live; i-- > 0u;)
        if (!drum_track(&trk[i]) && trk_eng(&trk[i]) != ENGI_VOICE) { k = i; break; }
    if (k >= trk_live) k = trk_live - 1u;
    undo_mark();
    set_engine_of(&trk[k], ENGI_ANALOG);
    for (i = 0; i < ENG_ANALOG.npresets; i++) {
        const char *n = ENG_ANALOG.presets[i].name;
        if (n[0] == 'A' && n[1] == 'C' && n[2] == 'I' && n[3] == 'D' && !n[4]) { apply_preset_to(&trk[k], i); break; }
    }
    str_cpy(b, "T", 2); b[1] = (char)('1' + k); b[2] = 0;
    str_cpy(b + 2, " NOW ACID 303", 14);
    cos_say(b);
    *made = 1;
    return k;
}
static int cos_empty(const track_t *t)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++)
        if (t->step[i].time == ST_NOTE && (t->step[i].n || t->step[i].hit)) return 0;
    return 1;
}
/* GEN entered: on a kit, over to ACID's track; an empty track gets a line at once */
static void cos_gen_enter(void)
{
    uint32_t made = 0;
    if (cos_kit()) { track_select(cos_acid_trk(&made)); cz.bank = 0; }
    cz.tab[CM_GEN] = 0;
    if (cos_empty(TSEL) && !chain_busy()) ac_throw(song.sel, 1);
    if (made) ui.msg_t = 0;                              /* (what it said: the track's new sound, not NEW LINE) */
}

/* SAVE: the four slots (JIANT's projects, SECT A..D): their names read again on entering and after a save or a load.
 * OCT+ saves the music to the slot picked (stopped: a flash erase), OCT- loads it */
static __attribute__((noinline)) void cos_proj_scan(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) proj_slot_name(i, cz.pn[i]);
    cz.pn_ok = 1;
}
static __attribute__((noinline)) void cos_proj_oct(int up)
{
    if (up) project_save(cz.slot);
    else if (!cz.pn[cz.slot][0]) cos_say("EMPTY SLOT");
    else { project_load(cz.slot); ui.msg_t = 0; cos_say("LOADED"); }
    cos_proj_scan();
}

static void cos_mode(uint32_t m)
{
    if (m == CM_PROJ && cz.mode != CM_PROJ) { cz.slot = proj_cur_slot() < 4u ? (uint8_t)proj_cur_slot() : cz.slot; cos_proj_scan(); }
    if (m == CM_GEN && cz.mode != CM_GEN) { cz.mode = (uint8_t)m; cz.hot = 0; cos_gen_enter(); return; }
    if (cz.mode == m) {                                 /* again: the next page (Elektron's way) */
        const cpg_t *p;
        uint32_t n;
        cos_pages(m, &p, &n);
        cz.tab[m] = (uint8_t)((cz.tab[m] + 1u) % n);
    }
    cz.mode = (uint8_t)m;
    cz.hot = 0;
}
/* the keys are the interface's (silent) on HOME, TRIG and a kit's GEN; elsewhere they play the track */
static int cos_keys_ui(void)
{
    return cz.mode == CM_HOME || cz.mode == CM_TRIG || cz.mode == CM_KEY || cz.mode == CM_PROJ || cz.mode == CM_DRONE || (cz.mode == CM_GEN && cos_kit()) ||
           ((fm1_in.buttons >> panel.btn[B_SCL]) & 1u);
}

static __attribute__((noinline)) void cos_input(void)
{
    uint32_t released = 0, pressed = fm1_input_edges(&released), notes = fm1_input_note_edges(), k, b;
    int scl_held = (int)((fm1_in.buttons >> panel.btn[B_SCL]) & 1u);
    int32_t s;
    song.grid = cos_keys_ui() ? 2u : 0u;                 /* (seq.c: 2, every key silent) */
    for (k = 0; k < 32u && pressed; k++) {
        if (!((pressed >> k) & 1u)) continue;
        pressed &= ~(1u << k);
        b = panel_btn_of(k);
        if (b == B_PLAY) transport_req = song.playing || seq_counting() ? 2u : 1u;
        else if (b == B_REC && cz.mode == CM_DRONE) { cos_drone_clear(); cos_say("DRONE OFF"); }
        else if (b == B_REC && cz.mode == CM_TRIG && cos_held()) {   /* a step held + REC: its locks go */
            uint64_t h = cos_held();
            uint32_t i, n = 0;
            for (i = 0; i < NSTEP; i++) if ((h >> i) & 1u) n += motion_clear_locks(TSEL, i);
            cz.lk_used = 1;
            cos_say(n ? "LOCKS CLEARED" : "NO LOCKS");
        }
        else if (b == B_REC && cz.mode == CM_GEN) cos_say(undo_do() ? "NOTHING TO UNDO" : "UNDONE");
        else if (b == B_REC) cos_say("REC: CLASSIC UI (HOME+GLO)");
        else if (b == B_SCL) { cz.scl_down = 1; cz.scl_used = 0; }   /* (its tap acts when let go: held, it is a shift) */
        else if (b == B_OCTDN || b == B_OCTUP) {
            int up = b == B_OCTUP;
            if (cz.mode == CM_TRIG) {
                uint32_t n = cos_banks();
                cz.bank = (uint8_t)((cz.bank + (up ? 1u : n - 1u)) % n);
            } else if (cz.mode == CM_SRC && cz.tab[CM_SRC] == 2u) cos_words(up);
            else if (cz.mode == CM_PROJ) cos_proj_oct(up);
            else if (cz.mode == CM_GEN && cos_kit()) eu_oct(up, cz.tab[CM_GEN] == 2u);
            else if (cz.mode == CM_GEN) ac_oct(up, cz.tab[CM_GEN] == 2u);
            else if (cz.mode == CM_DRONE) cz.dr_oct = (int8_t)clamp(cz.dr_oct + (up ? 1 : -1), -2, 2);
            else song.octave = (int8_t)clamp(song.octave + (up ? 1 : -1), -3, 3);
        } else
            for (s = 0; s < CM_N; s++)
                if (CM_BTN[s] == b) cos_mode((uint32_t)s);
    }
    if (cz.scl_down && ((released >> panel.btn[B_SCL]) & 1u)) {
        cz.scl_down = 0;
        if (!cz.scl_used) cos_mode(CM_KEY);
    }
    if ((s = panel_enc(EN_SELECT)) != 0) {
        const cpg_t *p;
        uint32_t n;
        cos_pages(cz.mode, &p, &n);
        cz.tab[cz.mode] = (uint8_t)clamp((int32_t)cz.tab[cz.mode] + (s > 0 ? 1 : -1), 0, (int32_t)n - 1);
        cz.hot = 0;
    }
    if ((s = panel_enc(EN_ALGO)) != 0) {
        track_select((uint32_t)clamp((int32_t)song.sel + (s > 0 ? 1 : -1), 0, (int32_t)trk_live - 1));
        cz.bank = 0;
    }
    if ((s = panel_enc(EN_PRESET)) != 0 && cz.mode == CM_TRIG) {
        uint32_t n = cos_banks();
        cz.bank = (uint8_t)clamp((int32_t)cz.bank + (s > 0 ? 1 : -1), 0, (int32_t)n - 1);
    }
    for (k = 0; k < 4u; k++)
        if ((s = panel_enc(EN_K1 + k)) != 0) {
            uint64_t h = cz.mode == CM_TRIG ? cos_held() : 0u;
            if (h) cos_lock_turn(k, s, h);                   /* (steps held: their locks) */
            else cos_turn(k, s);
        }
    if (cz.bank >= cos_banks()) cz.bank = 0;
    for (k = 0; k < 27u && notes; k++) {
        uint32_t w, j;
        if (!((notes >> k) & 1u)) continue;
        notes &= ~(1u << k);
        for (w = 0; w < 16u && WK[w] != k; w++) {}
        for (j = 0; j < 11u && BK[j] != k; j++) {}
        if (scl_held || cz.mode == CM_KEY) {               /* the song's new key */
            cz.scl_used = (uint8_t)scl_held;
            cos_key_ask(k);
        } else if (cz.mode == CM_DRONE) {
            cos_drone_key(k);
        } else if (cz.mode == CM_PROJ) {
            if (w < 4u) cz.slot = (uint8_t)w;
        } else if (cz.mode == CM_HOME) {
            if (w < trk_live) track_select(w);
            else if (j < trk_live) {
                trk[j].p[P_MUTE] = (int16_t)!trk[j].p[P_MUTE];
                cos_say(trk[j].p[P_MUTE] ? "MUTED" : "UNMUTED");
            }
        } else if (cz.mode == CM_TRIG) {
            if (w < 16u) { if (!cz.tg_down) cz.lk_used = 0; cz.tg_down |= 1u << k; }   /* (the trig on release) */
            else if (cos_kit() && j < NLANE) cz.lane = (uint8_t)j;
        } else if (cz.mode == CM_GEN && cos_kit() && j < NLANE) {
            eu_put(EK_K4, (int32_t)j + 1);                /* (EUCLID: a lane picked) */
        }
    }
    {                                                    /* TRIG: a white key let go: its trig, unless a lock was set */
        uint32_t up = cz.tg_down & ~fm1_in.notes, w;
        for (w = 0; w < 16u && up; w++)
            if ((up >> WK[w]) & 1u) {
                if (!cz.lk_used && cz.mode == CM_TRIG) cos_trig(w);
                cz.tg_down &= ~(1u << WK[w]);
                up &= ~(1u << WK[w]);
            }
        cz.tg_down &= fm1_in.notes;
    }
    if (cz.hot && fm1_ms - cz.hot_ms > 1500u) cz.hot = 0;
    if (ui.msg_t && ui.msg[0]) { cos_say(ui.msg); ui.msg_t = 0; }   /* (what JIANT's code says: on our top line) */
    cos_key_poll();
    ui_polls();                                          /* (what JIANT's frame keeps doing: ui_input.c) */
}

/* ------------------------------------------------------------ drawing --- */
/* the face: 8 x 8 a character; tall (values, titles) twice as high. Each row's runs of set pixels one rectangle */
enum { CZ_LBL, CZ_TALL };
static void cos_px(int32_t x, int32_t y, const char *s, uint16_t c, uint32_t tall)
{
    int32_t sy = tall ? 2 : 1, r, b, b0;
    for (; *s; s++, x += 8) {
        uint32_t ch = (uint32_t)(uint8_t)*s;
        const uint8_t *g;
        if (ch >= 'a' && ch <= 'z') ch -= 32u;
        if (ch < 32u || ch > 95u) ch = '?';
        g = COS_FONT[ch - 32u];
        for (r = 0; r < 8; r++)
            for (b = 0; b < 8;) {
                if (!((g[r] << b) & 0x80u)) { b++; continue; }
                for (b0 = b; b < 8 && ((g[r] << b) & 0x80u); b++) {}
                cv_rect(x + b0, y + r * sy, b - b0, sy, c);
            }
    }
}
static int32_t cos_w(const char *s) { return 8 * (int32_t)str_len(s); }
static void cos_tx(int32_t x, int32_t y, const char *s, uint16_t c) { cos_px(x, y, s, c, CZ_LBL); }
static void cos_txr(int32_t xr, int32_t y, const char *s, uint16_t c) { cos_px(xr - cos_w(s), y, s, c, CZ_LBL); }
static void cos_txc(int32_t xc, int32_t y, uint32_t tall, const char *s, uint16_t c) { cos_px(xc - cos_w(s) / 2, y, s, c, tall); }
static void cos_up(char *b) { for (; *b; b++) if (*b >= 'a' && *b <= 'z') *b = (char)(*b - 32); }

/* a step of track t at i: 0 empty, 1 a trig, 2 an accent, 3 a tie (a kit: lane l only, l < NLANE; else any lane) */
static uint32_t cos_step(const track_t *t, uint32_t i, uint32_t l)
{
    const step_t *s = &t->step[i % NSTEP];
    if (drum_track(t)) {
        uint32_t m = l < NLANE ? 1u << l : 0xFFu;
        return !(step_lanes(s) & m) ? 0u : (step_accents(s) & m) ? 2u : 1u;
    }
    if (s->time == ST_TIE) return 3u;
    return s->time != ST_NOTE || !s->n ? 0u : (s->flags & SF_ACCENT) ? 2u : 1u;
}

static void cos_head(void)
{
    char b[24];
    const track_t *t = TSEL;
    cv_rect(0, 0, 26, 14, C_FG);
    b[0] = 'T'; b[1] = (char)('1' + song.sel % NTRK); b[2] = 0;
    cos_tx(5, 3, b, C_BG);
    cos_tx(32, 3, CM_NAME[cz.mode], C_FG);
    if (cz.msg[0] && fm1_ms - cz.msg_ms < 1800u) {
        str_cpy(b, cz.msg, 19);                           /* (a long one over the key and the tempo: 18 at most) */
        cos_up(b);
        cos_tx(32 + 8 * 6, 3, b, C_RED);
        if (str_len(b) > 11u) goto tri;
    } else {
        str_cpy(b, trk_engp(t)->name, 9);
        cos_up(b);
        cos_tx(32 + 8 * 6, 3, b, C_DIM);
    }
    b[0] = 0;
    fmt_int(b, song.g[G_BPM]);
    cos_txr(226, 3, b, C_FG);
    cos_txr(226 - 8 * 4, 3, COS_NN[cz.key_next ? cz.key_next - 1u : cos_key()], cz.key_next ? C_RED : C_MID);
tri:
    {                                                     /* the play triangle, red while it runs */
        int32_t i;
        for (i = 0; i < 5; i++) cv_rect(230 + i, 2 + i, 1, 10 - 2 * i, song.playing ? C_RED : C_DIM);
    }
    cv_rect(0, 16, 240, 1, C_DARK);
}

static void cos_tabs(void)
{
    const cpg_t *p;
    uint32_t n, i;
    int32_t x = 2;
    if (cz.mode == CM_TRIG && cos_held()) {               /* (steps held: what a knob locks) */
        char b[24];
        str_cpy(b, "LOCK ", sizeof b);
        str_cpy(b + 5, cos_lk_page()->tab, 12);
        cv_rect(2, 20, cos_w(b) + 6, 12, C_RED);
        cos_tx(5, 22, b, C_BG);
        return;
    }
    cos_pages(cz.mode, &p, &n);
    for (i = 0; i < n; i++) {
        int32_t w = cos_w(p[i].tab) + 6;
        if (i == cz.tab[cz.mode] % n) { cv_rect(x, 20, w, 12, C_FG); cos_tx(x + 3, 22, p[i].tab, C_BG); }
        else cos_tx(x + 3, 22, p[i].tab, C_DIM);
        x += w + 4;
    }
    if (cz.mode == CM_TRIG && cos_banks() > 1u) {       /* the bank of 16: 1/4 */
        char b[8];
        b[0] = (char)('1' + cz.bank); b[1] = '/'; b[2] = (char)('0' + cos_banks()); b[3] = 0;
        cos_txr(238, 22, b, C_FG);
    }
}

/* the dial of knob k at (cx, cy): its arc 270 degrees, the value's part bright (red while turned), the label under it
 * and the value under that */
static void cos_dial(int32_t cx, int32_t cy, uint32_t k)
{
    uint64_t held = cz.mode == CM_TRIG ? cos_held() : 0u;   /* (TRIG with steps held: the lock page, the first one's locks) */
    const cpg_t *p = held ? cos_lk_page() : cos_page();
    int16_t *vp, lv;
    const param_desc_t *d = cos_knob(p, k, &vp);
    char val[24], b[16];
    const char *unit = 0;
    int32_t a, v, r = 12, lo, hi, z;
    uint32_t hot = cz.hot == k + 1u, locked = 0;
    if (!d || !vp) {
        cos_txc(cx, cy + r + 4, CZ_LBL, "-", C_DARK);
        return;
    }
    v = *vp;
    if (held && p->kind == CK_TRK) {
        uint32_t i;
        for (i = 0; i < NSTEP && !((held >> i) & 1u); i++) {}
        locked = (uint32_t)motion_lock_get(TSEL, i, p->id[k & 3u], &lv);
        v = locked ? lv : motion_base_value(TSEL, p->id[k & 3u]);
        hot |= locked;                                     /* (a locked value: red, the live thing) */
    }
    lo = d->min; hi = d->max;
    z = d->fmt == F_BIPCT || d->fmt == F_SEMI || (lo < 0 && hi > 0) ? 0 : lo;   /* (bipolar: from the middle) */
    for (a = 0; a <= 40; a++) {                           /* 40 dots round 270 degrees, from 7:30 to 4:30 */
        uint32_t ang = (uint32_t)(0x6000 + a * 0xC000 / 40) & 0xFFFFu;
        int32_t x = cx + ((r * og_cos(ang)) >> 15), y = cy + ((r * og_sin(ang)) >> 15);
        int32_t at = lo + (hi - lo) * a / 40, on = hi > lo && ((at >= z && at <= v) || (at <= z && at >= v));
        cv_rect(x - 1, y - 1, 2, 2, on ? (hot ? C_RED : C_FG) : C_DARK);
    }
    {                                                     /* the pointer */
        uint32_t ang = (uint32_t)(0x6000 + (hi > lo ? (v - lo) * 0xC000 / (hi - lo) : 0)) & 0xFFFFu;
        cv_line(cx, cy, cx + (((r - 5) * og_cos(ang)) >> 15), cy + (((r - 5) * og_sin(ang)) >> 15), C_FG);
    }
    str_cpy(b, d->label, sizeof b);
    cos_up(b);
    if (str_len(b) > 7u) b[7] = 0;
    cos_txc(cx, cy + r + 4, CZ_LBL, b, C_MID);
    param_format(d, v, val, &unit);
    cos_up(val);
    if (str_len(val) > 7u) val[7] = 0;
    cos_txc(cx, cy + r + 15, CZ_TALL, val, hot ? C_RED : C_FG);
}

/* the foot: the selected track's sixteen of the bank, the playhead's a red bar over its cell */
static void cos_foot(void)
{
    const track_t *t = TSEL;
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, now = song.playing ? t->seq_idx % len : ~0u;
    uint32_t lane = cz.mode == CM_TRIG && cos_kit() ? cz.lane : NLANE;
    uint64_t lks = motion_lock_steps(song.sel);
    for (i = 0; i < 16u; i++) {
        uint32_t st = cz.bank * 16u + i, v = st < len ? cos_step(t, st, lane) : 0u;
        int32_t x = 2 + (int32_t)i * 15, y = 224;
        uint16_t edge = st >= len ? C_BG : (i & 3u) ? C_DARK : C_DIM;
        cv_frame(x, y, 13, 13, edge);
        if (v == 1u) cv_rect(x + 1, y + 1, 11, 11, C_MID);   /* (a trig grey, an accent white: red is the playhead's) */
        else if (v == 2u) cv_rect(x + 1, y + 1, 11, 11, C_FG);
        else if (v == 3u) cv_rect(x + 1, y + 5, 11, 3, C_DIM);
        if (st == now) cv_rect(x, y - 3, 13, 2, C_RED);
        if (st < len && ((lks >> st) & 1u)) cv_rect(x + 5, y + 14, 3, 2, C_FG);   /* (its locks) */
    }
}

static void cos_body_home(void)
{
    uint32_t t, i;
    for (t = 0; t < trk_live && t < NTRK; t++) {
        const track_t *tr = &trk[t];
        uint32_t len = (uint32_t)clamp(tr->p[P_SLEN], 1, NSTEP), now = song.playing ? tr->seq_idx % len : ~0u;
        int32_t y = 38 + (int32_t)t * 18;
        int mute = tr->p[P_MUTE] != 0;
        char b[12];
        if (t == song.sel) cv_frame(0, y - 3, 240, 16, C_FG);
        b[0] = (char)('1' + t); b[1] = 0;
        cos_tx(4, y + 1, b, t == song.sel ? C_RED : C_FG);
        if (cz.dr_n && cz.dr_trk == t) cv_rect(236, y + 1, 3, 7, C_FG);   /* (its drone holds) */
        str_cpy(b, ENGINES[trk_eng(tr)]->name, 6);
        cos_up(b);
        cos_tx(14, y + 1, b, mute ? C_DIM : C_FG);
        for (i = 0; i < 16u; i++) {
            uint32_t st = (now != ~0u ? now / 16u * 16u : 0u) + i, v = st < len ? cos_step(tr, st, NLANE) : 0u;
            int32_t x = 64 + (int32_t)i * 11;
            uint16_t c = mute ? C_DIM : v == 2u ? C_FG : C_MID;
            if (v == 1u || v == 2u) cv_rect(x, y, 9, 9, c);
            else if (v == 3u) cv_rect(x, y + 3, 9, 3, C_DIM);
            else cv_frame(x, y, 9, 9, C_DARK);
            if (st == now) cv_rect(x, y + 10, 9, 1, C_RED);
        }
    }
    cos_tx(4, 144, "WHITE:TRACK  BLACK:MUTE", C_DIM);
}

/* TRIG: a kit's eight lanes (the picked one framed), a synth's notes as a roll */
static int32_t cz_rtop = 40;                             /* (the note roll's top: GEN's line of words above it) */
static void cos_lk_box(void)                            /* (steps held: which, and what the knobs and REC do) */
{
    uint64_t h = cos_held();
    uint32_t st = 0, n = 0;
    char b[16];
    if (!h) return;
    while (!((h >> st) & 1u)) st++;
    for (; h; h &= h - 1u) n++;
    str_cpy(b, "STEP ", 6);
    b[5] = (char)('0' + (st + 1u) / 10u); b[6] = (char)('0' + (st + 1u) % 10u); b[7] = 0;
    if (n > 1u) { b[7] = '+'; b[8] = (char)('0' + (n - 1u) % 10u); b[9] = 0; }
    cv_rect(28, 62, 184, 52, C_BG);
    cv_frame(28, 62, 184, 52, C_RED);
    cos_txc(120, 70, CZ_TALL, b, C_FG);
    cos_txc(120, 98, CZ_LBL, "KNOB LOCK  REC CLEAR", C_DIM);
}

static void cos_body_trig1(void);
static void cos_body_trig(void) { cos_body_trig1(); cos_lk_box(); }
static void cos_body_trig1(void)
{
    const track_t *t = TSEL;
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, l, now = song.playing ? t->seq_idx % len : ~0u;
    if (cos_kit()) {
        for (l = 0; l < NLANE; l++) {
            int32_t y = 38 + (int32_t)l * 14;
            if (l == cz.lane) cv_rect(0, y - 2, 240, 13, C_DARK);
            cos_tx(4, y + 1, COS_LN[l], l == cz.lane ? C_RED : C_MID);
            for (i = 0; i < 16u; i++) {
                uint32_t st = cz.bank * 16u + i, v = st < len ? cos_step(t, st, l) : 0u;
                int32_t x = 26 + (int32_t)i * 13;
                if (v) cv_rect(x, y, 10, 9, v == 2u ? C_FG : C_MID);
                else cv_rect(x + 4, y + 4, 2, 2, st < len ? C_DIM : C_BG);
                if (st == now) cv_rect(x, y + 10, 10, 1, C_RED);
            }
        }
        return;
    }
    {
        int32_t lo = 127, hi = 0;
        for (i = 0; i < 16u; i++) {
            const step_t *s = &t->step[(cz.bank * 16u + i) % NSTEP];
            if (s->time == ST_NOTE && s->n) { lo = s->note[0] < lo ? s->note[0] : lo; hi = s->note[0] > hi ? s->note[0] : hi; }
        }
        if (hi < lo) { lo = 36; hi = 48; }
        if (hi - lo < 12) hi = lo + 12;
        for (i = 0; i < 16u; i++) {
            uint32_t st = cz.bank * 16u + i, g = 1;
            const step_t *s = &t->step[st % NSTEP];
            int32_t x = 2 + (int32_t)i * 15, y;
            if (st == now) cv_rect(x, cz_rtop - 4, 13, 150 - cz_rtop, C_DARK);
            if (st >= len || s->time != ST_NOTE || !s->n) continue;
            while (i + g < 16u && t->step[(st + g) % NSTEP].time == ST_TIE) g++;
            y = 140 - (s->note[0] - lo) * (140 - cz_rtop) / (hi - lo);
            cv_rect(x, y, 15 * (int32_t)g - 2, (s->flags & SF_ACCENT) ? 5 : 3, (s->flags & SF_ACCENT) ? C_FG : C_MID);
            if (s->flags & SF_SLIDE) cv_rect(x + 15 * (int32_t)g - 2, y + 1, 4, 2, C_MID);
        }
    }
}

/* AMP: the envelope's shape; WORDS: the word it sings large, its voice's level; elsewhere the sound's name large */
static void cos_body_sound(void)
{
    const track_t *t = TSEL;
    char b[40];
    if (cz.mode == CM_SRC && cz.tab[CM_SRC] == 2u && trk_engp(t) == &ENG_FORMANT) {
        const sing_t *sv = sing_view(t);
        int32_t lv = clamp(sv->gv >> 9, 0, 64);
        str_cpy(b, t->p[P_VLANG] ? (sv->w[0] ? sv->w : "PLAY A NOTE") : "LANG: OFF", 20);
        cos_up(b);
        cos_txc(120, 52, CZ_TALL, b, C_FG);
        cos_tx(20, 84, "SAYS", C_DIM);
        cv_frame(56, 82, 130, 11, C_DIM);
        cv_rect(58, 84, lv * 2, 7, C_FG);     /* (the voicing: the mouth open) */
        str_cpy(b, N_LANG[t->p[P_VLANG] % LG_N], 8); str_cpy(b + str_len(b), " ", 2);
        str_cpy(b + str_len(b), N_LYRIC[(uint32_t)t->p[P_VLYR] % LYR_N], 6);
        cos_tx(20, 104, b, C_MID);
        cos_tx(20, 124, "OCT+ NEW WORDS  OCT- BEFORE", C_DIM);
        return;
    }
    if (cz.mode == CM_AMP && cz.tab[CM_AMP] == 0u) {
        int32_t a = t->p[P_ATK] * 50 / 127, d = t->p[P_DEC] * 50 / 127, s = t->p[P_SUS], r = t->p[P_REL] * 50 / 127;
        int32_t x0 = 20, y0 = 140, top = 50, sy = y0 - (y0 - top) * s / 127;
        cv_line(x0, y0, x0 + a, top, C_FG);
        cv_line(x0 + a, top, x0 + a + d, sy, C_FG);
        cv_line(x0 + a + d, sy, 200 - r, sy, C_FG);
        cv_line(200 - r, sy, 200, y0, C_FG);
        cv_rect(x0, y0, 182, 1, C_DARK);
        return;
    }
    sound_name(t, b);
    cos_up(b);
    if (str_len(b) > 18u) b[18] = 0;
    cos_txc(120, 70, CZ_TALL, b, C_FG);
    str_cpy(b, trk_engp(t)->name, 16);
    cos_up(b);
    cos_txc(120, 98, CZ_LBL, b, C_DIM);
}

/* KEY: the song's key large (the next one in red while it waits for the bar) and an octave of keys from F, the key lit */
static void cos_body_key(void)
{
    static const uint8_t BL[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};   /* from F: F F# G G# A A# B C C# D D# E */
    uint32_t i, cur = cos_key(), nx = cz.key_next ? cz.key_next - 1u : 99u;
    char b[24];
    str_cpy(b, COS_NN[cur], sizeof b);
    cos_px(20, 46, b, C_FG, CZ_TALL);
    cos_tx(20, 74, "THE SONG'S KEY", C_DIM);
    if (nx < 12u) { str_cpy(b, "NEXT BAR: ", sizeof b); str_cpy(b + str_len(b), COS_NN[nx], 4); cos_tx(110, 52, b, C_RED); }
    for (i = 0; i < 12u; i++) {                         /* white keys a row, black keys above between them */
        uint32_t pc = (5u + i) % 12u, wi = 0, j;
        int on = pc == cur, nxt = pc == nx;
        for (j = 0; j < i; j++) wi += !BL[j];
        if (BL[i]) cv_rect(14 + (int32_t)wi * 30 - 9, 92, 16, 26, nxt ? C_RED : on ? C_FG : C_DARK);
        else {
            cv_frame(14 + (int32_t)wi * 30, 92, 28, 50, C_DIM);
            if (on || nxt) cv_rect(16 + (int32_t)wi * 30, 122, 24, 18, nxt ? C_RED : C_FG);
        }
    }
    cos_tx(4, 146, "A KEY: THE SONG MOVES THERE", C_DIM);
}

/* DRONE: the track and its held notes large, an octave and a half of keys (the held ones lit), the octave; the drone
 * plays on through PLAY and STOP */
static void cos_body_drone(void)
{
    static const uint8_t BL[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};   /* from F */
    char b[24];
    uint32_t i, k, wi;
    int32_t base = 41 + 12 * cz.dr_oct;
    if (!cz.dr_n) {
        cos_px(20, 44, "PRESS KEYS", C_DIM, CZ_TALL);
        cos_tx(20, 70, "THEY HOLD. AGAIN: LET GO", C_DIM);
    } else {
        b[0] = 'T'; b[1] = (char)('1' + cz.dr_trk % NTRK); b[2] = 0;
        cos_px(4, 44, b, C_FG, CZ_TALL);
        for (i = 0, b[0] = 0; i < cz.dr_n; i++) {
            if (i) str_cpy(b + str_len(b), " ", 2);
            str_cpy(b + str_len(b), COS_NN[cz.dr_note[i] % 12u], 3);
            fmt_int(b + str_len(b), (int32_t)cz.dr_note[i] / 12 - 1);
        }
        cos_px(40, 44, b, C_FG, CZ_TALL);
        cos_tx(40, 66, song.playing ? "FREE RUNNING  REC: LET GO" : "HOLDING  REC: LET GO", C_DIM);
    }
    for (k = 0, wi = 0; k < 20u; k++) {                  /* F .. C, an octave and a half */
        int32_t note = base + (int32_t)k, on = 0;
        for (i = 0; i < cz.dr_n; i++) on |= cz.dr_note[i] == (uint32_t)note && cz.dr_trk == song.sel;
        if (BL[k % 12u]) cv_rect(8 + (int32_t)wi * 19 - 6, 86, 12, 26, on ? C_RED : C_DARK);
        else {
            cv_frame(8 + (int32_t)wi * 19, 86, 18, 50, C_DIM);
            if (on) cv_rect(10 + (int32_t)wi * 19, 116, 14, 18, C_RED);
            wi++;
        }
    }
    b[0] = 0; str_cpy(b, "OCT ", sizeof b); fmt_int(b + 4, cz.dr_oct);
    cos_tx(4, 142, b, C_MID);
}

/* GLO > MIDI: where each track goes (its MIDI OUT channel: the Syntakt's track), MIDI OUT, the clock */
static void cos_body_midi(void)
{
    static const char *const MO[4] = {"KEYS", "CLOCK", "ALL", "OFF"};
    static const char *const CK[3] = {"INT", "USB", "TRS"};
    char b[24];
    uint32_t t;
    for (t = 0; t < trk_live && t < NTRK; t++) {
        uint32_t v = favorites.factory[15][8u + t], ch = v && v <= 16u ? v : t + 1u;
        int32_t x = 4 + (int32_t)(t % 3u) * 80, y = 40 + (int32_t)(t / 3u) * 20;
        b[0] = 'T'; b[1] = (char)('1' + t); b[2] = ' '; b[3] = 'C'; b[4] = 'H'; b[5] = 0;
        fmt_int(b + 5, (int32_t)ch);
        cos_tx(x, y, b, t == song.sel ? C_RED : C_FG);
    }
    str_cpy(b, "OUT: ", sizeof b); str_cpy(b + 5, MO[midi_out_mode & 3u], 8);
    cos_tx(4, 94, b, (midi_out_mode & 3u) == 2u ? C_FG : C_MID);
    str_cpy(b, "CLOCK: ", sizeof b); str_cpy(b + 7, CK[(uint32_t)song.g[G_CLOCK] % 3u], 8);
    cos_tx(4, 108, b, C_FG);
    cos_tx(4, 128, (midi_out_mode & 3u) == 2u ? "NOTES GO OUT: SYNTAKT CAN REC" : "ALL: THE NOTES GO OUT TOO", C_DIM);
}

static void cos_body_gen(void)
{
    if (cos_kit()) { cos_body_trig(); return; }
    {
        const ac_t *a = ac_of(song.sel);
        eu_t key;
        char b[24];
        static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        static const char *const LK[4] = {"", "LOCK:RHYTHM", "LOCK:NOTES", "LOCK:R+N"};
        uint32_t i;
        ac_key(song.sel, a, &key);
        str_cpy(b, NN[key.root % 12u], sizeof b);
        for (i = 1; i < sizeof SCALE_MASK / sizeof SCALE_MASK[0] && SCALE_MASK[i] != key.mask; i++) {}
        if (i < sizeof SCALE_MASK / sizeof SCALE_MASK[0]) { str_cpy(b + str_len(b), " ", 2); str_cpy(b + str_len(b), N_SCALE[i], 8); }
        cos_tx(4, 38, b, C_FG);
        if (a->ms) cos_txr(236, 38, "MUTATED", C_RED);
        else if (a->lock & 3u) cos_txr(236, 38, LK[a->lock & 3u], C_DIM);
        else cos_txr(236, 38, "OCT+ NEW LINE", C_DIM);
        cz_rtop = 54;
        cos_body_trig();
        cz_rtop = 40;
    }
}

static void cos_body_proj(void)
{
    uint32_t i;
    char b[24];
    for (i = 0; i < 4u; i++) {
        int32_t y = 36 + (int32_t)i * 24;
        int pick = i == cz.slot, used = cz.pn[i][0] != 0;
        if (pick) cv_rect(0, y - 3, 240, 20, C_DARK);
        b[0] = (char)('A' + i); b[1] = 0;
        cos_px(8, y, b, pick ? C_FG : C_MID, CZ_TALL);
        cos_tx(32, y + 4, used ? cz.pn[i] : "EMPTY", used ? (pick ? C_FG : C_MID) : C_DIM);
        if (i == proj_cur_slot()) cos_txr(232, y + 4, "NOW", C_MID);
    }
    cos_txc(120, 136, CZ_LBL, song.playing ? "OCT- LOAD  (STOP TO SAVE)" : "OCT+ SAVE  OCT- LOAD", C_DIM);
}

/* the whole scene, drawn into the strip on the canvas (cv_oy: its top) */
static void cos_scene(void)
{
    cos_head();
    cos_tabs();
    switch (cz.mode) {
    case CM_HOME: cos_body_home(); break;
    case CM_TRIG: cos_body_trig(); break;
    case CM_GEN: cos_body_gen(); break;
    case CM_KEY: cos_body_key(); break;
    case CM_PROJ: cos_body_proj(); break;
    case CM_DRONE: cos_body_drone(); break;
    case CM_MIX: if (cz.tab[CM_MIX] == 1u) { cos_body_midi(); break; } cos_body_sound(); break;
    default: cos_body_sound(); break;
    }
    if (cz.mode == CM_GEN && !cos_kit()) {}            /* (GEN's key over its roll: drawn above) */
    {
        uint32_t k;
        cv_rect(0, 156, 240, 1, C_DARK);
        for (k = 0; k < 4u; k++) cos_dial(30 + (int32_t)k * 60, 172, k);
    }
    cos_foot();
}

/* six strips of 40 rows: each drawn whole, sent only when its pixels changed (the panel takes ~77 ms a screen) */
/* ------------------------------------------------------------ power-on --- */
/* the splash (ui_draw.c draw_splash / splash_write under COS_SLIM; main.c's boot loop calls it every ~4 ms for
 * SPL_WRITE + SPL_HOLD ms): black, a few still stars, the name 2x, the version, a 16-block bar filling like a pattern
 * (the block being filled red), READY. Drawn whole once, then only the bar's strip when a block fills */
#define COS_VERSION "0.4"
static void cos_px2(int32_t x, int32_t y, const char *s, uint16_t c)      /* (the face 2 x 2) */
{
    int32_t r, b, b0;
    for (; *s; s++, x += 16) {
        uint32_t ch = (uint32_t)(uint8_t)*s;
        const uint8_t *g;
        if (ch < 32u || ch > 95u) ch = '?';
        g = COS_FONT[ch - 32u];
        for (r = 0; r < 8; r++)
            for (b = 0; b < 8;) {
                if (!((g[r] << b) & 0x80u)) { b++; continue; }
                for (b0 = b; b < 8 && ((g[r] << b) & 0x80u); b++) {}
                cv_rect(x + b0 * 2, y + r * 2, (b - b0) * 2, 2, c);
            }
    }
}
static void cos_splash_scene(uint32_t n)
{
    uint32_t k;
    for (k = 0; k < 30u; k++) {                          /* the stars: still, away from the words */
        uint32_t h = (k + 7u) * 2654435761u;
        int32_t x = (int32_t)((h >> 4) % 236u) + 2, y = (int32_t)((h >> 14) % 236u) + 2;
        if (x > 30 && x < 210 && y > 64 && y < 196) continue;
        cv_rect(x, y, (h >> 30) == 3u ? 2 : 1, (h >> 30) == 3u ? 2 : 1, (h >> 28) & 1u ? C_MID : C_DIM);
    }
    cos_px2(120 - 72, 82, "CURSED-OS", C_FG);
    cv_rect(48, 104, 144, 1, C_DIM);
    cos_txc(120, 112, CZ_LBL, "FM-1  V" COS_VERSION, C_MID);
    for (k = 0; k < 16u; k++)                            /* the bar: 16 steps */
        cv_rect(25 + (int32_t)k * 12, 160, 9, 6, k < n ? C_FG : k == n ? C_RED : C_DARK);
    cos_txc(120, 174, CZ_LBL, n >= 16u ? "READY" : "BOOT", n >= 16u ? C_FG : C_DIM);
    cos_txc(120, 222, CZ_LBL, "ON JIANT " FELUCCA_VERSION, C_DARK);
}
static __attribute__((noinline)) void cos_splash(uint32_t ms, int whole)
{
    static uint8_t shown COS_RAM;
    uint32_t n = ms * 16u / (SPL_WRITE + SPL_HOLD - 350u), s;
    if (n > 16u) n = 16u;
    if (!whole && n == shown) return;
    for (s = whole ? 0u : 4u; s < (whole ? 6u : 5u); s++) {   /* (the bar and READY: strip 4, 160..199) */
        cv_begin(240, 40, C_BG);
        cv_oy = -(int32_t)s * 40;
        cos_splash_scene(n);
        cv_oy = 0;
        cv_blit(0, s * 40u);
    }
    shown = (uint8_t)n;
    lcd_sync();
}

static __attribute__((noinline)) void cos_draw(void)
{
    uint32_t s, i, h;
    ui.frame++;
    if (!scr_frame())
        return;
    for (s = 0; s < 6u; s++) {
        cv_begin(240, 40, C_BG);
        cv_oy = -(int32_t)s * 40;
        cos_scene();
        cv_oy = 0;
        for (h = 2166136261u, i = 0; i < 240u * 40u; i++) h = (h ^ cv_px[i]) * 16777619u;
        if (h != cz.sig[s] || !cz.ready) {
            cz.sig[s] = h;
            cv_blit(0, s * 40u);
        }
    }
    cz.ready = 1;
    scr_shown();
}

static __attribute__((noinline)) void cos_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0};
    const track_t *t = TSEL;
    uint32_t k, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), now = song.playing ? t->seq_idx % len : ~0u;
    static uint8_t ready COS_RAM;
    if (fm1_led_anim_on()) return;
    if (!ready) { led_pos_init(); ready = 1; }
    led_put(nl, panel.btn[CM_BTN[cz.mode]], 1);
    led_put(nl, panel.btn[B_REC], song.rec != 0u);
    for (k = 0; k < NB; k++) led_put(nd, panel.btn[k], 1);
    if (cz.mode == CM_TRIG || cz.mode == CM_HOME) {
        for (k = 0; k < 16u; k++) {
            uint32_t st = cz.bank * 16u + k;
            int on = cz.mode == CM_HOME ? k == song.sel : st < len && (cos_step(t, st, cos_kit() ? cz.lane : NLANE) != 0u) != (st == now);
            led_put(nl, 14u + WK[k], on);
        }
        for (k = 0; k < 11u; k++)
            led_put(nl, 14u + BK[k], cz.mode == CM_HOME ? k < trk_live && !trk[k].p[P_MUTE] : cos_kit() && k == cz.lane);
    } else if (cz.mode == CM_DRONE) {
        for (k = 0; k < 27u; k++) {
            uint32_t i, on = 0;
            for (i = 0; i < cz.dr_n; i++) on |= cz.dr_trk == song.sel && (int32_t)cz.dr_note[i] == 41 + (int32_t)k + 12 * cz.dr_oct;
            led_put(nl, 14u + k, (int)on);
        }
    } else
        for (k = 0; k < 27u; k++) led_put(nl, 14u + k, (int)((fm1_in.notes >> k) & 1u));
    for (k = 0; k < 27u; k++) led_put(nd, 14u + k, 1);
    if (song.playing) { led_clear(nd, panel.btn[B_PLAY]); nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u)); }
    fm1_led_dim_level(0);
    for (k = 0; k < FM1_NCOL; k++) {
        fm1_led_dim[k] = (uint8_t)(nd[k] & ~nl[k]);
        fm1_led_breath[k] = 0;
        fm1_led_hi[k] = 0;
        fm1_led_mid[k] = 0;
        fm1_led[k] = nl[k];
    }
}

/* HOME + GLO held together: cursed-os <-> JIANT's interface (checked before either runs; the edge once) */
static __attribute__((noinline)) int cos_switch(void)
{
    uint32_t both = (1u << panel.btn[B_HOME]) | (1u << panel.btn[B_GLO]), held = (fm1_in.buttons & both) == both;
    if (held && !cz.combo) {
        cz.combo = 1;
        cz.flip ^= 1u;
        cz.ready = 0;
        song.grid = 0;
        ui.force = 1;
        (void)fm1_input_edges(0);                         /* (the two presses are the switch's, not a page's) */
        return 1;
    }
    if (!held) cz.combo = 0;
    return 0;
}
