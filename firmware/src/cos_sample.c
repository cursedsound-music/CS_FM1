/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 cursedsound */
/* (cursed-os 0.9.5) the sampler's sounds sent from the phone (cos_remote.c's frames 10..13; the memory: cos_smp.h).
 * The transport stopped (flash writes stop the sound and the interrupts), one sound at a time:
 *   10 BEGIN  <seq> <slot 0..15> <name: 8 ASCII> <rate: 3 x 7 bits> <samples: 3 x 7> <bytes: 3 x 7>
 *             room found in the free sectors (its own old ones count as free) or NAK ROOM; nothing written yet
 *   11 DATA   <seq> <up to 256 bytes, 7 to 8 packed: each 8 the high bits of the next 7 first>
 *             in order (seq: BEGIN's + 1, + 2 .. mod 128; the one before again: ACKed again, nothing written), each
 *             sector erased as the data reaches it, written as it comes
 *   12 END    <seq> <the bytes' CRC-32 (zlib), 5 x 7 bits>: read back and checked, then the directory written (its
 *             other copy, a newer sequence): the sound is there; a bad CRC: NAK DATA, the directory as it was
 *   13 DELETE <seq> <slot>: the slot emptied (its sectors free)
 * Each answered ACK <seq> or NAK <code> (cos_remote.c). A BEGIN starts over whatever was under way */
_Static_assert(SMP_LO == 0xA0000u + 12u * 2u * 4096u && SMP_LO + SMP_SECS * 4096u == 0xD8000u,
               "the sampler's memory: songs 5..8's objects exactly");
