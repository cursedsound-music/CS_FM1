/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
#define FELUCCA_FAVORITES 1
/* Stable engine/preset references; fixed capacity independent of optional engines. */
typedef struct {
    uint8_t factory[16][32]; /* engine 0..15, preset 0..255 */
    uint32_t user, filter;
} favorites_t;
static favorites_t favorites;
/* (0.9.9-snd5) the stars of GRAIN's and SLICE's numbers (retired) cleared once: they are STRING's and ADD's now (ui.c
 * prefs_update: the settings' revision 3) */
static void settings_save(void);
static __attribute__((noinline)) void fav_rows_new(void)
{
    memset(favorites.factory[8], 0, sizeof favorites.factory[8]);           /* (core.h ENGI_STRING) */
    memset(favorites.factory[13], 0, sizeof favorites.factory[13]);         /* (core.h ENGI_ADD) */
    favorites.factory[15][25] = 3u;                    /* (prefs_rev) */
    settings_save();
}
/* (cursed-os) the row of an engine's stars: ACID (15) in SAMPLE's (4, retired, no presets): row 15 is the settings'
 * spare bytes (ui.c hint_given, cos.c's MIDI channels and master) */
#ifndef ENGI_ACID
#define ENGI_ACID 15u                                       /* (core.h's; a test without it) */
#endif
#ifndef ENGI_KICK
#define ENGI_KICK 4u
#endif
#ifndef ENGI_FMDRUM
#define ENGI_FMDRUM 9u
#endif
/* (cursed-os) KICK (4; 0.9.5 SAMPLER) in TRIO's (6, retired; its bytes 0..7: its presets were < 64, so 0; 22..31 Bluetooth's) */
static uint32_t fav_row(uint32_t engine) { return engine == ENGI_ACID ? 4u : engine == ENGI_SAMPLE ? 6u : engine; }
/* (cursed-os 0.11.0) the byte of a factory star: FMDRUM (9: CRASH_LOG's row, cpu_safe.c) in row 6's bytes 4..7, after
 * KICK's 0..3 (each at most 32 presets there; the row's bytes 8.. are settings) */
static uint8_t *fav_byte(uint32_t engine, uint32_t preset)
{
    if (engine == ENGI_FMDRUM)
        return preset < 32u ? &favorites.factory[6][4u + preset / 8u] : 0;
    if (engine == ENGI_KICK && preset >= 32u)
        return 0;
    return &favorites.factory[fav_row(engine)][preset / 8u];
}
static int favorite_has(uint32_t engine, uint32_t preset)
{
    const uint8_t *b;
    if (engine == NENGINES)
        return preset < UP_SLOTS && ((favorites.user >> preset) & 1u);
    if (!(engine < NENGINES && engine < 16u && preset < 256u) || !(b = fav_byte(engine, preset)))
        return 0;
    return (*b >> (preset % 8u)) & 1u;
}
static int favorite_set(uint32_t engine, uint32_t preset, int on)
{
    if (engine > NENGINES || (engine == NENGINES ? preset >= UP_SLOTS : engine >= 16u || preset >= 256u))
        return 0;
    if (favorite_has(engine, preset) == !!on) return 0;
    if (engine == NENGINES) {
        if (on) favorites.user |= 1u << preset;
        else favorites.user &= ~(1u << preset);
    } else {
        uint8_t *b = fav_byte(engine, preset);
        if (!b)
            return 0;
        if (on) *b |= (uint8_t)(1u << (preset % 8u));
        else *b &= (uint8_t)~(1u << (preset % 8u));
    }
    return 1;
}
