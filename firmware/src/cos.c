/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound
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
 *         LFO FX); held + REC: its locks go. Locked steps: a dot under them. SEQ held + REC twice (3 s): the track
 *         empty (cos_clear). A synth: steps held + PRESETS their note (in the scale), + black 1 ACCENT, 2 SLIDE;
 *         SEQ again: TRIG, STEP (a synth: STEP INPUT, a key the cursor's note, REC a rest, a key held + REC a tie:
 *         cos_si_*), EUCLID, MODES (JIANT's euclid on the track, OCT+ a new one)
 *   EDIT  SRC 1 / SRC 2: the engine (VOICE: and WORDS, the talking synth: LANG LYRIC THEME ARTIC, OCT+ new words)
 *   ENV   AMP / FLTR / LFO / LFO 2      FX  SEND / DIST
 *   LFO   DRUMS (0.13.0; DRONE before): white 1..8 held roll that drum, black 1..4 held repeat the drums' last 8 / 4 / 2 / 1
 *         steps; ROLL's RATE and RAMP, each drum's CHANCE (cos_drm.c)
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

enum { CM_HOME, CM_TRIG, CM_SRC, CM_AMP, CM_DRUMS, CM_FX, CM_GEN, CM_MIX, CM_KEY, CM_PROJ, CM_N };
enum { CK_NONE, CK_TRK, CK_GLO, CK_AC, CK_EU, CK_COS, CK_DB, CK_BL };   /* (0.15.1 CK_BL: GEN > BLEEP, ui_bleep.c) */
enum { CC_OUTCH, CC_MOUT, CC_CLOCK, CC_SLOT, CC_TRKS, CC_LOAD, CC_COMP, CC_CATK, CC_CREL, CC_LIM, CC_PF0, CC_PF3 = CC_PF0 + 3,
       CC_X2, CC_X2E = CC_X2 + 15, CC_OCT, CC_KICK, CC_KICKE = CC_KICK + 4, CC_LN0, CC_LNE = CC_LN0 + 7, CC_KDRV, CC_BT,
       CC_L1SYNC, CC_L1RATE, CC_L1WAVE, CC_L1MODE, CC_L1PH, CC_L1FADE, CC_L2SYNC, CC_L2RATE, CC_BPM, CC_BFINE, CC_USLOT, CC_LVDLY, CC_LVREV, CC_LVFB,
       CC_CIN, CC_CTHR, CC_CRAT, CC_CMK, CC_CKNEE, CC_CSC, CC_CMIX, CC_LIN, CC_LREL, CC_LVL, CC_MUTEQ, CC_PCCH, CC_PCOUT, CC_VJCH, CC_EQL, CC_EQM, CC_EQH, CC_MAC0, CC_MAC3 = CC_MAC0 + 3, CC_RB0, CC_RB3 = CC_RB0 + 3, CC_MOPOS, CC_MOBARS, CC_LVLEN, CC_LVRATE, CC_LVDN, CC_LVSH, CC_MCCH, CC_MCCC, CC_MCDIR, CC_MCMAP, CC_MC0, CC_MC3 = CC_MC0 + 3, CC_MOCC,
       CC_NAME, CC_IDLE, CC_SCT, CC_SCG, CC_SCTR, CC_SPV, CC_SPR, CC_SPS, CC_SCM, CC_SPSRC, CC_SCZ, CC_LUT, CC_LUR, CC_LOCAL, CC_KPAT, CC_SCALE, CC_QUANT, CC_DJF, CC_DSEL, CC_DLV, CC_DWAV, CC_DFLT, CC_DBT, CC_DDRV, CC_DLAY, CC_DFB, CC_DSNP, CC_DSRC, CC_DE0, CC_DEE = CC_DE0 + DE_N - 1,
       CC_RRATE, CC_RRAMP, CC_RVEL, CC_DCH0, CC_DCH7 = CC_DCH0 + 7,   /* (0.13.0) LFO > DRUMS' */
       CC_KSLOT,                                         /* (0.13.0) SAVE > KITS' slot */
       CC_CTLM, CC_CTLCH, CC_EXT,                        /* (0.13.0) GLO > MIDI 2: the knobs as a controller; SOUND */
       CC_TCOND, CC_TSLD, CC_TSND,                       /* (0.14.0) TRIG > COND: the held steps' CONDITION, SLIDE, SOUND */
       CC_DPAN, CC_DCHN,                                 /* (0.14.1) EDIT > SOUND on a kit: the drum's PAN, CHANCE */
       CC_STON, CC_STPAT, CC_STLEN,                      /* (0.17.0) GLO > SYNTAKT: ON, PATTERNS, PAT LEN */
       CC_LSAT, CC_QSTY, CC_QDRM,                        /* (0.18.0) GLO > LIMIT's SATURATE; SAVE > START's STYLE, DRUMS */
       CC_QELM, CC_QBAR };                               /* (0.18.1) START's ELEMENTS, BARS */
_Static_assert(CC_QBAR < 0xFBu && CC_DCH7 < 0xFBu, "the CK_COS ids fit a page's byte (0xFB..0xFF: BFINE, MUTE Q, BPM, OCTAVE, none)");
_Static_assert(X2_N <= 16u, "CC_X2 .. CC_X2E: room for every trk_x2 field");
static void cos_syntakt(uint32_t id, int32_t v);    /* (0.17.0) GLO > SYNTAKT's knobs */
/* (0.18.0, GG: "quick starts") SAVE > START: a jam ready in one press (cos_qs_load) */
enum { QS_HYPNO, QS_ACID, QS_DUB, QS_BLEEPS, QS_RAND, QS_N };   /* (0.18.2: RANDOM) */
static const char *const QS_NAME[QS_N + 1u] = {"HYPNOTIC", "ACID", "DUB TECHNO", "BLEEPS", "RAND", 0};   /* (RAND: the knob shows five letters) */
static const char *const QS_DRN[4] = {"FULL", "PERC", "NONE", 0};
static uint8_t qs_sty, qs_drm;                          /* (qs_drm: 0 the default, else DRUMS + 1) */
static uint8_t qs_elm, qs_bar = 2;                      /* (0.18.1) ELEMENTS: 0 ALL, 1 PICKED; BARS: 1 2 4 (0 .. 2) */
static const char *const QS_ELN[3] = {"ALL", "PICKED", 0};
static const char *const QS_BRN[4] = {"1", "2", "4", 0};
static uint8_t qs_mute, qs_have;                        /* (the mutes before it, bit a track: not in the UNDO's project) */
static uint8_t qs_keep, qs_pend;                        /* (0.18.2) KEEP: the tracks a load leaves alone; a load waiting
                                                         * for the bar's end (cos_qs_tick) */
static void cos_qs_undo(void);
static uint32_t qs_drm_get(void);
#define ID_MUTEQ 0xFCu                          /* (HOME 2's: MUTE Q, cos_knob) */
#define ID_BFINE 0xFBu                          /* (0.10.1) PERFORM's K2: the tempo's hundredths, .00 .. .99 */
#define ID_BPM 0xFDu                            /* (HOME's: BPM in tenths, cos_knob) */
#define ID_OCT 0xFEu                            /* (any page's knob: the track's OCTAVE, cos_knob) */   /* (CC_X2 + X2_*: params.c trk_x2, LFO 2 and FILTER 2) */   /* (CK_COS: cursed-os's own knobs) */
typedef struct { const char *tab; uint8_t kind, id[4]; } cpg_t;
#define X 0xFFu
static const cpg_t CZP_HOME[] = {{"PERFORM", CK_GLO, {ID_BPM, ID_BFINE, G_SWING, G_DJF}},   /* (0.10.1: BPM, FINE; OCTAVE */
                                {"LIVE", CK_COS, {CC_DJF, CC_LVREV, CC_LVDLY, CC_LVFB}},   /* (0.10.3) a set's moves on one page:
                                                         * white 1..8 the drums' sounds, black 1..6 the tracks; OCT- BACK TO
                                                         * NORMAL (cos_back), OCT+ this is normal */
                                {"PERFORM 2", CK_GLO, {G_DUCK, G_DREL, ID_MUTEQ, ID_OCT}},   /* to PERFORM 2) */
                                {"ISOLATOR", CK_COS, {CC_EQL, CC_EQM, CC_EQH, X}},   /* (cos_iso.c; HOME's black 9-11 the kills) */
                                {"MACROS", CK_COS, {CC_MAC0, CC_MAC0 + 1u, CC_MAC0 + 2u, CC_MAC3}},
                                {"MORPH", CK_COS, {CC_MOPOS, CC_MOBARS, X, X}},   /* (cos_morph.c: white 13 A, 14 B) */
                                {"LOOP", CK_COS, {CC_LVLEN, CC_LVRATE, CC_LVDN, CC_LVSH}}};   /* (JIANT's LIVE, seq_live.c: the
                                                         * track picked; white 15 back in phase) */   /* (the sounds' M1..M4 in the matrix) */   /* (JIANT's master EQ; HOME's black 9-11 the kills) */   /* (DUCK: the kick pumps the rest) */
/* SEQ: the sequencer's three ways, SEQ again (or SELECT) the next: TRIG the steps by hand, STEP the keys into the steps
 * one by one (a synth: STEP INPUT, cos_si_*), EUCLID (and its MODES) the track's steps as a euclidean rhythm, a kit's
 * lanes or a synth's notes in its scale (JIANT's EUCLID, ui_euclid.c; GEN's on a kit) */
#define CZ_COND {"COND", CK_COS, {CC_TCOND, CC_TSLD, CC_TSND, X}}   /* (0.14.0) hold steps: their CONDITION, SLIDE (the
                                                         * locks glide in), SOUND (a kit: the lane's sound on them) */
static const cpg_t CZP_TRIG[] = {{"TRIG", CK_TRK, {P_SLEN, P_SDIV, P_SSWING, P_SGATE}},
                                {"STEP", CK_TRK, {P_SLEN, P_SDIV, P_SSWING, P_SGATE}},
                                CZ_COND,
                                {"EUCLID", CK_EU, {EK_HITS, EK_STEPS, EK_ROT, EK_K4}},
                                {"MODES", CK_EU, {EK_M0, EK_M0 + 1u, EK_M0 + 2u, EK_M0 + 3u}}};
static const cpg_t CZP_TRIGK[] = {{"TRIG", CK_TRK, {P_SLEN, P_SDIV, P_SSWING, P_SGATE}},   /* (a kit: no STEP) */
                                 CZ_COND,
                                 {"EUCLID", CK_EU, {EK_HITS, EK_STEPS, EK_ROT, EK_K4}},
                                 {"MODES", CK_EU, {EK_M0, EK_M0 + 1u, EK_M0 + 2u, EK_M0 + 3u}}};
/* EDIT: the engine's eight (its own page names: cos_tabs), the voice, the matrix's four slots; a VOICE track WORDS
 * third. ENV: the amp, the filter, the envelope's modulation, the two LFO pages */
#define CZ_MOD(n) {"MOD " #n, CK_TRK, {P_M##n##SRC, P_M##n##DST, P_M##n##AMT, X}}
#define CZ_FILT {"FILTER", CK_TRK, {P_FTYPE, P_FCUT, P_FRES, P_ED_FLT}}   /* (every source's: the track's FILTER, as
                                                         * ENV > FILTER 1; ANALOG's its own CUT RES, CZP_SRCN) */
static const cpg_t CZP_SRC[] = {{"SRC 1", CK_TRK, {P_E0, P_E1, P_E2, P_E3}}, {"SRC 2", CK_TRK, {P_E4, P_E5, P_E6, P_E7}},
                               CZ_FILT,
                               {"VOICE", CK_TRK, {P_VOICE, P_GLIDE, P_DETUNE, ID_OCT}},
                               {"CHORD", CK_TRK, {P_CHRD, P_CVOI, P_CSTR, X}},   /* (a key plays a chord: seq.c chord_build) */
                               CZ_MOD(1), CZ_MOD(2), CZ_MOD(3), CZ_MOD(4)};
#define CZ_TRN {"TRANSIENT", CK_TRK, {P_DXIMG, P_MORPH, P_WRDEP, X}}   /* (0.11.0) FMDRUM's, KICK's (eng_fmdrum.c; 0.13.0
                                                         * T.PITCH: P_WRDEP) */
static const cpg_t CZP_SRCK[] = {{"KICK", CK_TRK, {P_E0, P_E1, P_E2, P_E3}}, {"KICK 2", CK_TRK, {P_E4, P_E5, P_E6, P_E7}},
                                CZ_TRN, CZ_FILT, CZ_MOD(1), CZ_MOD(2), CZ_MOD(3), CZ_MOD(4)};   /* (0.10.0) a KICK: hits, no VOICE / CHORD */
static const cpg_t CZP_SRCF[] = {{"FMDRUM", CK_TRK, {P_E0, P_E1, P_E2, P_E3}}, {"FMDRUM 2", CK_TRK, {P_E4, P_E5, P_E6, P_E7}},
                                CZ_TRN, CZ_FILT,
                                {"VOICE", CK_TRK, {P_VOICE, P_GLIDE, P_DETUNE, ID_OCT}},
                                CZ_MOD(1), CZ_MOD(2), CZ_MOD(3), CZ_MOD(4)};   /* (0.11.0) FMDRUM */
static const cpg_t CZP_SRCN[] = {{"SRC 1", CK_TRK, {P_E0, P_E1, P_E2, P_E3}}, {"SRC 2", CK_TRK, {P_E4, P_E5, P_E6, P_E7}},
                                {"FILTER", CK_TRK, {P_FTYPE, P_E4, P_E5, P_ED_FLT}},   /* (ANALOG: its CUT RES, E4 E5) */
                                {"VOICE", CK_TRK, {P_VOICE, P_GLIDE, P_DETUNE, ID_OCT}},
                                {"CHORD", CK_TRK, {P_CHRD, P_CVOI, P_CSTR, X}},
                                CZ_MOD(1), CZ_MOD(2), CZ_MOD(3), CZ_MOD(4)};
static const cpg_t CZP_SRCA[] = {{"SRC 1", CK_TRK, {P_E0, P_E1, P_E2, P_E3}}, {"SRC 2", CK_TRK, {P_E4, P_E5, P_E6, P_E7}},
                                CZ_FILT,
                                {"DEVIL", CK_TRK, {P_ATK, P_DEC, P_SUS, P_REL}},   /* (ACID: ACC DEC, DRV TYPE, SUB, KEY TRK in
                                                         * the envelope's slots: eng_acid.c ACID_X) */
                                {"SEND", CK_TRK, {P_DLY, P_REV, P_DIST, P_CHOR}},   /* (0.12.3, GG: the ACID's sends where its
                                                         * sound is; FX > SEND's, DELAY and REVERB first) */
                                {"VOICE", CK_TRK, {P_VOICE, P_GLIDE, P_DETUNE, ID_OCT}},
                                {"CHORD", CK_TRK, {P_CHRD, P_CVOI, P_CSTR, X}},
                                CZ_MOD(1), CZ_MOD(2), CZ_MOD(3), CZ_MOD(4)};
static const cpg_t CZP_SRCV[] = {{"SRC 1", CK_TRK, {P_E0, P_E1, P_E2, P_E3}}, {"SRC 2", CK_TRK, {P_E4, P_E5, P_E6, P_E7}},
                                CZ_FILT,
                                {"WORDS", CK_TRK, {P_VLANG, P_VLYR, P_VSTYLE, X}},   /* (THEME, ARTIC: on the first page; 0.9.4 STYLE) */
                                {"VOICE", CK_TRK, {P_VOICE, P_GLIDE, P_DETUNE, ID_OCT}},
                                {"CHORD", CK_TRK, {P_CHRD, P_CVOI, P_CSTR, X}},
                                CZ_MOD(1), CZ_MOD(2), CZ_MOD(3), CZ_MOD(4)};
/* a DRUM track's EDIT: a drum of the kit (DRUM's KNOB 1, or its key struck: the eight's own sound, its patches' both
 * MORPH ends moved together; its AHDSR and sends), the eight sounds' levels, then the kit's (KIT, KIT 2: the engine's
 * E1 .. E8), the matrix */
static const cpg_t CZP_SRCD[] = {{"DRUM", CK_COS, {CC_DSEL, CC_KICK + DXP_PITCH, CC_KICK + DXP_PMOD, CC_DLV}},
                                {"SOUND", CK_COS, {CC_DSEL, CC_DSRC, CC_DPAN, CC_DCHN}},   /* (0.12.1) any drum sound into it;
                                                         * (0.14.1, GG) its PAN and CHANCE, not TONE's WAVE, FILTER again */
                                {"TONE", CK_COS, {CC_KICK + DXP_NOISE, CC_KICK + DXP_COLOR, CC_DWAV, CC_DFLT}},
                                {"TONE 2", CK_COS, {CC_DBT, CC_DDRV, CC_DLAY, CC_DFB}},
                                {"ENV", CK_COS, {CC_DE0 + DE_ATK, CC_KICK + DXP_DECAY, CC_DE0 + DE_SUS, CC_DE0 + DE_REL}},
                                {"ENV 2", CK_COS, {CC_DE0 + DE_HLD, CC_DSNP, X, X}},
                                {"SEND", CK_COS, {CC_DE0 + DE_SDL, CC_DE0 + DE_SRV, CC_DE0 + DE_SCH, X}},
                                {"LEVELS", CK_COS, {CC_LN0, CC_LN0 + 1u, CC_LN0 + 2u, CC_LN0 + 3u}},
                                {"LEVELS 2", CK_COS, {CC_LN0 + 4u, CC_LN0 + 5u, CC_LN0 + 6u, CC_LN0 + 7u}},
                                {"KIT", CK_TRK, {P_E0, P_E1, P_E2, P_E3}}, {"KIT 2", CK_TRK, {P_E4, P_E5, P_E6, P_E7}},
                                CZ_FILT,                 /* (the kit's: on its sum, as ENV > FILTER 1) */
                                CZ_MOD(1), CZ_MOD(2), CZ_MOD(3), CZ_MOD(4)};
static const cpg_t CZP_AMP[] = {{"AMP", CK_TRK, {P_ATK, P_DEC, P_SUS, P_REL}},
                               {"FILTER 1", CK_TRK, {P_FTYPE, P_FCUT, P_FRES, P_ED_FLT}},
                               {"FILTER 2", CK_COS, {CC_X2 + X2_F2T, CC_X2 + X2_F2C, CC_X2 + X2_F2R, CC_X2 + X2_F2E}},
                               {"ENV MOD", CK_TRK, {P_ED_FLT, P_ED_PIT, P_ED_SHP, X}},
                               {"LFO 1", CK_COS, {CC_L1SYNC, CC_L1RATE, CC_L1WAVE, CC_L1MODE}},
                               {"LFO 1 TO", CK_TRK, {P_LD_FLT, P_LD_PIT, P_LD_SHP, P_LD_AMP}},
                               {"LFO 1+", CK_COS, {CC_L1PH, CC_L1FADE, X, X}},
                               {"LFO 2", CK_COS, {CC_L2SYNC, CC_L2RATE, CC_X2 + X2_WAVE, CC_X2 + X2_L2MODE}},
                               {"LFO 2+", CK_COS, {CC_X2 + X2_L2PH, CC_X2 + X2_F2L, X, X}}};
/* (ANALOG filters its notes itself: its FILTER 1 is its CUT and RES, E4 E5) */
static const cpg_t CZP_AMPA[] = {{"AMP", CK_TRK, {P_ATK, P_DEC, P_SUS, P_REL}},
                                {"FILTER 1", CK_TRK, {P_FTYPE, P_E4, P_E5, P_ED_FLT}},
                                {"FILTER 2", CK_COS, {CC_X2 + X2_F2T, CC_X2 + X2_F2C, CC_X2 + X2_F2R, CC_X2 + X2_F2E}},
                                {"ENV MOD", CK_TRK, {P_ED_FLT, P_ED_PIT, P_ED_SHP, X}},
                                {"LFO 1", CK_COS, {CC_L1SYNC, CC_L1RATE, CC_L1WAVE, CC_L1MODE}},
                                {"LFO 1 TO", CK_TRK, {P_LD_FLT, P_LD_PIT, P_LD_SHP, P_LD_AMP}},
                                {"LFO 1+", CK_COS, {CC_L1PH, CC_L1FADE, X, X}},
                                {"LFO 2", CK_COS, {CC_L2SYNC, CC_L2RATE, CC_X2 + X2_WAVE, CC_X2 + X2_L2MODE}},
                                {"LFO 2+", CK_COS, {CC_X2 + X2_L2PH, CC_X2 + X2_F2L, X, X}}};
static const cpg_t CZP_DRUMS[] = {{"ROLL", CK_COS, {CC_RRATE, CC_RRAMP, CC_RVEL, X}},   /* (0.13.0) LFO: DRUMS (cos_drm.c) */
                                 {"CHANCE", CK_COS, {CC_DCH0, CC_DCH0 + 1u, CC_DCH0 + 2u, CC_DCH0 + 3u}},
                                 {"CHANCE 2", CK_COS, {CC_DCH0 + 4u, CC_DCH0 + 5u, CC_DCH0 + 6u, CC_DCH7}}};
#define CZ_TABS 16u                                 /* (the most pages a mode may have: cos_tabs's names) */
static const cpg_t CZP_FX[] = {{"DJ", CK_COS, {CC_DJF, CC_EQL, CC_EQM, CC_EQH}},   /* (the mixer's: DJ FILTER, the ISOLATOR) */
                              {"SEND", CK_TRK, {P_DIST, P_CHOR, P_DLY, P_REV}}, {"DIST", CK_TRK, {P_DTYPE, P_DTONE, X, X}},
                              {"DELAY", CK_GLO, {G_DTIME, G_DFDBK, G_DCOLOR, G_DPING}},    /* (the buses: every track's) */
                              {"DELAY 2", CK_GLO, {G_DMIX, G_DDRV, G_DTYPE, G_DMOD}},     /* (the tape echo: MOD its wow and flutter;
                                                         * FEEDBK past 100 runs away into the clip: the dub spin) */
                              {"DELAY 3", CK_GLO, {G_WIDTH, G_DPIT, G_DSPRY, X}},   /* (the right echo later; the GRAIN delay) */
                              {"REVERB", CK_GLO, {G_RSIZE, G_RDAMP, G_RPRE, G_RTYPE}},
                              {"REVERB 2", CK_GLO, {G_RMOD, G_RRATE, G_RFILT, G_RWIDE}},   /* (its movement, input tone, width) */
                              {"RUMBLE", CK_COS, {CC_RB0, CC_RB0 + 1u, CC_RB0 + 2u, CC_RB3}}};   /* (rumble.c: the kick's dark tail) */
static const cpg_t CZP_GENA[] = {{"ACID", CK_AC, {AK_DENS, AK_SLIDE, AK_ACC, ID_OCT}}, {"ACID 2", CK_AC, {AK_STEPS, AK_NOTES, AK_TIE, AK_OCT}},
                                {"ACID 3", CK_AC, {AK_EVERY, AK_AMT, AK_ROOT, AK_SCALE}}, {"ACID 4", CK_AC, {AK_LOCK, AK_ROT, X, X}}};
#define CZ_BLEEP {"BLEEP", CK_BL, {BK_SEED, BK_DENS, BK_RANGE, ID_OCT}}, {"BLEEP 2", CK_BL, {BK_MOTIF, BK_LEN, BK_VARY, BK_ACC}}, \
                 {"BLEEP 3", CK_BL, {BK_EVERY, BK_SLIDE, BK_RATCH, BK_ECHO}}, {"BLEEP 4", CK_BL, {BK_DRIFT, BK_SPACE, BK_SND, ID_OCT}}
                 /* (0.15.1) bleeps and bloops (ui_bleep.c; 0.16.0: MOTIF, DRIFT, SPACE; BLEEP 3 the mutations') */
/* (a POLY synth track that is not ACID, the pad: DUB's chord stabs first, ACID's lines after them; a MONO one, the
 * bass: ACID's first, DUB's after, CZP_GENM) */
static const cpg_t CZP_GENS[] = {{"DUB", CK_DB, {DK_SHAPE, DK_DENS, DK_MOVE, DK_LEN}}, {"DUB 2", CK_DB, {DK_BARS, DK_VOIC, DK_ECHO, ID_OCT}},
                                {"DUB 3", CK_DB, {DK_ROOT, DK_EVERY, DK_AMT, DK_DLY}},
                                CZ_BLEEP,
                                {"ACID", CK_AC, {AK_DENS, AK_SLIDE, AK_ACC, ID_OCT}}, {"ACID 2", CK_AC, {AK_STEPS, AK_NOTES, AK_TIE, AK_OCT}},
                                {"ACID 3", CK_AC, {AK_EVERY, AK_AMT, AK_ROOT, AK_SCALE}}, {"ACID 4", CK_AC, {AK_LOCK, AK_ROT, X, X}}};
static const cpg_t CZP_GENM[] = {{"ACID", CK_AC, {AK_DENS, AK_SLIDE, AK_ACC, ID_OCT}}, {"ACID 2", CK_AC, {AK_STEPS, AK_NOTES, AK_TIE, AK_OCT}},
                                {"ACID 3", CK_AC, {AK_EVERY, AK_AMT, AK_ROOT, AK_SCALE}}, {"ACID 4", CK_AC, {AK_LOCK, AK_ROT, X, X}},
                                {"DUB", CK_DB, {DK_SHAPE, DK_DENS, DK_MOVE, DK_LEN}}, {"DUB 2", CK_DB, {DK_BARS, DK_VOIC, DK_ECHO, ID_OCT}},
                                {"DUB 3", CK_DB, {DK_ROOT, DK_EVERY, DK_AMT, DK_DLY}}, CZ_BLEEP};
static const cpg_t CZP_GENK[] = {{"KICK", CK_COS, {CC_KPAT, X, X, X}}};   /* (0.10.0) a KICK track's GEN */
static const cpg_t CZP_GENE[] = {{"EUCLID", CK_EU, {EK_HITS, EK_STEPS, EK_ROT, EK_K4}}, {"MODES", CK_EU, {EK_M0, EK_M0 + 1u, EK_M0 + 2u, EK_M0 + 3u}},
                                {"MUTATE", CK_EU, {EK_U0, EK_U0 + 1u, EK_U0 + 2u, EK_U0 + 3u}}};
static const cpg_t CZP_MIX[] = {{"MIX", CK_TRK, {P_LEVEL, P_PAN, P_GLIDE, P_MUTE}},
                               {"SCOPE", CK_COS, {CC_SCT, CC_SCG, CC_SCTR, CC_SCM}},   /* (the output: its wave, spectrum) */
                               {"SPECTRUM", CK_COS, {CC_SPV, CC_SPR, CC_SPS, CC_SPSRC}},
                               {"STEREO", CK_COS, {CC_SCZ, X, X, X}},
                               {"LOUDNESS", CK_COS, {CC_LUT, CC_LUR, X, X}},
                               {"COMP", CK_COS, {CC_COMP, CC_CTHR, CC_CRAT, CC_CMK}},
                               {"COMP 2", CK_COS, {CC_CIN, CC_CATK, CC_CREL, CC_CKNEE}},
                               {"COMP 3", CK_COS, {CC_CSC, CC_CMIX, X, X}},
                               {"LIMIT", CK_COS, {CC_LIN, CC_LIM, CC_LREL, CC_LSAT}},   /* (0.18.0: SATURATE; AUTO LV to SYNTAKT) */
                               {"MIDI", CK_COS, {CC_OUTCH, CC_MOUT, CC_CLOCK, CC_LOCAL}},   /* (0.9.4 LOCAL: OFF, the keys play Ableton only) */
                               {"MIDI 2", CK_COS, {CC_CTLM, CC_CTLCH, CC_EXT, X}},   /* (0.13.0) KNOBS: the FM-1's / + MIDI / MIDI
                                                         * only; the track's SOUND: here or another instrument's (EXT) */
                               {"SYNTAKT", CK_COS, {CC_STON, CC_STPAT, CC_STLEN, CC_LVL}},   /* (0.17.0) one knob: follow it;
                                                         * 0.18.0: K4 the leveler, off with it (PC CH on LINK) */
                               {"LINK", CK_COS, {CC_PCCH, CC_PCOUT, CC_VJCH, X}},   /* (a Syntakt's patterns: the sections) */
                               {"LINK 2", CK_COS, {CC_MCCH, CC_MCDIR, CC_MCMAP, CC_MOCC}},   /* (the macros and MORPH as CCs, both
                                                         * ways) */
                               {"LINK 3", CK_COS, {CC_MC0, CC_MC0 + 1u, CC_MC0 + 2u, CC_MC3}},   /* (.. each one's CC) */
                               {"SYSTEM", CK_COS, {CC_TRKS, CC_LOAD, CC_BT, CC_IDLE}}};
static const cpg_t CZP_KEY[] = {{"KEY", CK_COS, {CC_SCALE, CC_QUANT, X, X}}};   /* (the root: the keys; the song's scale,
                                                         * whichever track is picked: cos_mel) */
static const cpg_t CZP_PERF[] = {{"FX PAD", CK_COS, {CC_PF0, CC_PF0 + 1u, CC_PF0 + 2u, CC_PF3}}};   /* (FX held) */
static const cpg_t CZP_PROJ[] = {{"PROJECT", CK_COS, {CC_SLOT, CC_NAME, X, X}},
                                {"SOUNDS", CK_COS, {CC_USLOT, X, X, X}},
                                {"KITS", CK_COS, {CC_KSLOT, X, X, X}},
                                {"START", CK_COS, {CC_QSTY, CC_QDRM, CC_QELM, CC_QBAR}}};   /* (0.18.0) quick starts: cos_qs_load;
                                                         * 0.18.1 ELEMENTS, BARS */   /* (0.13.0) the user kits: kits.c */   /* (0.10.2) the user presets: KNOB 1 the slot */   /* (KNOB 1 the slot, every song's: 1A ..
                                                         * 8D; white keys 1..4 this song's, 9..16 the songs; KNOB 2 a name) */
#undef X
static const char *const CM_NAME[CM_N] = {"HOME", "TRIG", "SRC", "AMP", "DRUMS", "FX", "GEN", "MIX", "KEY", "SAVE"};
static const uint8_t CM_BTN[CM_N] = {B_HOME, B_SEQ, B_EDIT, B_ENV, B_LFO, B_FX, B_ARP, B_GLO, B_SCL, B_SAVE};
static const uint8_t WK[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};   /* the white keys */
static const uint8_t BK[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};                    /* the black keys */
/* the keys drawn on screen (KEY, DRONE), from F as the FM-1's own: F F# G G# A A# B C C# D D# E, 1 a black one (was C's
 * pattern from F: the black keys a place off, A shown where E is on a C keyboard; tests/cos_test.c checks it against BK) */
static const uint8_t COS_BLF[12] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0};
static const char *COS_LN[NLANE] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};   /* (0.14.1: [0] PC with a PERC: cos_k0_poll) */

static struct {
    uint8_t flip, mode, tab[CM_N], bank, lane, hot, combo, ready;   /* flip: HOME + GLO turned it from COS_DEFAULT */
    uint32_t hot_ms, msg_ms, sig[6];
    char msg[24];
    uint8_t key_next, scl_down, scl_used;              /* key_next: 1 + the key asked for (0 C .. 11 B) */
    uint32_t key_bar;                                   /* (bar_n when it was asked for) */
    uint32_t new_ms;                                    /* (0.9.3) SAVE's REC: when it was pressed (a second within 3 s: NEW) */
    uint8_t lk_mode, lk_tab, lk_used;                   /* LOCK: the sound page used last; a knob turned while steps held */
    uint32_t tg_down, notes0;                           /* TRIG: the white keys down since their press; the keys a frame ago */
    uint8_t ly_prev;                                    /* WORDS: the lyric before OCT+ */
    int16_t cc_v[16];                                   /* (CK_COS: the knobs' views) */
    uint8_t fx_down, fx_used;                           /* FX held: the pad (keys: perform.c's effects, knobs its macros) */
    uint8_t ed_down, ed_used;                           /* EDIT held (+ PRESETS: the next engine); its tap acts let go */
    uint8_t home_down, home_used, home_tab;             /* HOME held (the tempo's button): on HOME its tap, the next page, let go */
    uint8_t rec_down, rec_used;                         /* REC held (+ OCT-: the motion cleared); its tap arms, let go */
    uint8_t clr_k;                                      /* SEQ held + REC: the track asked about (+ 1), cos_clear */
    uint8_t si, si_pos;                                 /* STEP INPUT on (SEQ, a synth); its cursor's step (cos_si_*) */
    uint8_t sv_down, sv_used;
    uint8_t sq_down, sq_used;                           /* SEQ held on SEQ (its tap: the next way, cos_pages CZP_TRIG) */
    uint8_t pg_sel;                                     /* the track the pages were last shown for (cos_pg_follow) */
    const char *pg_tab[CM_N];                           /* .. and each mode's page then, by name */                           /* SAVE held (+ OCT+ KEEP, OCT- BACK: cos_keep); its tap: SAVE page */
    uint32_t clr_t;                                     /* .. when (fm1_ms) */
    uint32_t set_ms;                                    /* a setting turned (MASTER): saved 2 s after, not every detent */
    uint8_t slot, pn_ok;                                /* SAVE: the slot picked (0..3); the names below read */
    uint8_t uslot;                                      /* (0.10.2) SAVE > SOUNDS: the user preset picked (0..UP_SLOTS-2) */
    uint8_t kslot, kit_seen;                            /* (0.13.0) SAVE > KITS: the kit picked (0..15); its names read */
    uint32_t snd_ms;                                    /* .. OCT+ on a used one / REC: when asked (again within 3 s: done) */
    uint8_t idle_up, idle_eat, batt_low;                /* the IDLE screen up; its waking touch swallowed; LOW said */
    uint32_t act_ms, idle_t;                            /* the last touch; the IDLE frame's time (every strip the same) */
    uint16_t nm_n;                                      /* .. NAME's turns (each a name of its own: back is the same) */
    uint32_t nm_seed;                                   /* .. this power-on's names (0: none drawn yet) */
    char pn[4][13];                     /* .. each slot's name ("": empty) */
} cz COS_RAM;
#if COS_SLIM
static int cos_active(void) { return 1; }               /* (its interface only: JIANT's own is not in the image) */
#else
static int cos_active(void) { return (COS_DEFAULT ^ cz.flip) != 0; }
#endif

static int cos_kit(void) { return drum_track(TSEL) != 0; }
/* (0.10.0) a track of hits, no notes to key: the kit, a KICK (the song's scale, key and root are the melodic tracks') */
static int cos_hits(const track_t *t) { return trk_hits(t); }
static int cos_dead(const track_t *t, uint32_t id);      /* (below: a parameter the track's engine does not use) */
static void cos_pages(uint32_t m, const cpg_t **p, uint32_t *n)
{
    static const cpg_t *const P[CM_N] = {CZP_HOME, CZP_TRIG, CZP_SRC, CZP_AMP, CZP_DRUMS, CZP_FX, CZP_GENA, CZP_MIX, CZP_KEY, CZP_PROJ};
    static const uint8_t N[CM_N] = {7, 5, 9, 9, 3, 9, 4, 16, 1, 4};   /* (each at most CZ_TABS: cos_tabs) */
    _Static_assert(NELEM(CZP_HOME) == 7 && NELEM(CZP_TRIG) == 5 && NELEM(CZP_SRC) == 9 && NELEM(CZP_AMP) == 9 &&
                   NELEM(CZP_DRUMS) == 3 && NELEM(CZP_FX) == 9 && NELEM(CZP_GENA) == 4 && NELEM(CZP_MIX) == 16 &&
                   NELEM(CZP_KEY) == 1 && NELEM(CZP_PROJ) == 4 && NELEM(CZP_SRCD) <= CZ_TABS && NELEM(CZP_GENS) <= CZ_TABS &&
                   NELEM(CZP_SRCV) == 10 && NELEM(CZP_AMPA) == 9 && NELEM(CZP_SRCN) == 9,
                   "N: the pages each mode has (0.9.2: a count past its list read past it)");
    int vo = m == CM_SRC && trk_engp(TSEL) == &ENG_FORMANT;
    if (m == CM_SRC && cos_kit()) { *p = CZP_SRCD; *n = NELEM(CZP_SRCD); return; }
    if (m == CM_TRIG && cos_kit()) { *p = CZP_TRIGK; *n = NELEM(CZP_TRIGK); return; }   /* (a kit: KICK, LEVELS, its own ..) */
    if (m == CM_GEN && cos_kit()) { *p = CZP_GENE; *n = 3; return; }
    if (m == CM_GEN && trk_eng(TSEL) == ENGI_KICK) { *p = CZP_GENK; *n = NELEM(CZP_GENK); return; }   /* (0.10.0) */
    if (m == CM_GEN && trk_eng(TSEL) != ENGI_ACID) {     /* (the pad: DUB first; the bass, a MONO one: ACID first) */
        int poly = TSEL->p[P_VOICE] == V_POLY;
        *p = poly ? CZP_GENS : CZP_GENM;
        *n = poly ? NELEM(CZP_GENS) : NELEM(CZP_GENM);
        return;
    }
    if (m == CM_SRC && trk_engp(TSEL) == &ENG_ACID) { *p = CZP_SRCA; *n = NELEM(CZP_SRCA); return; }   /* (+ DEVIL) */
    if (m == CM_SRC && trk_engp(TSEL) == &ENG_KICK) { *p = CZP_SRCK; *n = NELEM(CZP_SRCK); return; }   /* (0.10.0) */
    if (m == CM_SRC && trk_engp(TSEL) == &ENG_FMDRUM) { *p = CZP_SRCF; *n = NELEM(CZP_SRCF); return; }   /* (0.11.0) */
    *p = vo ? CZP_SRCV : m == CM_AMP && trk_engp(TSEL) == &ENG_ANALOG ? CZP_AMPA :
         m == CM_SRC && trk_engp(TSEL) == &ENG_ANALOG ? CZP_SRCN : P[m % CM_N];
    *n = N[m % CM_N] + (uint32_t)vo;                                      /* (VOICE: WORDS too) */
    if (m == CM_AMP) {                                   /* (a page all of whose knobs the engine does not use: left out,
                                                         * cos_dead; a kit's AMP, ACID's AMP and ENV MOD) */
        static cpg_t keep[12];
        uint32_t i, j, o = 0;
        for (i = 0; i < *n && o < NELEM(keep); i++) {
            const cpg_t *q = &(*p)[i];
            uint32_t live = q->kind != CK_TRK;
            for (j = 0; j < 4u && !live; j++)
                live = q->id[j] != 0xFFu && !cos_dead(TSEL, q->id[j]);
            if (q->id[0] == P_ATK && q->kind == CK_TRK && trk_engp(TSEL) == &ENG_ACID) live = 0;   /* (ACID: those are
                                                         * EDIT > DEVIL's) */
            if (live) keep[o++] = *q;
        }
        if (o != *n) { *p = keep; *n = o; }
    }
}
/* (cursed-os 0.16.0, GG 2026-11-13: "the ui on the synth is too cluttered and too menu divy"; SIMPLE + DEEP) each mode's
 * everyday pages: its button's taps go round only these (and the tab bar shows only these, the deep page in use too, and
 * how many more there are); SELECT turns through every page, as ever. Nothing gone: the rest one SELECT away. A mode of
 * three pages or fewer: all of them */