/* zlib's CRC-32, 4 bits a step (as storage.c's: its own here, the host tests build without the storage) */
static uint32_t smp_crc_add(uint32_t c, const void *p, uint32_t n)
{
    static const uint32_t T[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    const uint8_t *b = p;
    while (n--) {
        c ^= *b++;
        c = (c >> 4) ^ T[c & 15u];
        c = (c >> 4) ^ T[c & 15u];
    }
    return c;
}
static uint32_t smp_crc(const void *p, uint32_t n) { return ~smp_crc_add(0xFFFFFFFFu, p, n); }
static int st_erase(uint32_t off);                       /* (storage_hw.c) */
static int st_prog(uint32_t off, const void *src, uint32_t n);
static struct {
    uint8_t on, slot, seq, nsec;
    uint8_t sec[SMP_MAXSEC];
    char name[8];
    uint32_t rate, len, nbytes, wr;
} su;
static int smp_erase(uint32_t sec)                       /* a sector of the region */
{
#if defined(COS_SMP_XIP)
    return st_erase(SMP_LO + sec * 4096u);
#else
    memset(smp_ram + sec * 4096u, 0xFF, 4096u);
    return 0;
#endif
}
static int smp_prog(uint32_t off, const void *src, uint32_t n)
{
#if defined(COS_SMP_XIP)
    return st_prog(SMP_LO + off, src, n);
#else
    memcpy(smp_ram + off, src, n);
    return 0;
#endif
}
static int smp_dir_ok(uint32_t k)
{
    const smp_dir_t *d = (const smp_dir_t *)SMP_PTR(k * 4096u);
    return d->magic == SMP_MAGIC && d->n == SMP_NSLOT && d->crc == smp_crc(d->e, sizeof d->e);
}
/* power-on (and after a write): the directory copy that counts */
static __attribute__((noinline)) void smp_boot(void)
{
    int a = smp_dir_ok(0), b = smp_dir_ok(1);
    uint32_t sa = a ? ((const smp_dir_t *)SMP_PTR(0))->seq : 0u, sb = b ? ((const smp_dir_t *)SMP_PTR(4096u))->seq : 0u;
    smp_dsel = (uint8_t)(a && b ? (sb - sa < 0x80000000u && sb != sa ? 1u : 0u) : a ? 0u : b ? 1u : 0xFFu);
}
/* the directory written: d's entries, into the copy that does not count */
static __attribute__((noinline)) int smp_dir_put(smp_dir_t *d)
{
    uint32_t k = smp_dsel == 0u ? 1u : 0u;
    const smp_dir_t *o = smp_dir();
    d->magic = SMP_MAGIC;
    d->n = SMP_NSLOT;
    d->seq = o ? o->seq + 1u : 1u;
    d->crc = smp_crc(d->e, sizeof d->e);
    if (smp_erase(k) || smp_prog(k * 4096u, d, sizeof *d)) return RN_FLASH;
    smp_boot();
    return smp_dsel == k ? RN_NONE : RN_FLASH;
}
static void smp_dir_copy(smp_dir_t *d)                   /* the directory as it is (none: empty) */
{
    const smp_dir_t *o = smp_dir();
    if (o) memcpy(d, o, sizeof *d);
    else memset(d, 0, sizeof *d);
}
/* the sectors free (bit k: sector k), slot `except`'s counted free */
static uint32_t smp_free(uint32_t except)
{
    const smp_dir_t *d = smp_dir();
    uint32_t m = 0, i, j;
    for (i = SMP_DATA0; i < SMP_SECS; i++) m |= 1u << i;
    for (i = 0; d && i < SMP_NSLOT; i++)
        if (i != except && d->e[i].valid == 1u)
            for (j = 0; j < d->e[i].nsec && j < SMP_MAXSEC; j++) m &= ~(1u << (d->e[i].sec[j] & 31u));
    return m;
}
static uint32_t smp_u21(const uint8_t *a) { return (uint32_t)a[0] | (uint32_t)a[1] << 7 | (uint32_t)a[2] << 14; }
static uint32_t smp_unpack(const uint8_t *a, uint32_t n, uint8_t *out)   /* 7 to 8 packed -> bytes */
{
    uint32_t i = 0, o = 0;
    while (i < n) {
        uint32_t hi = a[i++], k;
        for (k = 0; k < 7u && i < n; k++, i++) out[o++] = (uint8_t)(a[i] | ((hi >> k) & 1u) << 7);
    }
    return o;
}
static uint32_t rmt_sample(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t seq = n ? a[0] : 0u;
    if (song.playing) { cos_say("STOP TO RECEIVE"); return RN_STOP; }
    if (cmd == 0x10u) {                                  /* BEGIN */
        uint32_t fm, need, i, k;
        if (n < 19u || a[1] >= SMP_NSLOT) return RN_BAD;
        su.on = 0;
        su.slot = a[1];
        for (i = 0; i < 8u; i++) su.name[i] = (char)(a[2 + i] >= 32u && a[2 + i] <= 126u ? a[2 + i] : ' ');
        su.rate = smp_u21(a + 10);
        su.len = smp_u21(a + 13);
        su.nbytes = smp_u21(a + 16);
        need = (su.nbytes + 4095u) / 4096u;
        if (su.rate < 4000u || su.rate > 48000u || !su.len || su.nbytes != (su.len + SMP_BLK - 1u) / SMP_BLK * SMP_BLKB)
            return RN_DATA;
        if (need > SMP_MAXSEC) { cos_say("SAMPLE TOO LONG"); return RN_ROOM; }
        fm = smp_free(su.slot);
        for (i = SMP_DATA0, k = 0; i < SMP_SECS && k < need; i++)
            if ((fm >> i) & 1u) su.sec[k++] = (uint8_t)i;
        if (k < need) { cos_say("SAMPLER FULL"); return RN_ROOM; }
        su.nsec = (uint8_t)need;
        su.wr = 0;
        su.seq = (uint8_t)seq;
        su.on = 1;
        cos_say("RECEIVING");
        return RN_NONE;
    }
    if (cmd == 0x11u) {                                  /* DATA */
        uint8_t b[260];
        uint32_t m, k;
        if (!su.on) return RN_ORDER;
        if (seq == su.seq) return RN_NONE;               /* (the last one again: its ACK was lost) */
        if (seq != ((su.seq + 1u) & 127u)) return RN_ORDER;
        m = smp_unpack(a + 1, n - 1u, b);
        if (!m || m > 256u || su.wr + m > su.nbytes || (su.wr & 255u)) return RN_DATA;
        k = su.wr >> 12;
        if (!(su.wr & 4095u) && smp_erase(su.sec[k])) { su.on = 0; return RN_FLASH; }
        if (smp_prog(su.sec[k] * 4096u + (su.wr & 4095u), b, m)) { su.on = 0; return RN_FLASH; }
        su.wr += m;
        su.seq = (uint8_t)seq;
        {
            char t[20], v[12];
            str_cpy(t, "RECEIVING ", sizeof t);
            fmt_int(v, (int32_t)(su.wr * 100u / su.nbytes));
            str_cpy(t + str_len(t), v, 5);
            str_cpy(t + str_len(t), "%", 2);
            cos_say(t);
        }
        return RN_NONE;
    }
    if (cmd == 0x12u) {                                  /* END */
        smp_dir_t d;
        smp_ent_t *e;
        uint32_t c = 0xFFFFFFFFu, i, crc;
        if (!su.on) return seq == su.seq ? RN_NONE : RN_ORDER;   /* (done already: its ACK lost) */
        if (n < 6u || su.wr != su.nbytes) return RN_DATA;
        crc = (uint32_t)a[1] | (uint32_t)a[2] << 7 | (uint32_t)a[3] << 14 | (uint32_t)a[4] << 21 | (uint32_t)a[5] << 28;
        for (i = 0; i < su.nsec; i++) {                  /* (read back: what flash holds) */
            uint32_t k = su.nbytes - i * 4096u > 4096u ? 4096u : su.nbytes - i * 4096u;
            c = smp_crc_add(c, SMP_PTR(su.sec[i] * 4096u), k);
        }
        if (~c != crc) { su.on = 0; cos_say("SAMPLE DAMAGED: AGAIN"); return RN_DATA; }
        smp_dir_copy(&d);
        e = &d.e[su.slot];
        memset(e, 0, sizeof *e);
        memcpy(e->name, su.name, 8);
        e->len = su.len;
        e->rate = (uint16_t)su.rate;
        e->nsec = su.nsec;
        memcpy(e->sec, su.sec, su.nsec);
        e->valid = 1;
        su.on = 0;
        su.seq = (uint8_t)seq;
        if (smp_dir_put(&d)) { cos_say("FLASH FAILED"); return RN_FLASH; }
        cos_say("SAMPLE IN");
        return RN_NONE;
    }
    if (cmd == 0x13u) {                                  /* DELETE */
        smp_dir_t d;
        if (n < 2u || a[1] >= SMP_NSLOT) return RN_BAD;
        smp_dir_copy(&d);
        if (!d.e[a[1]].valid) return RN_NONE;
        memset(&d.e[a[1]], 0, sizeof d.e[a[1]]);
        if (smp_dir_put(&d)) return RN_FLASH;
        cos_say("SAMPLE DELETED");
        return RN_NONE;
    }
    return RN_BAD;
}
