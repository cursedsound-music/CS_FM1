/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursed-os 0.9.5) the sampler's memory: songs 5..8's flash (0xB8000..0xD7FFF, 128 KiB: storage.c's OBJ_SONG1 + 12 ..
 * + 27, given up: cos.c offers songs 1..4). 32 sectors of 4 KiB:
 *   0, 1   the directory, A / B (the newer valid one counts; a write goes to the other: a power cut keeps the last)
 *   2..31  the sounds' data (120 KiB): each sound in whole sectors, any of them (its table of up to 30)
 * A sound: IMA ADPCM, mono, at its own rate (the phone sends 22.05 kHz: ~11 s in all), blocks of 256 samples, each a
 * 4-byte header (the predictor before its first sample, int16 LE; the step index) and 128 bytes (two samples a byte,
 * the first in the low nibble): 132 bytes a block. 16 slots.
 * Reads: the audio interrupt through XIP (the plain window from 0x93000: fm1_flash.h fl_plain_window_init), the
 * directory's newer copy chosen once (smp_dsel) at power-on and after each write. The host and the emulator: RAM. */
#ifndef COS_SMP_H
#define COS_SMP_H
#define SMP_LO 0xB8000u
#define SMP_SECS 32u
#define SMP_DATA0 2u                                     /* the first data sector */
#define SMP_NSLOT 16u
#define SMP_MAXSEC 30u
#define SMP_BLK 256u                                     /* samples a block */
#define SMP_BLKB 132u                                    /* bytes a block */
#define SMP_MAGIC 0x504D5343u                            /* "CSMP" */
typedef struct {
    char name[8];
    uint32_t len;                                        /* samples */
    uint16_t rate;                                       /* Hz */
    uint8_t nsec, valid;                                 /* its sectors; 1 a sound */
    uint8_t sec[SMP_MAXSEC];                             /* the region's sectors (2..31), in order */
    uint8_t pad[2];
} smp_ent_t;
typedef struct {
    uint32_t magic, seq, crc, n;                         /* crc: of the entries */
    smp_ent_t e[SMP_NSLOT];
} smp_dir_t;
_Static_assert(sizeof(smp_ent_t) == 48u && sizeof(smp_dir_t) <= 4096u, "the sampler's directory");
#if defined(COS_SMP_XIP)                                /* (felucca.c: the device) */
#define SMP_PTR(off) ((const uint8_t *)FM1_XIP(SMP_LO + (off)))
#else
static uint8_t smp_ram[SMP_SECS * 4096u];               /* (the host, the emulator: the region in RAM) */
#define SMP_PTR(off) ((const uint8_t *)smp_ram + (off))
#endif
static volatile uint8_t smp_dsel = 0xFFu;                /* the directory copy that counts (0, 1; 0xFF none) */
static const smp_dir_t *smp_dir(void) { return smp_dsel < 2u ? (const smp_dir_t *)SMP_PTR(smp_dsel * 4096u) : 0; }
static const smp_ent_t *smp_ent(uint32_t slot)
{
    const smp_dir_t *d = smp_dir();
    return d && slot < SMP_NSLOT && d->e[slot].valid == 1u && d->e[slot].len && d->e[slot].nsec <= SMP_MAXSEC ? &d->e[slot] : 0;
}
/* byte o of a sound's data */
static inline uint32_t smp_byte(const smp_ent_t *e, uint32_t o)
{
    uint32_t s = o >> 12;
    return s < e->nsec ? SMP_PTR(e->sec[s] * 4096u + (o & 4095u))[0] : 0u;
}
/* IMA ADPCM */
static const int16_t SMP_STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
    118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
    6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767};
static const int8_t SMP_IDX[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
static inline int32_t smp_adpcm(uint32_t nib, int32_t *pred, int32_t *idx)
{
    int32_t st = SMP_STEP[*idx], d = st >> 3;
    if (nib & 4u) d += st;
    if (nib & 2u) d += st >> 1;
    if (nib & 1u) d += st >> 2;
    *pred = nib & 8u ? *pred - d : *pred + d;
    if (*pred > 32767) *pred = 32767;
    if (*pred < -32768) *pred = -32768;
    *idx += SMP_IDX[nib & 15u];
    if (*idx < 0) *idx = 0;
    if (*idx > 88) *idx = 88;
    return *pred;
}
/* sample i of a sound, decoding on from (pred, idx) at sample i (i: the next one to decode) */
static inline int32_t smp_next(const smp_ent_t *e, uint32_t i, int32_t *pred, int32_t *idx)
{
    uint32_t b = i / SMP_BLK, w = i % SMP_BLK, o = b * SMP_BLKB, nib;
    if (!w) {                                            /* (a block's start: its header) */
        *pred = (int16_t)(smp_byte(e, o) | smp_byte(e, o + 1u) << 8);
        *idx = (int32_t)(smp_byte(e, o + 2u) % 89u);
    }
    nib = smp_byte(e, o + 4u + (w >> 1)) >> ((w & 1u) * 4u) & 15u;
    return smp_adpcm(nib, pred, idx);
}
#endif
