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
 *         LFO FX); held + REC: its locks go. Locked steps: a dot under them. SEQ held + REC twice (3 s): the track
 *         empty (cos_clear). A synth: steps held + PRESETS their note (in the scale), + black 1 ACCENT, 2 SLIDE;
 *         SEQ again: TRIG, STEP (a synth: STEP INPUT, a key the cursor's note, REC a rest, a key held + REC a tie:
 *         cos_si_*), EUCLID, MODES (JIANT's euclid on the track, OCT+ a new one)
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
enum { CK_NONE, CK_TRK, CK_GLO, CK_AC, CK_EU, CK_COS, CK_DB };
enum { CC_OUTCH, CC_MOUT, CC_CLOCK, CC_SLOT, CC_TRKS, CC_LOAD, CC_COMP, CC_CATK, CC_CREL, CC_LIM, CC_PF0, CC_PF3 = CC_PF0 + 3,
       CC_X2, CC_X2E = CC_X2 + 15, CC_OCT, CC_KICK, CC_KICKE = CC_KICK + 4, CC_LN0, CC_LNE = CC_LN0 + 7, CC_KDRV, CC_BT,
       CC_L1SYNC, CC_L1RATE, CC_L1WAVE, CC_L1MODE, CC_L1PH, CC_L1FADE, CC_L2SYNC, CC_L2RATE, CC_BPM,
       CC_CIN, CC_CTHR, CC_CRAT, CC_CMK, CC_CKNEE, CC_CSC, CC_CMIX, CC_LIN, CC_LREL, CC_LVL, CC_MUTEQ, CC_PCCH, CC_PCOUT, CC_VJCH, CC_EQL, CC_EQM, CC_EQH, CC_MAC0, CC_MAC3 = CC_MAC0 + 3, CC_RB0, CC_RB3 = CC_RB0 + 3, CC_MOPOS, CC_MOBARS, CC_LVLEN, CC_LVRATE, CC_LVDN, CC_LVSH, CC_MCCH, CC_MCCC, CC_MCDIR, CC_MCMAP, CC_MC0, CC_MC3 = CC_MC0 + 3, CC_MOCC,
       CC_NAME, CC_IDLE, CC_SCALE, CC_QUANT, CC_DJF, CC_DSEL, CC_DLV, CC_DWAV, CC_DFLT, CC_DBT, CC_DDRV, CC_DLAY, CC_DFB, CC_DSNP, CC_DE0, CC_DEE = CC_DE0 + DE_N - 1 };
_Static_assert(CC_DEE < 0xFEu, "the CK_COS ids fit a page's byte (0xFE, 0xFF: OCTAVE, none)");
_Static_assert(X2_N <= 16u, "CC_X2 .. CC_X2E: room for every trk_x2 field");
#define ID_MUTEQ 0xFCu                          /* (HOME 2's: MUTE Q, cos_knob) */
#define ID_BPM 0xFDu                            /* (HOME's: BPM in tenths, cos_knob) */
#define ID_OCT 0xFEu                            /* (any page's knob: the track's OCTAVE, cos_knob) */   /* (CC_X2 + X2_*: params.c trk_x2, LFO 2 and FILTER 2) */   /* (CK_COS: cursed-os's own knobs) */
typedef struct { const char *tab; uint8_t kind, id[4]; } cpg_t;
#define X 0xFFu
static const cpg_t CZP_HOME[] = {{"PERFORM", CK_GLO, {ID_BPM, G_SWING, G_DJF, ID_OCT}},
                                {"PERFORM 2", CK_GLO, {G_DUCK, G_DREL, ID_MUTEQ, X}},
                                {"ISOLATOR", CK_COS, {CC_EQL, CC_EQM, CC_EQH, X}},   /* (cos_iso.c; HOME's black 9-11 the kills) */
                                {"MACROS", CK_COS, {CC_MAC0, CC_MAC0 + 1u, CC_MAC0 + 2u, CC_MAC3}},
                                {"MORPH", CK_COS, {CC_MOPOS, CC_MOBARS, X, X}},   /* (cos_morph.c: white 13 A, 14 B) */
                                {"LOOP", CK_COS, {CC_LVLEN, CC_LVRATE, CC_LVDN, CC_LVSH}}};   /* (JIANT's LIVE, seq_live.c: the
                                                         * track picked; white 15 back in phase) */   /* (the sounds' M1..M4 in the matrix) */   /* (JIANT's master EQ; HOME's black 9-11 the kills) */   /* (DUCK: the kick pumps the rest) */
/* SEQ: the sequencer's three ways, SEQ again (or SELECT) the next: TRIG the steps by hand, STEP the keys into the steps
 * one by one (a synth: STEP INPUT, cos_si_*), EUCLID (and its MODES) the track's steps as a euclidean rhythm, a kit's
 * lanes or a synth's notes in its scale (JIANT's EUCLID, ui_euclid.c; GEN's on a kit) */
static const cpg_t CZP_TRIG[] = {{"TRIG", CK_TRK, {P_SLEN, P_SDIV, P_SSWING, P_SGATE}},
                                {"STEP", CK_TRK, {P_SLEN, P_SDIV, P_SSWING, P_SGATE}},
                                {"EUCLID", CK_EU, {EK_HITS, EK_STEPS, EK_ROT, EK_K4}},
                                {"MODES", CK_EU, {EK_M0, EK_M0 + 1u, EK_M0 + 2u, EK_M0 + 3u}}};
static const cpg_t CZP_TRIGK[] = {{"TRIG", CK_TRK, {P_SLEN, P_SDIV, P_SSWING, P_SGATE}},   /* (a kit: no STEP) */
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
static const cpg_t CZP_SRCN[] = {{"SRC 1", CK_TRK, {P_E0, P_E1, P_E2, P_E3}}, {"SRC 2", CK_TRK, {P_E4, P_E5, P_E6, P_E7}},
                                {"FILTER", CK_TRK, {P_FTYPE, P_E4, P_E5, P_ED_FLT}},   /* (ANALOG: its CUT RES, E4 E5) */
                                {"VOICE", CK_TRK, {P_VOICE, P_GLIDE, P_DETUNE, ID_OCT}},
                                {"CHORD", CK_TRK, {P_CHRD, P_CVOI, P_CSTR, X}},
                                CZ_MOD(1), CZ_MOD(2), CZ_MOD(3), CZ_MOD(4)};
static const cpg_t CZP_SRCA[] = {{"SRC 1", CK_TRK, {P_E0, P_E1, P_E2, P_E3}}, {"SRC 2", CK_TRK, {P_E4, P_E5, P_E6, P_E7}},
                                CZ_FILT,
                                {"DEVIL", CK_TRK, {P_ATK, P_DEC, P_SUS, P_REL}},   /* (ACID: ACC DEC, DRV TYPE, SUB, KEY TRK in
                                                         * the envelope's slots: eng_acid.c ACID_X) */
                                {"VOICE", CK_TRK, {P_VOICE, P_GLIDE, P_DETUNE, ID_OCT}},
                                {"CHORD", CK_TRK, {P_CHRD, P_CVOI, P_CSTR, X}},
                                CZ_MOD(1), CZ_MOD(2), CZ_MOD(3), CZ_MOD(4)};
static const cpg_t CZP_SRCV[] = {{"SRC 1", CK_TRK, {P_E0, P_E1, P_E2, P_E3}}, {"SRC 2", CK_TRK, {P_E4, P_E5, P_E6, P_E7}},
                                CZ_FILT,
                                {"WORDS", CK_TRK, {P_VLANG, P_VLYR, X, X}},   /* (THEME, ARTIC: on the first page) */
                                {"VOICE", CK_TRK, {P_VOICE, P_GLIDE, P_DETUNE, ID_OCT}},
                                {"CHORD", CK_TRK, {P_CHRD, P_CVOI, P_CSTR, X}},
                                CZ_MOD(1), CZ_MOD(2), CZ_MOD(3), CZ_MOD(4)};
/* a DRUM track's EDIT: a drum of the kit (DRUM's KNOB 1, or its key struck: the eight's own sound, its patches' both
 * MORPH ends moved together; its AHDSR and sends), the eight sounds' levels, then the kit's (KIT, KIT 2: the engine's
 * E1 .. E8), the matrix */
static const cpg_t CZP_SRCD[] = {{"DRUM", CK_COS, {CC_DSEL, CC_KICK + DXP_PITCH, CC_KICK + DXP_PMOD, CC_DLV}},
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
static const cpg_t CZP_DRONE[] = {{"DRONE", CK_TRK, {P_LEVEL, P_FCUT, P_LRATE, P_LD_FLT}}, {"SPACE", CK_TRK, {P_ATK, P_REL, P_DLY, P_REV}}};
static const cpg_t CZP_FX[] = {{"DJ", CK_COS, {CC_DJF, CC_EQL, CC_EQM, CC_EQH}},   /* (the mixer's: DJ FILTER, the ISOLATOR) */
                              {"SEND", CK_TRK, {P_DIST, P_CHOR, P_DLY, P_REV}}, {"DIST", CK_TRK, {P_DTYPE, P_DTONE, X, X}},
                              {"DELAY", CK_GLO, {G_DTIME, G_DFDBK, G_DCOLOR, G_DPING}},    /* (the buses: every track's) */
                              {"DELAY 2", CK_GLO, {G_DMIX, G_DDRV, G_DTYPE, G_DMOD}},     /* (the tape echo: MOD its wow and flutter;
                                                         * FEEDBK past 100 runs away into the clip: the dub spin) */
                              {"REVERB", CK_GLO, {G_RSIZE, G_RDAMP, G_RPRE, G_RTYPE}},
                              {"RUMBLE", CK_COS, {CC_RB0, CC_RB0 + 1u, CC_RB0 + 2u, CC_RB3}}};   /* (rumble.c: the kick's dark tail) */
static const cpg_t CZP_GENA[] = {{"ACID", CK_AC, {AK_DENS, AK_SLIDE, AK_ACC, ID_OCT}}, {"ACID 2", CK_AC, {AK_STEPS, AK_NOTES, AK_TIE, AK_OCT}},
                                {"ACID 3", CK_AC, {AK_EVERY, AK_AMT, AK_ROOT, AK_SCALE}}, {"ACID 4", CK_AC, {AK_LOCK, AK_ROT, X, X}}};
/* (a POLY synth track that is not ACID, the pad: DUB's chord stabs first, ACID's lines after them; a MONO one, the
 * bass: ACID's first, DUB's after, CZP_GENM) */
static const cpg_t CZP_GENS[] = {{"DUB", CK_DB, {DK_SHAPE, DK_DENS, DK_MOVE, DK_LEN}}, {"DUB 2", CK_DB, {DK_BARS, DK_VOIC, DK_ECHO, ID_OCT}},
                                {"DUB 3", CK_DB, {DK_ROOT, DK_EVERY, DK_AMT, DK_DLY}},
                                {"ACID", CK_AC, {AK_DENS, AK_SLIDE, AK_ACC, ID_OCT}}, {"ACID 2", CK_AC, {AK_STEPS, AK_NOTES, AK_TIE, AK_OCT}},
                                {"ACID 3", CK_AC, {AK_EVERY, AK_AMT, AK_ROOT, AK_SCALE}}, {"ACID 4", CK_AC, {AK_LOCK, AK_ROT, X, X}}};
static const cpg_t CZP_GENM[] = {{"ACID", CK_AC, {AK_DENS, AK_SLIDE, AK_ACC, ID_OCT}}, {"ACID 2", CK_AC, {AK_STEPS, AK_NOTES, AK_TIE, AK_OCT}},
                                {"ACID 3", CK_AC, {AK_EVERY, AK_AMT, AK_ROOT, AK_SCALE}}, {"ACID 4", CK_AC, {AK_LOCK, AK_ROT, X, X}},
                                {"DUB", CK_DB, {DK_SHAPE, DK_DENS, DK_MOVE, DK_LEN}}, {"DUB 2", CK_DB, {DK_BARS, DK_VOIC, DK_ECHO, ID_OCT}},
                                {"DUB 3", CK_DB, {DK_ROOT, DK_EVERY, DK_AMT, DK_DLY}}};
static const cpg_t CZP_GENE[] = {{"EUCLID", CK_EU, {EK_HITS, EK_STEPS, EK_ROT, EK_K4}}, {"MODES", CK_EU, {EK_M0, EK_M0 + 1u, EK_M0 + 2u, EK_M0 + 3u}},
                                {"MUTATE", CK_EU, {EK_U0, EK_U0 + 1u, EK_U0 + 2u, EK_U0 + 3u}}};
static const cpg_t CZP_MIX[] = {{"MIX", CK_TRK, {P_LEVEL, P_PAN, P_GLIDE, P_MUTE}},
                               {"COMP", CK_COS, {CC_COMP, CC_CTHR, CC_CRAT, CC_CMK}},
                               {"COMP 2", CK_COS, {CC_CIN, CC_CATK, CC_CREL, CC_CKNEE}},
                               {"COMP 3", CK_COS, {CC_CSC, CC_CMIX, X, X}},
                               {"LIMIT", CK_COS, {CC_LIN, CC_LIM, CC_LREL, CC_LVL}},
                               {"MIDI", CK_COS, {CC_OUTCH, CC_MOUT, CC_CLOCK, X}},
                               {"LINK", CK_COS, {CC_PCCH, CC_PCOUT, CC_VJCH, X}},   /* (a Syntakt's patterns: the sections) */
                               {"LINK 2", CK_COS, {CC_MCCH, CC_MCDIR, CC_MCMAP, CC_MOCC}},   /* (the macros and MORPH as CCs, both
                                                         * ways) */
                               {"LINK 3", CK_COS, {CC_MC0, CC_MC0 + 1u, CC_MC0 + 2u, CC_MC3}},   /* (.. each one's CC) */
                               {"SYSTEM", CK_COS, {CC_TRKS, CC_LOAD, CC_BT, CC_IDLE}}};
static const cpg_t CZP_KEY[] = {{"KEY", CK_COS, {CC_SCALE, CC_QUANT, X, X}}};   /* (the root: the keys; the song's scale,
                                                         * whichever track is picked: cos_mel) */
static const cpg_t CZP_PERF[] = {{"FX PAD", CK_COS, {CC_PF0, CC_PF0 + 1u, CC_PF0 + 2u, CC_PF3}}};   /* (FX held) */
static const cpg_t CZP_PROJ[] = {{"PROJECT", CK_COS, {CC_SLOT, CC_NAME, X, X}}};   /* (KNOB 1 the slot, every song's: 1A ..
                                                         * 8D; white keys 1..4 this song's, 9..16 the songs; KNOB 2 a name) */
#undef X
static const char *const CM_NAME[CM_N] = {"HOME", "TRIG", "SRC", "AMP", "DRONE", "FX", "GEN", "MIX", "KEY", "SAVE"};
static const uint8_t CM_BTN[CM_N] = {B_HOME, B_SEQ, B_EDIT, B_ENV, B_LFO, B_FX, B_ARP, B_GLO, B_SCL, B_SAVE};
static const uint8_t WK[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};   /* the white keys */
static const uint8_t BK[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};                    /* the black keys */
/* the keys drawn on screen (KEY, DRONE), from F as the FM-1's own: F F# G G# A A# B C C# D D# E, 1 a black one (was C's
 * pattern from F: the black keys a place off, A shown where E is on a C keyboard; tests/cos_test.c checks it against BK) */
static const uint8_t COS_BLF[12] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0};
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
    int16_t cc_v[15];                                   /* (CK_COS: the knobs' views) */
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
static int cos_dead(const track_t *t, uint32_t id);      /* (below: a parameter the track's engine does not use) */
static void cos_pages(uint32_t m, const cpg_t **p, uint32_t *n)
{
    static const cpg_t *const P[CM_N] = {CZP_HOME, CZP_TRIG, CZP_SRC, CZP_AMP, CZP_DRONE, CZP_FX, CZP_GENA, CZP_MIX, CZP_KEY, CZP_PROJ};
    static const uint8_t N[CM_N] = {6, 4, 9, 9, 2, 7, 4, 10, 1, 1};
    int vo = m == CM_SRC && trk_engp(TSEL) == &ENG_FORMANT;
    if (m == CM_SRC && cos_kit()) { *p = CZP_SRCD; *n = NELEM(CZP_SRCD); return; }
    if (m == CM_TRIG && cos_kit()) { *p = CZP_TRIGK; *n = NELEM(CZP_TRIGK); return; }   /* (a kit: KICK, LEVELS, its own ..) */
    if (m == CM_GEN && cos_kit()) { *p = CZP_GENE; *n = 3; return; }
    if (m == CM_GEN && trk_eng(TSEL) != ENGI_ACID) {     /* (the pad: DUB first; the bass, a MONO one: ACID first) */
        int poly = TSEL->p[P_VOICE] == V_POLY;
        *p = poly ? CZP_GENS : CZP_GENM;
        *n = poly ? NELEM(CZP_GENS) : NELEM(CZP_GENM);
        return;
    }
    if (m == CM_SRC && trk_engp(TSEL) == &ENG_ACID) { *p = CZP_SRCA; *n = NELEM(CZP_SRCA); return; }   /* (+ DEVIL) */
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
static const cpg_t *cos_page(void)
{
    const cpg_t *p;
    uint32_t n;
    if (cz.fx_down) return CZP_PERF;
    cos_pages(cz.mode, &p, &n);
    return &p[cz.tab[cz.mode] % n];
}
static int cos_on_words(void) { return cz.mode == CM_SRC && trk_engp(TSEL) == &ENG_FORMANT && str_eq(cos_page()->tab, "WORDS"); }
static void cos_say(const char *s) { str_cpy(cz.msg, s, sizeof cz.msg); cz.msg_ms = fm1_ms; }
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
    if (!drum_track(TSEL)) return TSEL;
    for (k = 0; k < NTRK && drum_track(&trk[k]); k++) {}
    return k < NTRK ? &trk[k] : 0;
}
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
/* the LFOs' rates in time, slowest first on the knob: 32BAR 16BAR 8BAR, then JIANT's 4BAR .. 1/32 (stored: N_LSYNC's
 * index, the slow three appended at 12 .. 14) */
static const char *const COS_LSYNC[15] = {"32BAR", "16BAR", "8BAR", "4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8D", "1/4T",
                                          "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static uint32_t cos_lsync_idx(uint32_t pos) { return pos < 3u ? 14u - pos : pos - 3u; }
static uint32_t cos_lsync_pos(uint32_t idx) { return idx >= 12u ? 14u - (idx % 15u) : idx + 3u; }
/* (cursed-os) a drum of the kit, the one picked (dx_ui_lane: DRUM's KNOB 1, or its key struck on EDIT): which one, its
 * LEVEL, the oscillator's WAVE, the noise's FILTER, BEND's TYPE, DRIVE, LAYER, FEEDBK, SNAP (the noise's envelope a
 * quarter of the sound's), its AHDSR and sends (dx_de, DX_DE_D) */
static const char *const COS_DRUM[] = {"KICK", "SNARE", "CLAP", "HAT CL", "HAT OP", "TOM", "RIM", "BELL", 0};
static const char *const COS_DRUM5[] = {"KICK", "SNARE", "CLAP", "HH CL", "HH OP", "TOM", "RIM", "BELL", 0};   /* (a knob's
                                                         * value: five letters at most, param_format) */
static __attribute__((noinline)) const param_desc_t *cos_ddesc(uint32_t id, int16_t **vp)
{
    static const char *const WV[] = {"SINE", "FM", "METAL", "BELL", 0}, *const FT[] = {"LP", "BP", "HP", 0};
    static const char *const BT[] = {"DECAY", "LONG", "NOISE", "SINE", 0}, *const OO[] = {"OFF", "ON", 0};
    static const param_desc_t D[] = {{"DRUM", F_ENUM, 0, 7, 0, COS_DRUM5, 0}, {"WAVE", F_ENUM, 0, 3, 0, WV, 0},
                                     {"FILTER", F_ENUM, 0, 2, 0, FT, 0}, {"BENDTYP", F_ENUM, 0, 3, 0, BT, 0},
                                     {"DRIVE", F_ENUM, 0, 1, 0, OO, 0}, {"LAYER", F_INT, 0, 15, 0, 0, 0},
                                     {"FEEDBK", F_INT, 0, 15, 0, 0, 0}, {"SNAP", F_ENUM, 0, 1, 0, OO, 0}};
    uint32_t l = dx_ui_lane & 7u, m = dx_kit[l].mode;
    int16_t *c = &cz.cc_v[14];
    *vp = c;
    switch (id) {
    case CC_DSEL: *c = (int16_t)l; return &D[0];
    case CC_DLV: {                                       /* (its LEVEL: LEVELS' knob, named so) */
        static param_desc_t lv;
        lv = *track_desc(TSEL, P_LN0 + l);
        lv.label = "LEVEL";
        *vp = &TSEL->p[P_LN0 + l];
        return &lv;
    }
    case CC_DWAV: *c = (int16_t)(m & 3u); return &D[1];
    case CC_DFLT: *c = (int16_t)(((m >> 2) & 3u) % 3u); return &D[2];
    case CC_DBT: *c = (int16_t)((m >> 5) & 3u); return &D[3];
    case CC_DDRV: *c = (int16_t)((m & DX_DRIVE) != 0); return &D[4];
    case CC_DLAY: *c = (int16_t)DX_LAY_L(dx_lay[l]); return &D[5];
    case CC_DFB: *c = (int16_t)DX_LAY_FB(dx_lay[l]); return &D[6];
    case CC_DSNP: *c = (int16_t)((m >> 4) & 1u); return &D[7];
    default: {
        uint32_t k = (id - CC_DE0) % DE_N, x = dx_de[l][k];
        *c = (int16_t)(k >= DE_SCH ? (int32_t)x - 1 : (int32_t)x);
        return &DX_DE_D[k];
    }
    }
}
static __attribute__((noinline)) void cos_dput(uint32_t id, int32_t v)
{
    uint32_t l = dx_ui_lane & 7u;
    dx_lane_t *L = &dx_kit[l];
    uint32_t m = L->mode;
    switch (id) {
    case CC_DSEL: dx_ui_lane = (uint8_t)clamp(v, 0, 7); break;
    case CC_DLV: TSEL->p[P_LN0 + l] = (int16_t)clamp(v, 0, 127); break;
    case CC_DWAV: m = (m & ~3u) | (uint32_t)clamp(v, 0, 3); break;
    case CC_DFLT: m = (m & ~0x0Cu) | (uint32_t)clamp(v, 0, 2) << 2; break;
    case CC_DBT: m = (m & ~0x60u) | (uint32_t)clamp(v, 0, 3) << 5; break;
    case CC_DDRV: m = v > 0 ? m | DX_DRIVE : m & ~DX_DRIVE; break;
    case CC_DSNP: m = v > 0 ? m | 0x10u : m & ~0x10u; break;
    case CC_DLAY: dx_lay[l] = (uint8_t)((dx_lay[l] & 0xF0u) | (uint32_t)clamp(v, 0, 15)); break;
    case CC_DFB: dx_lay[l] = (uint8_t)((dx_lay[l] & 0x0Fu) | (uint32_t)clamp(v, 0, 15) << 4); break;
    default: {
        uint32_t k = (id - CC_DE0) % DE_N;
        dx_de[l][k] = (uint8_t)(k >= DE_SCH ? clamp(v, -1, 127) + 1 : clamp(v, 0, 127));
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
static void cos_name_pick(void);                        /* (SAVE's NAME, below) */
static void cos_proj_scan(void);
static char *proj_name_buf(void);                       /* (project.c: the music's name, 13 bytes) */
static int song_open(uint32_t n);                       /* (song_main.c: song n's slots, the music kept) */
static const param_desc_t *cos_cdesc(uint32_t id, int16_t **vp)
{
    static const char *const MO[] = {"KEYS", "CLOCK", "ALL", "OFF", 0};
    static const param_desc_t D[2] = {{"OUT CH", F_INT, 1, 16, 1, 0, 0}, {"SENDS", F_ENUM, 0, 3, 0, MO, 0}};
    static const param_desc_t DS = {"SLOT", F_INT, 1, NSONG * 4, 1, 0, 0}, DN = {"NAME", F_INT, 0, 999, 0, 0, 0};
    uint32_t t = song.sel % NTRK, v = favorites.factory[15][8u + t];
    static const char *const LD[] = {"ECO", "LITE", "FULL", 0};
    static const param_desc_t DT = {"TRACKS", F_INT, 4, NTRK, 5, 0, 0}, DL = {"LOAD", F_ENUM, 0, 2, 2, LD, 0};
    if (id == CC_SLOT) { cz.cc_v[3] = (int16_t)(song_cur * 4u + cz.slot + 1u); *vp = &cz.cc_v[3]; return &DS; }
    if (id == CC_NAME) { cz.cc_v[3] = (int16_t)cz.nm_n; *vp = &cz.cc_v[3]; return &DN; }
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
    if (id == CC_BPM) {                                  /* BPM in tenths: HOME held, a knob's step is a tenth */
        static const param_desc_t DB = {"BPM", F_INT, 400, 2400, 1200, 0, 0};
        cz.cc_v[14] = (int16_t)(cos_follows() ? (int32_t)cos_ext10() : song.g[G_BPM] * 10 + bpm_frac % 10u);   /* (a clock: its) */
        *vp = &cz.cc_v[14];
        return &DB;
    }
    if (id == CC_OCT) {                                  /* OCTAVE: TRANS in octaves (-2 .. +2), its semitones kept; a kit none */
        static const param_desc_t DO = {"OCTAVE", F_INT, -2, 2, 0, 0, 0};
        int32_t tr = TSEL->p[P_TRANS];
        if (cos_kit()) return 0;
        cz.cc_v[14] = (int16_t)(tr >= 0 ? (tr + 6) / 12 : -((-tr + 5) / 12));
        *vp = &cz.cc_v[14];
        return &DO;
    }
    if (id >= CC_DSEL && id <= CC_DEE) return cos_ddesc(id, vp);   /* (a drum of the kit: its own) */
    if (id >= CC_KICK && id <= CC_KICKE) {               /* the drum picked: DRUM-X's settings, where MORPH has them */
        static const param_desc_t DK[5] = {{"PITCH", F_INT, 0, 127, 64, 0, 0}, {"BEND", F_PCT, 0, 127, 64, 0, 0},
                                           {"DECAY", F_PCT, 0, 127, 64, 0, 0}, {"NOISE", F_PCT, 0, 127, 0, 0, 0},
                                           {"TONE", F_PCT, 0, 127, 64, 0, 0}};
        uint32_t i = id - CC_KICK;
        const dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
        int32_t mo = clamp(TSEL->p[P_E0], 0, 127);
        cz.cc_v[14] = (int16_t)(L->a[i] + ((L->b[i] - L->a[i]) * mo) / 127);
        *vp = &cz.cc_v[14];
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
        uint32_t w = (uint32_t)clamp(v, 1, NSONG * 4) - 1u;
        if (w / 4u != song_cur) {                        /* (another song's slot: that song opened, the music kept) */
            if (song.playing) { cos_say("STOP TO CHANGE SONG"); return; }
            if (song_open(w / 4u)) return;
            cz.pn_ok = 0;
        }
        cz.slot = (uint8_t)(w % 4u);
        cos_proj_scan();
    } else if (id == CC_NAME) {
        cz.nm_n = (uint16_t)clamp(v, 0, 999);
        cos_name_pick();
    } else if (id == CC_SCALE || id == CC_QUANT) {
        track_t *m = cos_mel();
        uint32_t pi = id == CC_SCALE ? P_SCALE : P_QUANT, k;
        const param_desc_t *d = m ? track_desc(m, pi) : 0;
        if (!d) return;
        v = clamp(v, d->min, d->max);
        for (k = 0; k < NTRK; k++)                       /* (every melodic track at once: scale_link would, next frame) */
            if (!drum_track(&trk[k])) trk[k].p[pi] = (int16_t)v;
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
        motion_djf_set(TSEL, (int16_t)clamp(v, GP[G_DJF].min, GP[G_DJF].max));   /* (REC: recorded too, as PERFORM's) */
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
    if (e == &ENG_DRUM)
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
    if (id == ID_MUTEQ) return cos_cdesc(CC_MUTEQ, vp);
    switch (p->kind) {
    case CK_TRK: *vp = &TSEL->p[id]; return track_desc(TSEL, id);
    case CK_GLO:
        *vp = &song.g[id];
        if (song.g[G_RTYPE] == 3 && (id == G_RPRE || id == G_RDAMP))   /* (params.c: RESO's PITCH, FOLW in their slots; */
            return id == G_RPRE ? &RESO_PITCH : &RESO_FOLW;           /* SHIMMER / CLOUD's AIR for PRE) */
        if (id == G_RPRE)
            return song.g[G_RTYPE] == 2 || song.g[G_RTYPE] == 4 ? &REV_AIR : &REV_PRE;
        return &GP[id];
    case CK_AC: return ac_desc(id, vp);
    case CK_DB: return db_desc(id, vp);
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
/* HOME held: the tempo's button (as a Syntakt's TEMPO): OCT- / OCT+ nudge, a knob's step on BPM a tenth */
static int cos_home_held(void) { return (int)((fm1_in.buttons >> panel.btn[B_HOME]) & 1u); }
/* "128.4" */
static void cos_bpm_text(char *b, int32_t v)
{
    fmt_int(b, v / 10);
    b += str_len(b);
    b[0] = '.';
    b[1] = (char)('0' + v % 10);
    b[2] = 0;
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
        song.g[G_BPM] = (int16_t)(v / 10);
        bpm_frac = (uint8_t)(v % 10);
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
    if (p->id[k] == ID_BPM && cos_follows()) {           /* (an external clock leads: its tempo, not the knob's) */
        cos_say("EXT CLOCK LEADS");
        return;
    }
    if (p->id[k] == ID_BPM) {                            /* BPM: a whole one a step (accelerated); HOME held: a tenth */
        v = *vp + (cos_home_held() ? (s > 0 ? 1 : -1) : 10 * accel(EN_K1 + k, s, 200));
        v = clamp(v, 400, 2400);
        song.g[G_BPM] = (int16_t)(v / 10);
        bpm_frac = (uint8_t)(v % 10);
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

/* ACID's track: the first one on the ACID engine (the 303); none: the last live one that is neither a kit nor VOICE
 * becomes ACID's TB CLASSIC (an UNDO point first; REC on GEN undoes it) */
static __attribute__((noinline)) uint32_t cos_acid_trk(uint32_t *made)
{
    uint32_t i, k = trk_live;
    char b[24];
    for (i = 0; i < trk_live; i++)
        if (trk_eng(&trk[i]) == ENGI_ACID) return i;
    for (i = trk_live; i-- > 0u;)
        if (!drum_track(&trk[i]) && trk_eng(&trk[i]) != ENGI_VOICE) { k = i; break; }
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
static __attribute__((noinline)) void cos_proj_oct(int up)
{
    if (up) cos_name_auto();                             /* (a music with no name: one drawn for it) */
    if (up && song.playing) section_store(cz.slot);      /* (playing: kept now, written once stopped) */
    else if (up) project_save(cz.slot);
    else if (!cz.pn[cz.slot][0]) cos_say("EMPTY SLOT");
    else if (song.playing) cos_section(cz.slot);         /* (playing: in on the next bar) */
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
    return cz.mode == CM_HOME || (cz.mode == CM_TRIG && !cz.si) || cz.mode == CM_KEY || cz.mode == CM_PROJ || cz.mode == CM_DRONE || (cz.mode == CM_GEN && cos_kit()) ||
           ((fm1_in.buttons >> panel.btn[B_SCL]) & 1u);
}

/* the sound just loaded, on the top line: its name (engine: name when it fits) */
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
        memcpy(keep.dx, dx_kit, sizeof keep.dx);
        memcpy(keep.de, dx_de, sizeof keep.de);
        memcpy(keep.lay, dx_lay, sizeof keep.lay);
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
static __attribute__((noinline)) void cos_input(void)
{
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
        else if (b == B_REC && cz.mode == CM_DRONE) { cos_drone_clear(); cos_say("DRONE OFF"); }
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
        else if (b == B_REC && cz.mode == CM_GEN) cos_say(undo_do() ? "NOTHING TO UNDO" : "UNDONE");
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
            else if (cz.mode == CM_PROJ) cos_proj_oct(up);
            else if (cz.mode == CM_GEN && cos_kit()) eu_oct(up, cz.tab[CM_GEN] == 2u);
            else if (cz.mode == CM_GEN && cos_page()->kind == CK_DB) db_oct(up, cos_page()->id[0] == DK_ROOT);
            else if (cz.mode == CM_GEN) ac_oct(up, cos_page()->id[0] == AK_EVERY);
            else if (cz.mode == CM_DRONE) cz.dr_oct = (int8_t)clamp(cz.dr_oct + (up ? 1 : -1), -2, 2);
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
            const cpg_t *hp;
            uint32_t hn;
            cos_pages(CM_HOME, &hp, &hn);
            cz.tab[CM_HOME] = (uint8_t)(cz.home_used ? cz.home_tab : (cz.home_tab + 1u) % hn);   /* (a tap on HOME: its next page) */
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
    if ((s = panel_enc(EN_ALGO)) != 0) {
        track_select((uint32_t)clamp((int32_t)song.sel + (s > 0 ? 1 : -1), 0, (int32_t)trk_live - 1));
        cz.bank = 0;
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
        } else if (cz.mode != CM_TRIG && cz.mode != CM_PROJ) {   /* the track's next sound, every engine's in turn */
            preset_step(s > 0 ? 1 : -1);
            ui.msg_t = 0;
            cos_sound_say();
        }
    }
    for (k = 0; k < 4u; k++)
        if ((s = panel_enc(EN_K1 + k)) != 0) {
            uint64_t h = cz.mode == CM_TRIG ? cos_held() : 0u;
            if (h) cos_lock_turn(k, s, h);                   /* (steps held: their locks) */
            else cos_turn(k, s);
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
        } else if (cz.mode == CM_DRONE) {
            cos_drone_key(k);
        } else if (cz.mode == CM_TRIG && cz.si) {
            cos_si_key(k);                               /* (STEP INPUT: the note into the cursor's step) */
        } else if (cz.mode == CM_PROJ) {
            if (w < 4u) cz.slot = (uint8_t)w;
            else if (w >= 8u && !chain_busy()) {           /* (white 9 .. 16: the set list's songs 1 .. 8) */
                if (!song_go_live(w - 8u)) { cz.pn_ok = 0; cos_proj_scan(); }
            }
        } else if (cz.mode == CM_HOME) {                 /* white 1..6 the track, 9..12 the sections A..D on the bar; black
                                                         * 1..6 mute (HOME held: solo), 7 FILL, 8 BREAKDOWN (cos_live.c) */
            if (cos_home_held()) cz.home_used = 1;
            if (w == 15u && cos_home_held()) cos_tap(fm1_ms);   /* (HOME + white 16: TAP TEMPO) */
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
                trk[j].p[P_MUTE] = (int16_t)!trk[j].p[P_MUTE];
                cos_say(trk[j].p[P_MUTE] ? "MUTED" : "UNMUTED");
            }
            else if (j < trk_live) { fm1_irq_off(); live_qmute ^= (uint8_t)(1u << j); fm1_irq_on(); }   /* (on the next
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

/* (cursed-os) the IDLE screen: untouched for SYSTEM's IDLE (default 1 MIN), a tunnel of turning squares falling in on the
 * beat (one each beat while it plays, the bar's first red), an orb the output's wave shapes, the tempo, the music's name
 * and the battery. The first touch only wakes it (swallowed); the sound plays on throughout */
static uint32_t cos_idle_lim(void) { return cos_idle_min() * 60000u; }   /* (fm1_ms: the time since power-on, no clock) */
static inline int32_t cos_isin(uint32_t a) { return SINE[a & 1023u]; }   /* (a: 1024 a turn) */
#include "cos_skull.h"
/* the frame's plan, the same for its six strips: the beat's phase, the glitches (bands of rows torn sideways, some
 * turned red; red lines struck across), the skull (now and then, a few seconds) */
static struct {
    uint32_t ph, beat, rng, burst, g_next, sk_on, sk_next, lvl, pic;
    uint32_t fx, fy, f_t, f_next;                        /* the cube: its turns about X and Y (1024 a turn) when the last
                                                         * one ended; the one going on (from f_t, 700 ms), the next's time */
    int8_t f_ax, f_dir;                                  /* .. its axis (0 X, 1 Y; -1 none) and way */
    int16_t vx[8], vy[8], vz[8];                         /* .. its corners this frame, on the screen (vz: depth) */
    uint8_t nb, nl;
    struct { uint8_t y, h, red; int8_t dx; } b[6];
    struct { uint8_t y, x, w, t; } l[8];
} gl COS_RAM;
static uint32_t gl_rnd(void) { gl.rng ^= gl.rng << 13; gl.rng ^= gl.rng >> 17; gl.rng ^= gl.rng << 5; return gl.rng; }
static void cos_idle_start(void)                         /* (it comes up: the first glitch in seconds, a picture later) */
{
    gl.rng = (fm1_ms * 2654435761u) | 1u;
    gl.sk_on = 0;
    gl.sk_next = fm1_ms + 20000u + gl_rnd() % 20000u;
    gl.g_next = fm1_ms + 2000u + gl_rnd() % 4000u;
    gl.burst = 0;
    gl.f_ax = -1;
    gl.f_next = fm1_ms + 3000u + gl_rnd() % 4000u;
}
/* the cube's corners: its turns (X first, then Y with a slow drift), tilted 28 degrees towards us, in perspective */
static void cos_cube_prep(uint32_t t)
{
    uint32_t ax = gl.fx, ay = gl.fy + t / 64u, i;      /* (the drift: a turn in ~65 s) */
    if (gl.f_ax < 0 && t - gl.f_next < 0x80000000u) {   /* now and then it rolls onto another side: 90 degrees, 0.7 s */
        gl.f_ax = (int8_t)((gl_rnd() >> 12) & 1u);
        gl.f_dir = (int8_t)((gl_rnd() >> 13) & 1u ? 1 : -1);
        gl.f_t = t;
    }
    if (gl.f_ax >= 0) {
        uint32_t e = t - gl.f_t, k;
        if (e >= 700u) {                                 /* (done: the turn kept, the next in 3 .. 9 s) */
            if (gl.f_ax) gl.fy += (uint32_t)(256 * gl.f_dir); else gl.fx += (uint32_t)(256 * gl.f_dir);
            gl.f_ax = -1;
            gl.f_next = t + 3000u + gl_rnd() % 6000u;
            ax = gl.fx; ay = gl.fy + t / 64u;
        } else {
            k = e * 1024u / 700u;                        /* (eased: smoothstep, 0 .. 1024) */
            k = k * k * (3072u - 2u * k) >> 20;
            if (gl.f_ax) ay += (uint32_t)((int32_t)(256 * gl.f_dir) * (int32_t)k / 1024);
            else ax += (uint32_t)((int32_t)(256 * gl.f_dir) * (int32_t)k / 1024);
        }
    }
    {
        int32_t cxa = cos_isin(ax + 256u), sxa = cos_isin(ax), cya = cos_isin(ay + 256u), sya = cos_isin(ay);
        int32_t ct = 28898, st = 15384;                  /* (the tilt: cos, sin 28 degrees, Q15) */
        for (i = 0; i < 8u; i++) {
            int32_t x = i & 1u ? 46 : -46, y = i & 2u ? 46 : -46, z = i & 4u ? 46 : -46, y1, z1, x2, z2, y3, z3, d;
            y1 = (y * cxa - z * sxa) >> 15; z1 = (y * sxa + z * cxa) >> 15;          /* about X */
            x2 = (x * cya + z1 * sya) >> 15; z2 = (z1 * cya - x * sya) >> 15;        /* about Y */
            y3 = (y1 * ct - z2 * st) >> 15; z3 = (y1 * st + z2 * ct) >> 15;          /* the tilt */
            d = z3 + 260;                                                            /* (the eye 260 away) */
            gl.vx[i] = (int16_t)(120 + x2 * 230 / d);
            gl.vy[i] = (int16_t)(114 + y3 * 230 / d);
            gl.vz[i] = (int16_t)z3;
        }
    }
}
static void cos_idle_prep(void)
{
    uint32_t w = scope_w, j, bs = beat_samples(), t = fm1_ms, pk = 0;
    cz.idle_t = t;
    if (song.playing && bs) { gl.ph = ((beat_pos << 10) / bs) & 1023u; gl.beat = beat_n & 3u; }   /* (a beat under 2^21
                                                         * samples: 32 bits) */
    else { gl.ph = (t / 3u) & 1023u; gl.beat = 1; }      /* (stopped: a slow fall, ~3 s a square) */
    for (j = 0; j < 256u; j += 4u) {                     /* the output's peak just now: the louder, the more it tears */
        int32_t v = scope_buf[(w - 256u + j) & (SCOPE_N - 1u)];
        v = v < 0 ? -v : v;
        if ((uint32_t)v > pk) pk = (uint32_t)v;
    }
    gl.lvl = pk;
    cos_cube_prep(t);
    if (!gl.sk_on && t - gl.sk_next < 0x80000000u) {     /* a picture (the skull, or the hammer and sickle): under a
                                                         * second, now and then (20 .. 40 s in, then every 40 .. 120 s) */
        gl.sk_on = t + 300u + gl_rnd() % 180u;            /* (about half a second) */
        {                                                /* (the skull 40 %, the hammer and sickle 30 %, the circle-A 30) */
            uint32_t r = (gl_rnd() >> 8) % 100u;
            gl.pic = r < 40u ? 0u : r < 70u ? 1u : 2u;
        }
        gl.sk_next = t + 40000u + gl_rnd() % 80000u;
        gl.burst = t + 160u;                             /* (it tears in) */
    }
    if (gl.sk_on && t - gl.sk_on < 0x80000000u) { gl.sk_on = 0; gl.burst = t + 120u; }   /* (and out) */
    if (t - gl.g_next < 0x80000000u) {                   /* a glitch burst at random times: 4 .. 15 s apart, 60 .. 300 ms
                                                         * long (a little sooner when it's loud) */
        gl.burst = t + 60u + gl_rnd() % 240u;
        gl.g_next = t + 4000u + gl_rnd() % 11000u - (pk > 16384u ? 1500u : 0u);
    }
    gl.nb = gl.nl = 0;
    if (t - gl.burst >= 0x80000000u) {                   /* a burst: bands torn, red lines */
        uint32_t k;
        gl.nb = (uint8_t)(1u + gl_rnd() % 5u);
        for (k = 0; k < gl.nb; k++) {
            uint32_t r = gl_rnd();
            gl.b[k].y = (uint8_t)(18u + r % 190u);
            gl.b[k].h = (uint8_t)(2u + (r >> 8) % 18u);
            gl.b[k].dx = (int8_t)((int32_t)((r >> 16) % 61u) - 30);
            gl.b[k].red = (uint8_t)((r >> 24) % 3u == 0u);
        }
        gl.nl = (uint8_t)(2u + gl_rnd() % 6u);
    } else if (gl_rnd() % 1000u < 2u) {
        gl.nl = 1;                                       /* (between: a stray red line, rarely) */
    }
    for (j = 0; j < gl.nl; j++) {
        uint32_t r = gl_rnd();
        gl.l[j].y = (uint8_t)(18u + r % 192u);
        gl.l[j].x = (uint8_t)((r >> 8) % 200u);
        gl.l[j].w = (uint8_t)(20u + (r >> 16) % 220u);
        gl.l[j].t = (uint8_t)(1u + ((r >> 28) & 1u));
    }
}
/* a picture (w x h, 1 bit) at 2 x, its top left at x, y (whole-screen rows): straight into the strip's pixels */
static void cos_pic_at(const uint8_t *pic, uint32_t w, uint32_t h, int32_t x, int32_t y, uint16_t c)
{
    uint32_t sy, sx, sw = swap16(c);
    for (sy = 0; sy < h; sy++) {
        int32_t r, k;
        for (k = 0; k < 2; k++) {
            r = y + (int32_t)sy * 2 + k + cv_oy;
            if (r < 0 || r >= (int32_t)cv_h) continue;
            {
                uint16_t *row = cv_px + (uint32_t)r * cv_w;
                const uint8_t *src = pic + sy * (w / 8u);
                for (sx = 0; sx < w; sx++)
                    if ((src[sx >> 3] >> (7u - (sx & 7u))) & 1u) {
                        int32_t px = x + (int32_t)sx * 2;
                        if (px >= 0 && px + 1 < (int32_t)cv_w) row[px] = row[px + 1] = (uint16_t)sw;
                    }
            }
        }
    }
}
static void cos_idle_scene(void)
{
    static const uint8_t E[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7},
                                     {0, 4}, {1, 5}, {2, 6}, {3, 7}};   /* (the cube's edges: corners differing in a bit) */
    int32_t i;
    if (gl.sk_on) {                                      /* a picture: on black, a red ghost torn off it while it glitches */
        static const uint8_t *const PIC[3] = {SKULL, HSKL, ANAR};
        static const uint8_t PW[3] = {SKULL_W, HSKL_W, ANAR_W}, PH[3] = {SKULL_H, HSKL_H, ANAR_H};
        uint32_t k = gl.pic % 3u, w = PW[k], h = PH[k];
        int32_t sx = 120 - (int32_t)w, sy = 116 - (int32_t)h;
        uint16_t ink = k == 2u ? C_RED : C_FG, ghost = k == 2u ? C_FG : C_RED;   /* (the circle-A red, its ghost white) */
        if (gl.nb) cos_pic_at(PIC[k], w, h, sx + 4 + (int32_t)(gl.rng % 5u), sy, ghost);
        cos_pic_at(PIC[k], w, h, sx, sy, ink);
    } else
        for (i = 0; i < 12; i++) {                       /* the cube, a wire frame: the far edges dim */
            uint32_t a = E[i][0], c = E[i][1];
            cv_line(gl.vx[a], gl.vy[a], gl.vx[c], gl.vy[c], gl.vz[a] + gl.vz[c] > 0 ? C_DARK : C_MID);
        }
    for (i = 0; i < (int32_t)gl.nl; i++)                 /* red lines struck across */
        cv_rect(gl.l[i].x, gl.l[i].y, gl.l[i].w, gl.l[i].t, C_RED);
    cv_rect(0, 0, 240, 16, C_BG);                        /* (the words on black: the name, the music's) */
    cos_tx(4, 4, "CURSED-OS", C_DIM);
    cv_rect(0, 212, 240, 28, C_BG);
    if (proj_name_buf()[0]) cos_txc(120, 222, CZ_LBL, proj_name_buf(), C_DIM);
}
/* after a strip is drawn: the bands torn sideways (the rows of strip s, 40 from s x 40), some turned red */
static void cos_idle_post(uint32_t s)
{
    uint32_t k, y, x;
    uint16_t red = swap16(C_RED), tmp[240];
    for (k = 0; k < gl.nb; k++)
        for (y = gl.b[k].y; y < (uint32_t)gl.b[k].y + gl.b[k].h && y < 212u; y++) {
            uint16_t *row;
            int32_t dx = gl.b[k].dx;
            if (y < s * 40u || y >= s * 40u + 40u) continue;
            row = cv_px + (y - s * 40u) * cv_w;
            for (x = 0; x < 240u; x++) tmp[x] = row[x];
            for (x = 0; x < 240u; x++) {
                int32_t from = (int32_t)x - dx;
                uint16_t v = from >= 0 && from < 240 ? tmp[from] : 0u;
                row[x] = gl.b[k].red && v ? red : v;
            }
        }
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
        cos_idle_start();
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
    else if (bpm_frac) cos_bpm_text(b, song.g[G_BPM] * 10 + bpm_frac % 10u);
    else fmt_int(b, song.g[G_BPM]);
    if (bpm_nudge) str_cpy(b + str_len(b), bpm_nudge > 0 ? "+" : "-", 2);   /* (nudged: 3 %, red) */
    cos_txr(226, 3, b, bpm_nudge ? C_RED : C_FG);
    cos_txr(226 - 8 * 4, 3, COS_NN[cz.key_next ? cz.key_next - 1u : cos_key()], cz.key_next ? C_RED : C_MID);
    cos_batt_text(b);                                    /* (the battery: between the engine and the key) */
    if (b[0]) cos_txr(176, 3, b, cos_batt_pct() <= 15 ? C_RED : C_DIM);
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
    if (cz.mode == CM_TRIG && cos_held()) {               /* (steps held: what a knob locks) */
        char b[24];
        str_cpy(b, "LOCK ", sizeof b);
        str_cpy(b + 5, cos_lk_page()->tab, 12);
        cv_rect(2, 20, cos_w(b) + 6, 12, C_RED);
        cos_tx(5, 22, b, C_BG);
        return;
    }
    cos_pages(cz.mode, &p, &n);
    {
        uint32_t cur = cz.tab[cz.mode] % n, f = 0, j;
        const engine_t *e = trk_engp(TSEL);
        const char *nm[12];
        int32_t span;
        for (i = 0; i < n && i < 12u; i++)                /* (EDIT's first two: the engine's own names for them) */
            nm[i] = cz.mode == CM_SRC && i < 2u && !cos_kit() && e->page_title[i] ? e->page_title[i] : p[i].tab;
        do {                                             /* (the first one shown: the page picked fits) */
            for (span = 0, j = f; j <= cur; j++) span += cos_w(nm[j]) + 10;
        } while (span > 236 && ++f < cur);
        if (f) cos_tx(0, 22, "<", C_DIM);
        for (i = f, x = f ? 10 : 2; i < n && i < 12u; i++) {
            int32_t w = cos_w(nm[i]) + 6;
            if (x + w > 240) { cos_txr(240, 22, ">", C_DIM); break; }
            if (i == cur) { cv_rect(x, 20, w, 12, C_FG); cos_tx(x + 3, 22, nm[i], C_BG); }
            else cos_tx(x + 3, 22, nm[i], C_DIM);
            x += w + 4;
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
    uint64_t held = cz.mode == CM_TRIG ? cos_held() : 0u;   /* (TRIG with steps held: the lock page, the first one's locks) */
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
    if (held && p->kind == CK_TRK) {
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
    if (p->id[k & 3u] == ID_BPM) cos_bpm_text(val, v);
    if (p->kind == CK_COS && p->id[k & 3u] == CC_MOCC && v < 0) { str_cpy(val, "OFF", sizeof val); unit = 0; }
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
        if (v == 1u) cv_rect(x + 1, y + 1, 11, 11, C_MID);   /* (a trig grey, an accent white: red is the playhead's) */
        else if (v == 2u) cv_rect(x + 1, y + 1, 11, 11, C_FG);
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
    if (e != &ENG_DRUM && e != &ENG_ACID && e != &ENG_FORMANT && !tr->user && e->npresets && tr->preset < e->npresets)
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
        if (cz.dr_n && cz.dr_trk == t) cv_rect(236, y + 1, 3, 7, C_FG);   /* (its drone holds) */
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
        if (b[0]) cos_tx(4, 144, b, C_RED);
        else if (cz.tab[CM_HOME] == 5u)                  /* (LOOP) */
            cos_tx(4, 144, "W15 BACK  HOME+W15 ALL BACK", C_DIM);
        else if (cz.tab[CM_HOME] == 4u)                  /* (MORPH: the keys; A and B lit when stored) */
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
        cos_tx(44, 90, "B1 ACCENT", nt && (s->flags & SF_ACCENT) ? C_RED : C_DIM);
        cos_tx(132, 90, "B2 SLIDE", nt && (s->flags & SF_SLIDE) ? C_RED : C_DIM);
        cos_txc(120, 106, CZ_LBL, "PRESETS: THE NOTE", C_MID);
        cos_txc(120, 120, CZ_LBL, "KNOB LOCK  REC CLEAR", C_DIM);
        return;
    }
    cv_rect(28, 62, 184, 52, C_BG);
    cv_frame(28, 62, 184, 52, C_RED);
    cos_txc(120, 70, CZ_TALL, b, C_FG);
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
    for (i = 0; i < NTRK && drum_track(t); i++) t = &trk[i];   /* (the first melodic track's scale: all share it) */
    if (!drum_track(t) && t->p[P_SCALE]) mask = scale_mask(t);
    str_cpy(b, COS_NN[cur], sizeof b);
    if (!drum_track(t)) { str_cpy(b + str_len(b), " ", 2); str_cpy(b + str_len(b), N_SCALE[clamp(t->p[P_SCALE], 0, 15)], 7); }
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

/* DRONE: the track and its held notes large, an octave and a half of keys (the held ones lit), the octave; the drone
 * plays on through PLAY and STOP */
static void cos_body_drone(void)
{
    const uint8_t *BL = COS_BLF;
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
    cos_txc(120, 146, CZ_LBL, song.playing ? "OCT+ STORE  OCT- NEXT BAR" : "OCT+ SAVE  OCT- LOAD", C_DIM);
}

/* the FX pad (FX held): the 16 white keys' effects in two columns, the black keys' under them; an effect on: red */
static int cos_fx_on(uint32_t c)
{
    return c < CX_CODE ? (int)(((perf_held | perf_latched) >> c) & 1u) : (int)((cfx_now() >> (c - CX_CODE)) & 1u);
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
    cos_tx(8, 40, "DRIVE>LIMIT>CLIP>OUT", C_MID);
    cos_gr_meter(58, "LIMIT GAIN REDUCTION", gr, 1);
    cos_tx(8, 88, "CEILING: THE PEAK OUT, DBFS", C_DIM);
    cos_tx(8, 100, "DRIVE: HARDER INTO CEILING", C_DIM);
    cos_tx(8, 112, "AUTO LV ON: THE LEVELER", C_DIM);
    cos_tx(8, 124, "RIDES THE LOUDNESS. OFF: NOT", C_DIM);
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
    case CM_PROJ: cos_body_proj(); break;
    case CM_DRONE: cos_body_drone(); break;
    case CM_MIX: {
        const cpg_t *pg = cos_page();
        if (pg->kind == CK_COS && pg->id[0] == CC_OUTCH) cos_body_midi();
        else if (pg->kind == CK_COS && pg->id[0] == CC_TRKS) cos_body_sys();
        else if (pg->kind == CK_COS && (pg->id[0] == CC_COMP || pg->id[0] == CC_CIN || pg->id[0] == CC_CSC)) cos_body_master();
        else if (pg->kind == CK_COS && pg->id[0] == CC_LIN) cos_body_lim();
        else if (pg->kind == CK_COS && pg->id[0] == CC_PCCH) cos_body_link();
        else if (pg->kind == CK_COS && (pg->id[0] == CC_MCCH || pg->id[0] == CC_MC0)) cos_body_link2();
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
#define COS_VERSION "0.8.9"
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
    if (cz.idle_up) cos_idle_prep();                     /* (one time and one wave for every strip of the frame) */
    for (s = 0; s < 6u; s++) {
        cv_begin(240, 40, C_BG);
        cv_oy = -(int32_t)s * 40;
        cos_scene();
        if (cz.idle_up) cos_idle_post(s);                /* (the IDLE screen's torn bands) */
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
    if (!t || drum_track(t) || (cz.mode != CM_KEY && cos_kit()) || !t->p[P_SCALE] || cz.fx_down) return 0;
    if (cz.mode == CM_TRIG) return cz.si;
    return cz.mode != CM_HOME && cz.mode != CM_PROJ;
}
static __attribute__((noinline)) void cos_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0}, nm[FM1_NCOL] = {0}, nh[FM1_NCOL] = {0};
    const track_t *t = TSEL;
    uint32_t k, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), now = song.playing ? t->seq_idx % len : ~0u;
    static uint8_t ready COS_RAM;
    if (fm1_led_anim_on()) return;
    if (!ready) { led_pos_init(); ready = 1; }
    led_put(nl, panel.btn[CM_BTN[cz.mode]], 1);
    led_put(nl, panel.btn[B_REC], song.rec != 0u);
    for (k = 0; k < NB; k++) led_put(nd, panel.btn[k], 1);
    if ((cz.mode == CM_TRIG && !cz.si) || cz.mode == CM_HOME) {
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
        fm1_led_dim[k] = (uint8_t)(nd[k] & ~nl[k]);
        fm1_led_breath[k] = 0;
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
