/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursed-os 0.9.5) SAMPLER (engine 4, SAMPLE's number, retired in JIANT): the sounds the phone sent (cos_smp.h, the
 * flash of songs 5..8; tools/cursed_remote.py), played from flash as they decode (IMA ADPCM, a block of 256 at a
 * time), at the key's pitch.
 *   SAMPLE  which of the 16 (its name on the page)    START  where in it a note starts (0..127 of its length)
 *   LENGTH  how much of it from there (1..127)          PLAY   ONE (to its end, the key let go or not: a long REL)
 *                                                              GATE (while the key is held) LOOP (round, while held)
 *   PITCH   -24 .. +24 semitones                         KEYS   CHROMA (C4 the sound's own pitch, a key a semitone),
 *                                                              SLICE (START..END cut in SLICES, the keys up from C4
 *                                                              one each, at its pitch), FIXED (every key the same)
 *   GAIN    the level (100 the sound's)                  SLICES 2 .. 16 (SLICE)
 * The voice: ph[0] the next sample to decode, ph[1] the position's fraction (Q16), ph[2] the end; s[0] s[1] the
 * decoder (predictor, step index), s[2] s[3] the last two samples (the linear interpolation), s[4] the slot, s[5] the
 * loop's start, s[6] 1 when it ran out (silence). A slot empty or gone: silence */
static const char *const N_SMP_PLAY[] = {"ONE", "GATE", "LOOP", 0};
static const char *const N_SMP_KEYS[] = {"CHROMA", "SLICE", "FIXED", 0};
enum { SP_ONE, SP_GATE, SP_LOOP };
enum { SK_CHROMA, SK_SLICE, SK_FIXED };
static const char *const N_SMP_SLOT[] = {"S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8", "S9", "S10", "S11", "S12", "S13",
                                         "S14", "S15", "S16", 0};
/* where a note of track t plays: [*a, *b) of slot's samples */
static void smp_span(const track_t *t, const smp_ent_t *e, uint32_t note, uint32_t *a, uint32_t *b)
{
    const int16_t *p = t->p;
    uint32_t len = e->len, s0 = (uint32_t)((uint64_t)len * (uint32_t)clamp(p[P_E1], 0, 127) >> 7);
    uint32_t n = (len - s0) / 127u * (uint32_t)clamp(p[P_E2], 1, 127) + (len - s0) % 127u * (uint32_t)clamp(p[P_E2], 1, 127) / 127u;   /* (no
                                                         * 64-bit divide) */
    if (p[P_E5] == SK_SLICE) {
        uint32_t k = (uint32_t)clamp(p[P_E7], 2, 16), i = (uint32_t)(note >= 60u ? note - 60u : note + 12u) % k;
        s0 += n * i / k;
        n /= k;
    }
    if (n < 2u) n = 2u;
    if (s0 + n > len) n = len - s0;
    *a = s0;
    *b = s0 + n;
}
/* the decoder at sample i (a block's start, then decoded on: up to 255 samples, once a note or a loop) */
static void smp_seek(const smp_ent_t *e, voice_t *v, uint32_t i)
{
    int32_t pr = 0, ix = 0;
    uint32_t j;
    for (j = i - i % SMP_BLK; j < i; j++)
        (void)smp_next(e, j, &pr, &ix);
    v->ph[0] = i;
    v->s[0] = pr;
    v->s[1] = ix;
    if (i % SMP_BLK == 0u) v->s[0] = v->s[1] = 0;       /* (the header gives them) */
}
static void sampler_note_on(track_t *t, voice_t *v)
{
    uint32_t slot = (uint32_t)clamp(t->p[P_E0], 0, SMP_NSLOT - 1), a = 0, b = 0;
    const smp_ent_t *e = smp_ent(slot);
    v->s[4] = (int32_t)slot;
    v->s[2] = v->s[3] = 0;
    v->ph[1] = 0;
    v->s[6] = !e;
    if (!e) return;
    smp_span(t, e, v->note, &a, &b);
    v->s[5] = (int32_t)a;
    v->ph[2] = b;
    smp_seek(e, v, a);
}
static void sampler_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    const smp_ent_t *e = smp_ent((uint32_t)v->s[4]);
    uint32_t i, pos = v->ph[0], end = v->ph[2], fr = v->ph[1], inc, mode = (uint32_t)clamp(p[P_E3], 0, 2), keys = (uint32_t)clamp(p[P_E5], 0, 2);
    int32_t pr = v->s[0], ix = v->s[1], y0 = v->s[2], y1 = v->s[3], st16, g = clamp(p[P_E6], 0, 127) * 328;
    if (!e || v->s[6]) return;
    st16 = p[P_E4] * 16 + (keys == SK_CHROMA ? m->pitch16 - 60 * 16 : m->pitch16 - (int32_t)v->note * 16);
    inc = ((uint32_t)e->rate << 16) / 44100u;
    inc = (uint32_t)(((uint64_t)inc * fmt_ratio(st16)) >> 16);
    if (inc > (8u << 16)) inc = 8u << 16;
    for (i = 0; i < n; i++) {
        fr += inc;
        while (fr >= 65536u) {
            fr -= 65536u;
            if (pos >= end) {
                if (mode == SP_LOOP && end > (uint32_t)v->s[5] + 1u) {   /* (round again: the decoder from the start) */
                    voice_t tmp;
                    tmp = *v;
                    smp_seek(e, &tmp, (uint32_t)v->s[5]);
                    pos = tmp.ph[0]; pr = tmp.s[0]; ix = tmp.s[1];
                } else {
                    v->s[6] = 1;                          /* (ran out: silence on) */
                    y0 = y1 = 0;
                    break;
                }
            }
            y0 = y1;
            y1 = smp_next(e, pos, &pr, &ix);
            pos++;
        }
        if (v->s[6]) break;
        {
            int32_t s = y0 + (((y1 - y0) * (int32_t)(fr >> 1)) >> 15);
            out[i] += voice_amp((s * g) >> 15, m, i);
        }
    }
    v->ph[0] = pos; v->ph[1] = fr;
    v->s[0] = pr; v->s[1] = ix; v->s[2] = y0; v->s[3] = y1;
}
static const param_desc_t SMP_SLICES = {"SLICES", F_INT, 2, 16, 8, 0, 0};
static const param_desc_t *sampler_desc(const track_t *t, uint32_t k)
{
    (void)t;
    if (k == 7u) return &SMP_SLICES;
    return 0;
}
static const preset_t SAMPLER_PRESETS[] = {
    {"ONE SHOT", {0, 0, 127, SP_ONE, 0, SK_CHROMA, 100, 8}, {0, 64, 127, 90}, 0, TAG(FX), FX(0, 0, 0, 0), PAT(0)},
    {"CHOP", {0, 0, 127, SP_GATE, 0, SK_SLICE, 100, 8}, {0, 64, 127, 20}, 0, TAG(DRUM), FX(0, 0, 0, 0), PAT(0)},
    {"CHOP 16", {0, 0, 127, SP_GATE, 0, SK_SLICE, 100, 16}, {0, 64, 127, 15}, 0, TAG(DRUM), FX(0, 0, 0, 0), PAT(0)},
    {"LOOP", {0, 0, 127, SP_LOOP, 0, SK_FIXED, 100, 8}, {5, 64, 127, 40}, 0, TAG(FX), FX(0, 0, 0, 0), PAT(0)},
    {"KEYS", {0, 0, 127, SP_GATE, 0, SK_CHROMA, 100, 8}, {2, 64, 127, 50}, 0, TAG(KEYS), FX(0, 0, 0, 0), PAT(0)},
};
static const engine_t ENG_SAMPLER = {
    .name = "SAMPLER",
    .page_title = {"SAMPLE", "PLAY"},
    .edit = {
        {"SAMPLE", F_ENUM, 0, SMP_NSLOT - 1, 0, N_SMP_SLOT, 0},
        {"START", F_PCT, 0, 127, 0, 0, 0},
        {"LENGTH", F_PCT, 1, 127, 127, 0, 0},
        {"PLAY", F_ENUM, 0, 2, 0, N_SMP_PLAY, 0},
        {"PITCH", F_INT, -24, 24, 0, 0, "st"},
        {"KEYS", F_ENUM, 0, 2, 0, N_SMP_KEYS, 0},
        {"GAIN", F_PCT, 0, 127, 100, 0, 0},
        {"SLICES", F_INT, 2, 16, 8, 0, 0},
    },
    .presets = SAMPLER_PRESETS,
    .npresets = NELEM(SAMPLER_PRESETS),
    .note_on = sampler_note_on,
    .render = sampler_render,
    .knob = {P_E0, P_E1, P_E2, P_E4},
    .sampled = 1,
    .desc = sampler_desc,
};
