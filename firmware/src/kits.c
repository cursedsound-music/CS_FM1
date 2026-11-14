/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound */
/* (cursed-os 0.13.0) the user KITS (GG: "the drums should be highly customizable"): SAVE > KITS, K01 .. K16. A kit:
 * its eight sounds as made (dx_kit: the WAVE, FILTER, FM, A and B; LAYER / FEEDBK), each one's LEVEL (the DRUM track's
 * P_LN0..7), AHDSR and sends (dx_de, dx_snd), CHANCE (dx_skip), the kit's TONE, HANDS, GLUE; its name (the DRUM
 * track's sound). Loading one puts it in the kit now (playing too); the DRUM track's other knobs and its steps stay.
 * Flash: one object (OBJ_KITS, 0xDA000 / 0xDB000, the A / B pair of storage.c), read and written through st_buf (no RAM
 * of its own but the names' cache, in the pool); without flash (host tests) a RAM copy */
#define KIT_N 16u
#define KIT_USED 0x4Bu                                  /* 'K' */
#define KIT_MAGIC 0x3154494Bu                           /* "KIT1" */
typedef struct {
    uint8_t used, ver;
    char name[10];
    dx_lane_t lane[8];
    uint8_t lay[8], snd[8], lvl[8], skip[8];
    uint8_t de[8][DE_N];
    uint8_t tone, hands, glue, rsv[1];
    int8_t pan[8];                                      /* (0.14.1) each drum's PAN (eng_drum.c dx_pan) */
} kit_rec_t;
#define KIT_RSIZE0 192u                                 /* (0.13.0's record, before PAN: kit_bank widens it) */
typedef struct {
    uint32_t magic;
    uint16_t rsize, n;
    kit_rec_t r[KIT_N];
} kit_bank_t;
_Static_assert(sizeof(kit_rec_t) == KIT_RSIZE0 + 8u, "a kit's record: 0.13.0's and the PANs");
static uint16_t kit_used;                               /* (the bank's slots in use; kit_scan) */
static char kit_nm[KIT_N][10] __attribute__((section(".lowbuf")));   /* (their names, for SAVE > KITS) */
#if FELUCCA_FLASH
_Static_assert(sizeof(kit_bank_t) <= ST_PAYLOAD_MAX, "the kits in one sector");
static kit_bank_t *kit_bank(void)                       /* the bank in st_buf (an empty one when none is stored) */
{
    st_hdr_t h;
    kit_bank_t *b = (kit_bank_t *)(void *)st_buf;
    if (st_current(OBJ_KITS, &h) >= 0 && b->magic == KIT_MAGIC && b->rsize == KIT_RSIZE0 && b->n == KIT_N &&
        h.len == 8u + KIT_N * KIT_RSIZE0) {             /* (0.14.1) 0.13.0's bank: each record widened (from the last:
                                                         * each lands past where it was), no PAN */
        uint8_t *r = (uint8_t *)b->r;
        uint32_t k, j;
        for (k = KIT_N; k-- > 0u;) {
            for (j = KIT_RSIZE0; j-- > 0u;) r[k * sizeof(kit_rec_t) + j] = r[k * KIT_RSIZE0 + j];
            for (j = KIT_RSIZE0; j < sizeof(kit_rec_t); j++) r[k * sizeof(kit_rec_t) + j] = 0;
        }
        b->rsize = sizeof(kit_rec_t);
        return b;
    }
    if (st_current(OBJ_KITS, &h) < 0 || h.len != sizeof *b || b->magic != KIT_MAGIC || b->rsize != sizeof(kit_rec_t)) {
        memset(b, 0, sizeof *b);
        b->magic = KIT_MAGIC;
        b->rsize = sizeof(kit_rec_t);
        b->n = KIT_N;
    }
    return b;
}
static int kit_commit(kit_bank_t *b) { return st_save(OBJ_KITS, b, sizeof *b); }
#else
static kit_bank_t kit_ram = {KIT_MAGIC, sizeof(kit_rec_t), KIT_N, {{0}}};
static kit_bank_t *kit_bank(void) { return &kit_ram; }
static int kit_commit(kit_bank_t *b) { (void)b; return 0; }
#endif
static const track_t *kit_trk(void)                     /* the DRUM track (its LEVELs) */
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) if (trk[k].engine == ENGI_DRUM) return &trk[k];
    return 0;
}
static void kit_scan(void)
{
    const kit_bank_t *b = kit_bank();
    uint32_t k;
    kit_used = 0;
    for (k = 0; k < KIT_N; k++) {
        int u = b->r[k].used == KIT_USED && dx_kit_ok(b->r[k].lane);
        kit_used |= (uint16_t)((uint32_t)u << k);
        if (u) memcpy(kit_nm[k], b->r[k].name, 10); else memset(kit_nm[k], 0, 10);
    }
}
static int kit_save(uint32_t k, const char *name)
{
    kit_bank_t *b;
    kit_rec_t *r;
    const track_t *t = kit_trk();
    uint32_t i;
    if (k >= KIT_N) return -1;
    b = kit_bank();
    r = &b->r[k];
    memset(r, 0, sizeof *r);
    r->used = KIT_USED;
    r->ver = 1;
    for (i = 0; i < 10u && name[i]; i++) r->name[i] = name[i];
    dx_kit_own(r->lane, r->lay);                        /* (0.14.0: the own, not a sound lock's) */
    memcpy(r->snd, dx_snd, 8);
    memcpy(r->skip, dx_skip, 8);
    memcpy(r->de, dx_de, sizeof r->de);
    for (i = 0; i < 8u; i++) r->lvl[i] = (uint8_t)(t ? clamp(t->p[P_LN0 + i], 0, 127) : 100);
    memcpy(r->pan, dx_pan, 8);
    r->tone = dx_tone;
    r->hands = dx_hands;
    r->glue = dx_glue;
    i = (uint32_t)kit_commit(b);
    kit_scan();
    return (int)i;
}
static int kit_load(uint32_t k)
{
    const kit_bank_t *b = kit_bank();
    const kit_rec_t *r;
    track_t *t = (track_t *)(uintptr_t)kit_trk();
    uint32_t i;
    if (k >= KIT_N || b->r[k].used != KIT_USED || !dx_kit_ok(b->r[k].lane)) return -1;
    r = &b->r[k];
    fm1_irq_off();
    memcpy(dx_kit, r->lane, sizeof dx_kit);
    memcpy(dx_lay, r->lay, 8);
    dx_slk_forget();
    memcpy(dx_snd, r->snd, 8);
    memcpy(dx_skip, r->skip, 8);
    memcpy(dx_de, r->de, sizeof dx_de);
    memcpy(dx_pan, r->pan, 8);
    dx_tone = r->tone;
    dx_hands = r->hands & 15u;
    dx_glue = r->glue;
    if (t) for (i = 0; i < 8u; i++) t->p[P_LN0 + i] = r->lvl[i];
    fm1_irq_on();
    return 0;
}
static int kit_erase(uint32_t k)
{
    kit_bank_t *b;
    int rc;
    if (k >= KIT_N) return -1;
    b = kit_bank();
    memset(&b->r[k], 0, sizeof b->r[k]);
    rc = kit_commit(b);
    kit_scan();
    return rc;
}
static int kit_is_used(uint32_t k) { return k < KIT_N && ((kit_used >> k) & 1u); }
static const char *kit_name(uint32_t k) { return kit_nm[k % KIT_N]; }
