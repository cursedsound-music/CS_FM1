/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursed-os 0.10.0) KICK: DRUM-X's kick on a track of its own (engine 4: SAMPLE's number, SAMPLER's in 0.9.5..0.9.9),
 * so the kick comes in, goes out and is shaped apart from the rest of the kit (GG: "drums without kicks, then the
 * kick, then the other instruments"). The DRUM engine's voice, its render, its bus (DRIVE and the knee), its kill (the
 * FX pad's KICK KILL, DXG_KICK), its DUCK trigger: eng_drum.c's, with:
 *   - the sound: one of KICK_PATCH (the factory kits' kicks and three of its own), per track (E0 KIND), not the
 *     section's kit (dx_kit, the DRUM track's): changing the DRUM track's kit leaves the kick as it is
 *   - the keys: every key the kick, tuned by its distance from C (the first C: as struck; up an octave, down two: a
 *     kick's bass line, the rumble's notes)
 *   - one voice: a hit cuts the last (a kick is mono)
 * Parameters (E0..E7): KIND, TUNE, TONE, DECAY, CLICK (the kit's NOISE), SWEEP (FM ENV), FOLD, DRIVE: TUNE .. DRIVE
 * are DRUM's own (dx_run's offsets, drum_bus's DRV), so a kit's knobs and a kick's do the same. No MORPH: a kick
 * plays its patch's A side, moved by the LFO's SHAPE (64: the middle) */
static const char *const KICK_N[] = {"DRUM-X", "909", "808", "LOFI", "INDUS", "ACOUST", "DUB", "TECHNO", "BREAKS",
                                     "DEEP", "PUNCH", "RUMBLE", 0};
#define KICK_NKIT 9u                                      /* (the first nine: DX_KITS' kicks, in their order) */
static const dx_lane_t KICK_OWN[3] = {
    {DX_MODE(DXW_SINE, DXF_LP, 1) | DXM_LONG << 5, {2, 50, 112, 2, 10}, {5, 80, 124, 6, 30}},   /* DEEP: low, long */
    {DX_MODE(DXW_SINE, DXF_LP, 1) | DX_DRIVE, {10, 110, 56, 8, 70}, {14, 127, 70, 16, 100}},     /* PUNCH: short, hard */
    {DX_MODE(DXW_SINE, DXF_LP, 1) | DXM_LONG << 5 | DX_DRIVE, {4, 60, 120, 4, 40}, {7, 90, 127, 10, 70}},   /* RUMBLE */
};
_Static_assert(sizeof DX_KITS / sizeof DX_KITS[0] == KICK_NKIT, "KICK's kinds: the kits' kicks first");
static const dx_lane_t *kick_patch(const track_t *t)
{
    uint32_t k = (uint32_t)clamp(t->p[P_E0], 0, (int32_t)(KICK_NKIT + 2u));
    return k < KICK_NKIT ? &DX_KITS[k].lane[DV_KICK] : &KICK_OWN[k - KICK_NKIT];
}

static void kick_note_on(track_t *t, voice_t *v)
{
    drum_lane_t *K = drum_kit_of(t), *L;
    uint32_t i = (uint32_t)(v - t->v);
    if (!K || i >= NVOICE)
        return;
    L = &K[DV_KICK];
    L->st = (int8_t)clamp((int32_t)v->note - 36 + t->pfx_rpit / 16 + t->dpit, -36, 36);   /* (the first C: as struck) */
    L->bc = 0;
    L->bd = t->dbend;
    L->bs = (int16_t)(t->dbend ? ((t->dbend < 0 ? -t->dbend : t->dbend) + t->dbt) / (t->dbt ? t->dbt : 1u) : 0);
    L->owner = (uint8_t)(i + 1u);
    L->dm = 0;
    L->fl = 0;
    L->rm = L->rt = 0;
    L->mg = DXG_FADE;
    L->age = 0;
    v->s[0] = DV_KICK;
    v->env_out = v->vel * 258;                           /* (as drum_note_on: the hit at its level, the ADSR held) */
    v->env = 1 << 24;
    duck_hit = 1;                                        /* (DUCK: the kick's) */
    dx_trigger(&L->x);
}

/* the keys: the first C (key 7) is C2 (36), the kick as struck, as DRUM's */
static int32_t kick_keys(const track_t *t, uint32_t k)
{
    (void)t;
    return clamp(29 + (int32_t)k, 0, 127);
}

/* {KIND, TUNE, TONE, DECAY, CLICK, SWEEP, FOLD, DRIVE} */
static const preset_t KICK_PRESETS[] = {
    {"TECHNO", {7, 64, 64, 64, 64, 0, 0, 20}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 0), PAT(0)},
    {"909", {1, 64, 64, 64, 64, 0, 0, 10}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 0), PAT(0)},
    {"808 LONG", {2, 64, 60, 84, 60, 0, 0, 0}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 0), PAT(0)},
    {"DEEP", {9, 62, 56, 72, 56, 0, 0, 10}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 0), PAT(0)},
    {"PUNCH", {10, 64, 72, 56, 70, 10, 0, 40}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 0), PAT(0)},
    {"RUMBLE", {11, 60, 58, 84, 60, 0, 0, 50}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 0), PAT(0)},
    {"INDUSTRIAL", {4, 66, 84, 64, 70, 20, 20, 60}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 0), PAT(0)},
    {"DUB", {6, 60, 52, 76, 56, 0, 0, 0}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 0), PAT(0)},
};