static const char *const CZ_ESS[CM_N] = {
    "PERFORM|LIVE",                                     /* HOME */
    "TRIG|STEP|EUCLID",                                 /* SEQ */
    "DRUM|SOUND|KICK|KICK 2|SRC 1|FILTER|DEVIL|WORDS",  /* EDIT: the sound and its filter (the first page always) */
    "AMP|FILTER 1|LFO 1",                               /* ENV */
    "", "SEND|DELAY|REVERB",                            /* LFO: DRUMS (3), FX */
    "ACID|DUB|BLEEP|EUCLID|KICK",                       /* GEN: each generator's first */
    "MIX|LIMIT|SYNTAKT", "", "PROJECT|SOUNDS|KITS|START"};   /* GLO (0.17.0: SYNTAKT); KEY; SAVE (0.18.0: all four) */
static int cos_ess(uint32_t m, const cpg_t *p, uint32_t i, uint32_t n)
{
    const char *l = CZ_ESS[m % CM_N], *t = p[i].tab;
    if (n <= 3u || !i) return 1;
    while (*l) {                                        /* (each name between the bars) */
        uint32_t j = 0;
        while (t[j] && l[j] == t[j]) j++;
        if (!t[j] && (l[j] == '|' || !l[j])) return 1;
        while (*l && *l != '|') l++;
        if (*l) l++;
    }
    return 0;
}
/* the next everyday page after page cur of mode m (a tap on its button) */
static uint32_t cos_ess_next(uint32_t m, uint32_t cur)
{
    const cpg_t *p;
    uint32_t n, j;
    cos_pages(m, &p, &n);
    if (!n) return 0;
    for (j = 1; j <= n; j++)
        if (cos_ess(m, p, (cur + j) % n, n)) return (cur + j) % n;
    return (cur + 1u) % n;
}
static const cpg_t *cos_page(void)
{
    const cpg_t *p;
    uint32_t n;
    if (cz.fx_down) return CZP_PERF;
    cos_pages(cz.mode, &p, &n);
    return &p[cz.tab[cz.mode] % n];
}
static void cos_snap(void);                              /* (0.10.3: LIVE's normal, below) */
static void cos_back(int up);
static int cos_on_live(void) { return cz.mode == CM_HOME && !cz.fx_down && str_eq(cos_page()->tab, "LIVE"); }   /* (0.10.3) */
static int cos_on_words(void) { return cz.mode == CM_SRC && trk_engp(TSEL) == &ENG_FORMANT && str_eq(cos_page()->tab, "WORDS"); }
static void cos_say(const char *s) { str_cpy(cz.msg, s, sizeof cz.msg); cz.msg_ms = fm1_ms; }
static const param_desc_t *cos_drm_desc(uint32_t id, int16_t **vp);   /* (0.13.0) LFO > DRUMS: cos_drm.c (after the drawing) */
static void cos_drm_put(uint32_t id, int32_t v);
static void cos_drm_key(uint32_t k);
static void cos_drm_poll(void);
static void cos_body_drm(void);
static void cos_drm_leds(uint8_t *nl, uint8_t *nb);
/* (0.13.0) GLO > MIDI 2 KNOBS: FM-1 (as ever), +MIDI (each turn also sends the value it gave, 0..127, as a CC), MIDI
 * (the knobs only send: a relative CC, 1 .. 63 up, 65 .. 127 down: Ableton's "Relative (2's Comp.)"; the FM-1's sound
 * stays). The CC: 20 + the page's number x 4 + the knob (20 .. 83), on CTRL CH + the mode (HOME 0, SEQ 1, EDIT 2, ENV 3,
 * LFO 4, FX 5, ARP 6, GLO 7, SCL 8, SAVE 9; 16 wraps to 1): every knob of every page its own, as Ableton maps them */
static uint32_t cos_ctl_mode(void) { return favorites.factory[15][1] <= 2u ? favorites.factory[15][1] : 0u; }
static uint32_t cos_ctl_ch(void) { uint32_t c = favorites.factory[15][2]; return (c >= 1u && c <= 16u ? c : 7u) - 1u; }
static uint32_t cos_ctl_last;                             /* (the last sent: channel << 8 | CC, + 1 << 16; MIDI 2 shows it) */
static void cos_ctl_send(uint32_t k, uint32_t v)
{
    uint32_t ch = (cos_ctl_ch() + cz.mode) % 16u, cc = 20u + (cz.tab[cz.mode] % 16u) * 4u + (k & 3u);
    midi_out_event(0x0Bu | (0xB0u | ch) << 8 | cc << 16 | (v & 127u) << 24);
    cos_ctl_last = 1u << 16 | ch << 8 | cc;
}
static void kit_scan(void);                              /* (0.13.0) the user KITS: kits.c */
static int kit_save(uint32_t k, const char *name);
static int kit_load(uint32_t k);
static int kit_erase(uint32_t k);
static int kit_is_used(uint32_t k);
static const char *kit_name(uint32_t k);
static uint32_t cos_ext10(void);                        /* (below: an external clock's tempo) */
/* (cursed-os) the delay's TIME, right is longer: the divisions shortest to longest (1/32 .. 1/1, the dotted and the
 * triplets in their places), then the free times 5 ms .. (JIANT's own order: 4BAR down to 1/32, then 5 ms up). 2BAR and
 * 4BAR left out: past the line (DLY_MAX, 2.97 s) under 162 / 323 BPM, so they were 2.97 s, not what they said
 * (tests/cos_acc_test.c). Their places (COS_DT_SYNC, + 1) a gap the turn steps over; one a project brings shows as 1/1 */
#define COS_DT_SYNC 11u
static const uint8_t COS_DT[COS_DT_SYNC] = {3, 5, 2, 4, DT_DOT + 1u, 1, DT_DOT, 0, DT_DOT + 2u, 6, 7};
static int32_t cos_dt_rank(int32_t i)                    /* N_DTIME's index -> its place */
{
    uint32_t r;
    for (r = 0; r < COS_DT_SYNC; r++)
        if (COS_DT[r] == (uint32_t)i) return (int32_t)r;
    if (i == 8 || i == 9) return (int32_t)COS_DT_SYNC - 1;   /* (2BAR, 4BAR: at 1/1's place) */
    return (int32_t)COS_DT_SYNC + 2 + i - 10;           /* (the free times: 10 .. DT_DOT - 1) */
}
/* a place -> N_DTIME's index; one in the gap: past it, the way turned (dir) */
static int32_t cos_dt_index(int32_t r, int32_t dir)
{
    if (r >= (int32_t)COS_DT_SYNC && r < (int32_t)COS_DT_SYNC + 2) r = dir > 0 ? (int32_t)COS_DT_SYNC + 2 : (int32_t)COS_DT_SYNC - 1;
    return r < (int32_t)COS_DT_SYNC ? COS_DT[r] : r - (int32_t)COS_DT_SYNC - 2 + 10;
}
static int cos_follows(void);
/* GLO > LINK 2 / 3: the macros as CCs; MAP: a Syntakt's (midi_control.c's note) */
static const uint8_t MC_MAP[2][4] = {{74, 75, 84, 85}, {70, 71, 24, 30}};   /* ST TRACK, ST FX */
static uint32_t cos_mc_map(void)
{
    uint32_t i;
    for (i = 0; i < 2u; i++)
        if (!memcmp((const void *)cos_mc_cc, MC_MAP[i], 4)) return i + 1u;
    return 0;                                            /* (CUSTOM) */
}
static void cos_mc_set(const uint8_t *c)                 /* the four CCs, kept in the settings (ui_input.c ui_polls) */
{
    static const uint8_t AT[4] = {1, 3, 4, 5};
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        cos_mc_cc[k] = c[k] & 127u;
        favorites.factory[8][24u + AT[k]] = (uint8_t)(cos_mc_cc[k] + 1u);
    }
}
/* (cos_morph.c, in fx.c) MORPH: */
/* HOME's white 13 / 14 (x 0 A, 1 B); store: HOME held */
static void mph_key(uint32_t x, int store)
{
    x &= 1u;
    if (store || !((mph.set >> x) & 1u)) {
        mph_read(mph.s[x]);
        mph.set |= (uint8_t)(1u << x);
        mph.pos = x ? 1000 : 0;
        mph.gl = mph.run = 0;
        cos_say(x ? "B STORED" : "A STORED");
        return;
    }
    if ((mph.set & 3u) != 3u) {                   /* (only one stored: nothing to glide between; the other first) */
        cos_say(x ? "STORE A FIRST" : "STORE B FIRST");
        return;
    }
    mph.from = mph.pos;
    mph.to = x ? 1000 : 0;
    mph.gl = 1;
    mph.run = 0;
    cos_say(x ? (MO_BARS[mph.bars % 6u] ? "TO B" : "B NEXT BAR") : (MO_BARS[mph.bars % 6u] ? "TO A" : "A NEXT BAR"));
}

static const char *const COS_NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

/* the song's key and scale live on its melodic tracks (ui_input.c scale_link keeps them alike): the first of them, or
 * the selected one when it is melodic */
