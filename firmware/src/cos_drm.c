/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursed-os 0.13.0) LFO > DRUMS, in DRONE's place (GG: "remove the drone"): the drums played live.
 *   white 1..8 held  ROLL that drum (KICK SNARE CLAP HATCL HATOP TOM RIM BELL) at ROLL's RATE, on the beat's grid
 *   black 1..4 held  REPEAT: the kit and the KICK play their last 8 / 4 / 2 / 1 steps again (a beat repeat), let go:
 *                    back where they would have been (pfx.c, a LOOP's way)
 *   ROLL   RATE 1/8 1/16 1/32 1/16T 1/32T, RAMP FLAT / UP / DOWN (over 16 hits), VEL
 *   CHANCE, CHANCE 2  each drum's CHANCE, 0 .. 100 % (the project keeps it: 'DC'; seq.c seq_step)
 * Its keys play nothing else (cos_keys_ui); the ISR's side: eng_drum.c drm_*, pfx.c drm_roll_block */
static const char *const DRM_RATE[] = {"1/8", "1/16", "1/32", "1/16T", "1/32T", 0};
static const char *const DRM_RAMP[] = {"FLAT", "UP", "DOWN", 0};
static const uint8_t DRM_LPN[4] = {8, 4, 2, 1};          /* black 1..4: the steps repeated */
static const param_desc_t *cos_drm_desc(uint32_t id, int16_t **vp)
{
    static const param_desc_t DR[3] = {{"RATE", F_ENUM, 0, 4, 1, DRM_RATE, 0}, {"RAMP", F_ENUM, 0, 2, 0, DRM_RAMP, 0},
                                       {"VEL", F_INT, 1, 127, 110, 0, 0}};
    static const param_desc_t DC[8] = {{"KICK", F_INT, 0, 100, 100, 0, "%"}, {"SNARE", F_INT, 0, 100, 100, 0, "%"},
                                       {"CLAP", F_INT, 0, 100, 100, 0, "%"}, {"HATCL", F_INT, 0, 100, 100, 0, "%"},
                                       {"HATOP", F_INT, 0, 100, 100, 0, "%"}, {"TOM", F_INT, 0, 100, 100, 0, "%"},
                                       {"RIM", F_INT, 0, 100, 100, 0, "%"}, {"BELL", F_INT, 0, 100, 100, 0, "%"}};
    int16_t *c = &cz.cc_v[14];
    *vp = c;
    if (id == CC_RRATE) { *c = drm_rate; return &DR[0]; }
    if (id == CC_RRAMP) { *c = drm_ramp; return &DR[1]; }
    if (id == CC_RVEL) { *c = drm_vel; return &DR[2]; }
    *c = (int16_t)(100 - dx_skip[(id - CC_DCH0) & 7u]);
    return &DC[(id - CC_DCH0) & 7u];
}
static void cos_drm_put(uint32_t id, int32_t v)
{
    if (id == CC_RRATE) drm_rate = (uint8_t)clamp(v, 0, 4);
    else if (id == CC_RRAMP) drm_ramp = (uint8_t)clamp(v, 0, 2);
    else if (id == CC_RVEL) drm_vel = (uint8_t)clamp(v, 1, 127);
    else dx_skip[(id - CC_DCH0) & 7u] = (uint8_t)(100 - clamp(v, 0, 100));
}
/* (each frame) what is held: the rolls and the REPEAT, for the ISR; nothing outside DRUMS */
static void cos_drm_poll(void)
{
    uint32_t r = 0, l = 0, k, notes = fm1_in.notes;
    if (cz.mode == CM_DRUMS && !cz.fx_down) {
        for (k = 0; k < 8u; k++) r |= ((notes >> WK[k]) & 1u) << k;
        for (k = 0; k < 4u; k++) if ((notes >> BK[k]) & 1u) l = DRM_LPN[k];   /* (the shortest held wins) */
    }
    drm_roll = (uint8_t)r;
    drm_lpn = (uint8_t)l;
}
static void cos_drm_key(uint32_t k)
{
    uint32_t w, j;
    for (w = 0; w < 16u && WK[w] != k; w++) {}
    for (j = 0; j < 11u && BK[j] != k; j++) {}
    if (w < 8u) { static const char *const N[8] = {"KICK ROLL", "SNARE ROLL", "CLAP ROLL", "HATCL ROLL", "HATOP ROLL",
                                                   "TOM ROLL", "RIM ROLL", "BELL ROLL"}; cos_say(N[w]); }
    else if (j < 4u) {
        static const char *const R[4] = {"REPEAT 8 STEPS", "REPEAT 4 STEPS", "REPEAT 2 STEPS", "REPEAT 1 STEP"};
        cos_say(song.playing ? R[j] : "REPEAT: PLAY FIRST");
    }
}
/* the keys as the drums (white: lit while rolling; black 1..4 the REPEATs) */
static void cos_drm_leds(uint8_t *nl, uint8_t *nb)
{
    uint32_t k;
    (void)nb;
    for (k = 0; k < 8u; k++) led_put(nl, 14u + WK[k], (int)((drm_roll >> k) & 1u) || !cos_drum_out(k));
    for (k = 0; k < 4u; k++) led_put(nl, 14u + BK[k], drm_lpn == DRM_LPN[k]);
}
static void cos_body_drm(void)
{
    static const char *const D[8] = {"KICK", "SNARE", "CLAP", "HATCL", "HATOP", "TOM", "RIM", "BELL"};
    static const char *const R[4] = {"8", "4", "2", "1"};
    char b[24];
    uint32_t k;
    cos_tx(4, 36, "HOLD A WHITE KEY: ROLL", C_MID);
    for (k = 0; k < 8u; k++) {
        int32_t x = 4 + (int32_t)(k % 4u) * 59, y = 50 + (int32_t)(k / 4u) * 22;
        int on = (drm_roll >> k) & 1u;
        if (on) cv_rect(x - 2, y - 3, 57, 15, C_RED);
        cos_tx(x, y, D[k], on ? C_BG : cos_drum_out(k) ? C_DIM : C_FG);
        if (dx_skip[k]) {                                  /* (its CHANCE, under 100 %) */
            fmt_int(b, 100 - (int32_t)dx_skip[k]);
            str_cpy(b + str_len(b), "%", 2);
            cos_tx(x + 40, y, b, on ? C_BG : C_MID);
        }
    }
    cos_tx(4, 100, "HOLD BLACK 1-4: REPEAT", C_MID);
    for (k = 0; k < 4u; k++) {
        int32_t x = 4 + (int32_t)k * 59, on = drm_lpn == DRM_LPN[k];
        if (on) cv_rect(x - 2, 111, 57, 15, C_RED);
        str_cpy(b, R[k], 4);
        str_cpy(b + str_len(b), k == 3u ? " STEP" : " STEPS", 7);
        cos_tx(x, 114, b, on ? C_BG : song.playing ? C_FG : C_DIM);
    }
    str_cpy(b, "ROLL ", 6);
    str_cpy(b + 5, DRM_RATE[drm_rate % 5u], 6);
    str_cpy(b + str_len(b), "  ", 3);
    str_cpy(b + str_len(b), DRM_RAMP[drm_ramp % 3u], 6);
    cos_tx(4, 136, b, C_DIM);
}