static const engine_t ENG_KICK = {
    .name = "KICK",
    .page_title = {"KICK", "KICK 2"},
    .edit = {
        {"KIND", F_ENUM, 0, 11, 7, KICK_N, 0},
        {"TUNE", F_PCT, 0, 127, 64, 0, 0},
        {"TONE", F_PCT, 0, 127, 64, 0, 0},
        {"DECAY", F_PCT, 0, 127, 64, 0, 0},
        {"CLICK", F_PCT, 0, 127, 64, 0, 0},
        {"SWEEP", F_PCT, 0, 127, 0, 0, 0},
        {"FOLD", F_PCT, 0, 127, 0, 0, 0},
        {"DRIVE", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = KICK_PRESETS,
    .npresets = NELEM(KICK_PRESETS),
    .note_on = kick_note_on,
    .render = drum_render,
    .amp = drum_amp,
    .knob = {P_E1, P_E3, P_E2, P_E7},
    .poly = 1,
    .oneshot = 1,
    .keys = kick_keys,
};
/* GEN on a KICK track: its PATTERN (16 steps, the kick's C; cos.c cos_kick_pat) */
#define KICK_NPAT 8u
static const char *const KICK_PAT_N[KICK_NPAT + 1u] = {"4/4", "4/4+", "ROLL", "BROKEN", "HALF", "GALLOP", "NO 4", "OFF 3", 0};
static const uint16_t KICK_PAT[KICK_NPAT] = {     /* bit i: a kick on step i + 1 */
    0x1111u,                                      /* 4/4: every beat */
    0x9111u,                                      /* 4/4+: and a sixteenth before the next bar */
    0x4949u,                                      /* ROLL: 1 4 7 . (3 3 2), the rolling one */
    0x4911u,                                      /* BROKEN: the second half broken */
    0x0101u,                                      /* HALF: beats 1 and 3 */
    0x1919u,                                      /* GALLOP: 1 4 5 . 9 12 13 */
    0x0111u,                                      /* NO 4: the fourth beat left out (a lift) */
    0x5111u,                                      /* OFF 3: and the third beat's offbeat */
};
/* a part that plays DRUM-X's hits (the kit, or the kick): its bus, its DUCK, its core */
#define ENG_HITS(e) ((e) == &ENG_DRUM || (e) == &ENG_KICK)