static track_t *cos_mel(void)
{
    uint32_t k;
    if (!cos_hits(TSEL)) return TSEL;
    for (k = 0; k < NTRK && cos_hits(&trk[k]); k++) {}
    return k < NTRK ? &trk[k] : 0;
}
/* the song's key: the first synth track's root (C with none) */
static uint32_t cos_key(void)
{
    uint32_t k;
    for (k = 0; k < NTRK && cos_hits(&trk[k]); k++) {}
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

/* the song into key pc (0..11): the nearest interval (-6 .. +5), every synth track's notes and root moved by it (the
 * notes as stored: MIDI OUT, ACID and the scale all agree; a kit's lanes stay) */
static void cos_key_apply(uint32_t pc)
{
    int32_t d = ((int32_t)pc - (int32_t)cos_key() + 18) % 12 - 6;
    uint32_t k, i, c;
    for (k = 0; d && k < NTRK; k++) {
        track_t *t = &trk[k];
        if (cos_hits(t)) continue;
        for (i = 0; i < NSTEP; i++) {
            step_t *s = &t->step[i];
            if (s->time != ST_NOTE) continue;
            for (c = 0; c < s->n && c < 4u; c++) s->note[c] = (uint8_t)clamp((int32_t)s->note[c] + d, 0, 127);
        }
        t->p[P_ROOT] = (int16_t)((t->p[P_ROOT] + d + 120) % 12);
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
static int cos_dsrc_get(uint32_t i, dx_lane_t *L, uint8_t *lay);
static void cos_k0_poll(void);
static void cos_key_poll(void)
{
    cos_k0_poll();                                      /* (0.14.1) the PERC */
    if (cz.key_next && (!song.playing || bar_n != cz.key_bar))
        cos_key_apply(cz.key_next - 1u);
}

/* GLO > MIDI's knobs: the selected track's MIDI OUT channel (the settings: bytes 8 .. 13 of engine 15's, 0 its own),
 * MIDI OUT (KEYS CLOCK ALL OFF: the menu's), the clock the music follows (the project's G_CLOCK: INT USB TRS) */
/* the LFOs' rates in time, slowest first on the knob: 32BAR 16BAR 8BAR, then JIANT's 4BAR .. 1/32 (stored: N_LSYNC's
 * index, the slow three appended at 12 .. 14) */
static const char *const COS_LSYNC[15] = {"32BAR", "16BAR", "8BAR", "4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8D", "1/4T",
                                          "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static uint32_t cos_lsync_idx(uint32_t pos) { return pos < 3u ? 14u - pos : pos - 3u; }
static uint32_t cos_lsync_pos(uint32_t idx) { return idx >= 12u ? 14u - (idx % 15u) : idx + 3u; }
/* (cursed-os) a drum of the kit, the one picked (dx_ui_lane: DRUM's KNOB 1, or its key struck on EDIT): which one, its
 * LEVEL, the oscillator's WAVE, the noise's FILTER, BEND's TYPE, DRIVE, LAYER, FEEDBK, SNAP (the noise's envelope a
 * quarter of the sound's), its AHDSR and sends (dx_de, DX_DE_D) */
static const char *COS_DRUM[] = {"KICK", "SNARE", "CLAP", "HAT CL", "HAT OP", "TOM", "RIM", "BELL", 0};
static const char *COS_DRUM5[] = {"KICK", "SNARE", "CLAP", "HH CL", "HH OP", "TOM", "RIM", "BELL", 0};   /* (a knob's
                                                         * value: five letters at most, param_format) */
/* (0.12.1) SOUND > SOURCE: any drum sound the firmware has, loaded into the drum picked (then its own to shape): 1..72
 * every factory kit's eight (by sound, then kit: the snares of all nine together), 73..75 KICK's own (DEEP PUNCH RUMBLE),
 * 76..85 FMDRUM's presets on the FM voice (FILTER FM, drumx_voice.c dx_fmd). 0 OWN: as it is (nothing loaded). The
 * load copies the sound (its mode, A and B, LAYER / FEEDBK); its LEVEL, AHDSR and sends stay the drum's */
#define DSRC_KITS (DX_NKIT * 8u)
#define DSRC_FMD (DSRC_KITS + 3u)
#define DSRC_N (DSRC_FMD + (uint32_t)NELEM(FMD_PRESETS))
static const char *const DSRC_KIT3[] = {"DX", "909", "808", "LFI", "IND", "ACO", "DUB", "TEC", "BRK"};
static const char *const DSRC_LANE3[8] = {"KCK", "SNR", "CLP", "HHC", "HHO", "TOM", "RIM", "BEL"};
static const char *const DSRC_FMDN[] = {"FM KICK", "FM TOM", "FM SNR", "FM CLAP", "FM RIDE", "FM CRSH", "FM HHC", "FM HHO",
                                        "FM COW", "FM ZAP"};
static const uint8_t DSRC_FMDP[] = {18, 40, 46, 52, 70, 66, 80, 80, 62, 30};   /* (their PITCH on the kit's scale) */
_Static_assert(NELEM(DSRC_KIT3) == DX_NKIT && NELEM(DSRC_FMDN) == NELEM(FMD_PRESETS) && NELEM(DSRC_FMDP) == NELEM(FMD_PRESETS),
               "a short name for every kit and FMDRUM preset");
static uint8_t dx_src[8];                                /* (RAM: the last loaded, + 1; 0 none: the knob turns on from it) */
static void cos_dsrc_name(uint32_t i, char *b)           /* (7 letters at most: a knob's value) */
{
    if (!i || i > DSRC_N) { str_cpy(b, "OWN", 8); return; }
    i--;
    if (i < DSRC_KITS) {
        str_cpy(b, DSRC_KIT3[i % DX_NKIT], 8);
        str_cpy(b + str_len(b), " ", 8 - str_len(b));
        str_cpy(b + str_len(b), DSRC_LANE3[i / DX_NKIT], 8 - str_len(b));
    } else if (i < DSRC_FMD) {
        str_cpy(b, KICK_N[KICK_NKIT + (i - DSRC_KITS)], 8);
    } else {
        str_cpy(b, DSRC_FMDN[i - DSRC_FMD], 8);
    }
}
/* source i (1..DSRC_N) as a sound: *L, its LAYER / FEEDBK *lay; 0: none */
static __attribute__((noinline)) int cos_dsrc_get(uint32_t i, dx_lane_t *L, uint8_t *lay)
{
    if (!i || i > DSRC_N)
        return 0;
    i--;
    *lay = 0;
    if (i < DSRC_KITS) {
        const dx_kit_t *K = &DX_KITS[i % DX_NKIT];
        *L = K->lane[i / DX_NKIT];
        *lay = K->lay[i / DX_NKIT];
    } else if (i < DSRC_FMD) {
        *L = KICK_OWN[i - DSRC_KITS];
    } else {                                             /* an FMDRUM preset, its values on the kit's scales */
        const preset_t *P = &FMD_PRESETS[i - DSRC_FMD];
        uint32_t k, ti = 0, kind = P->xp & 15u;
        for (k = 0; k < 8u; k++) if (DXF_TRN[k] == kind) ti = k;
        L->mode = (uint8_t)(((uint32_t)P->e[0] & 3u) | DXF_FM << 2 | ti << 4);
        L->a[DXP_PITCH] = DSRC_FMDP[i - DSRC_FMD];
        L->a[DXP_PMOD] = (uint8_t)clamp(P->e[2], 0, 127);
        L->a[DXP_DECAY] = (uint8_t)clamp(P->env[1] * 3 / 2 - 50, 0, 127);   /* (its DEC, as DRUM-X's tau: ~ the same length) */
        L->a[DXP_NOISE] = (uint8_t)clamp(P->e[6], 0, 127);
        L->a[DXP_COLOR] = (uint8_t)clamp(P->e[4], 0, 127);
        for (k = 0; k < DXP_N; k++) L->b[k] = L->a[k];
        *lay = (uint8_t)(((uint32_t)clamp(P->e[5], 0, 11) + 1u) | ((uint32_t)clamp(P->e[1], 0, 3) + 1u) << 4 |
                         (uint32_t)(P->e[3] > 50) << 7);   /* (0.13.0: its RATIO, WAVE, a long S.TIME) */
    }
    return 1;
}
static void cos_dsrc_load(uint32_t l, uint32_t i)
{
    dx_lane_t L;
    uint8_t lay;
    dx_src[l & 7u] = (uint8_t)(i > DSRC_N ? 0u : i);
    if (cos_dsrc_get(i, &L, &lay)) {
        dx_kit[l & 7u] = L;
        dx_lay[l & 7u] = lay;
    }
}
static int cos_kick_trk(void);
static void cos_k0_poll(void)                            /* (each frame) */
{
    uint32_t p = cos_kick_trk() >= 0;
    dx_src_get = cos_dsrc_get;                          /* (0.14.0) the sound locks' sounds (eng_drum.c dx_slk_hit) */
    dx_k0p = (uint8_t)p;                                /* (0.14.1) a KICK track plays the kick: the kit's first a PERC */
    COS_LN[0] = p ? "PC" : "BD";
    COS_DRUM[0] = COS_DRUM5[0] = p ? "PERC" : "KICK";
    if (dx_k0_new) {                                    /* (a new set: the PERC a RIDE; ui.c power_sound_of) */
        dx_k0_new = 0;
        cos_dsrc_load(0, DSRC_FMD + 5u);
    }
}
/* the drum still the source last loaded (not edited since, nor a kit loaded over it) */
static int cos_dsrc_same(uint32_t l)
{
    dx_lane_t L;
    uint8_t lay;
    const dx_lane_t *K = &dx_kit[l & 7u];
    uint32_t k;
    if (!cos_dsrc_get(dx_src[l & 7u], &L, &lay) || L.mode != K->mode || lay != dx_lay[l & 7u])
        return 0;
    for (k = 0; k < DXP_N; k++)
        if (L.a[k] != K->a[k] || L.b[k] != K->b[k])
            return 0;
    return 1;
}
static __attribute__((noinline)) const param_desc_t *cos_ddesc(uint32_t id, int16_t **vp)
{
    static const char *const WV[] = {"SINE", "FM", "METAL", "BELL", 0}, *const FT[] = {"LP", "BP", "HP", "FM", 0};
    static const char *const TY[] = {"DRUM", "SNARE", "CYMBL", "HAT", 0};   /* (0.12.0) FILTER FM: FMDRUM's voice (drumx_voice.c
                                                         * dx_fmd): WAVE its TYPE, SNAP / BENDTYP its TRANSIENT */
    static const char *const TR[] = {"OFF", "SNAP", "CLICK", "CLK 2", "RIM", "CLAP", "PEDAL", "ACOUS", 0};
    static const param_desc_t DF[2] = {{"TYPE", F_ENUM, 0, 3, 0, TY, 0}, {"TRANS", F_ENUM, 0, 7, 0, TR, 0}};
    static const char *const BT[] = {"DECAY", "LONG", "NOISE", "SINE", 0}, *const OO[] = {"OFF", "ON", 0};
    static const param_desc_t D[] = {{"DRUM", F_ENUM, 0, 7, 0, COS_DRUM5, 0}, {"WAVE", F_ENUM, 0, 3, 0, WV, 0},
                                     {"FILTER", F_ENUM, 0, 3, 0, FT, 0}, {"BENDTYP", F_ENUM, 0, 3, 0, BT, 0},
                                     {"DRIVE", F_ENUM, 0, 1, 0, OO, 0}, {"LAYER", F_INT, 0, 15, 0, 0, 0},
                                     {"FEEDBK", F_INT, 0, 15, 0, 0, 0}, {"SNAP", F_ENUM, 0, 1, 0, OO, 0}};
    uint32_t l = dx_ui_lane & 7u, m = dx_kit[l].mode, fm = ((m >> 2) & 3u) == DXF_FM;
    int16_t *c = &cz.cc_v[14];
    *vp = c;
    if (fm && id == CC_DWAV) { *c = (int16_t)(m & 3u); return &DF[0]; }
    if (fm && id == CC_DSNP) { *c = (int16_t)((m >> 4) & 7u); return &DF[1]; }
    if (fm && (id == CC_DBT || id == CC_DLAY || id == CC_DFB)) {   /* (0.13.0) FM: S.TIME, RATIO, WAVE in its LAYER byte
                                                         * (drumx_voice.c DXF_*: 0 each the TYPE's own) */
        static const char *const ST[] = {"SHORT", "LONG", 0}, *const RT[] = {"AUTO", "0.5", "1", "1.5", "2", "3", "4", "5", "8",
                                         "1.41", "2.76", "3.73", "5.33", 0}, *const WV2[] = {"AUTO", "SINE", "TRI", "SAW", "SQUARE", 0};
        static const param_desc_t DG[3] = {{"S.TIME", F_ENUM, 0, 1, 0, ST, 0}, {"RATIO", F_ENUM, 0, 12, 0, RT, 0},
                                           {"WAVE", F_ENUM, 0, 4, 0, WV2, 0}};
        uint32_t y = dx_lay[l];
        *c = (int16_t)(id == CC_DBT ? DXF_LT(y) : id == CC_DLAY ? DXF_LR(y) : DXF_LW(y));
        return &DG[id == CC_DBT ? 0 : id == CC_DLAY ? 1 : 2];
    }
    switch (id) {
    case CC_DSEL: *c = (int16_t)l; return &D[0];
    case CC_DSRC: {
        static const param_desc_t SR = {"SOURCE", F_INT, 0, (int16_t)DSRC_N, 0, 0, 0};
        *c = (int16_t)dx_src[l];
        return &SR;
    }
    case CC_DLV: {                                       /* (its LEVEL: LEVELS' knob, named so) */
        static param_desc_t lv;
        lv = *track_desc(TSEL, P_LN0 + l);
        lv.label = "LEVEL";
        *vp = &TSEL->p[P_LN0 + l];
        return &lv;
    }
    case CC_DWAV: *c = (int16_t)(m & 3u); return &D[1];
    case CC_DFLT: *c = (int16_t)((m >> 2) & 3u); return &D[2];
    case CC_DBT: *c = (int16_t)((m >> 5) & 3u); return &D[3];
    case CC_DDRV: *c = (int16_t)((m & DX_DRIVE) != 0); return &D[4];
    case CC_DLAY: *c = (int16_t)DX_LAY_L(dx_lay[l]); return &D[5];
    case CC_DFB: *c = (int16_t)DX_LAY_FB(dx_lay[l]); return &D[6];
    case CC_DSNP: *c = (int16_t)((m >> 4) & 1u); return &D[7];
    default: {
        uint32_t k = (id - CC_DE0) % DE_N, x = dx_de[l][k];
        *c = (int16_t)(k >= DE_SCH ? (x ? (int32_t)x - 1 : 0) : (int32_t)x);   /* (0.14.1: 0 OFF, as the stored 0 now) */
        return &DX_DE_D[k];
    }
    }
}
static __attribute__((noinline)) void cos_dput(uint32_t id, int32_t v)
{
    uint32_t l = dx_ui_lane & 7u, m;
    dx_lane_t *L = &dx_kit[l];
    if (dx_slk_on) { fm1_irq_off(); dx_slk_back(); fm1_irq_on(); }   /* (0.14.0) an edit is of the kit's own sound */
    m = L->mode;
    if (((m >> 2) & 3u) == DXF_FM && id == CC_DSNP) {   /* (0.12.0) FM: the TRANSIENT, bits 4..6 */
        L->mode = (uint8_t)((m & ~0x70u) | (uint32_t)clamp(v, 0, 7) << 4);
        return;
    }
    if (((m >> 2) & 3u) == DXF_FM && (id == CC_DBT || id == CC_DLAY || id == CC_DFB)) {   /* (0.13.0) FM: S.TIME RATIO WAVE */
        uint32_t y = dx_lay[l];
        if (id == CC_DBT) y = (y & 0x7Fu) | (uint32_t)(v > 0) << 7;
        else if (id == CC_DLAY) y = (y & 0xF0u) | (uint32_t)clamp(v, 0, 12);
        else y = (y & 0x8Fu) | (uint32_t)clamp(v, 0, 4) << 4;
        dx_lay[l] = (uint8_t)y;
        return;
    }
    switch (id) {
    case CC_DSEL: dx_ui_lane = (uint8_t)clamp(v, 0, 7); break;
    case CC_DSRC: cos_dsrc_load(l, (uint32_t)clamp(v, 0, (int32_t)DSRC_N)); return;
    case CC_DLV: TSEL->p[P_LN0 + l] = (int16_t)clamp(v, 0, 127); break;
    case CC_DWAV: m = (m & ~3u) | (uint32_t)clamp(v, 0, 3); break;
    case CC_DFLT:
        if ((((m >> 2) & 3u) == DXF_FM) != (clamp(v, 0, 3) == DXF_FM)) dx_lay[l] = 0;   /* (0.13.0: to or from FM, its LAYER
                                                         * byte the other's meaning: back to its own defaults) */
        m = (m & ~0x0Cu) | (uint32_t)clamp(v, 0, 3) << 2;
        break;
    case CC_DBT: m = (m & ~0x60u) | (uint32_t)clamp(v, 0, 3) << 5; break;
    case CC_DDRV: m = v > 0 ? m | DX_DRIVE : m & ~DX_DRIVE; break;
    case CC_DSNP: m = v > 0 ? m | 0x10u : m & ~0x10u; break;
    case CC_DLAY: dx_lay[l] = (uint8_t)((dx_lay[l] & 0xF0u) | (uint32_t)clamp(v, 0, 15)); break;
    case CC_DFB: dx_lay[l] = (uint8_t)((dx_lay[l] & 0x0Fu) | (uint32_t)clamp(v, 0, 15) << 4); break;
    default: {
        uint32_t k = (id - CC_DE0) % DE_N;
        dx_de[l][k] = (uint8_t)(k >= DE_SCH ? (v > 0 ? clamp(v, 0, 127) + 1 : 0) : clamp(v, 0, 127));
        break;
    }
    }
    L->mode = (uint8_t)m;
}
/* SYSTEM's IDLE: minutes untouched before the IDLE screen, 0 OFF, 1 .. 60. The settings' engine-15 byte 16: 0 the
 * default (1), 0x80 | minutes; 1 .. 6 0.8.3's picks (OFF 30S 1MIN 2MIN 5MIN 10MIN) */
static uint32_t cos_idle_min(void)
{
    static const uint8_t OLD[7] = {1, 0, 1, 1, 2, 5, 10};
    uint32_t b = favorites.factory[15][16];
    return b & 0x80u ? (b & 0x7Fu) % 61u : OLD[b < 7u ? b : 0u];
}
/* GLO > SCOPE, SPECTRUM: their knobs (not kept; cos_scope_prep, cos_spec_prep) */
static uint8_t sc_time = 1, sc_gain = 0, sc_trig = 1, sc_mode = 0, sp_view, sp_range = 1, sp_speed = 1, sp_src, sc_zoom,
               lu_rst, lu_tgt = 3;   /* the pages' knobs (not kept) */
static void cos_name_pick(void);                        /* (SAVE's NAME, below) */
static void cos_proj_scan(void);
static char *proj_name_buf(void);                       /* (project.c: the music's name, 13 bytes) */
static void cos_lu_reset(void);                         /* (LOUDNESS's RESET, below) */
static int song_open(uint32_t n);                       /* (song_main.c: song n's slots, the music kept) */
#define COS_NSONG 8u                                    /* the songs offered (0.9.5..0.9.9: 4, 5..8's flash the sampler's) */
static uint8_t kick_pat_cur[NTRK];                       /* (0.10.0) each KICK track's last PATTERN (GEN > KICK) */
static void cos_kick_pat(track_t *t, uint32_t n);
/* (cursed-os 0.14.0) TRIG > COND: hold steps (white keys), turn a knob: their CONDITION (core.h TC_*: plays only on that
 * pass, or with FILL: in the chance's place), SLIDE (their locks glide in over the step; a synth's notes slide too, B2's),
 * SOUND (a kit: the lane picked with the black keys plays this sound on them: a sound lock, motion.c MOT_SND). Nothing
 * held: the last held step's, shown */
static uint64_t cos_held(void);
static const char *const TC_NAME[TC_N + 2u] = {"--", "1ST", "!1ST", "FILL", "!FILL", "1:2", "2:2", "1:3", "2:3", "3:3", "1:4",
                                               "2:4", "3:4", "4:4", "1:8", "2:8", "3:8", "4:8", "5:8", "6:8", "7:8", "8:8", 0};
static uint8_t cos_cnd_st;                              /* (the step shown: the last held) */
static uint32_t cos_cnd_first(void)
{
    uint64_t h = cos_held();
    uint32_t i;
    for (i = 0; i < NSTEP; i++)
        if ((h >> i) & 1u) { cos_cnd_st = (uint8_t)i; return i; }
    return cos_cnd_st % NSTEP;
}
static uint32_t cos_snd_of(const track_t *t, uint32_t st, uint32_t l)   /* step st's sound lock on lane l (0: none) */
{
    int32_t i = motion_find(t, st, MOT_SND(l));
    uint32_t u;
    if (i < 0 || !(motion.event[i].param & MOTION_LOCK)) return 0;
    u = (uint32_t)(MOTION_VAL(&motion.event[i]) + 64);
    return (u >= 96u) == (l & 1u) ? u % 96u : 0u;
}
static const param_desc_t *cos_cnd_desc(uint32_t id, int16_t **vp)
{
    static const char *const OO[] = {"OFF", "ON", 0};
    static const param_desc_t D[3] = {{"COND", F_ENUM, 0, (int16_t)TC_N, 0, TC_NAME, 0}, {"SLIDE", F_ENUM, 0, 1, 0, OO, 0},
                                      {"SOUND", F_INT, 0, (int16_t)DSRC_N, 0, 0, 0}};
    const step_t *s = &TSEL->step[cos_cnd_first()];
    int16_t *c = &cz.cc_v[3];
    *vp = c;
    if (id == CC_TCOND) { *c = (int16_t)(s->probability >= TC_BASE ? s->probability - TC_BASE + 1u : 0u); return &D[0]; }
    if (id == CC_TSLD) { *c = (int16_t)((s->flags & SF_SLIDE) != 0); return &D[1]; }
    if (!cos_kit()) return 0;                            /* (SOUND: a kit's) */
    *c = (int16_t)cos_snd_of(TSEL, cos_cnd_first(), cz.lane % NLANE);
    return &D[2];
}
static void cos_cnd_put(uint32_t id, int32_t v)
{
    track_t *t = TSEL;
    uint64_t h = cos_held();
    uint32_t i, l = cz.lane % NLANE;
    if (!h) { cos_say("HOLD A STEP"); return; }
    if (chain_busy()) { cos_say("SONG PLAYS: STOP IT"); return; }
    cz.lk_used = 1;                                      /* (the keys let go: no trig) */
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &t->step[i];
        if (!((h >> i) & 1u)) continue;
        if (id == CC_TCOND) s->probability = (uint8_t)(v > 0 ? TC_BASE + (uint32_t)clamp(v, 1, (int32_t)TC_N) - 1u : 0u);
        else if (id == CC_TSLD) s->flags = (uint8_t)(v > 0 ? s->flags | SF_SLIDE : s->flags & ~SF_SLIDE);
        else if (cos_kit()) {
            if (v <= 0) { if (cos_snd_of(t, i, l)) motion_delete_event(t, i, MOT_SND(l)); }
            else if (motion_set_lock(t, i, MOT_SND(l), MOT_SNDV(l, clamp(v, 1, (int32_t)DSRC_N))) == 2) { cos_say("LOCKS FULL (64)"); return; }
        }
    }
}
static const param_desc_t *cos_cdesc(uint32_t id, int16_t **vp)
{
    static const char *const MO[] = {"KEYS", "CLOCK", "ALL", "OFF", 0};
    static const param_desc_t D[2] = {{"OUT CH", F_INT, 1, 16, 1, 0, 0}, {"SENDS", F_ENUM, 0, 3, 0, MO, 0}};
    static const param_desc_t DS = {"SLOT", F_INT, 1, COS_NSONG * 4, 1, 0, 0}, DN = {"NAME", F_INT, 0, 999, 0, 0, 0};
    uint32_t t = song.sel % NTRK, v = favorites.factory[15][8u + t];
    if (id >= CC_RRATE && id <= CC_DCH7) return cos_drm_desc(id, vp);   /* (0.13.0) LFO > DRUMS */
    static const char *const LD[] = {"ECO", "LITE", "FULL", 0};
    static const param_desc_t DT = {"TRACKS", F_INT, 4, NTRK, 5, 0, 0}, DL = {"LOAD", F_ENUM, 0, 2, 2, LD, 0};
    if (id == CC_SLOT) { cz.cc_v[3] = (int16_t)(song_cur * 4u + cz.slot + 1u); *vp = &cz.cc_v[3]; return &DS; }
    if (id == CC_NAME) { cz.cc_v[3] = (int16_t)cz.nm_n; *vp = &cz.cc_v[3]; return &DN; }
    if (id == CC_USLOT) {                                /* (0.10.2) SAVE > SOUNDS: U01 .. U31 (U32: the LFO shapes') */
        static const param_desc_t DU = {"SOUND", F_INT, 1, UP_SLOTS - 1u, 1, 0, 0};
        cz.cc_v[3] = (int16_t)(cz.uslot + 1u);
        *vp = &cz.cc_v[3];
        return &DU;
    }
    if (id == CC_EXT) {                                  /* (0.13.0) the selected track's SOUND: INT, EXT (settings row 15, 3) */
        static const char *const EX[] = {"INT", "EXT", 0};
        static const param_desc_t DEX = {"SOUND", F_ENUM, 0, 1, 0, EX, 0};
        cz.cc_v[3] = (int16_t)((favorites.factory[15][3] >> (song.sel % NTRK)) & 1u);
        *vp = &cz.cc_v[3];
        return &DEX;
    }
    if (id == CC_TCOND || id == CC_TSLD || id == CC_TSND) return cos_cnd_desc(id, vp);   /* (0.14.0) TRIG > COND */
    if (id == CC_LSAT) {                                 /* (0.18.0) GLO > LIMIT's SATURATE: STRING's row 8, byte 23 */
        static const char *const SN[14] = {"OFF", "0 dB", "+1 dB", "+2 dB", "+3 dB", "+4 dB", "+5 dB", "+6 dB", "+7 dB",
                                           "+8 dB", "+9 dB", "+10 dB", "+11 dB", "+12 dB"};
        static const param_desc_t DS = {"SAT", F_ENUM, 0, 13, 1, SN, 0};   /* (0.18.1: 0 dB the default) */
        uint32_t sb = favorites.factory[8][23];          /* (0.18.1, GG: "on by default": 0 there 0 dB, 14 OFF) */
        cz.cc_v[3] = (int16_t)(sb == 0u ? 1u : sb == 14u ? 0u : sb <= 13u ? sb : 1u);
        *vp = &cz.cc_v[3];
        return &DS;
    }
    if (id == CC_QSTY || id == CC_QDRM || id == CC_QELM || id == CC_QBAR) {   /* (0.18.0) SAVE > START */
        static const param_desc_t DS[2] = {{"STYLE", F_ENUM, 0, QS_N - 1u, 0, QS_NAME, 0}, {"DRUMS", F_ENUM, 0, 2, 0, QS_DRN, 0}},
                                  DE[2] = {{"ELEMENTS", F_ENUM, 0, 1, 0, QS_ELN, 0}, {"BARS", F_ENUM, 0, 2, 2, QS_BRN, 0}};
        cz.cc_v[3] = (int16_t)(id == CC_QSTY ? qs_sty : id == CC_QDRM ? qs_drm_get() : id == CC_QELM ? qs_elm : qs_bar);
        *vp = &cz.cc_v[3];
        return id >= CC_QELM ? &DE[id - CC_QELM] : &DS[id - CC_QSTY];
    }
    if (id >= CC_STON && id <= CC_STLEN) {               /* (0.17.0) GLO > SYNTAKT */
        static const char *const OO[] = {"OFF", "ON", 0}, *const PF[] = {"OFF", "ON", 0},
                                 *const PL[] = {"1", "2", "4", "8", "16", 0};
        static const param_desc_t DS[3] = {{"SYNTAKT", F_ENUM, 0, 1, 0, OO, 0}, {"PATTERNS", F_ENUM, 0, 1, 0, PF, 0},
                                           {"PAT BARS", F_ENUM, 0, 4, 0, PL, 0}};
        uint32_t st = favorites.factory[8][31];
        cz.cc_v[3] = (int16_t)(id == CC_STON ? (st & 1u) && song.g[G_CLOCK] == 2 : id == CC_STPAT ? cos_pc_ch != 0u : (st >> 1) % 5u);
        *vp = &cz.cc_v[3];
        return &DS[id - CC_STON];
    }
    if (id == CC_DPAN || id == CC_DCHN) {               /* (0.14.1) EDIT > SOUND: the drum's PAN, CHANCE */
        static const param_desc_t DP2[2] = {{"PAN", F_INT, -64, 63, 0, 0, 0}, {"CHANCE", F_INT, 0, 100, 100, 0, "%"}};
        uint32_t l = dx_ui_lane & 7u;
        cz.cc_v[3] = (int16_t)(id == CC_DPAN ? dx_pan[l] : 100 - (int32_t)dx_skip[l]);
        *vp = &cz.cc_v[3];
        return &DP2[id == CC_DCHN];
    }
    if (id == CC_CTLM || id == CC_CTLCH) {               /* (0.13.0) GLO > MIDI 2: KNOBS, CTRL CH (settings row 15, 1 and 2) */
        static const char *const KN[] = {"FM-1", "+MIDI", "MIDI", 0};
        static const param_desc_t DKN[2] = {{"KNOBS", F_ENUM, 0, 2, 0, KN, 0}, {"CTRL CH", F_INT, 1, 16, 7, 0, 0}};
        cz.cc_v[3] = (int16_t)(id == CC_CTLM ? cos_ctl_mode() : (int32_t)cos_ctl_ch() + 1);
        *vp = &cz.cc_v[3];
        return &DKN[id == CC_CTLCH];
    }
    if (id == CC_KSLOT) {                                /* (0.13.0) SAVE > KITS: K01 .. K16 */
        static const param_desc_t DK2 = {"KIT", F_INT, 1, 16, 1, 0, 0};
        cz.cc_v[3] = (int16_t)(cz.kslot + 1u);
        *vp = &cz.cc_v[3];
        return &DK2;
    }
    if (id >= CC_SCT && id <= CC_LUR) {                  /* (GLO > SCOPE .. LOUDNESS: how they show, not kept) */
        static const char *const TM[] = {"5MS", "11MS", "44MS", "87MS", 0}, *const GN[] = {"AUTO", "X1", "X2", "X4", "X8", 0};
        static const char *const VW[] = {"BARS", "FALL", 0}, *const RG[] = {"60DB", "48DB", "36DB", 0};
        static const char *const SP[] = {"FAST", "MID", "SLOW", 0}, *const OO[] = {"OFF", "ON", 0};
        static const char *const MD[] = {"MONO", "L/R", "M/S", 0}, *const SR[] = {"MID", "SIDE", 0};
        static const char *const TG[] = {"-14", "-12", "-10", "-9", "-8", "-6", 0}, *const RS[] = {"TURN", "DONE", 0};
        static const param_desc_t D[11] = {{"TIME", F_ENUM, 0, 3, 1, TM, 0}, {"GAIN", F_ENUM, 0, 4, 0, GN, 0},
                                           {"TRIG", F_ENUM, 0, 1, 1, OO, 0}, {"VIEW", F_ENUM, 0, 1, 0, VW, 0},
                                           {"RANGE", F_ENUM, 0, 2, 1, RG, 0}, {"SPEED", F_ENUM, 0, 2, 1, SP, 0},
                                           {"MODE", F_ENUM, 0, 2, 0, MD, 0}, {"SOURCE", F_ENUM, 0, 1, 0, SR, 0},
                                           {"ZOOM", F_ENUM, 0, 4, 0, GN, 0}, {"TARGET", F_ENUM, 0, 5, 3, TG, 0},
                                           {"RESET", F_ENUM, 0, 1, 0, RS, 0}};
        const uint8_t *v[11] = {&sc_time, &sc_gain, &sc_trig, &sp_view, &sp_range, &sp_speed, &sc_mode, &sp_src, &sc_zoom,
                                &lu_tgt, &lu_rst};
        cz.cc_v[13] = *v[id - CC_SCT];
        *vp = &cz.cc_v[13];
        return &D[id - CC_SCT];
    }
    if (id == CC_SCALE || id == CC_QUANT) {               /* (SEL: the song's scale and how the keys keep to it) */
        track_t *m = cos_mel();
        uint32_t pi = id == CC_SCALE ? P_SCALE : P_QUANT;
        if (!m) { *vp = 0; return 0; }
        *vp = &m->p[pi];
        return track_desc(m, pi);
    }
    if (id == CC_IDLE) {                                 /* (the IDLE screen after this many minutes untouched, 0 OFF:
                                                         * cos_idle_min) */
        static const param_desc_t DI = {"IDLE", F_INT, 0, 60, 1, 0, 0};
        cz.cc_v[13] = (int16_t)cos_idle_min();
        *vp = &cz.cc_v[13];
        return &DI;
    }
    if (id == CC_BT) {                                   /* (MENU > SYSTEM > BLUETOOTH: a BLE-MIDI device, at once) */
        static const char *const OO[] = {"OFF", "ON", 0};
        static const param_desc_t DB = {"BT MIDI", F_ENUM, 0, 1, 0, OO, 0};
        cz.cc_v[14] = (int16_t)bt_get();
        *vp = &cz.cc_v[14];
        return &DB;
    }
    if (id == CC_TRKS) { cz.cc_v[4] = (int16_t)trk_live; *vp = &cz.cc_v[4]; return &DT; }
    if (id == CC_LOAD) { cz.cc_v[5] = (int16_t)load_get(); *vp = &cz.cc_v[5]; return &DL; }
    if (id >= CC_L1SYNC && id <= CC_L2RATE) {            /* the LFOs: SYNC (HZ / BPM: RATE's sign), RATE in its kind, MODE */
        static const char *const SY[] = {"HZ", "BPM", 0}, *const MO3[] = {"FREE", "TRIG", "ONE", 0};
        static const param_desc_t DSY = {"SYNC", F_ENUM, 0, 1, 0, SY, 0}, DHZ = {"RATE", F_LFOHZ, 0, 127, 60, 0, 0},
                                  DBP = {"RATE", F_ENUM, 0, 14, 7, COS_LSYNC, 0}, DMO = {"MODE", F_ENUM, 0, 2, 0, MO3, 0},
                                  DPH = {"PHASE", F_INT, 0, 127, 0, 0, 0};
        const track_t *t = TSEL;
        uint32_t k = song.sel % NTRK;
        int32_t r = id >= CC_L2SYNC ? trk_x2[k][X2_RATE] : t->p[P_LRATE];
        *vp = &cz.cc_v[14];
        switch (id) {
        case CC_L1SYNC: case CC_L2SYNC: cz.cc_v[14] = (int16_t)(r < 0); return &DSY;
        case CC_L1RATE: case CC_L2RATE: cz.cc_v[14] = (int16_t)(r < 0 ? cos_lsync_pos((uint32_t)(-r - 1)) : r); return r < 0 ? &DBP : &DHZ;
        case CC_L1WAVE: *vp = &TSEL->p[P_LWAVE]; return track_desc(TSEL, P_LWAVE);
        case CC_L1MODE: cz.cc_v[14] = (int16_t)(t->p[P_LPHASE] < 0 ? LM_FREE : trk_x2[k][X2_L1ONE] ? LM_ONE : LM_TRIG); return &DMO;
        case CC_L1PH: cz.cc_v[14] = (int16_t)(t->p[P_LPHASE] < 0 ? lfo_ph_mem[k] : t->p[P_LPHASE]); return &DPH;
        default: *vp = &TSEL->p[P_LFADE]; return track_desc(TSEL, P_LFADE);
        }
    }
    if (id >= CC_LVLEN && id <= CC_LVSH) {               /* HOME > LOOP: JIANT's LIVE layer on the track picked (never
                                                         * written into it: white 15 takes it back, in phase) */
        static const char *const NL[33] = {"ALL", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14",
                                           "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27",
                                           "28", "29", "30", "31", "32"};
        static const char *const ND[13] = {"-90", "-80", "-70", "-60", "-50", "-40", "-30", "-20", "-10", "ALL", "x2", "x3", "x4"};
        static const param_desc_t DL[4] = {{"LEN", F_ENUM, 0, 32, 0, NL, 0}, {"RATE", F_ENUM, 0, 8, 4, LV_RNAME, 0},
                                           {"DENS", F_ENUM, 0, 12, 9, ND, 0}, {"SHIFT", F_INT, -16, 16, 0, 0, 0}};
        uint32_t i = song.sel % NTRK;
        cz.cc_v[14] = (int16_t)(id == CC_LVLEN ? (lv.cut[i] > 100u ? lv.cut[i] - 100u : 0u) : id == CC_LVRATE ? lv.rate[i] + 4 :
                                id == CC_LVDN ? lv.dn[i] + 9 : lv.rot[i]);
        *vp = &cz.cc_v[14];
        return &DL[id - CC_LVLEN];
    }
    if (id == CC_MOPOS || id == CC_MOBARS) {             /* HOME > MORPH: A-B by hand, the glide's BARS */
        static const char *const NB[6] = {"CUT", "1", "2", "4", "8", "16"};
        static const param_desc_t DMP = {"A-B", F_PCT, 0, 100, 0, 0, 0}, DMB = {"BARS", F_ENUM, 0, 5, 3, NB, 0};
        cz.cc_v[14] = id == CC_MOPOS ? (int16_t)((mph.pos + 5) / 10) : (int16_t)mph.bars;
        *vp = &cz.cc_v[14];
        return id == CC_MOPOS ? &DMP : &DMB;
    }
    if (id >= CC_RB0 && id <= CC_RB3) {                  /* FX > RUMBLE (rumble.c rmb_v), saved with the project */
        static const param_desc_t DRB[4] = {{"LEVEL", F_PCT, 0, 100, 0, 0, 0}, {"DECAY", F_PCT, 0, 100, 70, 0, 0},
                                            {"TONE", F_PCT, 0, 100, 50, 0, 0}, {"DUCK", F_PCT, 0, 100, 80, 0, 0}};
        cz.cc_v[14] = rmb_v[(id - CC_RB0) & 3u];
        *vp = &cz.cc_v[14];
        return &DRB[(id - CC_RB0) & 3u];
    }
    if (id >= CC_MAC0 && id <= CC_MAC3) {                /* HOME > MACROS: M1..M4 (mod.c macro_v), what each sound's MOD pages
                                                         * route them to; saved with the project */
        static const param_desc_t DMC[4] = {{"M1", F_PCT, 0, 127, 0, 0, 0}, {"M2", F_PCT, 0, 127, 0, 0, 0},
                                            {"M3", F_PCT, 0, 127, 0, 0, 0}, {"M4", F_PCT, 0, 127, 0, 0, 0}};
        cz.cc_v[14] = macro_v[(id - CC_MAC0) & 3u];
        *vp = &cz.cc_v[14];
        return &DMC[(id - CC_MAC0) & 3u];
    }
    if (id == CC_DJF) { *vp = &song.g[G_DJF]; return &GP[G_DJF]; }   /* (FX > DJ: the master's DJ FILTER, as PERFORM's) */
    if (id == CC_LVDLY || id == CC_LVREV) {              /* (0.10.3) HOME > LIVE: the global sends */
        static const param_desc_t DS2[2] = {{"DELAY", F_PCT, 0, 127, 0, 0, 0}, {"REVERB", F_PCT, 0, 127, 0, 0, 0}};
        cz.cc_v[12] = lv_snd[id == CC_LVREV];
        *vp = &cz.cc_v[12];
        return &DS2[id == CC_LVREV];
    }
    if (id == CC_LVFB) { *vp = &song.g[G_DFDBK]; return &GP[G_DFDBK]; }   /* (LIVE: the delay's FEEDBACK, the throws' tail) */
    if (id >= CC_EQL && id <= CC_EQH) {                  /* HOME > ISOLATOR (cos_iso.c iso_v): LOW MID HIGH, KILL -24 .. +6 dB */
        static const char *const ISN[33] = {"KILL", "-24", "-23", "-22", "-21", "-20", "-19", "-18", "-17", "-16", "-15", "-14", "-13", "-12", "-11", "-10", "-9", "-8", "-7", "-6", "-5", "-4", "-3", "-2", "-1", "0", "+1", "+2", "+3", "+4", "+5", "+6", 0};
        static const param_desc_t DE[3] = {{"LOW", F_INT, ISO_KILL, 6, 0, ISN, "dB"}, {"MID", F_INT, ISO_KILL, 6, 0, ISN, "dB"},
                                           {"HIGH", F_INT, ISO_KILL, 6, 0, ISN, "dB"}};
        cz.cc_v[14] = iso_v[id - CC_EQL];
        *vp = &cz.cc_v[14];
        return &DE[id - CC_EQL];
    }
    if (id >= CC_PCCH && id <= CC_VJCH) {                /* GLO > LINK (midi_control.c cos_pc_ch, cos_pc_out, cos_vj_ch) */
        static const char *const OO[2] = {"OFF", "ON"};
        static const param_desc_t DL[3] = {{"PC CH", F_INT, 0, 16, 0, 0, 0}, {"PC OUT", F_ENUM, 0, 1, 0, OO, 0},
                                           {"VJ CH", F_INT, 0, 16, 0, 0, 0}};
        cz.cc_v[14] = (int16_t)(id == CC_PCCH ? cos_pc_ch : id == CC_PCOUT ? cos_pc_out : cos_vj_ch);
        *vp = &cz.cc_v[14];
        return &DL[id - CC_PCCH];
    }
    if (id == CC_MOCC) {                                 /* GLO > LINK 2: MORPH's A-B as a CC (-1: OFF) */
        static const param_desc_t DO = {"MO CC", F_INT, -1, 127, -1, 0, 0};
        cz.cc_v[14] = (int16_t)((int32_t)cos_mo_cc - 1);
        *vp = &cz.cc_v[14];
        return &DO;
    }
    if (id >= CC_MCCH && id <= CC_MC3) {                 /* GLO > LINK 2 / 3 (midi_control.c cos_mc_*) */
        static const char *const MD[3] = {"BOTH", "OUT", "IN"};
        static const char *const MP[3] = {"OWN", "TRACK", "FX"};   /* (OWN: LINK 3's; the Syntakt's maps) */
        static const param_desc_t DM[3] = {{"MAC CH", F_INT, 0, 16, 0, 0, 0}, {"M1 CC", F_INT, 0, 127, 74, 0, 0},
                                           {"DIR", F_ENUM, 0, 2, 0, MD, 0}};
        static const param_desc_t DP = {"MAP", F_ENUM, 0, 2, 1, MP, 0};
        static const param_desc_t DC[4] = {{"M1 CC", F_INT, 0, 127, 74, 0, 0}, {"M2 CC", F_INT, 0, 127, 75, 0, 0},
                                           {"M3 CC", F_INT, 0, 127, 84, 0, 0}, {"M4 CC", F_INT, 0, 127, 85, 0, 0}};
        *vp = &cz.cc_v[14];
        if (id >= CC_MC0) { cz.cc_v[14] = cos_mc_cc[(id - CC_MC0) & 3u]; return &DC[(id - CC_MC0) & 3u]; }
        if (id == CC_MCMAP) { cz.cc_v[14] = (int16_t)cos_mc_map(); return &DP; }
        cz.cc_v[14] = (int16_t)(id == CC_MCCH ? cos_mc_ch : id == CC_MCCC ? cos_mc_cc[0] : cos_mc_dir);
        return &DM[id - CC_MCCH];
    }
    if (id == CC_MUTEQ) {                                /* HOME 2: when a mute or a solo goes in (cos_live.c) */
        static const char *const QN[3] = {"NOW", "BEAT", "BAR"};
        static const param_desc_t DQ = {"MUTE Q", F_ENUM, 0, 2, 2, QN, 0};
        cz.cc_v[14] = (int16_t)(cos_mq % 3u);
        *vp = &cz.cc_v[14];
        return &DQ;
    }
    if (id == CC_BPM) {                                  /* BPM in hundredths (0.10.1; tenths before): the whole on K1 */
        static const param_desc_t DB = {"BPM", F_INT, 4000, 24000, 12000, 0, 0};
        cz.cc_v[14] = (int16_t)(cos_follows() ? (int32_t)cos_ext10() * 10 : song.g[G_BPM] * 100 + bpm_frac % 100u);   /* (a clock: its) */
        *vp = &cz.cc_v[14];
        return &DB;
    }
    if (id == CC_BFINE) {                                /* (0.10.1) the hundredths: 130 + .16 = 130.16 */
        static const param_desc_t DF = {"FINE", F_INT, 0, 99, 0, 0, 0};
        cz.cc_v[13] = (int16_t)(cos_follows() ? (int32_t)cos_ext10() % 10 * 10 : bpm_frac % 100u);
        *vp = &cz.cc_v[13];
        return &DF;
    }
    if (id == CC_OCT) {                                  /* OCTAVE: TRANS in octaves (-2 .. +2), its semitones kept; a kit none */
        static const param_desc_t DO = {"OCTAVE", F_INT, -2, 2, 0, 0, 0};
        int32_t tr = TSEL->p[P_TRANS];
        if (cos_kit() || trk_eng(TSEL) == ENGI_KICK) return 0;   /* (0.10.0: a KICK, hits: TUNE is its pitch) */
        cz.cc_v[14] = (int16_t)(tr >= 0 ? (tr + 6) / 12 : -((-tr + 5) / 12));
        *vp = &cz.cc_v[14];
        return &DO;
    }
    if (id >= CC_DSEL && id <= CC_DEE) return cos_ddesc(id, vp);   /* (a drum of the kit: its own) */
    if (id >= CC_KICK && id <= CC_KICKE) {               /* the drum picked: DRUM-X's settings, where MORPH has them */
        static const param_desc_t DK[5] = {{"PITCH", F_INT, 0, 127, 64, 0, 0}, {"BEND", F_PCT, 0, 127, 64, 0, 0},
                                           {"DECAY", F_PCT, 0, 127, 64, 0, 0}, {"NOISE", F_PCT, 0, 127, 0, 0, 0},
                                           {"TONE", F_PCT, 0, 127, 64, 0, 0}};
        static const param_desc_t DKF[2] = {{"SWEEP", F_PCT, 0, 127, 64, 0, 0}, {"FM", F_PCT, 0, 127, 64, 0, 0}};
        uint32_t i = id - CC_KICK;
        const dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
        int32_t mo = clamp(TSEL->p[P_E0], 0, 127);
        cz.cc_v[14] = (int16_t)(L->a[i] + ((L->b[i] - L->a[i]) * mo) / 127);
        *vp = &cz.cc_v[14];
        if (((L->mode >> 2) & 3u) == DXF_FM && (i == DXP_PMOD || i == DXP_COLOR))   /* (0.12.0) FMDRUM's names */
            return &DKF[i == DXP_COLOR];
        return &DK[i];
    }
    if (id >= CC_LN0 && id <= CC_LNE) { *vp = &TSEL->p[P_LN0 + (id - CC_LN0)]; return track_desc(TSEL, P_LN0 + (id - CC_LN0)); }
    if (id == CC_KDRV) { *vp = &TSEL->p[P_E7]; return track_desc(TSEL, P_E7); }
    if (id >= CC_X2 && id < CC_X2 + X2_N) {               /* LFO 2, FILTER 2: the track's (params.c trk_x2) */
        uint32_t i = id - CC_X2;
        cz.cc_v[14] = trk_x2[song.sel % NTRK][i];
        *vp = &cz.cc_v[14];
        return &X2_DESC[i];
    }
    if (id >= CC_PF0 && id <= CC_PF3) {                  /* the FX pad's macros (perform.c perf_k): FILTER - LP + HP, CRUSH,
                                                         * THROW (into the delay and reverb), DEPTH (the repeats' level) */
        static const param_desc_t DP[4] = {{"FILTER", F_INT, -100, 100, 0, 0, 0}, {"CRUSH", F_INT, 0, 100, 0, 0, "%"},
                                           {"THROW", F_INT, 0, 100, 0, 0, "%"}, {"DEPTH", F_INT, 0, 100, 100, 0, "%"}};
        uint32_t i = id - CC_PF0;
        cz.cc_v[10 + i] = (int16_t)(i == 3u ? 100 - perf_k[3] : perf_k[i]);
        *vp = &cz.cc_v[10 + i];
        return &DP[i];
    }
    if (id >= CC_CIN && id <= CC_LVL) {                  /* GLO > COMP, LIMIT: TRIO's retired row's bytes 8 .. 17 (ui_input.c
                                                         * ui_polls; 0: each default) */
        static const char *const RAT[8] = {"1.5:1", "2:1", "3:1", "4:1", "6:1", "8:1", "12:1", "20:1"};
        static const char *const SCN[7] = {"OFF", "40HZ", "60HZ", "90HZ", "120HZ", "150HZ", "200HZ"};
        static const char *const LRN[6] = {"5MS", "10MS", "25MS", "45MS", "90MS", "180MS"};
        static const char *const ONO[2] = {"ON", "OFF"};
        static const param_desc_t DX[10] = {{"INPUT", F_INT, -12, 12, 0, 0, "dB"}, {"THRESH", F_INT, -40, 0, -12, 0, "dB"},
                                            {"RATIO", F_ENUM, 0, 7, 3, RAT, 0}, {"MAKEUP", F_INT, 0, 18, 0, 0, "dB"},
                                            {"KNEE", F_INT, 0, 12, 6, 0, "dB"}, {"SC HPF", F_ENUM, 0, 6, 0, SCN, 0},
                                            {"MIX", F_INT, 0, 100, 100, 0, "%"}, {"DRIVE", F_INT, 0, 12, 0, 0, "dB"},
                                            {"RELEASE", F_ENUM, 0, 5, 3, LRN, 0}, {"AUTO LV", F_ENUM, 0, 1, 0, ONO, 0}};
        const uint8_t *x = &favorites.factory[6][8];
        uint32_t i = id - CC_CIN;
        cz.cc_v[14] = (int16_t)(i == 0u ? (x[0] && x[0] <= 25u ? x[0] - 13 : 0) : i == 1u ? (x[1] && x[1] <= 41u ? -(x[1] - 1) : -12)
                              : i == 2u ? (x[2] && x[2] <= 8u ? x[2] - 1 : 3) : i == 3u ? (x[3] <= 18u ? x[3] : 0)
                              : i == 4u ? (x[4] && x[4] <= 13u ? x[4] - 1 : 6) : i == 5u ? (x[5] <= 6u ? x[5] : 0)
                              : i == 6u ? (x[6] && x[6] <= 101u ? x[6] - 1 : 100) : i == 7u ? (x[7] <= 12u ? x[7] : 0)
                              : i == 8u ? (x[8] && x[8] <= 6u ? x[8] - 1 : 3) : x[9] == 1u);
        *vp = &cz.cc_v[14];
        return &DX[i];
    }
    if (id >= CC_COMP && id <= CC_LIM) {                 /* GLO > COMP's ON, ATTACK, RELEASE, LIMIT's CEILING: the settings'
                                                         * bytes 4 .. 7 (ui_input.c ui_polls) */
        static const char *const OF[2] = {"OFF", "ON"};
        static const param_desc_t DM[4] = {{"COMP", F_ENUM, 0, 1, 0, OF, 0}, {"ATTACK", F_INT, 0, 100, 20, 0, 0},
                                           {"RELEASE", F_INT, 0, 100, 40, 0, 0}, {"CEILING", F_INT, -18, -6, -6, 0, "dB"}};   /* (the
                                                         * output's peak, dBFS: the soft clip after it counted) */
        const uint8_t *m = &favorites.factory[15][4];
        uint32_t i = id - CC_COMP;
        cz.cc_v[6 + i] = (int16_t)(i == 0u ? m[0] != 0u : i == 3u ? -6 - (int32_t)m[3] : i == 1u ? (m[1] ? m[1] - 1 : 20) : (m[2] ? m[2] - 1 : 40));
        *vp = &cz.cc_v[6 + i];
        return &DM[i];
    }
    if (id == CC_KPAT) {                                 /* (0.10.0) a KICK track's PATTERN: the last one thrown */
        static const param_desc_t DK = {"PATTERN", F_ENUM, 0, KICK_NPAT - 1u, 0, KICK_PAT_N, 0};
        cz.cc_v[15] = (int16_t)(kick_pat_cur[song.sel % NTRK] % KICK_NPAT);
        *vp = &cz.cc_v[15];
        return &DK;
    }
    if (id == CC_LOCAL) {                                /* (0.9.4) LOCAL ON / OFF: the settings' byte 0 (seq.c kb_local_off) */
        static const char *const LO[3] = {"ON", "OFF", 0};
        static const param_desc_t DLO = {"LOCAL", F_ENUM, 0, 1, 0, LO, 0};
        cz.cc_v[15] = (int16_t)(favorites.factory[15][0] == 1u);
        *vp = &cz.cc_v[15];
        return &DLO;
    }
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
    if (id >= CC_RRATE && id <= CC_DCH7) { cos_drm_put(id, v); return; }   /* (0.13.0) LFO > DRUMS */
    if (id == CC_OUTCH) {
        v = clamp(v, 1, 16);
        favorites.factory[15][8u + t] = (uint8_t)((uint32_t)v == t + 1u ? 0u : (uint32_t)v);   /* (its own: 0, as JIANT) */
        settings_save();
    } else if (id == CC_MOUT) {
        midi_out_mode = (uint8_t)(clamp(v, 0, 3));
        settings_save();
    } else if (id == CC_KPAT) {
        cos_kick_pat(TSEL, (uint32_t)clamp(v, 0, KICK_NPAT - 1));
    } else if (id == CC_LOCAL) {
        uint32_t k;
        favorites.factory[15][0] = (uint8_t)(v > 0);
        kb_local_off = (uint8_t)(v > 0);
        for (k = 0; k < 16u; k++) midi_out_event(0x0Bu | (0xB0u | k) << 8 | 123u << 16);   /* (ALL NOTES OFF: none left
                                                         * hanging in the computer across the change) */
        settings_save();
    } else if (id == CC_SLOT) {
        uint32_t w = (uint32_t)clamp(v, 1, COS_NSONG * 4) - 1u;
        if (w / 4u != song_cur) {                        /* (another song's slot: that song opened, the music kept) */
            if (song.playing) { cos_say("STOP TO CHANGE SONG"); return; }
            if (song_open(w / 4u)) return;
            cz.pn_ok = 0;
        }
        cz.slot = (uint8_t)(w % 4u);
        cos_proj_scan();
    } else if (id == CC_USLOT) {
        cz.uslot = (uint8_t)(clamp(v, 1, UP_SLOTS - 1) - 1);
        cz.snd_ms = 0;
    } else if (id == CC_EXT) {
        uint8_t m = (uint8_t)(favorites.factory[15][3] & ((1u << NTRK) - 1u));
        favorites.factory[15][3] = (uint8_t)(v ? m | 1u << t : m & ~(1u << t));
        settings_save();
    } else if (id >= CC_STON && id <= CC_STLEN) {
        cos_syntakt(id, v);                              /* (0.17.0) GLO > SYNTAKT */
    } else if (id == CC_LSAT) {
        favorites.factory[8][23] = (uint8_t)(v <= 0 ? 14 : v == 1 ? 0 : clamp(v, 2, 13));   /* (0.18.1: OFF 14, 0 dB 0) */
        cz.set_ms = fm1_ms;
    } else if (id == CC_QSTY) {
        qs_sty = (uint8_t)clamp(v, 0, QS_N - 1);
    } else if (id == CC_QDRM) {
        qs_drm = (uint8_t)(clamp(v, 0, 2) + 1);
    } else if (id == CC_QELM) {
        qs_elm = (uint8_t)clamp(v, 0, 1);
    } else if (id == CC_QBAR) {
        qs_bar = (uint8_t)clamp(v, 0, 2);
    } else if (id == CC_DPAN) {
        dx_pan[dx_ui_lane & 7u] = (int8_t)clamp(v, -64, 63);
    } else if (id == CC_DCHN) {
        dx_skip[dx_ui_lane & 7u] = (uint8_t)(100 - clamp(v, 0, 100));
    } else if (id == CC_TCOND || id == CC_TSLD || id == CC_TSND) {
        cos_cnd_put(id, v);                              /* (0.14.0) TRIG > COND: the held steps' */
    } else if (id == CC_CTLM) {
        favorites.factory[15][1] = (uint8_t)clamp(v, 0, 2);
        settings_save();
    } else if (id == CC_CTLCH) {
        favorites.factory[15][2] = (uint8_t)clamp(v, 1, 16);
        settings_save();
    } else if (id == CC_KSLOT) {
        cz.kslot = (uint8_t)(clamp(v, 1, 16) - 1);
        cz.snd_ms = 0;
    } else if (id == CC_NAME) {
        cz.nm_n = (uint16_t)clamp(v, 0, 999);
        cos_name_pick();
    } else if (id >= CC_SCT && id <= CC_LUR) {
        static const uint8_t MX[11] = {3, 4, 1, 1, 2, 2, 2, 1, 4, 5, 1};
        uint8_t *const pv[11] = {&sc_time, &sc_gain, &sc_trig, &sp_view, &sp_range, &sp_speed, &sc_mode, &sp_src, &sc_zoom,
                                 &lu_tgt, &lu_rst};
        if (id == CC_LUR) { lu_rst = (uint8_t)(v != 0); cos_lu_reset(); return; }   /* (any turn: from now) */
        *pv[id - CC_SCT] = (uint8_t)clamp(v, 0, MX[id - CC_SCT]);
    } else if (id == CC_SCALE || id == CC_QUANT) {
        track_t *m = cos_mel();
        uint32_t pi = id == CC_SCALE ? P_SCALE : P_QUANT, k;
        const param_desc_t *d = m ? track_desc(m, pi) : 0;
        if (!d) return;
        v = clamp(v, d->min, d->max);
        for (k = 0; k < NTRK; k++)                       /* (every melodic track at once: scale_link would, next frame) */
            if (!cos_hits(&trk[k])) trk[k].p[pi] = (int16_t)v;
    } else if (id == CC_IDLE) {
        favorites.factory[15][16] = (uint8_t)(0x80 | clamp(v, 0, 60));
        cz.act_ms = fm1_ms;
        settings_save();
    } else if (id == CC_BT) {
        bt_put((uint32_t)clamp(v, 0, 1));
    } else if (id == CC_TRKS) {                          /* (MENU > TRACKS: 6 plays T5 and T6 too, more for the CPU) */
#if COS_SLIM
        favorites.factory[15][15] = (uint8_t)clamp(v, 4, NTRK);   /* (ui.c rp_apply reads it) */
        rp_apply();
#else
        trks_put(v >= NTRK);
#endif
        if (song.sel >= trk_live) track_select(trk_live - 1u);
        settings_save();
    } else if (id == CC_LOAD) {
        load_put((uint32_t)clamp(v, 0, 2));
        settings_save();
    } else if (id >= CC_L1SYNC && id <= CC_L2RATE) {
        track_t *t = TSEL;
        uint32_t k = song.sel % NTRK;
        int lf2 = id >= CC_L2SYNC;
        int32_t r = lf2 ? trk_x2[k][X2_RATE] : t->p[P_LRATE], nr = r;
        if (id == CC_L1SYNC || id == CC_L2SYNC)          /* (HZ -> BPM: 1/4; BPM -> HZ: about a beat's at 120) */
            nr = v ? (r < 0 ? r : -5) : (r < 0 ? 60 : r);
        else if (id == CC_L1RATE || id == CC_L2RATE)
            nr = r < 0 ? -((int32_t)cos_lsync_idx((uint32_t)clamp(v, 0, 14)) + 1) : clamp(v, 0, 127);
        if (id == CC_L1SYNC || id == CC_L1RATE || id == CC_L2SYNC || id == CC_L2RATE) {
            if (lf2) trk_x2[k][X2_RATE] = (int8_t)nr;
            else t->p[P_LRATE] = (int16_t)nr;
        } else if (id == CC_L1WAVE) {
            t->p[P_LWAVE] = (int16_t)clamp(v, TP[P_LWAVE].min, TP[P_LWAVE].max);
        } else if (id == CC_L1MODE) {                    /* FREE: PHASE -1 (TRIG's kept in lfo_ph_mem); TRIG, ONE: PHASE back */
            v = clamp(v, 0, 2);
            if (v == LM_FREE && t->p[P_LPHASE] >= 0) { lfo_ph_mem[k] = (uint8_t)t->p[P_LPHASE]; t->p[P_LPHASE] = -1; }
            else if (v != LM_FREE && t->p[P_LPHASE] < 0) t->p[P_LPHASE] = (int16_t)lfo_ph_mem[k];
            trk_x2[k][X2_L1ONE] = (int8_t)(v == LM_ONE);
        } else if (id == CC_L1PH) {
            if (t->p[P_LPHASE] >= 0) t->p[P_LPHASE] = (int16_t)clamp(v, 0, 127);
            else lfo_ph_mem[k] = (uint8_t)clamp(v, 0, 127);
        } else {
            t->p[P_LFADE] = (int16_t)clamp(v, TP[P_LFADE].min, TP[P_LFADE].max);
        }
    } else if (id >= CC_LVLEN && id <= CC_LVSH) {
        uint32_t i = song.sel % NTRK;
        if (id == CC_LVLEN) lv.cut[i] = (uint8_t)(v <= 0 ? 0 : 100 + clamp(v, 1, 32));
        else if (id == CC_LVRATE) lv.rate[i] = (int8_t)(clamp(v, 0, 8) - 4);
        else if (id == CC_LVDN) lv.dn[i] = (int8_t)(clamp(v, 0, 12) - 9);
        else lv.rot[i] = (int8_t)clamp(v, -16, 16);
        live_publish();
    } else if (id == CC_MOPOS) {
        mph_hand(v);
    } else if (id == CC_MOBARS) {
        mph.bars = (uint8_t)clamp(v, 0, 5);
    } else if (id >= CC_RB0 && id <= CC_RB3) {
        rmb_v[(id - CC_RB0) & 3u] = (uint8_t)clamp(v, 0, 100);
    } else if (id >= CC_MAC0 && id <= CC_MAC3) {
        macro_v[(id - CC_MAC0) & 3u] = (uint8_t)clamp(v, 0, 127);
    } else if (id == CC_DJF) {
        cos_snap();                                      /* (0.10.3: LIVE's normal, before the first move) */
        motion_djf_set(TSEL, (int16_t)clamp(v, GP[G_DJF].min, GP[G_DJF].max));   /* (REC: recorded too, as PERFORM's) */
    } else if (id == CC_LVDLY || id == CC_LVREV) {
        cos_snap();
        lv_snd[id == CC_LVREV] = (uint8_t)clamp(v, 0, 127);
    } else if (id == CC_LVFB) {
        cos_snap();
        song.g[G_DFDBK] = (int16_t)clamp(v, GP[G_DFDBK].min, GP[G_DFDBK].max);
    } else if (id >= CC_EQL && id <= CC_EQH) {
        iso_v[id - CC_EQL] = (int8_t)clamp(v, ISO_KILL, 6);
    } else if (id >= CC_PCCH && id <= CC_VJCH) {
        uint8_t *x = &favorites.factory[6][19];             /* (TRIO's retired row: 0 there, OFF) */
        if (id == CC_PCCH) { x[0] = (uint8_t)clamp(v, 0, 16); cos_pc_ch = x[0]; }
        else if (id == CC_PCOUT) { x[1] = (uint8_t)clamp(v, 0, 1); cos_pc_out = x[1]; }
        else { x[2] = (uint8_t)clamp(v, 0, 16); cos_vj_ch = x[2]; }
        cz.set_ms = fm1_ms;
    } else if (id == CC_MOCC) {
        cos_mo_cc = (uint8_t)(clamp(v, -1, 127) + 1);
        favorites.factory[8][30] = cos_mo_cc;            /* (STRING's row past its stars: 0 there, OFF) */
        cos_mo_sent = 0xFFu;
        cz.set_ms = fm1_ms;
    } else if (id >= CC_MCCH && id <= CC_MC3) {
        uint8_t *m = &favorites.factory[8][24];         /* (STRING's row past its stars: 0 there, OFF, ST TRACK, BOTH) */
        if (id == CC_MCCH) { m[0] = (uint8_t)clamp(v, 0, 16); cos_mc_ch = m[0]; }
        else if (id == CC_MCDIR) { m[2] = (uint8_t)clamp(v, 0, 2); cos_mc_dir = m[2]; }
        else if (id == CC_MCMAP) { if (v >= 1) cos_mc_set(MC_MAP[clamp(v, 1, 2) - 1]); }
        else {                                           /* (CC_MCCC: M1's, as LINK 3's first) */
            uint8_t c[4];
            memcpy(c, (const void *)cos_mc_cc, 4);
            c[id == CC_MCCC ? 0u : (id - CC_MC0) & 3u] = (uint8_t)clamp(v, 0, 127);
            cos_mc_set(c);
        }
        cz.set_ms = fm1_ms;
    } else if (id == CC_MUTEQ) {
        favorites.factory[6][18] = (uint8_t)(clamp(v, 0, 2) + 1);   /* (TRIO's retired row: 0 there, BAR) */
        cos_mq = (uint8_t)clamp(v, 0, 2);
        cz.set_ms = fm1_ms;
    } else if (id == CC_OCT) {
        int32_t tr = TSEL->p[P_TRANS], o = tr >= 0 ? (tr + 6) / 12 : -((-tr + 5) / 12);
        TSEL->p[P_TRANS] = (int16_t)clamp(tr + (clamp(v, -2, 2) - o) * 12, -24, 24);
    } else if (id >= CC_DSEL && id <= CC_DEE) {
        cos_dput(id, v);
    } else if (id >= CC_KICK && id <= CC_KICKE) {        /* both ends moved by the turn: MORPH's span kept */
        uint32_t i = id - CC_KICK;
        dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
        int32_t mo = clamp(TSEL->p[P_E0], 0, 127), cur = L->a[i] + ((L->b[i] - L->a[i]) * mo) / 127, d = v - cur;
        L->a[i] = (uint8_t)clamp(L->a[i] + d, 0, 127);
        L->b[i] = (uint8_t)clamp(L->b[i] + d, 0, 127);
    } else if (id >= CC_LN0 && id <= CC_LNE) {
        TSEL->p[P_LN0 + (id - CC_LN0)] = (int16_t)clamp(v, 0, 127);
    } else if (id == CC_KDRV) {
        TSEL->p[P_E7] = (int16_t)clamp(v, 0, 127);
    } else if (id >= CC_X2 && id < CC_X2 + X2_N) {
        uint32_t i = id - CC_X2;
        trk_x2[song.sel % NTRK][i] = (int8_t)clamp(v, X2_DESC[i].min, X2_DESC[i].max);
    } else if (id >= CC_PF0 && id <= CC_PF3) {
        uint32_t i = id - CC_PF0;
        cz.fx_used = 1;
        perf_k[i] = (int8_t)(i == 0u ? clamp(v, -100, 100) : i == 3u ? 100 - clamp(v, 0, 100) : clamp(v, 0, 100));
    } else if (id >= CC_CIN && id <= CC_LVL) {
        uint8_t *x = &favorites.factory[6][8];
        uint32_t i = id - CC_CIN;
        x[i] = (uint8_t)(i == 0u ? clamp(v, -12, 12) + 13 : i == 1u ? -clamp(v, -40, 0) + 1 : i == 2u ? clamp(v, 0, 7) + 1
                       : i == 3u ? clamp(v, 0, 18) : i == 4u ? clamp(v, 0, 12) + 1 : i == 5u ? clamp(v, 0, 6)
                       : i == 6u ? clamp(v, 0, 100) + 1 : i == 7u ? clamp(v, 0, 12) : i == 8u ? clamp(v, 0, 5) + 1 : clamp(v, 0, 1));
        cz.set_ms = fm1_ms;                              /* (saved when the knob rests: cos_input) */
    } else if (id >= CC_COMP && id <= CC_LIM) {
        uint8_t *m = &favorites.factory[15][4];
        uint32_t i = id - CC_COMP;
        m[i] = (uint8_t)(i == 0u ? (v ? 100 : 0) : i == 3u ? -6 - clamp(v, -18, -6) : clamp(v, 0, 100) + 1);
        cz.set_ms = fm1_ms;                              /* (saved when the knob rests: cos_input) */
    } else
        song.g[G_CLOCK] = (int16_t)clamp(v, GP[G_CLOCK].min, GP[G_CLOCK].max);
}

/* knob k of the page: its description and its value (0: none) */
/* (cursed-os) a track's parameter its engine does not use: no dial for it (a blank), so no knob turns nothing.
 *   a kit (DRUM-X): its lanes' own envelopes (drum_amp): no ATTACK DECAY SUSTAIN RELEASE, no ENV MOD to PITCH or
 *     SHAPE (it reads neither), no GLIDE, no GATE (one-shots), no SCALE or QUANT (hits, not notes)
 *   an engine with its own envelopes (ACID, FM6, STRING: ownenv): no ADSR (STRING's RELEASE is its damper's), no ENV
 *     MOD (ENV is 0 for them)
 *   ACID: one voice (no unison: no DETUNE), no SHAPE (it reads none: no LFO to SHAPE) */
static int cos_dead(const track_t *t, uint32_t id)
{
    const engine_t *e = trk_engp(t);
    if (ENG_HITS(e))
        return id == P_ATK || id == P_DEC || id == P_SUS || id == P_REL || id == P_ED_PIT || id == P_ED_SHP ||
               id == P_GLIDE || id == P_SGATE || id == P_SCALE || id == P_QUANT;   /* (and no scale: hits) */
    if (e->ownenv && (((id == P_ATK || id == P_DEC || id == P_SUS || id == P_REL) && e != &ENG_ACID &&
                       (id != P_REL || e != &ENG_STRING)) ||
                      id == P_ED_FLT || id == P_ED_PIT || id == P_ED_SHP))
        return 1;
    if (e == &ENG_ACID && (id == P_DETUNE || id == P_LD_SHP))
        return 1;
    return 0;
}
static const param_desc_t *cos_knob(const cpg_t *p, uint32_t k, int16_t **vp)
{
    uint32_t id = p->id[k & 3u];
    *vp = 0;
    if (id == 0xFFu) return 0;
    if (p->kind == CK_TRK && cos_dead(TSEL, id)) return 0;   /* (this engine does not use it) */
    if (id == ID_OCT) return cos_cdesc(CC_OCT, vp);     /* (any page's: the track's OCTAVE) */
    if (id == ID_BPM) return cos_cdesc(CC_BPM, vp);
    if (id == ID_BFINE) return cos_cdesc(CC_BFINE, vp);
    if (id == ID_MUTEQ) return cos_cdesc(CC_MUTEQ, vp);
    switch (p->kind) {
    case CK_TRK: *vp = &TSEL->p[id]; return track_desc(TSEL, id);
    case CK_GLO:
        *vp = &song.g[id];
        if (song.g[G_RTYPE] == 3 && (id == G_RPRE || id == G_RDAMP))   /* (params.c: RESO's PITCH, FOLW in their slots; */
            return id == G_RPRE ? &RESO_PITCH : &RESO_FOLW;           /* SHIMMER / CLOUD's AIR for PRE) */
        if (id == G_RWIDE && song.g[G_RTYPE] == 6)
            return &REV_EARLY;
        if (id == G_RPRE)
            return song.g[G_RTYPE] == 2 || song.g[G_RTYPE] == 4 ? &REV_AIR : &REV_PRE;
        return &GP[id];
    case CK_AC: return ac_desc(id, vp);
    case CK_DB: return db_desc(id, vp);
    case CK_BL: return bl_desc(id, vp);
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
static int cos_on_cnd(void)                             /* (0.14.0) TRIG > COND shown: held steps' knobs are its own */
{
    const cpg_t *p = cos_page();
    return cz.mode == CM_TRIG && p->kind == CK_COS && p->id[0] == CC_TCOND;
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
/* HOME held: the tempo's button (as a Syntakt's TEMPO): OCT- / OCT+ nudge, a knob's step on BPM a tenth */
static int cos_home_held(void) { return (int)((fm1_in.buttons >> panel.btn[B_HOME]) & 1u); }
/* "128.4" */
static void cos_bpm_text(char *b, int32_t v)            /* tenths: 1284 -> 128.4 */
{
    fmt_int(b, v / 10);
    b += str_len(b);
    b[0] = '.';
    b[1] = (char)('0' + v % 10);
    b[2] = 0;
}
static void cos_bpm100_text(char *b, int32_t v)         /* (0.10.1) hundredths: 13016 -> 130.16, 12840 -> 128.4, 12000 -> 120 */
{
    fmt_int(b, v / 100);
    if (!(v % 100)) return;
    b += str_len(b);
    b[0] = '.';
    b[1] = (char)('0' + v / 10 % 10);
    b[2] = v % 10 ? (char)('0' + v % 10) : 0;
    b[3] = 0;
}
static void cos_bpm_set(int32_t v)                      /* (0.10.1) the tempo in hundredths, 40.00 .. 240.00 */
{
    v = clamp(v, 4000, 24000);
    song.g[G_BPM] = (int16_t)(v / 100);
    bpm_frac = (uint8_t)(v % 100);
}
/* (external clock) the tempo it plays at, BPM x 10 for the header: midi_clock.c measures a quarter beat in ms, so
 * each reading is +-1 % (128 shows 127 .. 129); smoothed here (1/32 a frame), a jump of 4 % or more taken at once,
 * and the shown value moved only by 3 tenths or more. 0: the FM-1's own tempo */
static uint32_t cz_ext_q, cz_ext_shown;
static uint32_t cos_ext10(void)
{
    uint32_t b = song.g[G_CLOCK] ? midi_beat_samples : 0u, t;
    if (!b) { cz_ext_q = cz_ext_shown = 0; return 0; }
    t = (600u * (uint32_t)FS + b / 2u) / b;              /* (tenths: 60 s x 10 / a beat) */
    if (!cz_ext_q || t * 16u > cz_ext_q + t * 16u / 25u || t * 16u + t * 16u / 25u < cz_ext_q) cz_ext_q = t << 4;   /* (4 %:
                                                         * a reading's own error is up to ~1.5 % at 180 BPM) */
    else cz_ext_q = (uint32_t)((int32_t)cz_ext_q + (((int32_t)(t << 4) - (int32_t)cz_ext_q) >> 5));
    t = (cz_ext_q + 8u) >> 4;
    if ((t + 2u) % 10u <= 4u) t = (t + 2u) / 10u * 10u;   /* (0.14.1: within 0.2 of a whole BPM, the whole: Ableton's 135
                                                         * reads 135.0, not 134.9) */
    if (!cz_ext_shown || t >= cz_ext_shown + 3u || t + 3u <= cz_ext_shown) cz_ext_shown = t;
    return cz_ext_shown;
}
static int cos_follows(void) { return song.g[G_CLOCK] && midi_beat_samples; }   /* (an external clock leads now) */

/* HOME held + white 16: TAP TEMPO. The last eight taps' gaps averaged (a gap over 2 s starts again), BPM in tenths */
static struct { uint32_t t[8]; uint8_t n; } cz_tap;
static __attribute__((noinline)) void cos_tap(uint32_t now)
{
    uint32_t i;
    if (cos_follows()) { cos_say("EXT CLOCK LEADS"); return; }   /* (the clock's tempo, not a tap's) */
    if (cz_tap.n && now - cz_tap.t[cz_tap.n - 1u] > 2000u) cz_tap.n = 0;
    if (cz_tap.n == 8u) { for (i = 0; i < 7u; i++) cz_tap.t[i] = cz_tap.t[i + 1u]; cz_tap.n = 7; }
    cz_tap.t[cz_tap.n++] = now;
    if (cz_tap.n >= 2u) {
        uint32_t gap = (cz_tap.t[cz_tap.n - 1u] - cz_tap.t[0]) / (cz_tap.n - 1u);
        int32_t v = gap ? clamp((int32_t)((600000u + gap / 2u) / gap), 400, 2400) : 1200;
        char b[16];
        cos_bpm_set(v * 10);                             /* (tenths: a tap's) */
        str_cpy(b, "TAP ", 5);
        cos_bpm_text(b + 4, v);
        cos_say(b);
    } else cos_say("TAP AGAIN");
}
static void cos_turn(uint32_t k, int32_t s)
{
    const cpg_t *p = cos_page();
    int16_t *vp;
    const param_desc_t *d = cos_knob(p, k, &vp);
    int32_t v;
    if (!d || !vp || d->max == d->min) return;
    if (cz.mode == CM_SRC || cz.mode == CM_AMP || cz.mode == CM_FX) { cz.lk_mode = cz.mode; cz.lk_tab = cz.tab[cz.mode]; }
    if (cos_home_held()) cz.home_used = 1;               /* (HOME + a knob: no page turn when let go) */
    if ((p->id[k] == ID_BPM || p->id[k] == ID_BFINE) && cos_follows()) {   /* (an external clock leads: its tempo) */
        cos_say("EXT CLOCK LEADS");
        return;
    }
    if (p->id[k] == ID_BPM) {                            /* BPM: a whole one a step (accelerated), the hundredths kept; HOME
                                                         * held: a tenth */
        cos_bpm_set(*vp + (cos_home_held() ? (s > 0 ? 10 : -10) : 100 * accel(EN_K1 + k, s, 200)));
        cz.hot = (uint8_t)(k + 1u);
        cz.hot_ms = fm1_ms;
        return;
    }
    if (p->id[k] == ID_BFINE) {                          /* (0.10.1) FINE: .00 .. .99, a hundredth a step (accelerated); the
                                                         * whole BPM stays (no carry: 130.99 stays 130.99) */
        bpm_frac = (uint8_t)clamp((int32_t)(bpm_frac % 100u) + accel(EN_K1 + k, s, 200), 0, 99);
        cz.hot = (uint8_t)(k + 1u);
        cz.hot_ms = fm1_ms;
        return;
    }
    if (d->names == N_DTIME)                             /* (the delay's TIME: by length, cos_dt_rank) */
        v = d->min + cos_dt_index(clamp(cos_dt_rank(*vp - d->min) + accel(EN_K1 + k, s, 0), 0, d->max - d->min), s);
    else
        v = param_turn(d, *vp, accel(EN_K1 + k, s, d->fmt == F_ENUM ? 0 : d->max - d->min));
    cz.hot = (uint8_t)(k + 1u);
    cz.hot_ms = fm1_ms;
    if (p->id[k] == ID_OCT) cos_cput(CC_OCT, v);
    else if (p->id[k] == ID_MUTEQ) cos_cput(CC_MUTEQ, v);
    else if (p->kind == CK_AC) ac_put(p->id[k], v);
    else if (p->kind == CK_DB) db_put(p->id[k], v);
    else if (p->kind == CK_BL) bl_put(p->id[k], v);
    else if (p->kind == CK_EU) eu_put(p->id[k], v);
    else if (p->kind == CK_COS) cos_cput(p->id[k], v);
    else if (p->kind == CK_GLO && p->id[k] == G_DJF) motion_djf_set(TSEL, (int16_t)v);   /* (REC: recorded too) */
    else if (p->kind == CK_TRK && vp == &TSEL->p[p->id[k]]) {   /* (a track's: REC armed and playing, recorded at the
                                                         * step it falls on, as JIANT's AUTOMATION; else the new base) */
        *vp = (int16_t)v;
        (void)motion_capture(TSEL, p->id[k], (int16_t)v);
    }
    else *vp = (int16_t)v;
    if (cos_page() != p) {                               /* (the turn changed the mode's pages: DUB on a MONO bass makes it
                                                         * POLY, its GEN pages DUB first. The same page stays under the knob) */
        const cpg_t *q;
        uint32_t n, i;
        cos_pages(cz.mode, &q, &n);
        for (i = 0; i < n; i++)
            if (str_eq(q[i].tab, p->tab) && q[i].kind == p->kind) { cz.tab[cz.mode] = (uint8_t)i; break; }
    }
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
static uint64_t cos_held(void);
/* (a synth on SEQ) steps held + PRESETS: their notes up or down the track's scale (SCL's key, ACID 3's SCALE; an
 * empty step becomes the root first), C0 .. C7; the first one's note on the top line */
static __attribute__((noinline)) void cos_step_note(int32_t d)
{
    track_t *t = TSEL;
    uint64_t h = cos_held();
    uint32_t i, first = NSTEP;
    char b[24];
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &t->step[i];
        if (!((h >> i) & 1u)) continue;
        if (s->time != ST_NOTE || !s->n) {
            step_clear(s);
            s->time = ST_NOTE; s->n = 1; s->vel = 100;
            s->note[0] = (uint8_t)(36 + (t->p[P_ROOT] % 12 + 12) % 12);
        } else
            s->note[0] = (uint8_t)clamp(scale_steps(t, s->note[0], d), 12, 96);
        if (first == NSTEP) first = i;
    }
    if (first == NSTEP) return;
    cz.lk_used = 1;                                      /* (the step's key let go: no trig) */
    str_cpy(b, "STEP ", 6);
    fmt_int(b + 5, (int32_t)first + 1);
    str_cpy(b + str_len(b), ": ", 3);
    note_name(b + str_len(b), t->step[first].note[0]);
    cos_say(b);
}
/* (a synth on SEQ) STEP INPUT, the Syntakt's step recording, on SEQ's STEP page: the keys are a keyboard, each one
 * played writes its note into the cursor's step (as it sounds: seq.c kb_map, OCT-/+ the octave) and the cursor moves on;
 * REC writes a rest and moves on, REC with a key held a tie (the note before goes on); PRESETS moves the cursor. The
 * steps' locks stay */
static void cos_si_go(int32_t d)
{
    uint32_t len = (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP);
    cz.si_pos = (uint8_t)(((int32_t)cz.si_pos % (int32_t)len + d + (int32_t)len) % (int32_t)len);
    cz.bank = (uint8_t)(cz.si_pos / 16u);
}
static void cos_si_said(const char *what)                /* "STEP 5: D2", "STEP 6: REST" */
{
    char b[24];
    str_cpy(b, "STEP ", 6);
    fmt_int(b + 5, (int32_t)cz.si_pos + 1);
    str_cpy(b + str_len(b), ": ", 3);
    str_cpy(b + str_len(b), what, 8);
    cos_say(b);
}
static __attribute__((noinline)) void cos_si_key(uint32_t k)
{
    track_t *t = TSEL;
    step_t *s = &t->step[cz.si_pos % NSTEP];
    uint32_t n = kb_map(t, k);
    char nm[8];
    if (n == KB_SILENT || n > 127u) return;
    if (s->time != ST_NOTE) step_clear(s);
    s->time = ST_NOTE; s->n = 1; s->note[0] = (uint8_t)n; s->vel = 100;
    s->flags &= (uint8_t)~(SF_ACCENT | SF_SLIDE);
    note_name(nm, n);
    cos_si_said(nm);
    cos_si_go(1);
}
static __attribute__((noinline)) void cos_si_rest(int tie)
{
    track_t *t = TSEL;
    step_t *s = &t->step[cz.si_pos % NSTEP];
    step_clear(s);
    if (tie) s->time = ST_TIE;
    cos_si_said(tie ? "TIE" : "REST");
    cos_si_go(1);
}
/* (a synth on SEQ) steps held + black 1 / 2: ACCENT / SLIDE on or off (the first one's decides; a rest: nothing) */
static __attribute__((noinline)) void cos_step_flag(uint32_t f)
{
    track_t *t = TSEL;
    uint64_t h = cos_held();
    uint32_t i, on = 2u;
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &t->step[i];
        if (!((h >> i) & 1u) || s->time != ST_NOTE || !s->n) continue;
        if (on == 2u) on = !(s->flags & f);
        s->flags = (uint8_t)(on ? s->flags | f : s->flags & ~f);
    }
    cz.lk_used = 1;
    cos_say(on == 2u ? "A REST: NO NOTE" : f == SF_ACCENT ? (on ? "ACCENT ON" : "ACCENT OFF") : (on ? "SLIDE ON" : "SLIDE OFF"));
}

/* (0.12.4) a step's LENGTH (GG: "how can I make it longer and shorter"): steps held + black 3 shorter, black 4 longer.
 * In 16ths of a step, the step's own gate (step_t ctime's LEN, seq.c seq_step: 1/16 .. 15/16), then whole steps: the
 * steps after it TIEs (only over rests and ties: a note there stops it). 0: none set, the track's GATE. The value: n
 * sixteenths (1..15) or 16 * steps (32, 48, .. 256: 2 .. 16 steps) */
static const uint8_t COS_SLEN[] = {1, 2, 3, 4, 6, 8, 10, 12, 15};
static uint32_t cos_slen_get(const track_t *t, uint32_t i, uint32_t len)
{
    uint32_t k = 1;
    while (k < len && k < 16u && t->step[(i + k) % len].time == ST_TIE) k++;
    if (k > 1u) return 16u * k;
    return CT_LEN(&t->step[i % NSTEP]);
}
/* step i to v (as cos_slen_get's); 0 if a note in the way cut it short */
static int cos_slen_set(track_t *t, uint32_t i, uint32_t v, uint32_t len)
{
    step_t *s = &t->step[i % NSTEP];
    uint32_t k, want = v >= 32u ? v / 16u : 1u, ok = 1;
    for (k = 1; k < len && k < 16u && t->step[(i + k) % len].time == ST_TIE; k++)
        step_clear(&t->step[(i + k) % len]);             /* (its ties, gone: laid again below) */
    for (k = 1; k < want && k < len; k++) {
        step_t *n = &t->step[(i + k) % len];
        if (n->time == ST_NOTE && n->n) { ok = 0; break; }   /* (a note there: as far as it) */
        step_clear(n);
        n->time = ST_TIE;
    }
    s->ctime = (uint8_t)((s->ctime & ~0x78u) | (v >= 32u ? 0u : (v & 15u) << 3));   /* (tied: the last step's GATE ends it) */
    return (int)ok;
}
static __attribute__((noinline)) void cos_step_len(int up)
{
    track_t *t = TSEL;
    uint64_t h = cos_held();
    uint32_t i, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), v = ~0u, ok = 1;
    char b[24];
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &t->step[i];
        if (!((h >> i) & 1u) || s->time != ST_NOTE || !s->n) continue;
        if (v == ~0u) {                                  /* (the first held note decides, the rest follow it) */
            uint32_t c = cos_slen_get(t, i, len), j;
            if (!c) c = (uint32_t)clamp(t->p[P_SGATE], 1, 127) / 8u;   /* (none set: from the track's GATE) */
            if (c >= 16u) v = up ? (c / 16u < 16u ? c + 16u : c) : (c > 32u ? c - 16u : COS_SLEN[NELEM(COS_SLEN) - 1u]);
            else if (up) {
                for (j = 0; j < NELEM(COS_SLEN) && COS_SLEN[j] <= c; j++) {}
                v = j < NELEM(COS_SLEN) ? COS_SLEN[j] : 32u;
            } else {
                for (j = NELEM(COS_SLEN); j > 0u && COS_SLEN[j - 1u] >= c; j--) {}
                v = j ? COS_SLEN[j - 1u] : COS_SLEN[0];
            }
        }
        ok &= (uint32_t)cos_slen_set(t, i, v, len);
    }
    cz.lk_used = 1;
    if (v == ~0u) { cos_say("A REST: NO NOTE"); return; }
    str_cpy(b, "LENGTH ", 8);
    if (v >= 32u) { fmt_int(b + 7, (int32_t)(v / 16u)); str_cpy(b + str_len(b), " STEPS", 8); }
    else { fmt_int(b + 7, (int32_t)v); str_cpy(b + str_len(b), "/16", 4); }
    if (!ok) str_cpy(b, "A NOTE IN THE WAY", 18);
    cos_say(b);
}

/* ACID's track: the first one on the ACID engine (the 303); none: the last live one that is neither a kit nor VOICE
 * becomes ACID's TB CLASSIC (an UNDO point first; REC on GEN undoes it) */
static __attribute__((noinline)) uint32_t cos_acid_trk(uint32_t *made)
{
    uint32_t i, k = trk_live;
    char b[24];
    for (i = 0; i < trk_live; i++)
        if (trk_eng(&trk[i]) == ENGI_ACID) return i;
    for (i = trk_live; i-- > 0u;)
        if (!cos_hits(&trk[i]) && trk_eng(&trk[i]) != ENGI_VOICE) { k = i; break; }
    if (k >= trk_live) k = trk_live - 1u;
    undo_mark();
    set_engine_of(&trk[k], ENGI_ACID);
    apply_preset_to(&trk[k], 0);
    str_cpy(b, "T", 2); b[1] = (char)('1' + k); b[2] = 0;
    str_cpy(b + 2, " NOW ACID 303", 14);
    cos_say(b);
    *made = 1;
    return k;
}
/* (0.10.0) the drums' sounds in and out, one at a time (HOME + white 1..8, the phone's CC 91..98): 0 the KICK (the
 * first KICK track's mute; no KICK track: the kit's kick), 1..7 the kit's SNARE CLAP HATCL HATOP TOM RIM BELL (each
 * sound's own mute, eng_drum.c DXM_LANE). With MUTE Q on the beat or the bar, as the tracks' mutes (cos_live.c) */
/* (0.10.3) HOME > LIVE's NORMAL: what the set was before the first move (the DJ FILTER, the global sends, the delay's
 * FEEDBACK, the tracks' mutes, the drums' sounds), taken by that move; OCT- on LIVE goes back to it (on the beat / bar as
 * MUTE Q says: the mutes, the sounds and the knobs at once, a drop), OCT+ makes now the normal */
static struct { uint8_t ok, mute, lane, snd[2]; int16_t djf, fb; } cos_nrm;
static uint8_t cos_mute_eff(void)                        /* the tracks' mutes as they will be (a queued flip counted) */
{
    uint32_t k, m = 0;
    for (k = 0; k < NTRK; k++) m |= (uint32_t)((trk[k].p[P_MUTE] != 0) ^ ((live_qmute >> k) & 1u)) << k;
    return (uint8_t)m;
}
static uint8_t cos_lane_eff(void) { return (uint8_t)(((dx_mute >> 8) & 0xFFu) ^ live_qlane); }
static void cos_snap(void)
{
    if (cos_nrm.ok) return;
    cos_nrm.ok = 1;
    cos_nrm.mute = cos_mute_eff();
    cos_nrm.lane = cos_lane_eff();
    cos_nrm.snd[0] = lv_snd[0];
    cos_nrm.snd[1] = lv_snd[1];
    cos_nrm.djf = song.g[G_DJF];
    cos_nrm.fb = song.g[G_DFDBK];
}
/* (0.12.5, GG: "the current tail shouldn't get lost ... like a normal send") BACK TO NORMAL lets the effects ring out:
 * the global sends go back over ~100 ms (no more signal in, no click), but what is already in the delay and the reverb
 * plays on; the delay's FEEDBACK glides back over two bars (at once, a high FEEDBACK let down cut the echoes short).
 * A knob turned meanwhile takes over (its ramp ends). The DJ FILTER: at once (the drop) */
static struct { uint8_t on, snd0[2], snd1[2]; int16_t fb0, fb1, fb_set, snd_set[2]; uint32_t t0, fb_ms; } cos_rmp;
static void cos_back_knobs(void)                         /* the knobs to NORMAL (the main loop's) */
{
    uint32_t bpm = (uint32_t)clamp(song.g[G_BPM], 40, 300);
    if (song.g[G_DJF] != cos_nrm.djf) motion_djf_set(TSEL, cos_nrm.djf);
    cos_rmp.on = 1;
    cos_rmp.t0 = fm1_ms;
    cos_rmp.fb0 = song.g[G_DFDBK];
    cos_rmp.fb1 = cos_nrm.fb;
    cos_rmp.fb_set = song.g[G_DFDBK];
    cos_rmp.fb_ms = cos_rmp.fb0 > cos_rmp.fb1 && song.playing ? 2u * 4u * 60000u / bpm : 0u;   /* (two bars; down only:
                                                         * a FEEDBACK lower than normal comes back up at once) */
    cos_rmp.snd0[0] = lv_snd[0]; cos_rmp.snd0[1] = lv_snd[1];
    cos_rmp.snd1[0] = cos_nrm.snd[0]; cos_rmp.snd1[1] = cos_nrm.snd[1];
    cos_rmp.snd_set[0] = lv_snd[0]; cos_rmp.snd_set[1] = lv_snd[1];
    cos_nrm.ok = 0;
}
static void cos_rmp_poll(void)                           /* (each frame) BACK TO NORMAL's glides */
{
    uint32_t t = fm1_ms - cos_rmp.t0, i, on = 0;
    if (!cos_rmp.on) return;
    if (cos_rmp.fb_set >= 0) {
        if (song.g[G_DFDBK] != cos_rmp.fb_set) cos_rmp.fb_set = -1;   /* (FEEDBACK turned: its own now) */
        else {
            int32_t v = t >= cos_rmp.fb_ms ? cos_rmp.fb1 : cos_rmp.fb0 + ((cos_rmp.fb1 - cos_rmp.fb0) * (int32_t)t) / (int32_t)cos_rmp.fb_ms;   /* (32 bits: 127 x 12 s) */
            song.g[G_DFDBK] = (int16_t)v;
            cos_rmp.fb_set = (int16_t)v;
            on |= t < cos_rmp.fb_ms;
        }
    }
    for (i = 0; i < 2u; i++) {
        if (cos_rmp.snd_set[i] < 0) continue;
        if (lv_snd[i] != (uint8_t)cos_rmp.snd_set[i]) { cos_rmp.snd_set[i] = -1; continue; }   /* (turned: its own) */
        {
            int32_t a = cos_rmp.snd0[i], b = cos_rmp.snd1[i], v = t >= 100u ? b : a + (b - a) * (int32_t)t / 100;
            lv_snd[i] = (uint8_t)v;
            cos_rmp.snd_set[i] = (int16_t)v;
            on |= t < 100u;
        }
    }
    cos_rmp.on = (uint8_t)on;
}
static __attribute__((noinline)) void cos_back(int up)
{
    uint8_t dm, dl;
    uint32_t k;
    if (up) { cos_nrm.ok = 0; cos_snap(); cos_say("THIS IS NORMAL"); return; }
    if (!cos_nrm.ok) { cos_say("ALREADY NORMAL"); return; }
    dm = (uint8_t)(cos_mute_eff() ^ cos_nrm.mute);
    dl = (uint8_t)(cos_lane_eff() ^ cos_nrm.lane);
    if (!song.playing || !cos_mq) {                      /* (stopped, or MUTE Q NOW: at once) */
        fm1_irq_off();
        live_qmute = 0;
        live_qlane = 0;
        live_qback = 0;
        fm1_irq_on();
        for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = (int16_t)((cos_nrm.mute >> k) & 1u);
        dx_mute_set((uint16_t)((dx_mute & ~0xFF00u) | (uint32_t)cos_nrm.lane << 8));
        cos_back_knobs();
        cos_say("BACK TO NORMAL");
        return;
    }
    fm1_irq_off();                                       /* (on the beat / bar: the ISR flips them whole, cos_live.c) */
    live_qmute ^= dm;
    live_qlane ^= dl;
    live_qback = 1;
    fm1_irq_on();
    cos_say(cos_mq == 1u ? "NORMAL ON THE BEAT" : "NORMAL ON THE BAR");
}
static void cos_back_poll(void)                          /* (each frame) the knobs back when the ISR says it is time */
{
    cos_rmp_poll();
    if (live_qback == 2u) {
        live_qback = 0;
        cos_back_knobs();
        cos_say("BACK TO NORMAL");
    }
}
static const char *const DRUM_SND_N[9] = {"KICK", "SNARE", "CLAP", "HAT", "OPEN HAT", "TOM", "RIM", "BELL", "PERC"};
static int cos_kick_trk(void)                            /* the first live KICK track, -1 none */
{
    uint32_t k;
    for (k = 0; k < trk_live && k < NTRK; k++)
        if (trk_eng(&trk[k]) == ENGI_KICK) return (int)k;
    return -1;
}
static int cos_drum_out(uint32_t s)                      /* the sound out (muted), as it will be once a pending flip goes in;
                                                         * (0.14.1) 8: the PERC, the kit's first */
{
    int kt = s ? -1 : cos_kick_trk();
    if (s == 8u) s = 0;
    if (kt >= 0) return (trk[kt].p[P_MUTE] != 0) != (((live_qmute >> kt) & 1u) != 0u);
    return ((dx_mute & DXM_LANE(s & 7u)) != 0u) != (((live_qlane >> (s & 7u)) & 1u) != 0u);
}
static void cos_drum_flip(uint32_t s)
{
    uint32_t nm = s > 8u ? 0u : s;
    int kt, out;
    char b[24];
    if (s == 8u && !dx_k0p) return;                      /* (0.14.1: PERC, with a KICK track) */
    out = !cos_drum_out(s);
    kt = s == 8u || (s & 7u) ? -1 : cos_kick_trk();
    s &= 7u;
    if (nm == 8u) s = 0;
    cos_snap();
    if (kt >= 0) {
        if (!cos_mq) trk[kt].p[P_MUTE] = (int16_t)!trk[kt].p[P_MUTE];
        else { fm1_irq_off(); live_qmute ^= (uint8_t)(1u << kt); fm1_irq_on(); }
    } else if (!cos_mq) {
        dx_mute_set(dx_mute ^ DXM_LANE(s));
    } else {
        fm1_irq_off(); live_qlane ^= (uint8_t)(1u << s); fm1_irq_on();
    }
    str_cpy(b, DRUM_SND_N[nm], sizeof b);
    str_cpy(b + str_len(b), out ? " OUT" : " IN", 5);
    if (cos_mq && song.playing) str_cpy(b + str_len(b), cos_mq == 1u ? " BEAT" : " BAR", 6);
    cos_say(b);
}
static void cos_drum_set(uint32_t s, int out) { if (cos_drum_out(s > 8u ? 0u : s) != !!out) cos_drum_flip(s); }
/* (0.10.0) a KICK track's pattern: KICK_PAT's n into steps 1..16 (the kick's C, the first beat accented), LEN 16 */
static void cos_kick_pat(track_t *t, uint32_t n)
{
    uint8_t note[16], fl[16];
    uint32_t i;
    n %= KICK_NPAT;
    for (i = 0; i < 16u; i++) {
        note[i] = (uint8_t)((KICK_PAT[n] >> i) & 1u ? 36u : 0u);
        fl[i] = (uint8_t)(i == 0u ? SF_ACCENT : 0u);
    }
    undo_mark();
    load_pat16(t, note, fl);
    kick_pat_cur[(uint32_t)(t - trk) % NTRK] = (uint8_t)n;
    cos_say(KICK_PAT_N[n]);
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
    if (trk_eng(TSEL) == ENGI_KICK) {                    /* (0.10.0) a KICK: FOUR ON THE FLOOR if empty, PATTERN on KNOB 1 */
        if (cos_empty(TSEL) && !chain_busy()) cos_kick_pat(TSEL, 0);
        return;
    }
    if (cos_empty(TSEL) && !chain_busy()) {
        if (trk_eng(TSEL) == ENGI_ACID || TSEL->p[P_VOICE] != V_POLY) ac_throw(song.sel, 1);   /* (ACID, a MONO bass: a line) */
        else {                                           /* (a POLY synth, the pad: dub chord stabs, on DUB STAB; VOICE keeps
                                                         * its singer) */
            undo_mark();
            if (trk_eng(TSEL) != ENGI_VOICE) db_sound(song.sel);
            db_throw(song.sel, 1);
            if (!made) cos_say("DUB CHORDS");
        }
    }
    if (made) ui.msg_t = 0;                              /* (what it said: the track's new sound, not NEW LINE) */
}

/* (cursed-os) a name for the music: two words (12 letters at most), drawn from NAME's turn and this power-on's seed (the
 * FM-1 has no clock that knows the date, so no date in it); a music with no name gets one as it is saved */
static void cos_name_of(uint32_t n, char *b)
{
    static const char *const A[] = {"DEEP", "DARK", "VOID", "IRON", "ACID", "LUNAR", "NEON", "COLD", "HAZE", "DUST",
                                    "GHOST", "BLACK", "INNER", "SLOW", "LOST", "NIGHT", "SOLAR", "STONE", "MIST", "FERAL",
                                    "ASH", "OCEAN", "SILK", "DREAM", "PALE", "WILD", "STILL", "ECHO"};
    static const char *const B[] = {"RITUAL", "TUNNEL", "SIGNAL", "ORBIT", "PULSE", "TRANCE", "MIRROR", "SMOKE", "STATIC",
                                    "TEMPLE", "VORTEX", "CIRCLE", "DRIFT", "SPIRAL", "GARDEN", "SHADOW", "MOTION", "RIVER",
                                    "ENGINE", "FOREST", "SYSTEM", "RHYTHM", "CANYON", "CHAPEL", "COMET", "LOOP"};
    uint32_t h;
    if (!cz.nm_seed) cz.nm_seed = (fm1_ms * 2654435761u) | 1u;
    h = (n + 1u) * 2654435761u ^ cz.nm_seed;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
    str_cpy(b, A[(h >> 8) % NELEM(A)], 13);
    str_cpy(b + str_len(b), " ", 13 - str_len(b));
    str_cpy(b + str_len(b), B[(h >> 20) % NELEM(B)], 13 - str_len(b));
}
static void cos_name_pick(void) { cos_name_of(cz.nm_n, proj_name_buf()); }
static void cos_name_auto(void) { if (!proj_name_buf()[0]) cos_name_of(cz.nm_n, proj_name_buf()); }

/* SAVE: the four slots (JIANT's projects, SECT A..D): their names read again on entering and after a save or a load.
 * OCT+ saves the music to the slot picked (stopped: a flash erase), OCT- loads it */
static void cos_proj_scan(void)
{
    uint32_t i;
    for (i = 0; i < 4u; i++) proj_slot_name(i, cz.pn[i]);
    cz.pn_ok = 1;
}
static int section_jump(uint32_t s);                     /* (song_main.c) in on the next bar (stopped: now) */
static int song_go_live(uint32_t n);                     /* .. the set list: song n, its A on the next bar */
static void section_store(uint32_t s);                   /* .. the music now into it (RAM; flash once stopped) */
/* a section asked for here (HOME's keys, SAVE's OCT-): in on the next bar, and with LINK's PC OUT its number sent */
static void cos_section(uint32_t s)
{
    if (!section_jump(s) && cos_pc_out && cos_pc_ch) cos_pc_out_req = (uint8_t)(s % 4u + 1u);
}
/* (0.10.2) SAVE > SOUNDS: OCT+ the selected track's sound into the slot (a used one: OCT+ again within 3 s), OCT- the
 * slot's sound into the track; REC twice: the slot erased. Flash: stopped only (up_put, STOP TO SAVE) */
static int up_used(uint32_t k);
static void up_ui_named(uint32_t op, uint32_t k, const char *name);
static void sound_name(const track_t *t, char *b);
static __attribute__((noinline)) void cos_snd_oct(int up)
{
    uint32_t k = cz.uslot % (UP_SLOTS - 1u);
    if (!up) { up_ui_named(0, k, 0); cz.snd_ms = 0; return; }
    if (up_used(k) && (!cz.snd_ms || fm1_ms - cz.snd_ms > 3000u)) {
        cz.snd_ms = fm1_ms ? fm1_ms : 1u;
        cos_say("OCT+ AGAIN: REPLACE");
        return;
    }
    {
        char n[24];
        sound_name(TSEL, n);
        n[12] = 0;
        cz.snd_ms = 0;
        up_ui_named(2, k, n);
    }
}
static __attribute__((noinline)) void cos_snd_erase(void)
{
    uint32_t k = cz.uslot % (UP_SLOTS - 1u);
    if (!up_used(k)) { cos_say("EMPTY SLOT"); return; }
    if (!cz.snd_ms || fm1_ms - cz.snd_ms > 3000u) { cz.snd_ms = fm1_ms ? fm1_ms : 1u; cos_say("REC AGAIN: ERASE"); return; }
    cz.snd_ms = 0;
    up_ui_named(1, k, 0);
}
/* (0.13.0) SAVE > KITS: OCT+ the kit into the slot (a used one: again to replace; stopped only), OCT- the slot's kit in
 * (playing too), REC twice erases */
static __attribute__((noinline)) void cos_kit_oct(int up)
{
    uint32_t k = cz.kslot % 16u;
    if (!up) {
        cz.snd_ms = 0;
        cos_say(!kit_is_used(k) ? "EMPTY SLOT" : kit_load(k) ? "NOT A KIT" : "KIT LOADED");
        return;
    }
    if (song.playing) { cos_say("STOP TO SAVE"); return; }
    if (kit_is_used(k) && (!cz.snd_ms || fm1_ms - cz.snd_ms > 3000u)) {
        cz.snd_ms = fm1_ms ? fm1_ms : 1u;
        cos_say("OCT+ AGAIN: REPLACE");
        return;
    }
    {
        char n[24];
        uint32_t i, t;
        for (t = 0; t < NTRK && trk[t].engine != ENGI_DRUM; t++) {}
        if (t < NTRK) sound_name(&trk[t], n); else str_cpy(n, "KIT", 4);
        n[10] = 0;
        for (i = 0; n[i]; i++) if (n[i] >= 'a' && n[i] <= 'z') n[i] = (char)(n[i] - 32);
        cz.snd_ms = 0;
        cos_say(kit_save(k, n) ? "NOT SAVED" : "KIT SAVED");
    }
}
static __attribute__((noinline)) void cos_kit_erase(void)
{
    uint32_t k = cz.kslot % 16u;
    if (!kit_is_used(k)) { cos_say("EMPTY SLOT"); return; }
    if (song.playing) { cos_say("STOP TO ERASE"); return; }
    if (!cz.snd_ms || fm1_ms - cz.snd_ms > 3000u) { cz.snd_ms = fm1_ms ? fm1_ms : 1u; cos_say("REC AGAIN: ERASE"); return; }
    cz.snd_ms = 0;
    cos_say(kit_erase(k) ? "NOT ERASED" : "ERASED");
}
static void cos_qs_load(void);
static void cos_qs_press(void);
static __attribute__((noinline)) void cos_proj_oct(int up)
{
    if (cz.tab[CM_PROJ] == 3u) { if (up) cos_qs_press(); else cos_say("OCT+: LOAD IT"); return; }   /* (0.18.0) START */
    if (cz.tab[CM_PROJ] == 2u) { cos_kit_oct(up); return; }
    if (cz.tab[CM_PROJ] == 1u) { cos_snd_oct(up); return; }
    if (up) cos_name_auto();                             /* (a music with no name: one drawn for it) */
    if (up && song.playing) section_store(cz.slot);      /* (playing: kept now, written once stopped) */
    else if (up) project_save(cz.slot);
    else if (!cz.pn[cz.slot][0]) cos_say("EMPTY SLOT");
    else if (song.playing) cos_section(cz.slot);         /* (playing: in on the next bar) */
    else { project_load(cz.slot); ui.msg_t = 0; cos_say("LOADED"); }
    cos_proj_scan();
}

/* (0.9.3) SAVE > REC twice within 3 s: a NEW PROJECT, as at power-on (project.c system_init: the power-on sounds, no
 * steps, no automation, the mix and the effects at their defaults, no name; the saved slots and the setup kept). The
 * transport stops; GLO held + G4 (UNDO) brings the music back */
static void undo_mark(void);                             /* (project.c) */
static int undo_gen(void);
static int undo_do(void);
static __attribute__((noinline)) void cos_proj_new(void)
{
    if (chain_busy()) { cos_say("STOP THE SONG FIRST"); return; }
    if (!cz.new_ms || fm1_ms - cz.new_ms > 3000u) { cz.new_ms = fm1_ms ? fm1_ms : 1u; cos_say("REC AGAIN: NEW PROJECT"); return; }
    cz.new_ms = 0;
    undo_mark();
    if (song.playing) transport_req = 2;
    system_init();
    cos_say("NEW PROJECT");
    cos_proj_scan();
}
/* (0.18.0, GG: "quick starts") SAVE > START: OCT+ a jam in one press, the FM-1 as extra voices to a Syntakt or on its
 * own; again: another of the same STYLE (new seeds). Each track its sound and a generated line, the ones not needed
 * muted; DRUMS: FULL (the KICK track 4/4 and the kit's groove), PERC (the kit's only: a Syntakt's kick), NONE (the
 * default PERC with SYNTAKT ON, else FULL). The tempo and the key stay. An UNDO point first (REC on the page brings
 * the music before back: undo_do). The transport plays on.
 * (0.18.1, GG: "complete loops with all the elements") ELEMENTS ALL (the default): every track a part, a whole loop
 * (the kit's hats, claps and percussion as the style's genre plays them, the kick, an acid line, a sub bass under it,
 * chords or an atmosphere, a bleep motif); PICKED: the style's few, as 0.18.0. BARS 1, 2, 4: the drums, the acid, the
 * bass and the chords that long (the drums with their genre's variations; the bleeps' motifs their own length).
 *   ALL:  HYPNOTIC   kit, kick, T3 ACID rolling, T4 SUB BASS, T5 ATMOS, T6 SONAR PING 5
 *         ACID       kit, kick, T3 ACID busy, T4 SUB BASS, T5 DUB STABS, T6 R2 BLIPS 3
 *         DUB TECHNO kit, kick, T3 SUB BASS, T4 DUB STABS, T5 ATMOS, T6 GLASS PING 6
 *         BLEEPS     kit, kick, T3 SUB BASS, T4 WOOD BLOCK 3, T5 ATMOS, T6 WATER DROP 5
 *   PICKED: HYPNOTIC T3 ACID, T5 ATMOS, T6 SONAR; ACID T3 ACID, T6 R2 BLIPS; DUB TECHNO T4 STABS, T5 ATMOS, T6 GLASS;
 *         BLEEPS T4 WOOD, T5 ATMOS, T6 WATER DROP */
static uint32_t qs_drm_get(void)
{
    if (qs_drm) return (uint32_t)(qs_drm - 1u) % 3u;
    return (favorites.factory[8][31] & 1u) && song.g[G_CLOCK] == 2 ? 1u : 0u;
}
static void qs_sound(uint32_t k, uint32_t e, const char *nm)
{
    const engine_t *en = ENGINES[e];
    uint32_t i;
    set_engine_of(&trk[k], e);
    for (i = 0; i < en->npresets && !str_eq(en->presets[i].name, nm); i++) {}
    apply_preset_to(&trk[k], i < en->npresets ? i : 0u);
    trk[k].p[P_MUTE] = 0;
}
static void qs_bleep(uint32_t k, const char *nm, int32_t motif)
{
    qs_sound(k, ENGI_DUB, nm);
    track_select(k);
    bl_put(BK_MOTIF, motif);
    bl_oct(1, 0);
}
static void qs_chords(uint32_t k, uint32_t atmos, uint32_t bars)
{
    if (atmos) qs_sound(k, ENGI_DUB, "ATMOS DARK"); else qs_sound(k, ENGI_ANALOG, "DUB STAB");
    track_select(k);
    db_put(DK_BARS, (int32_t)bars);
    if (atmos) { db_put(DK_DENS, 60); db_put(DK_MOVE, 1); db_put(DK_LEN, 21); }   /* (0.18.3: chords held a bar each, a
                                                         * second one in the progression; DB_LENN 21 = 1 BAR) */
    db_throw(k, 1);
}
/* an acid line (ACID's engine) or a sub bass (ANALOG's SUB BASS, the same generator: sparse, no slides) */
static void qs_line(uint32_t k, uint32_t sub, int32_t dens, uint32_t ctr, uint32_t bars)
{
    ac_t *a;
    if (sub) qs_sound(k, ENGI_ANALOG, "SUB BASS"); else qs_sound(k, ENGI_ACID, "");
    track_select(k);
    a = ac_of(k);
    a->ctr = (uint8_t)ctr;                               /* (the register: the acid above the sub) */
    a->steps = (uint8_t)(16u << bars);
    if (sub) { a->slide = 0; a->acc = 10; }
    ac_put(AK_DENS, dens);
    ac_oct(1, 0);
}
/* (0.18.2, GG: "happy accidents i can layer or use live") RANDOM: every part a role and a sound of its own, drawn: the
 * kit's genre any of GEN's, the KICK's sound and pattern (4/4 most often), T3 an acid line or a sub bass, T4 a sub
 * bass, an acid line above, stabs or a bleep, T5 an atmosphere, stabs or a bleep, T6 a bleep; each sound any of its
 * kind (ACID's eight, the basses, DUB's stabs, atmospheres and bleeps), the motifs 3 5 6 7 steps, the densities their
 * own. PICKED: some of T3 .. T5 muted. KEEP (white 1 .. 6 on the page): those tracks left as they are, so a loop
 * builds up layer by layer. Playing, a load comes in at the bar's end (cos_qs_tick), SYNTAKT ON with PAT BARS: at its
 * pattern's end */
static uint32_t qs_rs;
static uint32_t qs_r(uint32_t n) { qs_rs = qs_rs * 1664525u + 1013904223u; return n ? (qs_rs >> 8) % n : 0u; }
static uint32_t qs_pick(uint32_t e, int32_t type)       /* a preset of engine e (DUB: of TYPE type), at random */
{
    const engine_t *en = ENGINES[e];
    uint32_t i, c = 0, n;
    static const char *const BASS[4] = {"SUB BASS", "FAT BASS", "SQR BASS", "REESE"};
    if (e == ENGI_ANALOG) {
        const char *nm = BASS[qs_r(4)];
        for (i = 0; i < en->npresets && !str_eq(en->presets[i].name, nm); i++) {}
        return i < en->npresets ? i : 0u;
    }
    for (i = 0; i < en->npresets; i++) c += type < 0 || en->presets[i].e[0] == type;
    n = qs_r(c ? c : 1u);
    for (i = 0; i < en->npresets; i++)
        if ((type < 0 || en->presets[i].e[0] == type) && !n--) return i;
    return 0;
}
static void qs_sound_i(uint32_t k, uint32_t e, uint32_t pr)
{
    set_engine_of(&trk[k], e);
    apply_preset_to(&trk[k], pr);
    trk[k].p[P_MUTE] = 0;
}
/* a random part on track k: 0 acid, 1 sub bass, 2 stabs, 3 atmosphere, 4 bleep */
static void qs_rand_part(uint32_t k, uint32_t role, uint32_t bars)
{
    static const uint8_t MO[4] = {1, 2, 3, 4};           /* (BLEEP's MOTIF: 3 5 6 7) */
    ac_t *a;
    track_select(k);
    if (role <= 1u) {
        qs_sound_i(k, role ? ENGI_ANALOG : ENGI_ACID, qs_pick(role ? ENGI_ANALOG : ENGI_ACID, -1));
        a = ac_of(k);
        a->ctr = (uint8_t)(role ? 36u : 40u + qs_r(16));
        a->steps = (uint8_t)(16u << bars);
        a->slide = (uint8_t)(role ? 0u : 10u + qs_r(50));
        a->acc = (uint8_t)(role ? 10u : 15u + qs_r(50));
        ac_put(AK_DENS, role ? 20 + (int32_t)qs_r(25) : 35 + (int32_t)qs_r(50));
        ac_oct(1, 0);
    } else if (role <= 3u) {
        qs_sound_i(k, ENGI_DUB, qs_pick(ENGI_DUB, role == 3u ? 1 : 0));
        db_put(DK_BARS, (int32_t)bars);
        db_put(DK_SHAPE, (int32_t)qs_r(7));
        if (role == 3u) {                                /* (0.18.3: held 1/2, 1 or 2 bars, a progression drawn) */
            db_put(DK_DENS, 40 + (int32_t)qs_r(50)); db_put(DK_MOVE, (int32_t)qs_r(3)); db_put(DK_LEN, 20 + (int32_t)qs_r(3));
        } else { db_put(DK_DENS, 25 + (int32_t)qs_r(50)); db_put(DK_LEN, 1 + (int32_t)qs_r(9)); }   /* (10 .. 50 %) */
        db_throw(k, 1);
    } else {
        qs_sound_i(k, ENGI_DUB, qs_pick(ENGI_DUB, 2));
        bl_put(BK_DENS, 30 + (int32_t)qs_r(45));
        bl_put(BK_MOTIF, MO[qs_r(4)]);
        bl_oct(1, 0);
    }
}
static __attribute__((noinline)) void cos_qs_load(void)
{
    uint32_t k, sel = song.sel % NTRK, st = qs_sty % QS_N, dm = qs_drm_get(), gi, all = !qs_elm, bars = qs_bar % 3u;
    uint32_t kp = qs_keep;
    static const char *const GN[QS_N] = {"HYPNOTIC", "TECHNO", "DUB TECHNO", "HYPNOTIC", ""};
    static const char *const BL[QS_N] = {"SONAR PING", "R2 BLIPS", "GLASS PING", "WATER DROP", ""};
    static const uint8_t BM[QS_N] = {2, 1, 3, 2, 2};
    if (chain_busy()) { cos_say("STOP THE SONG FIRST"); return; }
    if ((kp & ((1u << NTRK) - 1u)) == (1u << NTRK) - 1u) { cos_say("ALL TRACKS KEPT"); return; }
    undo_mark();
    for (k = 0, qs_mute = 0; k < NTRK; k++) qs_mute |= (uint8_t)((trk[k].p[P_MUTE] != 0) << k);
    qs_have = 1;
    qs_rs ^= fm1_ms * 2654435761u + 0x9E3779B9u;
    ac_ms = db_ms = eu_ms = fm1_ms;                      /* (one UNDO point: this one, not the generators' own) */
    for (k = 0; k < NTRK; k++) {
        if ((kp >> k) & 1u) continue;                    /* (KEEP: as it is) */
        track_defaults_steps(&trk[k]);
        trk[k].p[P_SLEN] = 16;
        trk[k].p[P_MUTE] = 1;
    }
    /* drums: T2 the KICK (4/4; RANDOM: its sound and pattern drawn), T1 the kit's groove (GEN's drums of the genre) */
    if (!((kp >> 1) & 1u)) {
        if (st == QS_RAND) {
            static const uint8_t KP[8] = {0, 0, 0, 0, 1, 2, 3, 6};
            qs_sound_i(1, ENGI_KICK, qs_pick(ENGI_KICK, -1));
            cos_kick_pat(&trk[1], KP[qs_r(8)]);
        } else {
            qs_sound(1, ENGI_KICK, "TECHNO");
            cos_kick_pat(&trk[1], 0);
        }
        trk[1].p[P_MUTE] = (int16_t)(dm != 0u);
    }
    if (!(kp & 1u)) {
        if (st == QS_RAND) gi = qs_r(NELEM(GENRE));
        else for (gi = 0; gi < NELEM(GENRE) && !str_eq(GENRE[gi].name, GN[st]); gi++) {}
        gs.gi = (uint8_t)(gi % NELEM(GENRE));
        gs.g = &GENRE[gs.gi];
        gs.amt[GP_DRUMS] = (uint8_t)(all ? 55u + qs_r(st == QS_RAND ? 30 : 10) : 35u);
        gs.energy = all ? 65u : 50u;
        gs.complex = st == QS_RAND ? 20u + qs_r(60) : 40u;
        gp.seed = (uint16_t)(qs_r(10000));
        trk[0].p[P_MUTE] = (int16_t)(dm == 2u);
        trk[0].p[P_SLEN] = (int16_t)(16u << bars);
        gen_drums(&trk[0], 16u << bars);
    }
    /* the voices */
    if (st == QS_RAND) {
        uint32_t r3 = qs_r(2), r4 = r3 == 0u ? (qs_r(3) ? 1u : 2u + 2u * qs_r(2)) : (qs_r(2) ? 0u : 2u + 2u * qs_r(2));
        uint32_t r5 = qs_r(3) ? (qs_r(2) ? 3u : 2u) : 4u, m = all ? 0u : 1u + qs_r(6);   /* (PICKED: one or two of T3..T5 out) */
        if (r4 == 0u) r4 = 0u;                           /* (an acid above a sub: ac's register, qs_rand_part) */
        if (!((kp >> 2) & 1u)) { qs_rand_part(2, r3, bars); if (m & 1u) trk[2].p[P_MUTE] = 1; }
        if (!((kp >> 3) & 1u)) { qs_rand_part(3, r4, bars); if (m & 2u) trk[3].p[P_MUTE] = 1; }
        if (!((kp >> 4) & 1u)) { qs_rand_part(4, r5, bars); if ((m & 4u) && m != 7u) trk[4].p[P_MUTE] = 1; }
        if (!((kp >> 5) & 1u)) qs_rand_part(5, 4, bars);
    } else {
        if (!((kp >> 2) & 1u)) {
            if (st == QS_HYPNO || st == QS_ACID) qs_line(2, 0, st == QS_ACID ? 78 : 50, all ? 48u : 40u, bars);
            else if (all) qs_line(2, 1, 30, 36u, bars);
        }
        if (!((kp >> 3) & 1u)) {
            if (all && (st == QS_HYPNO || st == QS_ACID)) qs_line(3, 1, 30, 36u, bars);
            if (st == QS_DUB) qs_chords(3, 0, bars);
            if (st == QS_BLEEPS) qs_bleep(3, "WOOD BLOCK", 1);
        }
        if (!((kp >> 4) & 1u)) {
            if (st != QS_ACID) qs_chords(4, 1, bars);
            else if (all) qs_chords(4, 0, bars);
        }
        if (!((kp >> 5) & 1u)) qs_bleep(5, BL[st], BM[st]);
    }
    track_select(sel);
    ac_ms = db_ms = eu_ms = fm1_ms;
    cos_say(st == QS_RAND ? GENRE[gs.gi].name : QS_NAME[st]);
}
/* OCT+ on START: stopped, at once; playing, at the end of the bar (SYNTAKT ON: of its pattern), so it lands in time */
static void cos_qs_press(void)
{
    if (!song.playing) { qs_pend = 0; cos_qs_load(); return; }
    qs_pend = 1;
    cos_say("ON THE NEXT BAR");
}
/* every UI frame: a load waiting, in the bar's last sixteenth (after its last step, before the next bar's first) */
static __attribute__((noinline)) void cos_qs_tick(void)
{
    uint32_t bs, pb;
    if (!qs_pend) return;
    if (!song.playing) { qs_pend = 0; return; }
    bs = beat_samples();
    pb = (favorites.factory[8][31] & 1u) && song.g[G_CLOCK] == 2 ? cos_pc_bars : 1u;
    if (beat_n != 3u || beat_pos < bs * 3u / 4u + bs / 16u || (pb > 1u && ((bar_n + 1u) % pb))) return;
    qs_pend = 0;
    cos_qs_load();
}
static __attribute__((noinline)) void cos_qs_undo(void)
{
    uint32_t k;
    uint8_t m = qs_mute;
    if (undo_do()) { cos_say("NOTHING TO UNDO"); return; }
    if (qs_have) {                                       /* (the mutes back too; again: the quick start's) */
        for (k = 0, qs_mute = 0; k < NTRK; k++) qs_mute |= (uint8_t)((trk[k].p[P_MUTE] != 0) << k);
        for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = (int16_t)((m >> k) & 1u);
    }
    cos_say("UNDONE");
}
static void cos_mode(uint32_t m)
{
    if (m == CM_PROJ && cz.mode != CM_PROJ) { cz.slot = proj_cur_slot() < 4u ? (uint8_t)proj_cur_slot() : cz.slot; cos_proj_scan(); }
    if (m == CM_GEN && cz.mode != CM_GEN) { cz.mode = (uint8_t)m; cz.hot = 0; cos_gen_enter(); return; }
    if (cz.mode == m) {                                 /* again: the next page (Elektron's way) */
        cz.tab[m] = (uint8_t)cos_ess_next(m, cz.tab[m]);   /* (0.16.0: the everyday pages only; SELECT: all) */
    }
    cz.mode = (uint8_t)m;
    cz.hot = 0;
}
/* the keys are the interface's (silent) on HOME, TRIG and a kit's GEN; elsewhere they play the track */
static int cos_keys_ui(void)
{
    return cz.mode == CM_HOME || (cz.mode == CM_TRIG && !cz.si) || cz.mode == CM_KEY || cz.mode == CM_PROJ || cz.mode == CM_DRUMS || (cz.mode == CM_GEN && cos_kit()) ||
           ((fm1_in.buttons >> panel.btn[B_SCL]) & 1u);
}

/* the sound just loaded, on the top line: its name (engine: name when it fits) */
/* (0.10.2) the sound list (ui.c preset_at: the engines' presets in ENGINE_ORDER, then the user presets) by engine:
 * ALGO the next / last engine (its first sound), PRESETS the next / last sound of this one (a user preset counts as its
 * engine's) */
static uint32_t cos_snd_eng(uint32_t n)
{
    uint32_t k, e = preset_at(n, &k);
    return e == NENGINES ? (k < UP_SLOTS ? up_engine(k) : NENGINES) : e;
}
static __attribute__((noinline)) void cos_engine_step(int32_t dir)
{
    uint32_t total, cur = preset_pos(&total), k, i, n = 0, e = NENGINES, e0 = TSEL->eng_req;
    if (!total) return;
    if (cur >= total) cur = 0;
    for (i = 1; i <= total; i++) {
        n = (cur + (dir > 0 ? i : total * 2u - i)) % total;
        e = preset_at(n, &k);
        if (e != NENGINES && e != e0) break;
    }
    if (i > total) { cos_say("ONE ENGINE"); return; }
    for (i = 1; i < total; i++) {                        /* (back to the engine's first sound: each engine's in a row) */
        uint32_t m = (n + total - 1u) % total;
        if (preset_at(m, &k) != e) break;
        n = m;
    }
    preset_go(n);
}
static __attribute__((noinline)) void cos_preset_in(int32_t dir)
{
    uint32_t total, cur = preset_pos(&total), i, n, e0 = TSEL->eng_req;
    if (!total) return;
    if (cur >= total) cur = dir > 0 ? total - 1u : 0u;
    for (i = 1; i <= total; i++) {
        n = (cur + (dir > 0 ? i : total * 2u - i)) % total;
        if (cos_snd_eng(n) == e0) { preset_go(n); return; }
    }
}
static __attribute__((noinline)) void cos_sound_say(void)
{
    char b[40], n[24];
    sound_name(TSEL, n);
    str_cpy(b, trk_engp(TSEL)->name, 8);
    if (str_len(b) + 1u + str_len(n) <= 18u) { str_cpy(b + str_len(b), ":", 2); str_cpy(b + str_len(b), n, 20); }
    else str_cpy(b, n, 19);
    cos_say(b);
}

/* KEEP and BACK, Elektron's FUNC + YES (a temporary save) and FUNC + NO (reload): SAVE held + OCT+ keeps the sound as it
 * is, in RAM; SAVE held + OCT- brings it back, while it plays (nothing stops, nothing is written to flash). What it keeps:
 * every track's engine, sound and its parameters (but its MUTE), the kit (DRUM-X), FILTER 2 and LFO 2, the macros, RUMBLE,
 * the ISOLATOR and the EQ, and the globals (the FX, the DJ FILTER .. but the tempo, the clock and the settings). Not the
 * steps: SAVE's slots keep those. Gone at power-off */
static const uint8_t KEEP_NOT[] = {G_BPM, G_CLOCK, G_TUNE, G_MIDI, G_ROUTE, G_INFO, G_SLOT, G_LOAD, G_SAVE, G_ENGSEL,
                                   G_CLRSEQ, G_INITSND};
static struct {
    uint8_t ok, eng[NTRK], pre[NTRK], usr[NTRK];
    int16_t p[NTRK][P_COUNT];
    int16_t g[G_COUNT];
    int8_t x2[NTRK][X2_N];
    uint8_t mac[4], rmb[4];
    int8_t iso[3];
    int16_t eq[4];
    dx_lane_t dx[8];
    uint8_t de[8][DE_N], lay[8];
} keep __attribute__((section(".pool")));               /* (~1.9 KB of the pool's spare) */
static __attribute__((noinline)) void cos_keep(int back)
{
    uint32_t k, i;
    if (!back) {
        for (k = 0; k < NTRK; k++) {
            keep.eng[k] = trk[k].eng_req; keep.pre[k] = trk[k].preset; keep.usr[k] = trk[k].user;
            memcpy(keep.p[k], trk[k].p, sizeof keep.p[k]);
        }
        memcpy(keep.g, song.g, sizeof keep.g);
        memcpy(keep.x2, (const void *)trk_x2, sizeof keep.x2);
        memcpy(keep.mac, (const void *)macro_v, sizeof keep.mac);
        memcpy(keep.rmb, (const void *)rmb_v, sizeof keep.rmb);
        memcpy(keep.iso, (const void *)iso_v, sizeof keep.iso);
        memcpy(keep.eq, eq_v, sizeof keep.eq);
        dx_kit_own(keep.dx, keep.lay);                  /* (0.14.0: the own, not a sound lock's) */
        memcpy(keep.de, dx_de, sizeof keep.de);
        keep.ok = 1;
        cos_say("SOUND KEPT");
        return;
    }
    if (!keep.ok) { cos_say("NOTHING KEPT"); return; }
    for (k = 0; k < NTRK; k++)
        if (trk[k].eng_req != keep.eng[k]) set_engine_of(&trk[k], keep.eng[k]);   /* (another engine meanwhile) */
    fm1_irq_off();                                      /* (one block sees the whole of it) */
    for (k = 0; k < NTRK; k++) {
        int16_t mute = trk[k].p[P_MUTE];
        memcpy(trk[k].p, keep.p[k], sizeof keep.p[k]);
        trk[k].p[P_MUTE] = mute;                        /* (the mutes stay as they are) */
        trk[k].preset = keep.pre[k]; trk[k].user = keep.usr[k];
    }
    {
        int16_t g[NELEM(KEEP_NOT)];
        for (i = 0; i < NELEM(KEEP_NOT); i++) g[i] = song.g[KEEP_NOT[i]];
        memcpy(song.g, keep.g, sizeof keep.g);
        for (i = 0; i < NELEM(KEEP_NOT); i++) song.g[KEEP_NOT[i]] = g[i];
    }
    memcpy((void *)trk_x2, keep.x2, sizeof keep.x2);
    memcpy((void *)macro_v, keep.mac, sizeof keep.mac);
    memcpy((void *)rmb_v, keep.rmb, sizeof keep.rmb);
    memcpy((void *)iso_v, keep.iso, sizeof keep.iso);
    memcpy(eq_v, keep.eq, sizeof keep.eq);
    memcpy(dx_kit, keep.dx, sizeof keep.dx);
    memcpy(dx_de, keep.de, sizeof keep.de);
    memcpy(dx_lay, keep.lay, sizeof keep.lay);
    dx_slk_forget();
    fm1_irq_on();
    cos_say("BACK TO KEPT");
}

/* SEQ held + REC: the selected track emptied, on a second press within 3 s (the first one asks: no pattern wiped by a
 * slip in a set). Every step and its locks; a generator's mutation EVERY off for it (else the next would fill it
 * again). GEN's REC undoes it (undo_mark) */
static __attribute__((noinline)) void cos_clear(void)
{
    uint32_t k = song.sel % NTRK, i;
    track_t *t = TSEL;
    if (cz.clr_k != k + 1u || fm1_ms - cz.clr_t > 3000u) {
        cz.clr_k = (uint8_t)(k + 1u);
        cz.clr_t = fm1_ms;
        cos_say("AGAIN TO CLEAR");
        return;
    }
    cz.clr_k = 0;
    undo_mark();
    for (i = 0; i < NSTEP; i++) {
        (void)motion_clear_locks(t, i);
        step_clear(&t->step[i]);
    }
    if (ac[k].on) ac[k].every = 0;
    if (db[k].on) db[k].every = 0;
    if (eu[k].on) eu[k].mu[0] = 0;
    cos_say("TRACK CLEARED");
}

/* another track picked: each mode's page found again by its name in that track's list (a kit's SEQ has no STEP, ACID's
 * EDIT a DEVIL: the same index is another page there); not there, its first */
static void cos_pg_follow(void)
{
    uint32_t m, i, n;
    const cpg_t *p;
    if (cz.pg_sel != song.sel) {
        for (m = 0; m < CM_N; m++) {                     /* (the lists that depend on the track: SEQ EDIT ENV GEN) */
            if (!cz.pg_tab[m] || (m != CM_TRIG && m != CM_SRC && m != CM_AMP && m != CM_GEN)) continue;
            cos_pages(m, &p, &n);
            for (i = 0; i < n && !str_eq(p[i].tab, cz.pg_tab[m]); i++) {}
            cz.tab[m] = (uint8_t)(i < n ? i : 0u);
        }
        cz.pg_sel = (uint8_t)song.sel;
    }
    {
        const cpg_t *q;
        cos_pages(cz.mode, &q, &n);
        cz.pg_tab[cz.mode] = q[cz.tab[cz.mode] % (n ? n : 1u)].tab;
    }
}

static int cos_idle_gate(void);                          /* (the IDLE screen, below) */
#include "cos_remote.c"                                 /* (0.9.5) the phone remote: channel 16, its SysEx */
static __attribute__((noinline)) void cos_input(void)
{
    cos_remote_poll();                                   /* (0.9.5: the phone's, the IDLE screen up or not) */
    cos_drm_poll();                                      /* (0.13.0) DRUMS: the rolls and the REPEAT held */
    mo_ext = (uint8_t)(favorites.factory[15][3] & ((1u << NTRK) - 1u));   /* (0.13.0) GLO > MIDI 2 SOUND EXT */
    if (cos_idle_gate()) return;                         /* (the IDLE screen: shown, or its waking touch) */
    uint32_t released = 0, pressed = fm1_input_edges(&released), notes = fm1_input_note_edges(), k, b;
    int scl_held = (int)((fm1_in.buttons >> panel.btn[B_SCL]) & 1u);
    int32_t s;
    cos_pg_follow();
    {                                                    /* (STEP INPUT: SEQ's STEP page, a synth; its cursor from the
                                                         * bank shown when it starts) */
        int on = cz.mode == CM_TRIG && !cos_kit() && str_eq(cos_page()->tab, "STEP");
        if (on && !cz.si) cz.si_pos = (uint8_t)(cz.bank * 16u);
        cz.si = (uint8_t)on;
    }
    song.grid = cos_keys_ui() ? 2u : 0u;                 /* (seq.c: 2, every key silent) */
    kb_mask = perf_mask = 1u << panel.btn[B_FX];         /* (seq.c keyboard_block: FX held, a key is an effect: the pad) */
    if (favorites.factory[15][14] < 2u) {                /* (once: the pad holds, each effect while its key is held; FX +
                                                         * OCT+ latches. The settings' byte 14 is cursed-os's revision) */
        favorites.factory[15][14] = 2u;
        ui_prefs &= ~PREF_LATCH;
        perf_latch_on = 0;
        settings_save();
    }
    {                                                    /* HOME + OCT- / OCT+ held: the tempo nudged while held */
        int h = cos_home_held(), dn = (int)((fm1_in.buttons >> panel.btn[B_OCTDN]) & 1u),
            up = (int)((fm1_in.buttons >> panel.btn[B_OCTUP]) & 1u);
        bpm_nudge = (int8_t)(h ? up - dn : 0);
        if (rmt_nudge && fm1_ms - rmt_nudge_ms > 5000u) rmt_nudge = 0;
        if (!bpm_nudge) bpm_nudge = rmt_nudge;           /* (0.9.6) the phone remote's NUDGE */
        if (bpm_nudge && cos_follows()) {                /* (an external clock leads: nothing to nudge here; the other
                                                         * machine's tempo is nudged) */
            bpm_nudge = 0;
            cos_say("EXT CLOCK LEADS");
        }
    }
    fx_lock_on = 0;
    for (k = 0; k < 32u && pressed; k++) {
        if (!((pressed >> k) & 1u)) continue;
        pressed &= ~(1u << k);
        b = panel_btn_of(k);
        if (b == B_PLAY) transport_req = song.playing || seq_counting() ? 2u : 1u;
        else if (b == B_REC && ((fm1_in.buttons >> panel.btn[B_SEQ]) & 1u)) { cz.sq_used = 1; cos_clear(); }   /* SEQ held +
                                                         * REC, twice */
        else if (b == B_REC && cz.mode == CM_PROJ && cz.tab[CM_PROJ] == 1u) cos_snd_erase();   /* (0.10.2) SOUNDS: x2 erase */
        else if (b == B_REC && cz.mode == CM_PROJ && cz.tab[CM_PROJ] == 2u) cos_kit_erase();   /* (0.13.0) KITS: x2 erase */
        else if (b == B_REC && cz.mode == CM_PROJ && cz.tab[CM_PROJ] == 3u) cos_qs_undo();   /* (0.18.0) START: the music
                                                         * before the quick start */
        else if (b == B_REC && cz.mode == CM_PROJ) cos_proj_new();   /* (0.9.3) SAVE: REC twice = a new, empty project */
        else if (b == B_REC && cz.mode == CM_TRIG && cos_held()) {   /* a step held + REC: its locks go */
            uint64_t h = cos_held();
            uint32_t i, n = 0;
            for (i = 0; i < NSTEP; i++) if ((h >> i) & 1u) n += motion_clear_locks(TSEL, i);
            cz.lk_used = 1;
            cos_say(n ? "LOCKS CLEARED" : "NO LOCKS");
        }
        else if (b == B_REC && cz.si && !cos_held())
            cos_si_rest((fm1_in.notes & ((1u << 27) - 1u)) != 0u);   /* (STEP: a rest; a key held, a tie) */
        else if (b == B_SEQ && cz.mode == CM_TRIG) { cz.sq_down = 1; cz.sq_used = 0; }   /* (on SEQ: its tap the next way,
                                                         * let go; held + REC twice, the clear) */
        else if (b == B_REC && cz.mode == CM_GEN) cos_say(undo_gen() ? "NOTHING TO UNDO" : "UNDONE");   /* (0.18.0: the
                                                         * track's steps, the transport on) */
        else if (b == B_REC && chain_busy()) cos_say("STOP THE SONG TO RECORD");
        else if (b == B_REC) { cz.rec_down = 1; cz.rec_used = 0; }   /* REC: a tap arms the track (playing, its knobs and keys
                                                         * record into it, saved with the section); held + OCT-: its motion
                                                         * cleared */
        else if (b == B_SCL) { cz.scl_down = 1; cz.scl_used = 0; }   /* (its tap acts when let go: held, it is a shift) */
        else if (b == B_EDIT) { cz.ed_down = 1; cz.ed_used = 0; }    /* (the same: held, PRESETS steps engines) */
        else if (b == B_SAVE) { cz.sv_down = 1; cz.sv_used = 0; }    /* (the same: held, OCT+ KEEP / OCT- BACK) */
        else if (cz.sv_down && (b == B_OCTUP || b == B_OCTDN)) { cz.sv_used = 1; cos_keep(b == B_OCTDN); }
        else if (b == B_FX) { cz.fx_down = 1; cz.fx_used = 0; cz.hot = 0; }   /* (held: the FX pad; a tap: the FX page) */
        else if (b == B_HOME) {                          /* (to HOME at once, PERFORM while held: the tempo's button, K1 BPM;
                                                         * let go: the page before, or on a tap on HOME the next one) */
            cz.home_down = 1;
            cz.home_used = cz.mode != CM_HOME;
            cz.home_tab = cz.tab[CM_HOME];               /* (the HOME page it comes back to) */
            if (cz.mode != CM_HOME) cos_mode(CM_HOME);
            cz.tab[CM_HOME] = 0;
        }
        else if (b == B_OCTUP && ((fm1_in.buttons >> panel.btn[B_SCL]) & 1u)) {   /* (0.18.3) SCL + OCT+: KEY LATCH */
            cz.scl_used = 1;
            kb_latch = (uint8_t)!kb_latch;
            cos_say(kb_latch ? "KEY LATCH ON" : "KEY LATCH OFF");
        }
        else if (cz.fx_down && b == B_OCTUP) {           /* FX + OCT+: LATCH (a key turns its effect on / off) or HOLD */
            cz.fx_used = 1;
            ui_prefs ^= PREF_LATCH;
            perf_latch_on = (ui_prefs & PREF_LATCH) != 0u;
            perf_latched = 0;
            cfx_latched = 0;
            settings_save();
            cos_say(perf_latch_on ? "FX LATCH" : "FX HOLD");
        }
        else if (cz.fx_down && b == B_REC && ((song.rec >> song.sel) & 1u)) {   /* (REC armed) FX + REC: the FX lane cleared */
            cz.fx_used = 1;
            memset((void *)cfx_lane, 0, sizeof cfx_lane);
            memset((void *)cfx_plane, 0, sizeof cfx_plane);
            cfx_lane_on = 0;
            cos_say("FX LANE CLEARED");
        }
        else if (cz.fx_down && b == B_OCTDN) {           /* FX + OCT-: every effect and macro off */
            cz.fx_used = 1;
            perf_latched = 0;
            cfx_latched = 0;
            bd_kill = 1;                                 /* (the BREAKDOWN too) */
            perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
            cos_say("FX ALL OFF");
        }
        else if ((b == B_OCTDN || b == B_OCTUP) && cos_home_held()) cz.home_used = 1;   /* (HOME + OCT: the nudge, above) */
        else if (b == B_OCTDN && cz.rec_down) {          /* (REC held + OCT-: the track's recorded motion cleared) */
            cz.rec_used = 1;
            undo_mark();
            motion_clear(TSEL);
            cos_say("MOTION CLEARED");
        }
        else if (b == B_OCTDN || b == B_OCTUP) {
            int up = b == B_OCTUP;
            if (cz.mode == CM_TRIG && cos_page()->kind == CK_EU) eu_oct(up, 0);   /* (EUCLID: a new variation, the one before) */
            else if (cz.mode == CM_TRIG && cz.si) song.octave = (int8_t)clamp(song.octave + (up ? 1 : -1), -3, 3);   /* (STEP:
                                                         * the keyboard's octave) */
            else if (cz.mode == CM_TRIG) {
                uint32_t n = cos_banks();
                cz.bank = (uint8_t)((cz.bank + (up ? 1u : n - 1u)) % n);
            } else if (cos_on_words()) cos_words(up);
            else if (cos_on_live()) cos_back(up);        /* (0.10.3) LIVE: OCT- BACK TO NORMAL, OCT+ this is normal */
            else if (cz.mode == CM_PROJ) cos_proj_oct(up);
            else if (cz.mode == CM_GEN && cos_kit()) eu_oct(up, cz.tab[CM_GEN] == 2u);
            else if (cz.mode == CM_GEN && cos_page()->kind == CK_DB) db_oct(up, cos_page()->id[0] == DK_ROOT);
            else if (cz.mode == CM_GEN && cos_page()->kind == CK_BL) bl_oct(up, cos_page()->id[0] == BK_EVERY);
            else if (cz.mode == CM_GEN) ac_oct(up, cos_page()->id[0] == AK_EVERY);
            else song.octave = (int8_t)clamp(song.octave + (up ? 1 : -1), -3, 3);
        } else
            for (s = 0; s < CM_N; s++)
                if (CM_BTN[s] == b) cos_mode((uint32_t)s);
    }
    if (cz.fx_down && ((released >> panel.btn[B_FX]) & 1u)) {
        cz.fx_down = 0;
        cz.hot = 0;
        if (!cz.fx_used) cos_mode(CM_FX);
        if (!perf_latch_on) perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;   /* (momentary: the macros go too) */
    }
    if (cz.home_down && ((released >> panel.btn[B_HOME]) & 1u)) {
        cz.home_down = 0;
        if (cz.mode == CM_HOME) {
            cz.tab[CM_HOME] = (uint8_t)(cz.home_used ? cz.home_tab : cos_ess_next(CM_HOME, cz.home_tab));   /* (a tap on HOME:
                                                         * its next everyday page, 0.16.0) */
        }
    }
    if (cz.rec_down && ((released >> panel.btn[B_REC]) & 1u)) {
        cz.rec_down = 0;
        if (!cz.rec_used) {                              /* (a tap: the track armed / disarmed) */
            uint8_t bit = (uint8_t)(1u << song.sel);
            if (!(song.rec & bit)) undo_mark();
            song.rec ^= bit;
            cos_say(song.rec & bit ? "REC: KNOBS AND KEYS" : "REC OFF");
        }
    }
    if (cz.ed_down && ((released >> panel.btn[B_EDIT]) & 1u)) {
        cz.ed_down = 0;
        if (!cz.ed_used) cos_mode(CM_SRC);
    }
    if (cz.sq_down && ((released >> panel.btn[B_SEQ]) & 1u)) {
        cz.sq_down = 0;
        if (!cz.sq_used && cz.mode == CM_TRIG) cos_mode(CM_TRIG);   /* (a tap on SEQ: the next way) */
    }
    if (cz.sv_down && ((released >> panel.btn[B_SAVE]) & 1u)) {
        cz.sv_down = 0;
        if (!cz.sv_used) cos_mode(CM_PROJ);             /* (a tap: the SAVE page, as before) */
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
    if ((s = panel_enc(EN_ALGO)) != 0) {                 /* (0.10.2) ALGO: the track's ENGINE, one a click (its first
                                                         * sound); EDIT held, or on SAVE: the track */
        if (cz.ed_down || cz.mode == CM_PROJ) {
            cz.ed_used |= cz.ed_down;
            track_select((uint32_t)clamp((int32_t)song.sel + (s > 0 ? 1 : -1), 0, (int32_t)trk_live - 1));
            cz.bank = 0;
        } else {
            cos_engine_step(s > 0 ? 1 : -1);
            ui.msg_t = 0;
            cos_sound_say();
        }
    }
    if ((s = panel_enc(EN_PRESET)) != 0) {
        if (cz.ed_down) {                                /* EDIT held: the first sound of the next engine */
            cz.ed_used = 1;
            preset_engine_jump(s > 0 ? 1 : -1);
            ui.msg_t = 0;
            cos_sound_say();
        } else if (cz.mode == CM_TRIG && cz.si && !cos_held()) {
            cos_si_go(s > 0 ? 1 : -1);                   /* (STEP INPUT: the cursor) */
        } else if (cz.mode == CM_TRIG && cos_held() && !cos_kit()) {
            cos_step_note(s > 0 ? 1 : -1);               /* (steps held, a synth: their notes) */
        } else if (cz.mode == CM_TRIG && !cos_held()) {  /* TRIG: the bank of 16 */
            uint32_t n = cos_banks();
            cz.bank = (uint8_t)clamp((int32_t)cz.bank + (s > 0 ? 1 : -1), 0, (int32_t)n - 1);
        } else if (cz.mode != CM_TRIG && cz.mode != CM_PROJ) {   /* (0.10.2) the track's next sound in its engine
                                                         * (and the user presets made with it) */
            cos_preset_in(s > 0 ? 1 : -1);
            ui.msg_t = 0;
            cos_sound_say();
        }
    }
    for (k = 0; k < 4u; k++)
        if ((s = panel_enc(EN_K1 + k)) != 0) {
            uint64_t h = cz.mode == CM_TRIG ? cos_held() : 0u;
            uint32_t cm = cos_ctl_mode();
            if (cos_page()->kind == CK_COS && cos_page()->id[0] == CC_CTLM)
                cm = 0;                                  /* (0.13.1) on GLO > MIDI 2 the knobs are always the FM-1's: the
                                                         * way back from MIDI (GG: "how do i exit midi mode") */
            if (cm == 2u && !cz.fx_down) {               /* (0.13.0) KNOBS MIDI: a relative CC only */
                cos_ctl_send(k, s > 0 ? (uint32_t)clamp(s, 1, 63) : (uint32_t)(128 - clamp(-s, 1, 63)));
                cz.hot = (uint8_t)(k + 1u); cz.hot_ms = fm1_ms;
                continue;
            }
            if (h && !cos_on_cnd()) cos_lock_turn(k, s, h);   /* (steps held: their locks; on COND its own) */
            else {
                cos_turn(k, s);
                if (cm == 1u && !cz.fx_down) {           /* (0.13.0) KNOBS +MIDI: the value it gave, sent too */
                    int16_t *vp = 0;
                    const param_desc_t *d = cos_knob(cos_page(), k, &vp);
                    if (d && vp && d->max > d->min)
                        cos_ctl_send(k, (uint32_t)((clamp(*vp, d->min, d->max) - d->min) * 127 / (d->max - d->min)));
                }
            }
        }
    if (cz.bank >= cos_banks()) cz.bank = 0;
    if (cz.fx_down) {                                    /* the keys are the pad's (seq.c gives them to perform.c) */
        cz.fx_used |= notes != 0u;
        notes = 0;
    }
    for (k = 0; k < 27u && notes; k++) {
        uint32_t w, j;
        if (!((notes >> k) & 1u)) continue;
        notes &= ~(1u << k);
        for (w = 0; w < 16u && WK[w] != k; w++) {}
        for (j = 0; j < 11u && BK[j] != k; j++) {}
        if (scl_held || cz.mode == CM_KEY) {               /* the song's new key */
            cz.scl_used = (uint8_t)scl_held;
            cos_key_ask(k);
        } else if (cz.mode == CM_DRUMS) {
            cos_drm_key(k);                              /* (0.13.0: a press shown; holding rolls / repeats: cos_drm_poll) */
        } else if (cz.mode == CM_TRIG && cz.si) {
            cos_si_key(k);                               /* (STEP INPUT: the note into the cursor's step) */
        } else if (cz.mode == CM_PROJ && cz.tab[CM_PROJ] == 3u) {   /* (0.18.2) START: white 1 .. 6 KEEP a track */
            if (w < NTRK) {
                qs_keep ^= (uint8_t)(1u << w);
                cos_say((qs_keep >> w) & 1u ? "KEPT" : "FREE");
            }
        } else if (cz.mode == CM_PROJ) {
            if (w < 4u) cz.slot = (uint8_t)w;
            else if (w >= 8u && w < 8u + COS_NSONG && !chain_busy()) {   /* (white 9 .. 16: the set list's songs 1 .. 8) */
                if (!song_go_live(w - 8u)) { cz.pn_ok = 0; cos_proj_scan(); }
            }
        } else if (cz.mode == CM_HOME) {                 /* white 1..6 the track, 9..12 the sections A..D on the bar; black
                                                         * 1..6 mute (HOME held: solo), 7 FILL, 8 BREAKDOWN (cos_live.c);
                                                         * (0.10.0) HOME + white 1..8: the drums' sounds in and out */
            if (cos_home_held()) cz.home_used = 1;
            if (w == 15u && cos_home_held()) cos_tap(fm1_ms);   /* (HOME + white 16: TAP TEMPO) */
            else if (w < 8u && (cos_home_held() || cos_on_live())) cos_drum_flip(w);   /* (0.10.3: LIVE without HOME) */
            else if (w == 8u && cos_home_held() && dx_k0p) cos_drum_flip(8u);   /* (0.14.1) HOME + white 9: the PERC */
            else if (w < trk_live) track_select(w);
            else if (w == 12u || w == 13u) mph_key(w - 12u, cos_home_held());   /* (MORPH's A, B: cos_morph.c) */
            else if (w == 14u) {                         /* (LOOP: the track picked back in phase on the beat; HOME held:
                                                         * every track) */
                live_return(cos_home_held() ? (1u << NTRK) - 1u : 1u << (song.sel % NTRK));
                cos_say(cos_home_held() ? "ALL BACK" : "BACK");
            }
            else if (w >= 8u && w < 12u) {
                if (!cz.pn_ok) cos_proj_scan();
                cos_section(w - 8u);
            }
            else if (j < trk_live && cos_home_held()) {
                uint8_t cur;
                fm1_irq_off();                           /* (the ISR takes these on the beat: never half of one) */
                cur = live_qsolo_on ? live_qsolo : perf_solo;
                live_qsolo = (uint8_t)(cur ^ (1u << j));
                live_qsolo_on = 1;
                fm1_irq_on();
                if (!cos_mq) cos_say(live_qsolo ? "SOLO" : "SOLO OFF");
            }
            else if (j < trk_live && !cos_mq) {
                cos_snap();
                trk[j].p[P_MUTE] = (int16_t)!trk[j].p[P_MUTE];
                cos_say(trk[j].p[P_MUTE] ? "MUTED" : "UNMUTED");
            }
            else if (j < trk_live) { cos_snap(); fm1_irq_off(); live_qmute ^= (uint8_t)(1u << j); fm1_irq_on(); }   /* (on the next
                                                         * beat / bar; the ISR takes it whole) */
            else if (j == 6u) { fill_req = 1; cos_say(song.playing ? "FILL" : "FILL: PLAY FIRST"); }
            else if (j == 7u) {
                static const char *const BS[6] = {"BREAK NEXT BAR", "BREAK", "BUILD NEXT BAR", "BUILD", "DROP NEXT BAR", ""};
                bd_req = 1;
                cos_say(BS[bd_state == BD_OFF ? 0 : bd_state == BD_BREAK ? 2 : bd_state == BD_BUILD ? 4 : 5]);
            }
        } else if (cz.mode == CM_TRIG) {
            if (w < 16u) { if (!cz.tg_down) cz.lk_used = 0; cz.tg_down |= 1u << k; }   /* (the trig on release) */
            else if (cos_kit() && j < NLANE) {
                cz.lane = (uint8_t)j;
                if (cos_page()->kind == CK_EU) eu_put(EK_K4, (int32_t)j + 1);   /* (EUCLID: that lane's own) */
            }
            else if (!cos_kit() && cos_held() && j < 2u) cos_step_flag(j ? SF_SLIDE : SF_ACCENT);   /* (B1 ACCENT, B2 SLIDE) */
            else if (!cos_kit() && cos_held() && (j == 2u || j == 3u)) cos_step_len(j == 3u);   /* (0.12.4: B3 SHORTER, B4 LONGER) */
        } else if (cz.mode == CM_GEN && cos_kit() && j < NLANE) {
            eu_put(EK_K4, (int32_t)j + 1);                /* (EUCLID: a lane picked) */
        }
    }
    if (cz.mode == CM_HOME && ((fm1_in.notes >> BK[6]) & 1u) && song.playing) fill_req = 1;   /* (FILL held: every bar) */
    cfx_rec = (uint8_t)(cz.fx_down && song.rec && song.playing);   /* (REC armed, FX held: the FX lane recorded) */
    cfx_home = cz.mode == CM_HOME && !cz.fx_down ?      /* (HOME's black 9 10 11 held: LOW MID HIGH KILL, the isolator's) */
        (((fm1_in.notes >> BK[8]) & 1u) ? CX_BIT(CX_LOWK) : 0u) | (((fm1_in.notes >> BK[9]) & 1u) ? CX_BIT(CX_MIDK) : 0u) |
        (((fm1_in.notes >> BK[10]) & 1u) ? CX_BIT(CX_HIK) : 0u) : 0u;
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
    if (cz.set_ms && fm1_ms - cz.set_ms > 2000u) { cz.set_ms = 0; settings_save(); }
    cos_key_poll();
    if (cz.mode == CM_SRC && cos_kit()) dx_follow();     /* (a drum's key struck on a kit's EDIT: that drum picked) */
    cos_pg_follow();                                     /* (the page now, by name, for the next track picked) */
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
static int cos_drum_out(uint32_t s);
#include "cos_drm.c"                                     /* (0.13.0) LFO > DRUMS: rolls, the drums' REPEAT, CHANCE */
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

/* (cursed-os) the battery, in percent: -1 not read yet, 101 on USB (a host powers it: the reading is the charger's).
 * main.c smooths ADC ch3 (song.batt_raw, 10 bit), the battery through a divider. An estimate: 3.3 V the ADC's top and
 * the divider a half (JIANT's three bars at 531 / 561 / 591 read 3.43 / 3.62 / 3.81 V so), a Li-ion's curve from that */
static int32_t cos_batt_mv(void) { return song.batt_raw > 0 ? song.batt_raw * 6600 / 1023 : 0; }
static int32_t cos_batt_pct(void)
{
    static const int16_t MV[10] = {3300, 3500, 3600, 3700, 3750, 3800, 3900, 4000, 4100, 4200};
    static const uint8_t PC[10] = {0, 5, 12, 30, 42, 55, 70, 82, 92, 100};
    int32_t v = cos_batt_mv(), i;
    if (usb.config && !usb.suspended) return 101;
    if (!v) return -1;
    if (v <= MV[0]) return 0;
    for (i = 1; i < 10; i++)
        if (v < MV[i]) return PC[i - 1] + (PC[i] - PC[i - 1]) * (v - MV[i - 1]) / (MV[i] - MV[i - 1]);
    return 100;
}
static void cos_batt_text(char *b)                       /* "78%", "USB", "" */
{
    int32_t p = cos_batt_pct();
    b[0] = 0;
    if (p > 100) str_cpy(b, "USB", 4);
    else if (p >= 0) { fmt_int(b, p); str_cpy(b + str_len(b), "%", 2); }
}

/* (cursed-os) the IDLE screen: untouched for SYSTEM's IDLE (default 1 MIN), the oscilloscope over the whole screen. The
 * first touch only wakes it (swallowed); the sound plays on throughout */
static uint32_t cos_idle_lim(void) { return cos_idle_min() * 60000u; }   /* (fm1_ms: the time since power-on, no clock) */
static inline int32_t cos_isin(uint32_t a) { return SINE[a & 1023u]; }   /* (a: 1024 a turn) */

/* (cursed-os) GLO's SCOPE, SPECTRUM, STEREO and LOUDNESS (the pages after MIX; the IDLE screen the scope): the output as
 * it leaves the master's COMP and LIMIT, from audio.c's rings (scope_buf L, scope_r R at 44.1 kHz, 512: 11.6 ms;
 * scope_lo the mid, scope_ls the side at 5.5 kHz, 512: 93 ms) and its 100 ms meter blocks (mt_ring). A frame's picture is
 * worked out once before its six strips (cos_scope_prep, cos_spec_prep, cos_lu_poll every frame) */
static struct {
    int16_t y[240], y2[240];                              /* the scope: a column's sample (the second trace: R or S) */
    uint8_t lv[32], pk[32];                               /* the SPECTRUM: each band's level and its peak (0 .. 120: dB
                                                         * over -60 dBFS, x 2) */
    uint8_t wf[80][16];                                   /* .. the waterfall: 80 columns of 32 bands, 4 bits each */
    uint8_t wf_w, wf_n, g, g2;                            /* .. its next column, how many filled; the scope's gain, the
                                                         * vectorscope's */
    uint32_t wf_ms, pk_ms;
} sc COS_RAM;
/* 32 bands 30 Hz .. 20 kHz, 0.3 of an octave apart (Hz); under 700 Hz from the 5.5 kHz rings */
static const uint16_t SP_F[32] = {30, 37, 46, 56, 69, 86, 106, 130, 161, 198, 244, 301, 372, 459, 566, 697, 860, 1061, 1309,
                                  1614, 1991, 2455, 3028, 3735, 4607, 5682, 7007, 8643, 10660, 13147, 16216, 20000};
/* (0.9.2) levels in integers (the main unit has no float code at all: no FPU flags, no soft-float library): log2 of a
 * 64-bit power in Q16 (the top bit's place, then 16 bits of the fraction by squaring), dB in hundredths */
static int32_t cos_l2q16(uint64_t p)                     /* log2 p, Q16; p >= 1 */
{
    uint32_t n = 63u, i, f = 0;
    uint64_t m;
    if (!p) return -(64 << 16);
    while (!(p >> n)) n--;
    m = n >= 31u ? p >> (n - 31u) : p << (31u - n);      /* (the mantissa, Q31: 1 .. 2) */
    for (i = 0; i < 16u; i++) {
        m = (m * m) >> 31;
        f <<= 1;
        if (m >> 32) { f |= 1u; m >>= 1; }
    }
    return (int32_t)(n << 16 | f);
}
static int32_t cos_cdb(uint64_t p)                       /* 10 log10 p, hundredths of a dB (0 for p 0: callers check) */
{
    return p ? (int32_t)(((int64_t)cos_l2q16(p) * 30103) >> 16) / 100 : -100000;
}
/* "-9.4": one decimal from hundredths */
static void cos_fmt1(char *b, int32_t v)
{
    int32_t t = v < 0 ? -((-v + 5) / 10) : (v + 5) / 10, a = t < 0 ? -t : t;
    char n[12];
    b[0] = 0;
    if (t < 0) str_cpy(b, "-", 2);
    fmt_int(n, a / 10); str_cpy(b + str_len(b), n, 8);
    str_cpy(b + str_len(b), ".", 2);
    fmt_int(n, a % 10); str_cpy(b + str_len(b), n, 2);
}
/* the scope: TIME 5, 11 ms (44.1 kHz, a column a sample or every 2nd), 46, 93 ms (5.5 kHz); MODE MONO (the mid), L/R (two
 * traces: 5 and 11 ms only), M/S (the mid and, red, the side); from a rising zero crossing (TRIG); GAIN AUTO, x1 .. x8 */
static void cos_scope_prep(int32_t h)
{
    uint32_t lo = sc_time >= 2u, st = 1u + (sc_time & 1u), w, span = 240u * st, i, start, two = sc_mode != 0u;
    const int16_t *b, *b2;
    if (sc_mode == 1u) lo = 0;                            /* (L/R: the full rate only) */
    w = lo ? scope_lw : scope_w;
    b = lo ? scope_lo : scope_buf;                        /* (MONO at 44.1 kHz: L and R averaged, below) */
    b2 = sc_mode == 2u ? (lo ? scope_ls : scope_r) : scope_r;
    start = w - span;
    if (sc_trig)
        for (i = 0; i < SCOPE_N - span - 2u; i++) {
            uint32_t q = w - span - 1u - i;
            if (b[(q - 1u) & (SCOPE_N - 1u)] < 0 && b[q & (SCOPE_N - 1u)] >= 0) { start = q; break; }
        }
    {
        uint32_t pk = 1;
        int32_t g = (int32_t)sc_gain - 1;
        for (i = 0; i < 240u; i++) {
            uint32_t q = (start + i * st) & (SCOPE_N - 1u);
            int32_t v = b[q], v2 = b2[q];
            if (!lo && sc_mode != 1u) {                   /* (44.1 kHz: the mid and the side from L and R) */
                v = (scope_buf[q] + scope_r[q]) >> 1;
                v2 = (scope_buf[q] - scope_r[q]) >> 1;
            }
            sc.y[i] = (int16_t)v; sc.y2[i] = (int16_t)v2;
            if ((uint32_t)(v < 0 ? -v : v) > pk) pk = (uint32_t)(v < 0 ? -v : v);
            if (two && (uint32_t)(v2 < 0 ? -v2 : v2) > pk) pk = (uint32_t)(v2 < 0 ? -v2 : v2);
        }
        if (!sc_gain) for (g = 0; g < 4 && (pk << (g + 1)) < 26000u; g++) {}
        sc.g = (uint8_t)g;
        for (i = 0; i < 240u; i++) {
            int32_t v = clamp(sc.y[i] << g, -32767, 32767), v2 = clamp(sc.y2[i] << g, -32767, 32767);
            sc.y[i] = (int16_t)(-v * h / 65536);
            sc.y2[i] = (int16_t)(two ? -v2 * h / 65536 : 0);
        }
    }
}
/* a band's level by Goertzel over a ring's 512, Hann-windowed (a ring pair: the mid, (a + b) / 2, or the side), in half
 * dB over -60 dBFS (SP_CAL: a full-scale sine at 120) */
#define SP_CAL 130
static int32_t sp_band(const int16_t *a, const int16_t *b, int32_t sgn, uint32_t w, uint32_t fs, uint32_t f)
{
    uint32_t i, ph = (f << 10) / fs, l2 = 0;
    int32_t c = cos_isin(ph + 256u);                     /* (2 cos in Q14: cos in Q15, the same number) */
    int64_t s1 = 0, s2 = 0, pw;
    for (i = 0; i < SCOPE_N; i++) {
        uint32_t q = (w + i) & (SCOPE_N - 1u);
        int32_t x = b ? (a[q] + sgn * b[q]) >> 1 : a[q], win = 16384 - (cos_isin(i * 2u + 256u) >> 1);   /* (Hann) */
        int64_t s0 = ((int64_t)x * win >> 15) + ((c * s1) >> 14) - s2;
        s2 = s1; s1 = s0;
    }
    pw = s1 * s1 + s2 * s2 - ((c * s1) >> 14) * s2;
    if (pw <= 0) return 0;
    while (pw >> 3) { pw >>= 1; l2++; }                     /* (log2 in quarters: the top bit's place, the next two) */
    return ((int32_t)(l2 * 4u + ((uint32_t)pw & 3u)) * 6) / 4 - SP_CAL;
}
static void cos_spec_prep(void)
{
    static const uint8_t DEC[3] = {20, 8, 3};             /* (SPEED: the bars' fall a frame, half dB) */
    uint32_t k, t = fm1_ms, wl = scope_lw, wh = scope_w;
    int32_t sg = sp_src ? -1 : 1;                         /* (SOURCE: MID, SIDE) */
    for (k = 0; k < 32u; k++) {
        int32_t v = SP_F[k] < 700u ? sp_band(sp_src ? scope_ls : scope_lo, 0, 1, wl, 5513u, SP_F[k])
                                   : sp_band(scope_buf, scope_r, sg, wh, 44100u, SP_F[k]);
        v = clamp(v, 0, 120);
        if (v >= sc.lv[k]) sc.lv[k] = (uint8_t)v;
        else sc.lv[k] = (uint8_t)(sc.lv[k] > DEC[sp_speed % 3u] ? sc.lv[k] - DEC[sp_speed % 3u] : 0);
        if (sc.lv[k] >= sc.pk[k]) sc.pk[k] = sc.lv[k];
    }
    if (t - sc.pk_ms > 60u) {                             /* (the peaks fall slowly) */
        sc.pk_ms = t;
        for (k = 0; k < 32u; k++) if (sc.pk[k]) sc.pk[k]--;
    }
    if (t - sc.wf_ms >= 60u) {                            /* the waterfall: a column every 60 ms (80: ~5 s) */
        static const uint8_t RG[3] = {120, 96, 72};       /* (RANGE: 60, 48, 36 dB over the 15 steps) */
        uint32_t rg = RG[sp_range % 3u];
        sc.wf_ms = t;
        for (k = 0; k < 32u; k++) {
            uint32_t lv = (uint32_t)sc.lv[k] + rg > 120u ? ((uint32_t)sc.lv[k] + rg - 120u) * 15u / rg : 0u;
            if (k & 1u) sc.wf[sc.wf_w][k >> 1] = (uint8_t)((sc.wf[sc.wf_w][k >> 1] & 0x0Fu) | lv << 4);
            else sc.wf[sc.wf_w][k >> 1] = (uint8_t)((sc.wf[sc.wf_w][k >> 1] & 0xF0u) | lv);
        }
        sc.wf_w = (uint8_t)((sc.wf_w + 1u) % 80u);
        if (sc.wf_n < 80u) sc.wf_n++;
    }
}
/* LOUDNESS (ITU-R BS.1770 / EBU R128): every 100 ms block cos_meter.c sums; here, each frame, the new ones taken in.
 * MOMENTARY the last 4 (400 ms), SHORT the last 30 (3 s), INTEGRATED all since RESET, gated (-70 LUFS, then 10 LU under
 * its own mean: a histogram of the momentary levels in half-LU steps from -70 to +5, each step's power from LU_PW); PEAK
 * the sample peak since RESET; the STEREO page's correlation, width and balance over the last 3 blocks. All in
 * hundredths (LUFS, dB) */
static const uint32_t LU_PW[150] = /* 10^((L + 0.691) / 10), Q24, at each half-LU step's middle L */
{2u, 2u, 3u, 3u, 3u, 4u, 4u, 5u, 5u, 6u, 7u, 7u, 8u, 9u, 10u, 12u, 13u, 15u, 17u, 19u, 21u, 23u, 26u, 29u, 33u,
    37u, 42u, 47u, 52u, 59u, 66u, 74u, 83u, 93u, 104u, 117u, 131u, 148u, 166u, 186u, 208u, 234u, 262u, 294u, 330u,
    371u, 416u, 466u, 523u, 587u, 659u, 739u, 830u, 931u, 1044u, 1172u, 1315u, 1475u, 1655u, 1857u, 2084u, 2338u,
    2623u, 2943u, 3302u, 3705u, 4157u, 4665u, 5234u, 5872u, 6589u, 7393u, 8295u, 9307u, 10443u, 11717u, 13147u,
    14751u, 16551u, 18570u, 20836u, 23379u, 26231u, 29432u, 33023u, 37053u, 41574u, 46647u, 52338u, 58725u, 65890u,
    73930u, 82951u, 93072u, 104429u, 117171u, 131468u, 147509u, 165508u, 185703u, 208363u, 233787u, 262313u,
    294320u, 330233u, 370527u, 415738u, 466466u, 523383u, 587246u, 658901u, 739299u, 829507u, 930722u, 1044287u,
    1171709u, 1314680u, 1475095u, 1655083u, 1857034u, 2083627u, 2337868u, 2623130u, 2943201u, 3302326u, 3705270u,
    4157382u, 4664659u, 5233833u, 5872458u, 6589006u, 7392986u, 8295067u, 9307218u, 10442871u, 11717094u, 13146795u,
    14750947u, 16550834u, 18570342u, 20836266u, 23378675u, 26231305u, 29432008u, 33023256u, 37052703u, 41573817u,
    46646589u, 52338334u, 58724577u};
#define LU_K (-5488)                                      /* (a block sum's LUFS: cos_cdb(sum) - cos_cdb(samples) + this: the
                                                         * sums Q18 (Q30 >> 12), 0 dBFS 2^30, BS.1770's -0.691) */
static struct {
    uint32_t seen, n;                                     /* the blocks taken, since RESET */
    uint16_t hist[150];
    int32_t pk, m, s, i, corr, width, bal;                /* (corr x 1000, width %, the rest hundredths of a dB) */
} lu;
static void cos_lu_reset(void) { memset(&lu, 0, sizeof lu); lu.seen = mt_n; lu.m = lu.s = lu.i = lu.pk = -100000; }
static int32_t cos_lufs(uint64_t sum, uint32_t blocks) { return sum ? cos_cdb(sum) - cos_cdb((uint64_t)blocks * 4410u) + LU_K : -100000; }
/* n / d without a 64-bit divide (the toolchain has none): both shifted down until they fit 31 bits; d > 0 */
static int32_t cos_div64(int64_t n, int64_t d)
{
    while (d >= (1ll << 31) || n >= (1ll << 31) || n < -(1ll << 31)) { n >>= 1; d >>= 1; }
    return d ? (int32_t)n / (int32_t)d : 0;
}
static uint64_t cos_isqrt(uint64_t v)                     /* (the correlation's root) */
{
    uint64_t r = 0, b = 1ull << 62;
    while (b > v) b >>= 2;
    while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
    return r;
}
static void cos_lu_poll(void)
{
    uint32_t now = mt_n, k, nb;
    uint64_t e;
    if (now == lu.seen) return;                           /* (nothing new: the readings as they are; 10 a second) */
    if (now - lu.seen > 30u) lu.seen = now - 30u;         /* (a long gap: the ring's last 3 s) */
    for (; lu.seen != now; lu.seen++) {
        const mt_blk_t *b = &mt_ring[lu.seen & 31u];
        int32_t pk = b->pk ? cos_cdb((uint64_t)b->pk * b->pk) - 9031 : -100000;   /* (dBFS: 2^30 is 0) */
        if (pk > lu.pk) lu.pk = pk;
        if (++lu.n >= 4u) {                               /* (a gating block: the 400 ms to this one) */
            int32_t l;
            for (e = 0, k = 0; k < 4u; k++) e += mt_ring[(lu.seen - k) & 31u].kw;
            l = cos_lufs(e, 4u);
            if (l > -7000) {
                uint32_t h = (uint32_t)clamp((l + 7000) / 50, 0, 149);
                if (lu.hist[h] == 0xFFFFu) for (k = 0; k < 150u; k++) lu.hist[k] = (uint16_t)((lu.hist[k] + 1u) >> 1);   /* (hours:
                                                         * halved, the shape kept) */
                lu.hist[h]++;
            }
        }
    }
    nb = lu.n < 30u ? lu.n : 30u;
    for (e = 0, k = 0; k < 4u && k < nb; k++) e += mt_ring[(now - 1u - k) & 31u].kw;
    lu.m = nb ? cos_lufs(e, nb < 4u ? nb : 4u) : -100000;
    for (e = 0, k = 0; k < nb; k++) e += mt_ring[(now - 1u - k) & 31u].kw;
    lu.s = nb ? cos_lufs(e, nb) : -100000;
    {                                                     /* INTEGRATED: the gated mean of the histogram's blocks */
        uint64_t sum = 0, cnt = 0;
        for (k = 0; k < 150u; k++) { sum += (uint64_t)lu.hist[k] * LU_PW[k]; cnt += lu.hist[k]; }
        if (cnt) {
            int32_t gate = cos_cdb(sum) - cos_cdb(cnt) - 7294 - 1000;   /* (Q24 power to LUFS: -24 x 3.0103 dB, -0.691; then 10 LU under) */
            uint32_t g0 = (uint32_t)clamp((gate + 7000 + 49) / 50, 0, 150);
            sum = cnt = 0;
            for (k = g0; k < 150u; k++) { sum += (uint64_t)lu.hist[k] * LU_PW[k]; cnt += lu.hist[k]; }
            lu.i = cnt ? cos_cdb(sum) - cos_cdb(cnt) - 7294 : -100000;
        } else lu.i = -100000;
    }
    {                                                     /* the stereo: L R's correlation, the side to the mid, L to R */
        uint64_t ll = 0, rr = 0, d;
        int64_t lr = 0;
        for (k = 0; k < 3u && k < nb; k++) { const mt_blk_t *b = &mt_ring[(now - 1u - k) & 31u]; ll += b->ll; rr += b->rr; lr += b->lr; }
        d = cos_isqrt(ll) * cos_isqrt(rr);
        lu.corr = d ? clamp(cos_div64(lr * 1000, (int64_t)d), -1000, 1000) : 1000;
        {
            int64_t dm = (int64_t)(ll + rr) + 2 * lr, ds = (int64_t)(ll + rr) - 2 * lr;   /* (the mid's and the side's power, x 4) */
            lu.width = dm + ds > 0 ? cos_div64(ds * 100, dm + ds) : 0;   /* (0 % mono, 50 % as wide as
                                                         * it is high: unrelated sides, 100 % all side) */
        }
        lu.bal = ll && rr ? cos_cdb(ll) - cos_cdb(rr) : 0;
    }
}
/* the scope drawn: its middle at y0 (sc.y), the second trace red (L/R: R; M/S: S), faint guides */
static void cos_scope_draw(int32_t y0, int32_t h, uint16_t col)
{
    uint32_t i;
    cv_rect(0, y0, 240, 1, C_DARK);
    cv_rect(0, y0 - h, 240, 1, C_DARK);
    cv_rect(0, y0 + h, 240, 1, C_DARK);
    if (sc_mode)
        for (i = 1; i < 240u; i++) cv_line((int32_t)i - 1, y0 + sc.y2[i - 1u], (int32_t)i, y0 + sc.y2[i], C_RED);
    for (i = 1; i < 240u; i++) cv_line((int32_t)i - 1, y0 + sc.y[i - 1u], (int32_t)i, y0 + sc.y[i], col);
}
static void cos_body_scope(void)
{
    static const char *const MD[3] = {"MONO (MID)", "L WHITE  R RED", "MID WHITE  SIDE RED"};
    char b[32];
    cos_scope_draw(86, 50, C_FG);
    str_cpy(b, MD[sc_mode % 3u], sizeof b);
    str_cpy(b + str_len(b), "  X", 4);
    fmt_int(b + str_len(b), 1 << sc.g);
    cos_tx(4, 140, b, C_DIM);
}
static void cos_body_spec(void)
{
    static const uint8_t RG[3] = {120, 96, 72};
    static const uint16_t RAMP[6] = {C_BG, C_DARK, C_DIM, C_MID, C_FG, C_RED};
    uint32_t k, rg = RG[sp_range % 3u];
    if (!sp_view) {                                       /* BARS: 32 of them, 7 wide, the peaks a line over them */
        for (k = 0; k < 32u; k++) {
            int32_t x = 1 + (int32_t)k * 7, lv = (int32_t)sc.lv[k] + (int32_t)rg - 120;
            int32_t pv = (int32_t)sc.pk[k] + (int32_t)rg - 120, hh = lv > 0 ? lv * 104 / (int32_t)rg : 0;
            if (hh) cv_rect(x, 140 - hh, 6, hh, lv >= (int32_t)rg - 6 ? C_RED : sp_src ? C_MID : C_FG);
            if (pv > 0) cv_rect(x, 140 - pv * 104 / (int32_t)rg, 6, 1, C_MID);
        }
        cv_rect(0, 141, 240, 1, C_DARK);
    } else {                                              /* WATERFALL: time left to right (the newest right), lows below */
        uint32_t c;
        for (c = 0; c < 80u; c++) {
            uint32_t col = (sc.wf_w + c) % 80u;
            if (c < 80u - sc.wf_n) continue;
            for (k = 0; k < 32u; k++) {
                uint32_t lv = (sc.wf[col][k >> 1] >> ((k & 1u) * 4u)) & 15u, r = lv ? 1u + (lv - 1u) * 5u / 15u : 0u;
                if (r) cv_rect((int32_t)c * 3, 139 - (int32_t)k * 3 - 3, 3, 3, RAMP[r > 5u ? 5u : r]);
            }
        }
    }
    cos_tx(1 + 7 * 3, 146, "50", C_DIM);                  /* (the bands under: 56 Hz, 198, 1061, 3735, 20000) */
    cos_tx(1 + 7 * 9, 146, "200", C_DIM);
    cos_tx(1 + 7 * 17, 146, "1K", C_DIM);
    cos_tx(1 + 7 * 23, 146, "4K", C_DIM);
    cos_txr(238, 146, "20K", C_DIM);
    if (sp_src) cos_txr(238, 34, "SIDE", C_RED);
}
/* STEREO: the vectorscope (mono a line straight up; out of phase lying flat), the correlation (-1 .. +1), the width and
 * the balance */
static void cos_body_stereo(void)
{
    uint32_t i, w = scope_w, pk = 1;
    int32_t cx = 70, cy = 88, r = 54, g;
    char b[24], n[12];
    for (i = 0; i < 512u; i++) {
        int32_t l = scope_buf[i], rr = scope_r[i], a = l < 0 ? -l : l, c = rr < 0 ? -rr : rr;
        if ((uint32_t)(a > c ? a : c) > pk) pk = (uint32_t)(a > c ? a : c);
    }
    g = sc_zoom ? (int32_t)sc_zoom - 1 : 0;
    if (!sc_zoom) for (g = 0; g < 4 && (pk << (g + 1)) < 30000u; g++) {}
    cv_line(cx - r, cy, cx + r, cy, C_DARK);              /* (the guides: M up, S across, L and R at 45 degrees) */
    cv_line(cx, cy - r, cx, cy + r, C_DARK);
    cv_line(cx - r * 7 / 10, cy - r * 7 / 10, cx + r * 7 / 10, cy + r * 7 / 10, C_DARK);
    cv_line(cx + r * 7 / 10, cy - r * 7 / 10, cx - r * 7 / 10, cy + r * 7 / 10, C_DARK);
    cos_tx(cx - r * 7 / 10 - 8, cy - r * 7 / 10 - 8, "L", C_DIM);
    cos_tx(cx + r * 7 / 10 + 2, cy - r * 7 / 10 - 8, "R", C_DIM);
    for (i = 0; i < 512u; i++) {
        uint32_t q = (w + i) & (SCOPE_N - 1u);
        int32_t l = clamp(scope_buf[q] << g, -32767, 32767), rr = clamp(scope_r[q] << g, -32767, 32767);
        int32_t x = cx + (rr - l) * r / 65536, y = cy - (l + rr) * r / 65536;
        cv_rect(x, y, 1, 1, C_FG);
    }
    {                                                     /* the correlation: a bar from -1 (left, red) to +1 */
        int32_t x0 = 136, x1 = 236, xm = (x0 + x1) / 2, xc = xm + lu.corr * (x1 - xm) / 1000;
        cos_tx(x0, 34, "CORRELATION", C_DIM);
        cv_rect(x0, 52, x1 - x0, 1, C_DARK);
        cv_rect(xm, 46, 1, 12, C_DIM);
        cv_rect(xc < xm ? xc : xm, 48, (xc < xm ? xm - xc : xc - xm) + 1, 8, lu.corr < 0 ? C_RED : C_FG);
        { char t[8]; int32_t c2 = lu.corr < 0 ? -((-lu.corr + 5) / 10) : (lu.corr + 5) / 10; t[0] = 0;
          if (c2 < 0) str_cpy(t, "-", 2); fmt_int(n, (c2 < 0 ? -c2 : c2) / 100); str_cpy(t + str_len(t), n, 3);
          str_cpy(t + str_len(t), ".", 2); fmt_int(n, (c2 < 0 ? -c2 : c2) % 100 + 100); str_cpy(t + str_len(t), n + 1, 3);
          cos_px(x0, 62, t, lu.corr < 0 ? C_RED : C_FG, CZ_TALL); }
        str_cpy(b, "WIDTH ", sizeof b);                   /* (the side's share of the power, percent) */
        fmt_int(n, lu.width); str_cpy(b + str_len(b), n, 8); str_cpy(b + str_len(b), "%", 2);
        cos_tx(x0, 94, b, C_MID);
        str_cpy(b, "BAL ", sizeof b);
        cos_fmt1(n, lu.bal); str_cpy(b + str_len(b), n, 10); str_cpy(b + str_len(b), " DB", 4);
        cos_tx(x0, 108, b, C_MID);
        cos_tx(x0, 122, lu.bal > 50 ? "LEFT LOUDER" : lu.bal < -50 ? "RIGHT LOUDER" : "CENTRED", C_DIM);
    }
    cos_tx(4, 140, "+1 MONO 0 WIDE -1 PHASE", C_DIM);
}
/* LOUDNESS: MOMENTARY, SHORT, INTEGRATED in LUFS, the sample PEAK in dBFS; a bar of the short-term against TARGET */
static void cos_body_loud(void)
{
    static const int8_t TG[6] = {-14, -12, -10, -9, -8, -6};
    char b[24], n[12];
    int32_t tg = TG[lu_tgt % 6u], k;
    static const char *const NM[3] = {"MOMENTARY", "SHORT 3S", "INTEGRATED"};
    int32_t v[3] = {lu.m, lu.s, lu.i};
    for (k = 0; k < 3; k++) {
        cos_tx(4, 36 + k * 20, NM[k], C_DIM);
        if (v[k] > -7000) cos_fmt1(n, v[k]); else str_cpy(n, "-INF", 6);
        str_cpy(b, n, sizeof b);
        cos_px(132, 32 + k * 20, b, k == 1 && v[k] > tg * 100 + 100 ? C_RED : C_FG, CZ_TALL);
        cos_tx(206, 36 + k * 20, "LUFS", C_DIM);
    }
    cos_tx(4, 96, "PEAK", C_DIM);
    {
        if (lu.pk > -7000) cos_fmt1(n, lu.pk); else str_cpy(n, "-INF", 6);
        cos_px(132, 92, n, lu.pk > -100 ? C_RED : C_FG, CZ_TALL);
        cos_tx(206, 96, "DBFS", C_DIM);
    }
    {                                                     /* the short-term as a bar, -30 .. 0 LUFS, TARGET a red mark */
        int32_t x0 = 4, x1 = 236, w = x1 - x0, x = x0 + (clamp(lu.s, -3000, 0) + 3000) * w / 3000, xt = x0 + (tg + 30) * w / 30;
        x = clamp(x, x0, x1);
        cv_rect(x0, 122, w, 8, C_DARK);
        if (x > x0) cv_rect(x0, 122, x - x0, 8, lu.s > tg * 100 + 100 ? C_RED : C_MID);
        cv_rect(xt, 118, 2, 16, C_RED);
        str_cpy(b, "TARGET ", sizeof b); fmt_int(n, tg); str_cpy(b + str_len(b), n, 6);
        cos_tx(4, 140, b, C_DIM);
        cos_txr(236, 140, "AFTER LIMIT", C_DIM);
    }
}
static void cos_idle_scene(void)                         /* the IDLE screen: the scope, the whole screen */
{
    cos_scope_draw(120, 100, C_FG);
}

/* cos_input, first: 1 = this pass is the IDLE screen's (it shows, or the touch that woke it goes on) */
static int cos_idle_gate(void)
{
    uint32_t held = fm1_in.buttons | fm1_in.notes, k, lim = cos_idle_lim();
    int32_t mv = 0;
    int touch = held || panel_moved;
    if (cz.idle_up || cz.idle_eat) {                     /* (shown or waking: the knobs' turns and the presses go) */
        for (k = 0; k < NE; k++) mv |= panel_enc(k);
        (void)fm1_input_edges(0);
        (void)fm1_input_note_edges();
        touch |= mv != 0;
    }
    panel_moved = 0;
    if (touch) cz.act_ms = fm1_ms;
    if (cz.idle_up && touch) {                           /* woken: the page as it was, the touch not acted on */
        cz.idle_up = 0;
        cz.idle_eat = 1;
        cz.ready = 0;
        ui.force = 1;
    }
    if (cz.idle_eat) {
        if (held) { song.grid = 2; return 1; }           /* (keys silent until let go) */
        cz.idle_eat = 0;
        return 1;
    }
    if (!cz.idle_up && lim && !ui.uboot && !cz.fx_down && fm1_ms - cz.act_ms >= lim) {
        cz.idle_up = 1;
        cz.ready = 0;
    }
    if (cz.idle_up) { song.grid = 2; return 1; }
    {                                                    /* the battery low: said once (again under 5 %) */
        int32_t p = cos_batt_pct();
        if (p >= 0 && p <= 100) {
            if (p <= 10 && cz.batt_low < 1u) { cz.batt_low = 1; cos_say("BATTERY LOW"); }
            else if (p <= 5 && cz.batt_low < 2u) { cz.batt_low = 2; cos_say("BATTERY 5% LEFT"); }
            else if (p > 20) cz.batt_low = 0;
        }
    }
    return 0;
}

static void cos_head(void)
{
    char b[24];
    const track_t *t = TSEL;
    cv_rect(0, 0, 26, 14, ((song.rec >> song.sel) & 1u) ? C_RED : C_FG);   /* (red: REC armed on the track) */
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
    if (cos_follows()) cos_bpm_text(b, (int32_t)cos_ext10());   /* (following a clock: its tempo, steady, in tenths) */
    else cos_bpm100_text(b, song.g[G_BPM] * 100 + bpm_frac % 100u);   /* (0.10.1: 130.16) */
    if (bpm_nudge) str_cpy(b + str_len(b), bpm_nudge > 0 ? "+" : "-", 2);   /* (nudged: 3 %, red) */
    {                                                     /* (0.10.1) the key and the battery left of the tempo, however long
                                                         * (130.16+ is wider than 120) */
        int32_t kx = 226 - cos_w(b) - 6, bx;
        const char *kn = COS_NN[cz.key_next ? cz.key_next - 1u : cos_key()];
        cos_txr(226, 3, b, bpm_nudge ? C_RED : C_FG);
        if (kx > 194) kx = 194;
        cos_txr(kx, 3, kn, cz.key_next ? C_RED : C_MID);
        bx = kx - cos_w(kn) - 8;
        if (bx > 176) bx = 176;
        cos_batt_text(b);                                /* (the battery: between the engine and the key) */
        if (b[0] && (bx >= 164 || cos_batt_pct() <= 15))   /* (a long tempo, 130.16: the battery left out unless
                                                         * it runs low, the engine's name has the room) */
            cos_txr(bx, 3, b, cos_batt_pct() <= 15 ? C_RED : C_DIM);
    }
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
    if (cz.fx_down) {                                    /* (the FX pad: its knobs' page, LATCH or HOLD) */
        cv_rect(2, 20, cos_w("FX PAD") + 6, 12, C_FG);
        cos_tx(5, 22, "FX PAD", C_BG);
        cos_txr(238, 22, perf_latch_on ? "LATCH  OCT-:ALL OFF" : "HOLD  OCT+:LATCH", C_DIM);
        return;
    }
    if (cz.mode == CM_HOME && cos_home_held()) {          /* (HOME held: the tempo's keys) */
        cv_rect(2, 20, cos_w("TEMPO") + 6, 12, C_FG);
        cos_tx(5, 22, "TEMPO", C_BG);
        cos_txr(238, 22, "OCT NUDGE K1 .1 W16 TAP", C_DIM);
        return;
    }
    if (cos_on_live() && cos_nrm.ok) {                   /* (0.10.3) LIVE moved off NORMAL: how to go back */
        cv_rect(2, 20, cos_w("LIVE") + 6, 12, C_RED);
        cos_tx(5, 22, "LIVE", C_BG);
        cos_txr(238, 22, live_qback ? "NORMAL COMING" : "MOVED  OCT- NORMAL", C_RED);
        return;
    }
    if (cz.mode == CM_TRIG && cos_held() && !cos_on_cnd()) {   /* (steps held: what a knob locks) */
        char b[24];
        str_cpy(b, "LOCK ", sizeof b);
        str_cpy(b + 5, cos_lk_page()->tab, 12);
        cv_rect(2, 20, cos_w(b) + 6, 12, C_RED);
        cos_tx(5, 22, b, C_BG);
        return;
    }
    cos_pages(cz.mode, &p, &n);
    {
        uint32_t cur = cz.tab[cz.mode] % n, f = 0, j, k = 0, more = 0;
        const engine_t *e = trk_engp(TSEL);
        const char *nm[CZ_TABS];                         /* (0.9.2: 16, MIX has 14; was 12, past its end) */
        int32_t span;
        if (n > CZ_TABS) n = CZ_TABS;
        cur = cz.tab[cz.mode] % n;
        for (i = 0, j = cur; i < n; i++) {     /* (EDIT's first two: the engine's own names for them; 0.16.0: the
                                                         * everyday pages and the one in use, the others counted) */
            if (!cos_ess(cz.mode, p, i, n) && i != j) { more++; continue; }
            if (i == j) cur = k;
            nm[k++] = cz.mode == CM_SRC && i < 2u && !cos_kit() && e->page_title[i] ? e->page_title[i] : p[i].tab;
        }
        n = k;
        do {                                             /* (the first one shown: the page picked fits) */
            for (span = 0, j = f; j <= cur; j++) span += cos_w(nm[j]) + 10;
        } while (span > 236 && ++f < cur);
        if (f) cos_tx(0, 22, "<", C_DIM);
        for (i = f, x = f ? 10 : 2; i < n; i++) {
            int32_t w = cos_w(nm[i]) + 6;
            if (x + w > 240) { cos_txr(240, 22, ">", C_DIM); break; }
            if (i == cur) { cv_rect(x, 20, w, 12, C_FG); cos_tx(x + 3, 22, nm[i], C_BG); }
            else cos_tx(x + 3, 22, nm[i], C_DIM);
            x += w + 4;
        }
        if (more && !(cz.mode == CM_TRIG && cos_banks() > 1u)) {   /* (the deep pages: SELECT) */
            char b[6];
            b[0] = '+';
            b[1] = (char)(more >= 10u ? '0' + more / 10u : '0' + more);
            b[2] = more >= 10u ? (char)('0' + more % 10u) : 0;
            b[3] = 0;
            if (x + cos_w(b) <= 238) cos_txr(238, 22, b, C_DIM);
        }
    }
    if (cz.mode == CM_TRIG && cos_banks() > 1u) {       /* the bank of 16: 1/4 */
        char b[8];
        b[0] = (char)('1' + cz.bank); b[1] = '/'; b[2] = (char)('0' + cos_banks()); b[3] = 0;
        cos_txr(238, 22, b, C_FG);
    }
}

/* the dial of knob k at (cx, cy): its arc 270 degrees, the value's part bright (red while turned), the label under it
 * and the value under that */
/* where the dial shows v, lo .. hi: right is more, as the knob turns it (param_turn): CHANCE's F_PCTINV the other way
 * round, an ordered list (enum_order) at its place in the order shown, the delay's TIME by length */
static int32_t cos_dial_at(const param_desc_t *d, int32_t v)
{
    if (d->names == N_DTIME) return d->min + cos_dt_rank(v - d->min);
    if (d->fmt == F_PCTINV) return d->max + d->min - v;
    if (enum_order(d)) return enum_rank(d, v);
    return v;
}
static void cos_dial(int32_t cx, int32_t cy, uint32_t k)
{
    uint64_t held = cz.mode == CM_TRIG && !cos_on_cnd() ? cos_held() : 0u;   /* (TRIG with steps held: the lock page, the
                                                         * first one's locks; on COND: COND itself) */
    const cpg_t *p = held ? cos_lk_page() : cos_page();
    int16_t *vp, lv;
    const param_desc_t *d = cos_knob(p, k, &vp);
    char val[24], b[16];
    const char *unit = 0;
    int32_t a, v, pv, r = 12, lo, hi, z;
    uint32_t hot = cz.hot == k + 1u, locked = 0;
    if (!d || !vp) {
        cos_txc(cx, cy + r + 4, CZ_LBL, "-", C_DARK);
        return;
    }
    v = *vp;
    if (held && p->kind == CK_TRK && motion_param(p->id[k & 3u])) {   /* (0.9.2: a lockable one only, as
                                                         * cos_lock_turn: OCT and the like read nothing of the motion's) */
        uint32_t i;
        for (i = 0; i < NSTEP && !((held >> i) & 1u); i++) {}
        locked = (uint32_t)motion_lock_get(TSEL, i, p->id[k & 3u], &lv);
        v = locked ? lv : motion_base_value(TSEL, p->id[k & 3u]);
        hot |= locked;                                     /* (a locked value: red, the live thing) */
    }
    lo = d->min; hi = d->max;
    z = d->fmt == F_BIPCT || d->fmt == F_SEMI || (lo < 0 && hi > 0) ? 0 : lo;   /* (bipolar: from the middle) */
    pv = cos_dial_at(d, v);                               /* (the pointer: right is more) */
    for (a = 0; a <= 40; a++) {                           /* 40 dots round 270 degrees, from 7:30 to 4:30 */
        uint32_t ang = (uint32_t)(0x6000 + a * 0xC000 / 40) & 0xFFFFu;
        int32_t x = cx + ((r * og_cos(ang)) >> 15), y = cy + ((r * og_sin(ang)) >> 15);
        int32_t at = a * (hi - lo), zz = (z - lo) * 40, pp = (pv - lo) * 40;   /* (x 40: a short range, an enum or
                                                         * OFF / ON, lit to its pointer exactly; was rounded down, past it) */
        int32_t on = hi > lo && pv != z && ((at >= zz && at <= pp) || (at <= zz && at >= pp));
        cv_rect(x - 1, y - 1, 2, 2, on ? (hot ? C_RED : C_FG) : C_DARK);
    }
    {                                                     /* the pointer */
        uint32_t ang = (uint32_t)(0x6000 + (hi > lo ? (pv - lo) * 0xC000 / (hi - lo) : 0)) & 0xFFFFu;
        cv_line(cx, cy, cx + (((r - 5) * og_cos(ang)) >> 15), cy + (((r - 5) * og_sin(ang)) >> 15), C_FG);
    }
    str_cpy(b, d->label, sizeof b);
    cos_up(b);
    if (str_len(b) > 7u) b[7] = 0;
    cos_txc(cx, cy + r + 4, CZ_LBL, b, C_MID);
    param_format(d, v, val, &unit);
    if (p->id[k & 3u] == ID_BPM) fmt_int(val, v / 100);   /* (0.10.1: the whole BPM; its hundredths on FINE) */
    if (p->id[k & 3u] == ID_BFINE) { val[0] = '.'; val[1] = (char)('0' + v / 10 % 10); val[2] = (char)('0' + v % 10); val[3] = 0; }
    if (p->kind == CK_COS && p->id[k & 3u] == CC_MOCC && v < 0) { str_cpy(val, "OFF", sizeof val); unit = 0; }
    if (p->kind == CK_COS && p->id[k & 3u] == CC_DSRC) {   /* (0.12.1) its name; edited since (or another kit): OWN */
        cos_dsrc_name(cos_dsrc_same(dx_ui_lane) ? (uint32_t)v : 0u, val);
        unit = 0;
    }
    if (p->kind == CK_COS && p->id[k & 3u] == CC_TSND) {   /* (0.14.0) the sound lock's name; none: -- */
        if (v > 0) cos_dsrc_name((uint32_t)v, val); else str_cpy(val, "--", 3);
        unit = 0;
    }
    if (p->kind == CK_COS && p->id[k & 3u] == CC_IDLE) {   /* (OFF, else "5 MIN") */
        if (v <= 0) str_cpy(val, "OFF", sizeof val);
        else { fmt_int(val, v); str_cpy(val + str_len(val), " MIN", 5); }
        unit = 0;
    }
    cos_up(val);
    if (str_len(val) > 7u) val[7] = 0;
    cos_txc(cx, cy + r + 15 + (str_len(val) > 5u ? 3 : 0), str_len(val) > 5u ? CZ_LBL : CZ_TALL, val, hot ? C_RED : C_FG);   /* (a long word in the
                                                         * small letters: whole) */
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
        if (v == 1u || v == 2u) {                        /* (a trig grey, an accent white: red is the playhead's; 0.12.4: a
                                                         * step with its own LENGTH under 1, as wide as it) */
            uint32_t sl = lane == NLANE && !drum_track(t) ? CT_LEN(&t->step[st % NSTEP]) : 0u;
            cv_rect(x + 1, y + 1, sl ? 1 + (int32_t)sl * 10 / 15 : 11, 11, v == 1u ? C_MID : C_FG);
        }
        else if (v == 3u) cv_rect(x + 1, y + 5, 11, 3, C_DIM);
        if (st == now) cv_rect(x, y - 3, 13, 2, C_RED);
        if (cz.si && cz.mode == CM_TRIG && st == cz.si_pos) { cv_frame(x - 1, y - 1, 15, 15, C_RED); cv_frame(x, y, 13, 13, C_RED); }
        if (st < len && ((lks >> st) & 1u)) cv_rect(x + 5, y + 14, 3, 2, C_FG);   /* (its locks) */
    }
}

/* HOME's name of a track: the role its sound plays (BASS, PAD, LEAD .. by the preset's type), DRUM ACID VOICE by their
 * engine, else the engine's name; 6 letters */
static void cos_trk_label(const track_t *tr, char *b)
{
    static const char *const TAGN[PT_N] = {"", "BASS", "LEAD", "PAD", "KEYS", "PLUCK", "BELL", "VOICE", "DRUM", "FX"};
    const engine_t *e = ENGINES[trk_eng(tr)];
    uint32_t tg = PT_NONE;
    if (e != &ENG_DRUM && e != &ENG_KICK && e != &ENG_ACID && e != &ENG_FORMANT && !tr->user && e->npresets && tr->preset < e->npresets)
        tg = PR_TAG(&e->presets[tr->preset]);
    str_cpy(b, tg && tg < PT_N ? TAGN[tg] : e->name, 7);
    b[6] = 0;
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
        cos_trk_label(tr, b);
        cos_up(b);
        cos_tx(14, y + 1, b, ((live_qmute >> t) & 1u) && (fm1_ms & 256u) ? C_RED : mute ? C_DIM : C_FG);   /* (a mute
                                                         * asked for: blinking until it goes in) */
        if ((perf_solo >> t) & 1u) cv_rect(58, y + 1, 3, 7, C_RED);   /* (soloed) */
        if (LV_ON(t)) cv_rect(54, y + 3, 3, 3, C_RED);   /* (LOOP: playing modified) */
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
    {                                                    /* the line under them: what is coming, else what the keys do */
        static const char *const BS[6] = {"", "BREAK NEXT BAR", "BREAKDOWN", "BUILD NEXT BAR", "BUILD", "DROP NEXT BAR"};
        char b[32];
        b[0] = 0;
        if (live.req >= 0) { str_cpy(b, "A NEXT BAR", sizeof b); b[0] = (char)('A' + live.req); }
        else if (bd_state != BD_OFF) {
            str_cpy(b, BS[bd_state % 6u], sizeof b);
            if (bd_state == BD_BUILD) { uint32_t l = str_len(b); b[l] = ' '; b[l + 1] = (char)('0' + bd_bars % 10u); b[l + 2] = 0; }
        } else if (fill_on || fill_req) str_cpy(b, "FILL", sizeof b);
        if (!b[0] && mph.gl) {                            /* (a MORPH gliding: where it goes, how far) */
            str_cpy(b, mph.to ? "TO B " : "TO A ", sizeof b);
            if (!mph.run || (int32_t)((song.playing ? ms_clock : fm1_ms) - mph.t0) < 0) str_cpy(b + 5, "NEXT BAR", 20);
            else { uint32_t q = (uint32_t)(mph.to ? mph.pos : 1000 - mph.pos) / 10u;   /* (how far: a percent) */
                   fmt_int(b + 5, (int32_t)q); str_cpy(b + str_len(b), "%", 2); }
        }
        if (!b[0] && (cos_home_held() || cos_on_live())) {   /* (0.10.0) HOME held, (0.10.3) LIVE: white 1..8, the drums'
                                                         * sounds (out: dim) */
            static const char *const AB[8] = {"KK", "SN", "CP", "HH", "OH", "TM", "RM", "BL"};
            uint32_t s;
            for (s = 0; s < 8u; s++) {
                int out = cos_drum_out(s);
                cos_tx(4 + (int32_t)s * 29, 144, AB[s], out ? C_DARK : C_FG);
                if (out) cv_rect(3 + (int32_t)s * 29, 147, 14, 1, C_RED);
            }
        } else if (b[0]) cos_tx(4, 144, b, C_RED);
        else if (dx_mute & 0xFE00u) {                    /* (0.10.0) a drum sound out: which */
            static const char *const AB[8] = {"KK", "SN", "CP", "HH", "OH", "TM", "RM", "BL"};
            uint32_t s;
            str_cpy(b, "DRUMS OUT:", sizeof b);
            for (s = 1; s < 8u; s++)
                if (dx_mute & DXM_LANE(s)) { str_cpy(b + str_len(b), " ", 2); str_cpy(b + str_len(b), AB[s], 3); }
            cos_tx(4, 144, b, C_MID);
        }
        else if (cz.tab[CM_HOME] == 6u)                  /* (LOOP) */
            cos_tx(4, 144, "W15 BACK  HOME+W15 ALL BACK", C_DIM);
        else if (cz.tab[CM_HOME] == 5u)                  /* (MORPH: the keys; A and B lit when stored) */
            cos_tx(4, 144, (mph.set & 3u) == 3u ? "W13 A  W14 B  HOME+ STORES" : "W13 / W14: STORE A / B", C_DIM);
        else cos_tx(4, 144, trk_live > 5u ? "B7 FILL B8 DROP B9-11 KILL" : "B7 FILL B8 BREAK B9-11 KILL", C_DIM);
        if (live.cur >= 0 && !b[0]) { char c[2] = {(char)('A' + live.cur), 0}; cos_txr(236, 144, c, C_MID); }
    }
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
    if (!cos_kit()) {                                    /* (a synth: the first held step's note, ACCENT, SLIDE: cos_step_note) */
        const step_t *s = &TSEL->step[st];
        int nt = s->time == ST_NOTE && s->n;
        if (n == 1u) {
            str_cpy(b + 7, " ", 2);
            if (nt) note_name(b + 8, s->note[0]); else str_cpy(b + 8, "REST", 5);
        }
        cv_rect(28, 58, 184, 76, C_BG);
        cv_frame(28, 58, 184, 76, C_RED);
        cos_txc(120, 64, CZ_TALL, b, C_FG);
        cos_tx(44, 86, "B1 ACCENT", nt && (s->flags & SF_ACCENT) ? C_RED : C_DIM);
        cos_tx(132, 86, "B2 SLIDE", nt && (s->flags & SF_SLIDE) ? C_RED : C_DIM);
        {                                                /* (0.12.4) B3 / B4: its LENGTH */
            uint32_t v = nt ? cos_slen_get(TSEL, st, (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP)) : 0u;
            char lb[24];
            str_cpy(lb, "B3- LEN ", 9);
            if (!v) str_cpy(lb + 8, "GATE", 5);
            else if (v >= 32u) { fmt_int(lb + 8, (int32_t)(v / 16u)); str_cpy(lb + str_len(lb), " STEPS", 7); }
            else { fmt_int(lb + 8, (int32_t)v); str_cpy(lb + str_len(lb), "/16", 4); }
            str_cpy(lb + str_len(lb), " +B4", 5);
            cos_txc(120, 99, CZ_LBL, lb, v ? C_RED : C_DIM);
        }
        cos_txc(120, 110, CZ_LBL, "PRESETS: THE NOTE", C_MID);
        cos_txc(120, 121, CZ_LBL, cos_on_cnd() ? "KNOBS: COND, SLIDE" : "KNOB LOCK  REC CLEAR", C_DIM);
        return;
    }
    cv_rect(28, 62, 184, 52, C_BG);
    cv_frame(28, 62, 184, 52, C_RED);
    cos_txc(120, 70, CZ_TALL, b, C_FG);
    if (cos_on_cnd()) {                                  /* (0.14.0) COND: SOUND is the lane's (the black keys pick it) */
        str_cpy(b, "SOUND ON ", 10);
        str_cpy(b + 9, drum_lane_name(TSEL, cz.lane % NLANE), 7);
        cos_txc(120, 92, CZ_LBL, b, C_MID);
        cos_txc(120, 103, CZ_LBL, "BLACK KEYS: THE DRUM", C_DIM);
    } else
        cos_txc(120, 98, CZ_LBL, "KNOB LOCK  REC CLEAR", C_DIM);
}

static void cos_body_trig1(void);
static void cos_body_trig(void)
{
    if (cz.mode == CM_TRIG && cos_page()->kind == CK_EU && !cos_kit()) {   /* (EUCLID on a synth: its say, the notes
                                                         * it wrote under; a kit: its lanes, as TRIG) */
        cos_tx(4, 38, "NOTES IN THE SCALE", C_MID);
        cos_txr(236, 38, "OCT+ NEW", C_DIM);
        cz_rtop = 54;
        cos_body_trig1();
        cz_rtop = 40;
        return;
    }
    cos_body_trig1();
    if (cz.si) {                                         /* (STEP INPUT: what the keys do now) */
        cv_rect(84, 20, 156, 12, C_RED);
        cos_tx(90, 22, "KEYS IN  REC: REST", C_BG);
    }
    cos_lk_box();
}
static void cos_body_trig1(void)
{
    const track_t *t = TSEL;
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, l, now = song.playing ? t->seq_idx % len : ~0u;
    if (cos_kit()) {
        for (i = 4; i < 16u; i += 4u)                    /* (0.14.1, GG: "a 16th grid") the beats apart: a line between */
            cv_rect(26 + (int32_t)i * 13 - 2, 36, 1, NLANE * 14, C_DARK);
        for (l = 0; l < NLANE; l++) {
            int32_t y = 38 + (int32_t)l * 14;
            if (l == cz.lane) cv_rect(0, y - 2, 240, 13, C_DARK);
            cos_tx(4, y + 1, COS_LN[l], l == cz.lane ? C_RED : C_MID);
            for (i = 0; i < 16u; i++) {
                uint32_t st = cz.bank * 16u + i, v = st < len ? cos_step(t, st, l) : 0u;
                int32_t x = 26 + (int32_t)i * 13;
                if (v && t->step[st % NSTEP].probability >= TC_BASE) cv_frame(x, y, 10, 9, v == 2u ? C_FG : C_MID);   /* (0.14.0:
                                                         * a CONDITION: hollow) */
                else if (v) cv_rect(x, y, 10, 9, v == 2u ? C_FG : C_MID);
                else if (st < len && !(i % 4u)) cv_rect(x + 3, y + 3, 4, 4, C_DIM);   /* (a beat's sixteenth: larger) */
                else cv_rect(x + 4, y + 4, 2, 2, st < len ? C_DIM : C_BG);
                if (v && cos_snd_of(t, st % NSTEP, l)) cv_rect(x + 7, y + 1, 2, 2, C_RED);   /* (a sound lock: a red dot) */
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
        for (i = 0; i < 16u; i++) {                      /* (0.14.1, GG: "a 16th grid") a column a sixteenth: the beats
                                                         * a line, the others dotted; past LEN none */
            int32_t x = 2 + (int32_t)i * 15, yy;
            if (cz.bank * 16u + i >= len) break;
            if (!(i % 4u)) cv_rect(x - 1, cz_rtop - 4, 1, 154 - cz_rtop, C_DARK);
            else for (yy = cz_rtop - 4; yy < 150; yy += 8) cv_rect(x - 1, yy, 1, 1, C_DARK);
            cv_rect(x + 4, 148, 4, i % 4u ? 1 : 3, i % 4u ? C_DIM : C_MID);   /* (a tick under it) */
        }
        for (i = 0; i < 16u; i++) {
            uint32_t st = cz.bank * 16u + i, g = 1;
            const step_t *s = &t->step[st % NSTEP];
            int32_t x = 2 + (int32_t)i * 15, y;
            if (st == now) cv_rect(x, cz_rtop - 4, 13, 150 - cz_rtop, C_DARK);
            if (st >= len || s->time != ST_NOTE || !s->n) continue;
            while (i + g < 16u && t->step[(st + g) % NSTEP].time == ST_TIE) g++;
            y = 140 - (s->note[0] - lo) * (140 - cz_rtop) / (hi - lo);
            if (s->probability >= TC_BASE)               /* (0.14.0) a CONDITION: hollow */
                cv_frame(x, y - 1, 15 * (int32_t)g - 2, (s->flags & SF_ACCENT) ? 7 : 5, (s->flags & SF_ACCENT) ? C_FG : C_MID);
            else
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
    if (cos_on_words()) {
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
    if (cz.mode == CM_SRC && cos_page()->id[0] >= P_M1SRC && cos_page()->id[0] <= P_M4SRC) {   /* MOD: the matrix */
        uint32_t k, cur = (uint32_t)(cos_page()->id[0] - P_M1SRC) / 3u;
        for (k = 0; k < 4u; k++) {
            uint32_t id = P_M1SRC + 3u * k;
            int32_t y = 40 + (int32_t)k * 24;
            const char *u = 0;
            char w[16];
            uint16_t c = k == cur ? C_FG : C_MID;
            if (k == cur) cv_rect(0, y - 4, 240, 20, C_DARK);
            b[0] = (char)('1' + k); b[1] = 0;
            cos_tx(6, y, b, c);
            if (!t->p[id]) { cos_tx(28, y, "OFF", C_DIM); continue; }
            param_format(track_desc(t, id), t->p[id], w, &u); cos_up(w); w[6] = 0;
            cos_tx(28, y, w, c);
            cos_tx(84, y, ">", C_DIM);
            param_format(track_desc(t, id + 1u), t->p[id + 1u], w, &u); cos_up(w); w[8] = 0;
            cos_tx(100, y, w, c);
            param_format(track_desc(t, id + 2u), t->p[id + 2u], w, &u); cos_up(w); w[5] = 0;
            cos_txr(234, y, w, c);
        }
        return;
    }
    if (cz.mode == CM_FX && cos_page()->kind == CK_GLO) {  /* DELAY, REVERB: the buses every track sends into */
        str_cpy(b, cos_page()->tab, 12);
        cos_txc(120, 52, CZ_TALL, b, C_FG);
        cos_txc(120, 84, CZ_LBL, "ONE FOR ALL TRACKS", C_MID);
        cos_txc(120, 100, CZ_LBL, "HOW MUCH GOES IN: SEND", C_DIM);
        cos_txc(120, 114, CZ_LBL, "(EACH TRACK ITS OWN)", C_DIM);
        return;
    }
    if (cz.mode == CM_AMP && cos_page()->tab[0] == 'L') {   /* LFO 1, LFO 1+, LFO 2: two cycles of the shape, where it is */
        int lf2 = cos_page()->tab[4] == '2';
        uint32_t w = lf2 ? (uint32_t)trk_x2[song.sel % NTRK][X2_WAVE] : (uint32_t)t->p[P_LWAVE], ph = lf2 ? t->lfo2_ph : t->lfo_ph;
        int32_t x, y0 = 0;
        cv_rect(20, 92, 200, 1, C_DARK);
        for (x = 0; x < 200; x++) {
            uint32_t q = ((uint32_t)x % 100u) * (0xFFFFFFFFu / 100u), cyc = (uint32_t)x / 100u;
            int32_t y = 92 - (lfo_shape(w, q, (cyc + 3u) * 2654435761u, (cyc + 2u) * 2654435761u) * 40 >> 15);
            if (x) cv_line(19 + x, y0, 20 + x, y, C_FG);
            y0 = y;
        }
        x = (int32_t)(ph >> 25);                          /* (0 .. 127 of a cycle: the first one) */
        cv_rect(20 + x * 100 / 128 - 1, 92 - (lfo_shape(w, ph, 3u * 2654435761u, 2u * 2654435761u) * 40 >> 15) - 1, 3, 3, C_RED);
        cos_txc(120, 140, CZ_LBL, N_LWAVE[w % (sizeof N_LWAVE / sizeof N_LWAVE[0])], C_MID);
        return;
    }
    if (cz.mode == CM_AMP && cos_page()->tab[0] == 'F') {   /* FILTER 1, FILTER 2: the curve, CUT where it bends */
        int f2 = cos_page()->tab[7] == '2';
        const volatile int8_t *x2 = trk_x2[song.sel % NTRK];
        int32_t ty = f2 ? x2[X2_F2T] : t->p[P_FTYPE] + 1, cut = f2 ? x2[X2_F2C] : (trk_engp(t) == &ENG_ANALOG ? t->p[P_E4] : t->p[P_FCUT]),
                res = f2 ? x2[X2_F2R] : (trk_engp(t) == &ENG_ANALOG ? t->p[P_E5] : t->p[P_FRES]), x, y0 = 0, xc = 20 + cut * 200 / 127;
        static const char *const FT1[] = {"LP", "HP", "BP", "COMB", "COMB-", "MS20"};
        for (x = 20; x < 220; x++) {
            int32_t d = x - xc, a = d < 0 ? -d : d, pk = res * 30 / 127, y;
            int32_t lp = d < 0 ? 0 : d * 2, hp = d > 0 ? 0 : -d * 2;
            y = ty == 2 ? hp : ty == 3 ? a * 2 : ty == 4 ? (a < 12 ? 40 - a * 3 : 0) : ty >= 5 ? ((x / 8) & 1) * 10 : ty == 0 ? 0 : lp;
            if (ty && ty != 4 && ty < 5) y -= a < 12 ? pk * (12 - a) / 12 : 0;   /* (the resonance's peak at CUT) */
            y = 74 + clamp(y, -30, 60);
            if (x > 20) cv_line(x - 1, y0, x, y, ty ? C_FG : C_DIM);
            y0 = y;
        }
        if (ty) cv_rect(xc, 140, 1, 4, C_RED);
        cos_txc(120, 140, CZ_LBL, f2 ? N_F2TYPE[ty % 5] : FT1[(ty - 1) % 6], C_MID);
        return;
    }
    if (trk_engp(t)->ownenv && cz.mode == CM_AMP && (cz.tab[CM_AMP] == 0u || cos_page()->id[1] == P_ED_PIT)) {
        str_cpy(b, trk_engp(t)->name, 16);
        cos_up(b);
        cos_txc(120, 64, CZ_TALL, b, C_FG);
        cos_txc(120, 96, CZ_LBL, "HAS ITS OWN ENVELOPES:", C_MID);
        cos_txc(120, 110, CZ_LBL, "SHAPE THEM ON EDIT", C_MID);
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
    if (cz.mode == CM_SRC && cos_kit() && cos_page()->kind == CK_COS && cos_page()->id[0] != CC_LN0) {
        uint32_t l = dx_ui_lane & 7u;                    /* a drum's page: which drum, and how to pick another */
        const uint8_t *de = dx_de[l];
        cos_txc(120, 52, CZ_TALL, COS_DRUM[l], C_FG);
        if (cos_page()->id[0] == CC_DE0 + DE_ATK || cos_page()->id[0] == CC_DE0 + DE_HLD) {   /* its envelope drawn */
            int32_t a = de[DE_ATK] * 40 / 127, h = de[DE_HLD] * 30 / 127, d = 10 + dx_kit[l].a[DXP_DECAY] * 50 / 127;
            int32_t su = de[DE_SUS], top = 80, y0 = 136, sy = y0 - (y0 - top) * su / 127, x = 30, r = de[DE_REL] * 40 / 127;
            cv_line(x, y0, x + a, top, C_FG); x += a;
            cv_line(x, top, x + h, top, C_FG); x += h;
            if (su) {
                cv_line(x, top, x + d, sy, C_FG); x += d;
                cv_line(x, sy, 200 - r, sy, C_FG);
                cv_line(200 - r, sy, 200, y0, C_FG);
            } else {
                cv_line(x, top, x + d * 2, y0, C_FG);
            }
            cv_rect(30, y0, 172, 1, C_DARK);
            cos_txc(120, 142, CZ_LBL, su ? "HELD: SUSTAIN, THEN RELEASE" : "SUSTAIN OFF: ONE SHOT", C_DIM);
        } else if (cos_page()->id[0] == CC_DE0 + DE_SDL) {
            cos_txc(120, 88, CZ_LBL, "THIS DRUM'S OWN SENDS", C_MID);
            cos_txc(120, 104, CZ_LBL, "KIT: AS FX > SEND (THE KIT'S)", C_DIM);
            cos_txc(120, 118, CZ_LBL, "0: DRY (THE KICK ON ITS OWN)", C_DIM);
        } else {
            cos_txc(120, 96, CZ_LBL, "STRIKE A DRUM'S KEY TO EDIT IT", C_DIM);
        }
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
    const uint8_t *BL = COS_BLF;
    uint32_t i, cur = cos_key(), nx = cz.key_next ? cz.key_next - 1u : 99u;
    char b[24];
    const track_t *t = TSEL;
    uint32_t mask = 0;
    for (i = 0; i < NTRK && cos_hits(t); i++) t = &trk[i];   /* (the first melodic track's scale: all share it) */
    if (!cos_hits(t) && t->p[P_SCALE]) mask = scale_mask(t);
    str_cpy(b, COS_NN[cur], sizeof b);
    if (!cos_hits(t)) { str_cpy(b + str_len(b), " ", 2); str_cpy(b + str_len(b), N_SCALE[clamp(t->p[P_SCALE], 0, 15)], 7); }
    cos_px(20, 46, b, C_FG, CZ_TALL);
    cos_tx(20, 74, mask ? "KEY AND SCALE: THE KEYS LIT" : "THE SONG'S KEY", C_DIM);
    if (nx < 12u) { str_cpy(b, "NEXT BAR: ", sizeof b); str_cpy(b + str_len(b), COS_NN[nx], 4); cos_tx(110, 52, b, C_RED); }
    for (i = 0; i < 12u; i++) {                         /* white keys a row, black keys above between them */
        uint32_t pc = (5u + i) % 12u, wi = 0, j;
        int on = pc == cur, nxt = pc == nx, in = mask && ((mask >> ((pc + 12u - cur) % 12u)) & 1u);
        for (j = 0; j < i; j++) wi += !BL[j];
        if (BL[i]) cv_rect(14 + (int32_t)wi * 30 - 9, 92, 16, 26, nxt ? C_RED : on ? C_FG : in ? C_MID : C_DARK);
        else {
            cv_frame(14 + (int32_t)wi * 30, 92, 28, 50, C_DIM);
            if (on || nxt) cv_rect(16 + (int32_t)wi * 30, 122, 24, 18, nxt ? C_RED : C_FG);
            else if (in) cv_rect(16 + (int32_t)wi * 30, 128, 24, 12, C_DIM);   /* (in the scale) */
        }
    }
    cos_tx(4, 146, "A KEY: THE SONG MOVES THERE", C_DIM);
}

/* (0.13.0) GLO > MIDI 2: the knobs as Ableton's controller */
static void cos_body_ctl(void)
{
    char b[32];
    uint32_t m = cos_ctl_mode();
    cos_tx(4, 40, m == 0u ? "KNOBS: THE FM-1 ONLY" : m == 1u ? "KNOBS: THE FM-1 AND A CC OUT" : "KNOBS: A CC OUT ONLY", C_FG);
    cos_tx(4, 56, m == 2u ? "ABLETON: RELATIVE (2'S COMP)" : "ABLETON: ABSOLUTE", C_MID);
    cos_tx(4, 72, "CC 20 + PAGE x 4 + KNOB", C_DIM);
    str_cpy(b, "CH ", 4); fmt_int(b + 3, (int32_t)cos_ctl_ch() + 1);
    str_cpy(b + str_len(b), " + THE MODE (HOME 0..)", 23);
    cos_tx(4, 86, b, C_DIM);
    if (cos_ctl_last) {
        str_cpy(b, "LAST: CH ", 10); fmt_int(b + 9, (int32_t)((cos_ctl_last >> 8) & 15u) + 1);
        str_cpy(b + str_len(b), " CC ", 5); fmt_int(b + str_len(b), (int32_t)(cos_ctl_last & 127u));
        cos_tx(4, 108, b, C_RED);
    }
    cos_tx(4, 128, "MAP: CMD/CTRL+M, CLICK, TURN", C_DIM);
    str_cpy(b, "EXT (NOT HEARD HERE):", 22);
    {
        uint32_t k, x = favorites.factory[15][3];
        for (k = 0; k < NTRK; k++) if ((x >> k) & 1u) { char c[4] = " T1"; c[2] = (char)('1' + k); str_cpy(b + str_len(b), c, 4); }
        if (!(x & ((1u << NTRK) - 1u))) str_cpy(b + str_len(b), " NONE", 6);
    }
    cos_tx(4, 142, b, (favorites.factory[15][3] & ((1u << NTRK) - 1u)) ? C_RED : C_DIM);
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
    cos_tx(4, 128, favorites.factory[15][0] == 1u ? "LOCAL OFF: KEYS PLAY THE COMPUTER" :
                   (midi_out_mode & 3u) == 2u ? "NOTES GO OUT: SYNTAKT CAN REC" : "ALL: THE NOTES GO OUT TOO",
           favorites.factory[15][0] == 1u ? C_RED : C_DIM);
}

static void cos_body_gen(void)
{
    if (cos_kit()) { cos_body_trig(); return; }
    if (cos_page()->kind == CK_DB) {                     /* DUB: the chord, its key, the move */
        static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        static const char *const MV[3] = {"STATIC", "2 CHORDS", "4 CHORDS"};
        const db_t *a = db_of(song.sel);
        char b[24];
        str_cpy(b, NN[db_root(song.sel, a)], sizeof b);
        str_cpy(b + str_len(b), " ", 2);
        str_cpy(b + str_len(b), DB_SHN[a->shape % 7u], 8);
        str_cpy(b + str_len(b), "  ", 3);
        str_cpy(b + str_len(b), MV[a->move % 3u], 10);
        cos_tx(4, 38, b, C_FG);
        cos_txr(236, 38, "OCT+ NEW", C_DIM);
        cz_rtop = 54;
        cos_body_trig();
        cz_rtop = 40;
        return;
    }
    if (cos_page()->kind == CK_BL) {                     /* (0.15.1) BLEEP: its seed, its key */
        static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        const bl_t *a = bl_of(song.sel);
        eu_t key;
        ac_t dm;
        char b[24];
        memset(&dm, 0, sizeof dm);
        ac_key(song.sel, &dm, &key);
        str_cpy(b, "SEED ", sizeof b);
        fmt_int(b + 5, a->seed);
        str_cpy(b + str_len(b), "  ", 3);
        str_cpy(b + str_len(b), NN[key.root % 12u], 3);
        cos_tx(4, 38, b, C_FG);
        cos_txr(236, 38, cos_page()->id[0] == BK_EVERY ? "OCT+ MUTATE" : "OCT+ NEW", C_DIM);
        cz_rtop = 54;
        cos_body_trig();
        cz_rtop = 40;
        return;
    }
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

/* (0.10.2) SAVE > SOUNDS: six slots around the one picked: U07, its name, its engine */
static void up_name(uint32_t k, char *b);
static __attribute__((noinline)) void cos_body_snd(void)
{
    uint32_t r, k0 = cz.uslot > 2u ? cz.uslot - 2u : 0u;
    char b[24];
    if (k0 + 6u > UP_SLOTS - 1u) k0 = UP_SLOTS - 1u - 6u;
    for (r = 0; r < 6u; r++) {
        uint32_t k = k0 + r;
        int32_t y = 36 + (int32_t)r * 14;
        int pick = k == cz.uslot, used = up_used(k);
        if (pick) cv_rect(0, y - 2, 240, 14, C_DARK);
        b[0] = 'U'; b[1] = (char)('0' + (k + 1u) / 10u); b[2] = (char)('0' + (k + 1u) % 10u); b[3] = 0;
        cos_tx(4, y, b, pick ? C_FG : C_MID);
        if (used) {
            up_name(k, b);
            cos_tx(36, y, b, pick ? C_FG : C_MID);
            cos_txr(236, y, ENGINES[up_engine(k) % NENGINES]->name, C_DIM);
        } else cos_tx(36, y, "EMPTY", C_DIM);
    }
    {
        char t[32] = "T1 ", n[24];
        t[1] = (char)('1' + song.sel % NTRK);
        sound_name(TSEL, n);
        n[14] = 0;
        str_cpy(t + 3, n, sizeof t - 3);
        cos_txc(120, 124, CZ_LBL, t, C_FG);
    }
    cos_txc(120, 140, CZ_LBL, cz.snd_ms && fm1_ms - cz.snd_ms < 3000u ? "AGAIN TO CONFIRM" :
                              "OCT+ SAVE OCT- LOAD REC2 DEL", cz.snd_ms && fm1_ms - cz.snd_ms < 3000u ? C_RED : C_DIM);
}
static __attribute__((noinline)) void cos_body_kits(void)   /* (0.13.0) SAVE > KITS: K01 .. K16, two columns */
{
    uint32_t k;
    char b[16];
    if (!cz.kit_seen) { kit_scan(); cz.kit_seen = 1; }
    for (k = 0; k < 16u; k++) {
        int32_t x = (int32_t)(k / 8u) * 120, y = 36 + (int32_t)(k % 8u) * 11;
        int pick = k == cz.kslot % 16u;
        if (pick) cv_rect(x, y - 2, 120, 11, C_DARK);
        b[0] = 'K'; b[1] = (char)('0' + (k + 1u) / 10u); b[2] = (char)('0' + (k + 1u) % 10u); b[3] = 0;
        cos_tx(x + 2, y, b, pick ? C_FG : C_MID);
        str_cpy(b, kit_is_used(k) ? kit_name(k) : "-", 11);
        b[10] = 0;
        cos_tx(x + 30, y, b, kit_is_used(k) ? (pick ? C_FG : C_MID) : C_DIM);
    }
    cos_txc(120, 128, CZ_LBL, "SOUNDS LEVELS ENV SENDS CHANCE", C_DIM);
    cos_txc(120, 140, CZ_LBL, cz.snd_ms && fm1_ms - cz.snd_ms < 3000u ? "AGAIN TO CONFIRM" :
                              "OCT+ SAVE OCT- LOAD REC2 DEL", cz.snd_ms && fm1_ms - cz.snd_ms < 3000u ? C_RED : C_DIM);
}
static void cos_body_proj(void)
{
    uint32_t i;
    char b[24];
    if (cz.tab[CM_PROJ] == 2u) { cos_body_kits(); return; }
    if (cz.tab[CM_PROJ] == 1u) { cos_body_snd(); return; }
    for (i = 0; i < 4u; i++) {
        int32_t y = 34 + (int32_t)i * 20;
        int pick = i == cz.slot, used = cz.pn[i][0] != 0;
        if (pick) cv_rect(0, y - 2, 240, 18, C_DARK);
        b[0] = (char)('A' + i); b[1] = 0;
        cos_px(8, y, b, pick ? C_FG : C_MID, CZ_TALL);
        cos_tx(32, y + 4, used ? cz.pn[i] : "EMPTY", used ? (pick ? C_FG : C_MID) : C_DIM);
        if (i == proj_cur_slot()) cos_txr(232, y + 4, "NOW", C_MID);
    }
    {                                                    /* the set list: the song these four are, the keys to the others */
        char sb[28] = "SONG 1 OF 8: 32 SLOTS";
        sb[5] = (char)('1' + song_cur % 8u);
        cos_txc(120, 126, CZ_LBL, sb, C_MID);
        cos_txc(120, 136, CZ_LBL, "KNOB 1 / W9-16: OTHER SONGS", C_DIM);
        str_cpy(sb, "NAME: ", sizeof sb);
        str_cpy(sb + 6, proj_name_buf()[0] ? proj_name_buf() : "NEW ON SAVE", sizeof sb - 6);
        cos_txc(120, 114, CZ_LBL, sb, C_FG);
    }
    cos_txc(120, 146, CZ_LBL, cz.new_ms && fm1_ms - cz.new_ms < 3000u ? "REC AGAIN: NEW PROJECT" :
                              song.playing ? "OCT+ STORE  OCT- NEXT BAR" : "OCT+ SAVE  OCT- LOAD  REC x2 NEW", cz.new_ms && fm1_ms - cz.new_ms < 3000u ? C_RED : C_DIM);
}

/* the FX pad (FX held): the 16 white keys' effects in two columns, the black keys' under them; an effect on: red */
static int cos_fx_on(uint32_t c)
{
    return c < CX_CODE ? (int)(((perf_held | perf_latched) >> c) & 1u) : (int)((cfx_now() >> (c - CX_CODE)) & 1u);
}
/* FX held: key k (0..26) lit, its effect on (the white keys CFX_W, the black CFX_B) */
static int cos_pad_lit(uint32_t k)
{
    uint32_t i;
    for (i = 0; i < 16u; i++) if (WK[i] == k) return cos_fx_on(CFX_W[i]);
    for (i = 0; i < 11u; i++) if (BK[i] == k) return cos_fx_on(CFX_B[i]);
    return 0;
}
static void cos_body_fxpad(void)
{
    static const char *const WN[16] = {"RPT 1/8", "RPT 1/16", "RPT 1/32", "GATE1/16", "GATE 1/8", "TAPESTOP", "GLITCH",
                                       "LPF", "HPF", "DRIVE", "CRUSH", "DLYTHROW", "REV WASH", "FREEZE", "HP RISER",
                                       "NOISE UP"};
    static const char *const BN[5] = {"KICK", "LOWS", "DRUM", "LP3", "LP7"};
    uint32_t i;
    int32_t x;
    char b[4];
    for (i = 0; i < 16u; i++) {
        int32_t y = 36 + (int32_t)(i % 8u) * 12;
        int act = cos_fx_on(CFX_W[i]);
        x = i < 8u ? 2 : 122;
        b[0] = (char)(i + 1u >= 10u ? '1' : ' '); b[1] = (char)('0' + (i + 1u) % 10u); b[2] = 0;
        cos_tx(x, y, b, C_DIM);
        if (act) cv_rect(x + 18, y - 2, cos_w(WN[i]) + 4, 11, C_RED);
        cos_tx(x + 20, y, WN[i], act ? C_BG : C_FG);
    }
    cos_tx(2, 132, "BLACK", C_DIM);                      /* the black keys: 1..6 the tracks muted, then the five */
    for (i = 0, x = 44; i < 6u; i++, x += 10) {
        int act = cos_fx_on(CFX_B[i]);
        b[0] = (char)('1' + i); b[1] = 0;
        if (act) cv_rect(x - 1, 130, 9, 11, C_RED);
        cos_tx(x, 132, b, act ? C_BG : C_MID);
    }
    cos_tx(x + 2, 132, "MUTE", C_DIM);
    {                                                    /* (7 .. 11 after it, on to the next line when the row is full) */
        int32_t y = 132;
        x += 2 + cos_w("MUTE") + 8;
        for (i = 0; i < 5u; i++) {
            int act = cos_fx_on(CFX_B[6u + i]);
            char n[12];
            n[0] = (char)(i + 7u >= 10u ? '1' : '0' + i + 7u);
            n[1] = (char)(i + 7u >= 10u ? '0' + (i + 7u) % 10u : 0); n[2] = 0;
            str_cpy(n + str_len(n), BN[i], 8);
            if (x + cos_w(n) > 238) { x = 2; y += 11; }
            if (act) cv_rect(x - 1, y - 2, cos_w(n) + 2, 11, C_RED);
            cos_tx(x, y, n, act ? C_BG : C_FG);
            x += cos_w(n) + 6;
        }
    }
}

/* a gain reduction meter: a bar and its dB (gr: Q8 octaves) */
static void cos_gr_meter(int32_t y, const char *what, int32_t gr, int on)
{
    int32_t db = gr * 6 / 256, w = clamp(gr * 160 / (256 * 4), 0, 160);   /* (Q8 octaves -> dB: ~6 an octave; full: 24 dB) */
    char b[16];
    cos_tx(8, y, what, C_DIM);
    cv_frame(8, y + 12, 164, 12, C_DIM);
    if (w) cv_rect(10, y + 14, w, 8, on ? C_RED : C_DIM);
    b[0] = db ? '-' : 0; b[1] = 0;
    fmt_int(b + (db ? 1 : 0), db);
    str_cpy(b + str_len(b), " DB", 4);
    cos_tx(180, y + 14, on ? b : "OFF", on ? C_FG : C_DIM);
}
/* GLO > COMP: the chain and the compressor's gain reduction now */
static void cos_body_master(void)
{
    cos_tx(8, 40, "IN>COMP>LEVEL>DRIVE>LIMIT", C_MID);
    cos_gr_meter(58, "COMP GAIN REDUCTION", comp_gr, mc_amt != 0u);
    cos_tx(8, 100, "SC HPF: NO KICK PUMPING", C_DIM);
    cos_tx(8, 112, "MIX UNDER 100: PARALLEL", C_DIM);
    cos_tx(8, 124, mc_amt ? "MAKEUP: AUTO LV OFF (LIMIT)" : "COMP OFF: NOT IN THE CHAIN", C_DIM);
}
/* GLO > LIMIT: its gain reduction now (the envelope over the ceiling) */
static void cos_body_lim(void)
{
    int32_t gr = lim_env > lim_t ? comp_lg2((uint32_t)lim_env) - comp_lg2((uint32_t)lim_t) : 0;
    cos_tx(8, 40, "DRIVE>LIMIT>SAT>OUT", C_MID);
    cos_gr_meter(58, "LIMIT GAIN REDUCTION", gr, 1);
    cos_tx(8, 88, "CEILING: THE PEAK OUT, DBFS", C_DIM);
    cos_tx(8, 100, "DRIVE: HARDER INTO CEILING", C_DIM);
    cos_tx(8, 112, "SAT: OFF CLEAN; ON A WARM", C_DIM);
    cos_tx(8, 124, "CLIP, SET WHEN YOU WANT IT", C_DIM);
}

/* FX > RUMBLE: what it is, and its level now (a meter of the return's peak, the kick's pump in it) */
/* FX > DJ: the DJ FILTER as a mixer's knob (LP left, HP right, the middle off) over the ISOLATOR's three bands */
static void cos_body_dj(void)
{
    static const char *const BN[3] = {"LOW", "MID", "HIGH"};
    int32_t f = song.g[G_DJF], x = 120 + f * 100 / 64, k;
    cos_txc(120, 38, CZ_LBL, "DJ FILTER", C_MID);
    cv_rect(20, 58, 200, 1, C_DARK);
    cv_rect(120, 52, 1, 13, C_DIM);
    cv_rect(x - 2, 52, 5, 13, f ? C_RED : C_FG);
    cos_tx(20, 68, "LP", C_DIM);
    cos_txr(220, 68, "HP", C_DIM);
    cos_txc(120, 88, CZ_LBL, "ISOLATOR", C_MID);
    for (k = 0; k < 3; k++) {                            /* each band: 0 dB the line, KILL empty, -24 .. +6 */
        int32_t v = iso_v[k], bx = 50 + k * 70, y0 = 128, h = v <= ISO_KILL ? 0 : (v + 24) * 30 / 30;
        cv_rect(bx - 14, y0 - 24, 28, 1, C_DARK);
        if (h) cv_rect(bx - 10, y0 - h, 20, h, v ? C_RED : C_FG);
        cos_txc(bx, y0 + 4, CZ_LBL, v <= ISO_KILL ? "KILL" : BN[k], v <= ISO_KILL ? C_RED : C_DIM);
    }
}
static void cos_body_rumble(void)
{
    int32_t w = rmb_v[0] ? clamp(rb.pk * 160 / 24000, 0, 160) : 0;
    cos_tx(8, 40, "KICK > LOWPASS > DARK VERB", C_MID);
    cos_tx(8, 58, "RUMBLE NOW", C_DIM);
    cv_frame(8, 70, 164, 12, C_DIM);
    if (w) cv_rect(10, 72, w, 8, C_RED);
    cos_tx(180, 72, rmb_v[0] ? "ON" : "OFF", rmb_v[0] ? C_FG : C_DIM);
    cos_tx(8, 100, "DUCK: DOWN AT THE KICK,", C_DIM);
    cos_tx(8, 112, "UP INTO THE OFFBEAT", C_DIM);
    cos_tx(8, 124, "KICK KILL: THE TAIL RINGS OUT", C_DIM);
}

/* GLO > LINK 2 / 3's page: what the macros send and take */
static void cos_body_link2(void)
{
    static const char *const MN[3][4] = {{"", "", "", ""}, {"FILTER FREQ", "FILTER RESO", "DELAY SEND", "REVERB SEND"},
                                         {"AFX FILTER", "AFX RESO", "DELAY FDBK", "REVERB DECAY"}};
    uint32_t m = cos_mc_map(), k;
    char b[32];
    if (!cos_mc_ch) cos_tx(8, 40, "MAC CH 0: OFF", C_DIM);
    else {
        str_cpy(b, "M1-M4 AS CCS ON CH ", sizeof b); fmt_int(b + str_len(b), cos_mc_ch);
        cos_tx(8, 40, b, C_FG);
    }
    for (k = 0; k < 4u; k++) {
        b[0] = 'M'; b[1] = (char)('1' + k); b[2] = ' '; fmt_int(b + 3, cos_mc_cc[k]);
        if (m) { str_cpy(b + str_len(b), " ", 2); str_cpy(b + str_len(b), MN[m][k], 16); }
        cos_tx(8, 54 + (int32_t)k * 12, b, C_MID);
    }
    cos_tx(8, 108, m == 2u ? "FX: THE SYNTAKT'S FX CHANNEL" : m == 1u ? "TRACK: A SYNTAKT TRACK'S CH" : "OWN: LINK 3, EACH ONE'S CC", C_DIM);
    if (cos_mo_cc) {                                     /* (MORPH's A-B on a CC too) */
        str_cpy(b, "MORPH A-B: CC ", sizeof b); fmt_int(b + str_len(b), cos_mo_cc - 1u);
        cos_tx(8, 118, b, C_MID);
    }
    cos_tx(8, 132, "IN: MIDI JACK  OUT: USB, BT", C_DIM);
}

/* (0.18.0) SAVE > START: what the STYLE puts on the tracks, how to load it */
static void cos_body_start(void)
{
    static const char *const A1[QS_N] = {"T3 ACID  T4 SUB BASS", "T3 ACID  T4 SUB BASS", "T3 SUB BASS  T4 STABS", "T3 SUB BASS  T4 WOOD 3", "EVERY PART DRAWN:"};
    static const char *const A2[QS_N] = {"T5 ATMOS  T6 SONAR 5", "T5 STABS  T6 R2 BLIPS 3", "T5 ATMOS  T6 GLASS 6", "T5 ATMOS  T6 DROP 5", "ROLES, SOUNDS, MOTIFS"};
    static const char *const P1[QS_N] = {"T3 ACID ROLLING", "T3 ACID BUSY", "T4 DUB STABS", "T4 WOOD 3-STEP", "EVERY PART DRAWN,"};
    static const char *const P2[QS_N] = {"T5 ATMOS, T6 SONAR 5", "T6 R2 BLIPS 3-STEP", "T5 ATMOS, T6 GLASS 6", "T5 ATMOS, T6 DROP 5", "SOME OF T3..T5 OUT"};
    static const char *const DR[3] = {"T1 KIT GROOVE, T2 KICK", "T1 KIT GROOVE (NO KICK)", "NO DRUMS"};
    char b[32];
    uint32_t st = qs_sty % QS_N, k, n = 5;
    cos_tx(8, 40, DR[qs_drm_get()], C_MID);
    cos_tx(8, 52, qs_elm ? P1[st] : A1[st], C_FG);
    cos_tx(8, 64, qs_elm ? P2[st] : A2[st], C_FG);
    str_cpy(b, "KEEP:", sizeof b);
    for (k = 0; k < NTRK; k++)
        if ((qs_keep >> k) & 1u) { b[n++] = ' '; b[n++] = 'T'; b[n++] = (char)('1' + k); }
    if (n == 5u) str_cpy(b + 5, " NONE (WHITE 1-6)", sizeof b - 5);
    else b[n] = 0;
    cos_tx(8, 80, b, qs_keep ? C_RED : C_DIM);
    cos_tx(8, 96, qs_pend ? "ON THE NEXT BAR ..." : "OCT+ LOADS; AGAIN: ANOTHER", qs_pend ? C_RED : C_FG);
    cos_tx(8, 108, "REC: THE MUSIC BEFORE IT", C_DIM);
}

/* (0.17.0) GLO > SYNTAKT: what comes in on the jack now (the cable, its clock, PLAY, its last pattern) and what to set
 * on the Syntakt */
static void cos_body_syntakt(void)
{
    char b[40], n[12];
    uint32_t on = (favorites.factory[8][31] & 1u) && song.g[G_CLOCK] == 2, cab = cos_rx_ms && fm1_ms - cos_rx_ms < 600u;
    if (!on) {
        cos_tx(8, 38, "SYNTAKT OFF: OWN CLOCK", C_DIM);
        cos_tx(8, 50, "TURN KNOB 1: FOLLOW IT", C_FG);
    } else {
        cos_tx(8, 38, cab ? "CABLE: OK" : "CABLE: NOTHING COMING IN", cab ? C_FG : C_RED);
        if (cab && midi_beat_samples) {
            uint32_t t10 = (uint32_t)FS * 600u / midi_beat_samples;
            str_cpy(b, "CLOCK: ", sizeof b); fmt_int(n, (int32_t)(t10 / 10u)); str_cpy(b + str_len(b), n, 8);
            str_cpy(b + str_len(b), ".", 2); fmt_int(n, (int32_t)(t10 % 10u)); str_cpy(b + str_len(b), n, 4);
            str_cpy(b + str_len(b), song.playing ? " BPM  PLAYING" : " BPM  STOPPED", 16);
            cos_tx(8, 50, b, C_FG);
        } else cos_tx(8, 50, "CLOCK: NONE YET", cab ? C_RED : C_DIM);
        if (cos_pc_ch && cos_pc_last) {                  /* (its last pattern: bank A..H, 01..16; the section it plays) */
            uint32_t p = cos_pc_last - 1u;
            str_cpy(b, "PATTERN ", sizeof b);
            b[8] = (char)('A' + p / 16u % 8u); b[9] = (char)('0' + (p % 16u + 1u) / 10u); b[10] = (char)('0' + (p % 16u + 1u) % 10u);
            str_cpy(b + 11, " -> SECTION ", 13);
            b[23] = (char)('A' + p % 4u); b[24] = 0;
            cos_tx(8, 62, b, C_MID);
        } else cos_tx(8, 62, cos_pc_ch ? "PATTERN: NONE YET" : "PATTERNS: NOT FOLLOWED", C_DIM);
    }
    cos_tx(8, 80, "ON THE SYNTAKT, MIDI CONFIG:", C_FG);
    cos_tx(8, 92, "SYNC: CLOCK SEND, TRANSPORT", C_DIM);
    cos_tx(8, 102, " SEND, PRG CH SEND: ON", C_DIM);
    str_cpy(b, "CHANNELS: PROG CHG OUT CH ", sizeof b); fmt_int(n, cos_pc_ch ? cos_pc_ch : 15); str_cpy(b + str_len(b), n, 4);
    cos_tx(8, 114, b, C_DIM);
    cos_tx(8, 124, "PORT CONFIG: OUT PORT MIDI", C_DIM);
    cos_tx(8, 136, "AUDIO: FM-1 OUT -> INPUT L/R", C_DIM);
}

/* (0.17.0) GLO > SYNTAKT's knobs: SYNTAKT, PATTERNS, PAT BARS, PC CH. ON: the FM-1 follows the Syntakt (CLOCK TRS: its tempo, PLAY and STOP; its patterns
 * the sections, on PROG CHG OUT CH, 15 unless set; PC OUT off: nothing back), kept over power-offs (STRING's retired
 * row 8, byte 31: bit 0 ON, bits 1..3 PAT LEN); OFF: the FM-1's own clock, no patterns followed. PATTERNS: ON / OFF.
 * PAT BARS: the Syntakt's pattern in bars, where its pattern changes land (song_play.c) */
static void cos_syntakt(uint32_t id, int32_t v)
{
    uint8_t *st = &favorites.factory[8][31], *lk = &favorites.factory[6][19];
    static const uint8_t PL[5] = {1, 2, 4, 8, 16};
    if (id == CC_STLEN) {
        *st = (uint8_t)((*st & 1u) | (uint32_t)clamp(v, 0, 4) << 1);
        cos_pc_bars = PL[clamp(v, 0, 4)];
    } else if (id == CC_STPAT) {
        lk[0] = (uint8_t)(v ? (cos_pc_ch ? cos_pc_ch : 15u) : 0u);
        cos_pc_ch = lk[0];
    } else if (v) {
        *st |= 1u;
        song.g[G_CLOCK] = 2;
        if (!cos_pc_ch) { lk[0] = 15; cos_pc_ch = 15; }
        lk[1] = 0; cos_pc_out = 0;
        favorites.factory[6][17] = 1;                    /* (0.18.0) AUTO LV OFF: the Syntakt's compressor after it */
        cos_say("SYNTAKT LEADS");
    } else {
        *st &= (uint8_t)~1u;
        song.g[G_CLOCK] = 0;
        lk[0] = 0; cos_pc_ch = 0;
        favorites.factory[6][17] = 0;                    /* (0.18.0) AUTO LV ON again */
        cos_say("OWN CLOCK");
    }
    cz.set_ms = fm1_ms;
}

/* GLO > LINK: what the PROGRAM CHANGEs do */
static void cos_body_link(void)
{
    cos_tx(8, 40, "PC IN: PATTERN TO SECTION", C_FG);
    cos_tx(8, 52, "1 5 9 13 A  2 6 10 14 B", C_DIM);
    cos_tx(8, 64, "3 7 11 15 C  4 8 12 16 D", C_DIM);
    cos_tx(8, 82, "PC OUT: A SECTION SENT", C_FG);
    cos_tx(8, 100, "VJ CH: EVENTS AS NOTES", C_FG);
    cos_tx(8, 112, "60 BREAK 61 BUILD 62 DROP", C_DIM);
    cos_tx(8, 124, "63 FILL 64-67 A-D CC20", C_DIM);
}

/* GLO > SYSTEM: what TRACKS and LOAD do */
#if FELUCCA_FLASH
static uint8_t flash_ok;                                 /* (storage_hw.c's, included after) */
static void flash_say_none(char *b);
#endif
static void cos_body_sys(void)
{
    cos_tx(8, 38, "TRACKS: 4, 5 OR 6 PLAYING", C_FG);
    cos_tx(8, 52, "LOAD: ECO 4, FULL 8 VOICES", C_DIM);
    {                                                    /* the audio's load (voice.c load_diag: MENU > ABOUT's) */
        char b[40], n[12];
        str_cpy(b, "CPU ", sizeof b); fmt_int(n, (int32_t)((song.cpu_q8 * 100u) >> 8)); str_cpy(b + str_len(b), n, 8);
        str_cpy(b + str_len(b), "% PEAK ", 9); fmt_int(n, (int32_t)load_diag.peak); str_cpy(b + str_len(b), n, 8);
        str_cpy(b + str_len(b), "% LATE ", 9); fmt_int(n, (int32_t)load_diag.late); str_cpy(b + str_len(b), n, 8);
        cos_tx(8, 66, b, load_diag.late ? C_RED : C_MID);
        cos_tx(8, 80, "BT MIDI: A BLE-MIDI LINK", C_DIM);
        {                                                /* the battery: the estimate, its volts and the ADC's reading */
            int32_t mv = cos_batt_mv();
            cos_batt_text(n);
            str_cpy(b, "BATT ", sizeof b);
            str_cpy(b + 5, n[0] ? n : "--", 8);
            str_cpy(b + str_len(b), "  ", 3);
            fmt_int(n, mv / 1000); str_cpy(b + str_len(b), n, 4); str_cpy(b + str_len(b), ".", 2);
            fmt_int(n, (mv % 1000) / 10 + 100); str_cpy(b + str_len(b), n + 1, 3);
            str_cpy(b + str_len(b), "V  RAW ", 8);
            fmt_int(n, song.batt_raw); str_cpy(b + str_len(b), n, 6);
            cos_tx(8, 100, b, C_FG);
            cos_tx(8, 114, "(THE % IS AN ESTIMATE)", C_DIM);
            cos_tx(8, 132, "IDLE: VISUALS AFTER NO TOUCH", C_DIM);
        }
#if FELUCCA_FLASH
        if (!flash_ok) {                                 /* (no flash found: SAVE keeps the music only until power-off;
                                                         * storage_hw.c flash_id_ok: the part it read) */
            flash_say_none(b);
            cos_tx(8, 150, b, C_RED);
        }
#endif
    }
}

/* the whole scene, drawn into the strip on the canvas (cv_oy: its top) */
static void cos_px2(int32_t x, int32_t y, const char *s, uint16_t c);
static void cos_scene(void)
{
    if (ui.uboot) {                                      /* OCT- + OCT+ held (main.c): UPDATE MODE in n seconds */
        char b[4] = {(char)('0' + ui.uboot % 10u), 0, 0, 0};
        cos_txc(120, 80, CZ_LBL, "UPDATE MODE IN", C_FG);
        cos_px2(120 - 8, 100, b, C_RED);
        cos_txc(120, 140, CZ_LBL, "LET GO TO CANCEL", C_DIM);
        return;
    }
    if (cz.idle_up) { cos_idle_scene(); return; }
    cos_head();
    cos_tabs();
    if (cz.fx_down) { cos_body_fxpad(); goto dials; }
    switch (cz.mode) {
    case CM_HOME: cos_body_home(); break;
    case CM_TRIG: cos_body_trig(); break;
    case CM_GEN: cos_body_gen(); break;
    case CM_KEY: cos_body_key(); break;
    case CM_PROJ: if (cz.tab[CM_PROJ] == 3u) cos_body_start(); else cos_body_proj(); break;   /* (0.18.0) START */
    case CM_DRUMS: cos_body_drm(); break;
    case CM_MIX: {
        const cpg_t *pg = cos_page();
        if (pg->kind == CK_COS && pg->id[0] == CC_OUTCH) cos_body_midi();
        else if (pg->kind == CK_COS && pg->id[0] == CC_SCT) cos_body_scope();
        else if (pg->kind == CK_COS && pg->id[0] == CC_SPV) cos_body_spec();
        else if (pg->kind == CK_COS && pg->id[0] == CC_SCZ) cos_body_stereo();
        else if (pg->kind == CK_COS && pg->id[0] == CC_LUT) cos_body_loud();
        else if (pg->kind == CK_COS && pg->id[0] == CC_TRKS) cos_body_sys();
        else if (pg->kind == CK_COS && (pg->id[0] == CC_COMP || pg->id[0] == CC_CIN || pg->id[0] == CC_CSC)) cos_body_master();
        else if (pg->kind == CK_COS && pg->id[0] == CC_LIN) cos_body_lim();
        else if (pg->kind == CK_COS && pg->id[0] == CC_PCCH) cos_body_link();
        else if (pg->kind == CK_COS && pg->id[0] == CC_STON) cos_body_syntakt();   /* (0.17.0) */
        else if (pg->kind == CK_COS && pg->id[0] == CC_QSTY) cos_body_start();     /* (0.18.0) */
        else if (pg->kind == CK_COS && (pg->id[0] == CC_MCCH || pg->id[0] == CC_MC0)) cos_body_link2();
        else if (pg->kind == CK_COS && pg->id[0] == CC_CTLM) cos_body_ctl();   /* (0.13.0) */
        else cos_body_sound();
        break;
    }
    case CM_FX:
        if (cos_page()->kind == CK_COS && cos_page()->id[0] == CC_RB0) cos_body_rumble();
        else if (cos_page()->kind == CK_COS && cos_page()->id[0] == CC_DJF) cos_body_dj();
        else cos_body_sound();
        break;
    default: cos_body_sound(); break;
    }
dials:
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
#define COS_VERSION "0.18.5"
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
    cos_lu_poll();                                        /* (the loudness, kept up whichever page) */
    if (cz.idle_up) { uint8_t g = sc_gain, m = sc_mode; sc_gain = 0; sc_mode = 0; cos_scope_prep(100); sc_gain = g; sc_mode = m; }   /* (AUTO gain) */   /* (the frame's picture, once for its six strips) */
    else if (cz.mode == CM_MIX && cos_page()->kind == CK_COS && cos_page()->id[0] == CC_SCT) cos_scope_prep(56);
    else if (cz.mode == CM_MIX && cos_page()->kind == CK_COS && cos_page()->id[0] == CC_SPV) cos_spec_prep();
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

/* (cursed-os) key k (0: the lowest F) on track t: 0 out of its scale, 1 in it, 2 its root. The key's own note (SNAP
 * rounds the others down to one in the scale, but the scale's own are what is lit); WHITE: the white keys walk the scale,
 * the black ones silent */
static uint32_t cos_key_in(const track_t *t, uint32_t k)
{
    int32_t n = 53 + (int32_t)k, d;
    if (t->p[P_QUANT] == QN_WHITE) {
        uint32_t m = kb_map(t, k);
        if (m == KB_SILENT) return 0;
        n = (int32_t)m;
    }
    d = ((n - t->p[P_ROOT]) % 12 + 12) % 12;
    if (!d) return 2;
    return (scale_mask(t) >> (uint32_t)d) & 1u;
}
/* the keys lit as the scale: a melodic track, a scale not CHR, and the keys playing it (live, STEP INPUT, DRONE, the KEY
 * page itself) */
static int cos_scale_view(void)
{
    const track_t *t = cz.mode == CM_KEY ? cos_mel() : TSEL;   /* (the KEY page: the song's, a kit picked or not) */
    if (!t || cos_hits(t) || (cz.mode != CM_KEY && cos_kit()) || !t->p[P_SCALE] || cz.fx_down) return 0;
    if (cz.mode == CM_TRIG) return cz.si;
    return cz.mode != CM_HOME && cz.mode != CM_PROJ;
}
static __attribute__((noinline)) void cos_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0}, nm[FM1_NCOL] = {0}, nh[FM1_NCOL] = {0}, nb[FM1_NCOL] = {0};
    const track_t *t = TSEL;
    uint32_t k, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), now = song.playing ? t->seq_idx % len : ~0u;
    static uint8_t ready COS_RAM;
    if (fm1_led_anim_on()) return;
    if (!ready) { led_pos_init(); ready = 1; }
    led_put(nl, panel.btn[CM_BTN[cz.mode]], 1);
    led_put(nl, panel.btn[B_REC], song.rec != 0u);
    led_put(nl, panel.btn[B_SCL], kb_latch != 0u);      /* (0.18.3) KEY LATCH on: SCL lit */
    for (k = 0; k < NB; k++) led_put(nd, panel.btn[k], 1);
    if (cz.fx_down) {                                    /* (FX held: the pad's keys, each lit while its effect is on,
                                                         * latched or held; the rest at the glow) */
        for (k = 0; k < 27u; k++) led_put(nl, 14u + k, cos_pad_lit(k));
    } else if ((cz.mode == CM_TRIG && !cz.si) || cz.mode == CM_HOME || cos_home_held()) {
        int hv = cz.mode == CM_HOME || cos_home_held();  /* (0.15.3, GG: HOME held, in any mode: the tracks and the drums
                                                         * as they sound, lit in, dark out) */
        uint32_t so = live_qsolo_on ? live_qsolo : perf_solo;
        for (k = 0; k < 16u; k++) {
            uint32_t st = cz.bank * 16u + k;
            int on = hv ? k == song.sel && !cos_home_held() : st < len && (cos_step(t, st, cos_kit() ? cz.lane : NLANE) != 0u) != (st == now);
            led_put(nl, 14u + WK[k], on);
        }
        for (k = 0; k < 11u; k++)                        /* (a track heard: not muted, and in the SOLO when one is on) */
            led_put(nl, 14u + BK[k], hv ? k < trk_live && !trk[k].p[P_MUTE] && (!so || ((so >> k) & 1u)) : cos_kit() && k == cz.lane);
        if (hv) {                                        /* (0.10.4) a mute queued for the beat / bar: its key breathes
                                                         * (lit now, dark next, or the other way round) */
            for (k = 0; k < NTRK && k < trk_live; k++)
                if ((live_qmute >> k) & 1u) { led_clear(nl, 14u + BK[k]); led_put(nb, 14u + BK[k], 1); }
        }
        if (cos_home_held() && dx_k0p)                   /* (0.14.1) white 9: the PERC */
            led_put(nl, 14u + WK[8], !(dx_mute & DXM_LANE(0)));
        if (cos_on_live() || cos_home_held()) {          /* (0.10.4) LIVE (0.15.3: and HOME held): white 1..8 lit while
                                                         * the drum sound is in (KICK: the KICK track playing), breathing
                                                         * when a flip waits */
            int kt = cos_kick_trk();
            for (k = 0; k < 8u; k++) {
                int in = k || kt < 0 ? !(dx_mute & DXM_LANE(k)) : !trk[kt].p[P_MUTE];
                int wait = k || kt < 0 ? (live_qlane >> k) & 1u : (live_qmute >> kt) & 1u;
                led_put(nl, 14u + WK[k], in && !wait);
                if (!in || wait) led_clear(nl, 14u + WK[k]);
                if (wait) led_put(nb, 14u + WK[k], 1);
            }
        }
    } else if (cz.mode == CM_PROJ && cz.tab[CM_PROJ] == 3u) {   /* (0.18.2) START: the kept tracks lit, a load waiting
                                                         * breathes on OCT+'s free ones */
        for (k = 0; k < NTRK; k++) {
            led_put(nl, 14u + WK[k], (qs_keep >> k) & 1u);
            if (qs_pend && !((qs_keep >> k) & 1u)) led_put(nb, 14u + WK[k], 1);
        }
    } else if (cz.mode == CM_DRUMS) {
        cos_drm_leds(nl, nb);
    } else
        for (k = 0; k < 27u; k++) led_put(nl, 14u + k, (int)((fm1_in.notes >> k) & 1u));
    if (cos_scale_view()) {                              /* (the keys play a melodic track in a scale: only its notes lit,
                                                         * the root brighter, a key held brightest; the rest dark) */
        const track_t *m = cz.mode == CM_KEY ? cos_mel() : t;
        for (k = 0; k < 27u; k++) {
            uint32_t r = cos_key_in(m, k);
            led_put(r == 2u ? nh : nm, 14u + k, r != 0u);
        }
    } else
        for (k = 0; k < 27u; k++) led_put(nd, 14u + k, 1);
    if (song.playing) { led_clear(nd, panel.btn[B_PLAY]); nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u)); }
    fm1_led_dim_level(0);
    for (k = 0; k < FM1_NCOL; k++) {
        fm1_led_dim[k] = (uint8_t)(nd[k] & ~nl[k] & ~nb[k]);   /* (a breathing key not held at the glow: 0.10.4) */
        fm1_led_breath[k] = nb[k];
        fm1_led_hi[k] = (uint8_t)(nh[k] & ~nl[k]);
        fm1_led_mid[k] = (uint8_t)(nm[k] & ~nl[k] & ~nh[k]);
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
