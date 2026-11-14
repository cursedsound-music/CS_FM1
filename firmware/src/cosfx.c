/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound */
/* (cursedsound) cursed-os's FX pad, for techno: FX held, a key holds its effect while it is held (FX + OCT+ makes them
 * latch). The white keys, F3 up:
 *   1 RPT 1/8  2 RPT 1/16  3 RPT 1/32 (perform.c's REPEAT)  4 GATE 1/16  5 GATE 1/8 (the master chopped on the grid)
 *   6 TAPE STOP (the master slows to a halt in a beat)  7 GLITCH (perform.c)  8 LPF  9 HPF (perform.c's sweeps)
 *   10 DRIVE (a soft clip, hard)  11 CRUSH (fewer bits, a held sample)  12 DLY THROW (the dry mix into the delay, its
 *   feedback up; let go, the echoes ring on)  13 REV WASH (the mix into the reverb, larger; the dry pulled back)
 *   14 FREEZE (the mix that plays as it is pressed, one delay time of it, into the delay, then held round: nothing new
 *   in)  15 HP RISER (a resonant high-pass up over 4 bars, 120 Hz .. 6 kHz; let go: the drop)  16 NOISE RISER (white
 *   noise swelling up over 4 bars, heard from the start: -20 dB under its top, the top ~-10 dBFS)
 * The black keys: 1..6 the tracks muted (perform.c), 7 KICK KILL (the kit's kicks: no new hits), 8 LOW KILL (the lows
 * under ~200 Hz out, as a DJ mixer's), 9 DRUMS ONLY (every track that is not a kit muted), 10 LOOP 3, 11 LOOP 7.
 * Out of the RAM code: fx.c lev_block runs it (cfx_block, in the image) after the leveler has measured the mix; the
 * sends go into the buses' next block (fxq_d, fxq_r) and the delay's and reverb's offsets through mod.c macro_master
 * (cfx_goff). TAPE STOP borrows the SLICER's buffer as REPEAT does: one or the other at a time. */
enum { CX_GATE16, CX_GATE8, CX_TAPE, CX_DRIVE, CX_CRUSH, CX_THROW, CX_WASH, CX_FREEZE, CX_HPR, CX_NOISE,
       CX_LOWK, CX_KICK, CX_DRUMS, CX_MIDK, CX_HIK, CX_N };   /* (MID / HIGH KILL: HOME's isolator keys) */
#define CX_BIT(e) (1u << (e))
#define CX_CODE 0x40u                         /* a key's code from here: cursed-os's own effect; below: perform.c's PF_* */
static const uint8_t CFX_W[16] = {PF_R8, PF_R16, PF_R32, CX_CODE + CX_GATE16, CX_CODE + CX_GATE8, CX_CODE + CX_TAPE,
                                  PF_GLT, PF_LPF, PF_HPF, CX_CODE + CX_DRIVE, CX_CODE + CX_CRUSH, CX_CODE + CX_THROW,
                                  CX_CODE + CX_WASH, CX_CODE + CX_FREEZE, CX_CODE + CX_HPR, CX_CODE + CX_NOISE};
static const uint8_t CFX_B[11] = {PF_M1, PF_M1 + 1, PF_M1 + 2, PF_M1 + 3, PF_M1 + 4, PF_M1 + 5, CX_CODE + CX_KICK,
                                  CX_CODE + CX_LOWK, CX_CODE + CX_DRUMS, PF_LP3, PF_LP7};
_Static_assert(NTRK == 6, "the black keys 1..6: the six tracks");
static volatile uint32_t cfx_held;            /* keys (seq.c): the effects held, CX_BIT */
static volatile uint32_t cfx_latched;         /* .. latched (FX LATCH; main clears it: FX + OCT-) */
static volatile uint32_t cfx_auto;            /* ISR: the BREAKDOWN's (cos_live.c live_tick) */
static volatile uint32_t cfx_home;            /* main: HOME's isolator kills held (LOW MID HIGH KILL) */
/* the FX lane: the pad's effects recorded a sixteenth at a time over two bars (REC armed, FX held, playing), played
 * back round and round while the transport plays; a project's / section's ('FL', project.c proj_fl) */
#define FXL_N 32u
static volatile uint16_t cfx_lane[FXL_N];
static volatile uint32_t cfx_plane[FXL_N];     /* .. and perform.c's effects of the pad a sixteenth (REPEAT, GLITCH, the
                                                * sweeps, the mutes, the LOOPs: CFX_PF_LANE), played through perf_lane */
#define CFX_PF_LANE (PF_REPEAT | PF_BIT(PF_LPF) | PF_BIT(PF_HPF) | PF_MUTE | PF_BIT(PF_GLT) | PF_LOOPS)
static volatile uint8_t cfx_lane_on;
static volatile uint8_t cfx_rec;              /* main: REC armed and FX held: the lane is written where it passes */
static uint32_t cfx_lane_bits(uint32_t n);
#define CX_SLOPE 256                          /* a share's step a sample: 0 .. full in 2.9 ms */
#define CX_TAPE_MAX 30000u                    /* TAPE STOP's longest halt (samples): its lag stays inside sl_buf */

static struct {
    uint32_t on;                              /* the effects at work this block */
    uint32_t ph;                              /* samples, while the transport is stopped (the gates' grid) */
    int32_t gg, gs;                           /* GATE: its gain, its share (Q15) */
    int32_t wd, wc, wh, wn, ww, wt;           /* shares: DRIVE CRUSH HP RISER NOISE WASH THROW */
    int32_t chl, chr;                         /* CRUSH: the held sample */
    uint32_t cn, nrnd;                        /* .. its count; NOISE's generator */
    uint32_t ht, nt;                          /* HP RISER's, NOISE RISER's samples since they began */
    uint32_t ft;                              /* FREEZE's samples since it began (the first delay time: the mix in) */
    int32_t hz[4], nz[2];                     /* .. their filters (Chamberlin: low, band; L R for HP RISER) */
    uint32_t tt, wst;                         /* THROW's, WASH's offsets: samples they stay after the key goes */
    /* TAPE STOP */
    uint8_t tape, todd, kick0, solo0, kicked, soloed;   /* 1 at work (sl_buf ours), 2 going back; .. KICK, DRUMS kept */
    int32_t tw, thl, thr;                     /* its share; the first of a pair */
    uint32_t twr, tn, tlen, trp, tinv;        /* samples written, since it began, the halt's length; read (Q16); 2^28 / tlen */
    uint8_t busy;                             /* a share or a tail still sounding */
} cx = {.gg = 32768, .nrnd = 0x2545F491u};
#include "cos_iso.c"                          /* (cursed-os) the ISOLATOR: its knobs and the kills (LOW MID HIGH KILL) */

/* seq.c keyboard_block: a key of the pad down / up (c: CFX_W / CFX_B; PF_N: none) */
static void cfx_press(uint32_t c, int down)
{
    if (c < CX_CODE) {
        if (down && cx.tape && (c <= PF_R32 || c == PF_GLT))
            return;                           /* (TAPE STOP has the buffer) */
        perf_press(c, down);
        return;
    }
    c -= CX_CODE;
    if (c >= CX_N)
        return;
    if (perf_latch_on) {
        if (down)
            cfx_latched ^= CX_BIT(c);
        return;
    }
    if (down)
        cfx_held |= CX_BIT(c);
    else
        cfx_held &= ~CX_BIT(c);
}
/* the effects on (the pad's map) */
static uint32_t cfx_now(void) { return cfx_held | cfx_latched | cfx_auto | cfx_home; }

/* 2^(x / 256), Q16 (x: octaves, Q8, under 15) */
static uint32_t cx_exp2(uint32_t x)
{
    uint32_t f = x & 255u, m = 65536u + ((f * (45426u + ((f * 20110u) >> 8))) >> 8);   /* 1 + f (0.693 + 0.307 f) */
    return m << (x >> 8);
}
/* a Chamberlin state variable filter's step (f, d Q14; z: low, band; x small): the high-pass, the band in z[1] */
static inline int32_t cx_svf(int32_t x, int32_t *z, int32_t f, int32_t d)
{
    int32_t hp = x - z[0] - ((d * z[1]) >> 14);
    z[1] = clamp(z[1] + ((f * hp) >> 14), -131071, 131071);
    z[0] = clamp(z[0] + ((f * z[1]) >> 14), -131071, 131071);
    return hp;
}
static inline int32_t cx_ramp(int32_t s, int on) { return s + clamp((on ? 32768 : 0) - s, -CX_SLOPE, CX_SLOPE); }

/* the effects' edges: KICK KILL and DRUMS ONLY take the kit's mutes and the solo, give back what was there; TAPE STOP
 * takes the buffer (not while REPEAT or GLITCH has it) */
static __attribute__((noinline)) void cfx_edges(uint32_t on, uint32_t was)
{
    uint32_t k, m = 0;
    if ((on & ~was) & CX_BIT(CX_KICK)) {
        cx.kick0 = (uint8_t)((dx_mute & DXG_KICK) != 0u);
        dx_mute = (uint16_t)(dx_mute | DXG_KICK);
        cx.kicked = 1;
    }
    if ((was & ~on) & CX_BIT(CX_KICK) && cx.kicked) {
        if (!cx.kick0)
            dx_mute = (uint16_t)(dx_mute & ~DXG_KICK);
        cx.kicked = 0;
    }
    if ((on & ~was) & CX_BIT(CX_DRUMS)) {
        for (k = 0; k < NTRK; k++)
            if (ENG_HITS(ENGINES[eng_idx(trk[k].eng_req)]))   /* (0.10.0: the KICK too) */
                m |= 1u << k;
        cx.solo0 = perf_solo;
        perf_solo = (uint8_t)(m ? m : 1u);
        cx.soloed = 1;
    }
    if ((was & ~on) & CX_BIT(CX_DRUMS) && cx.soloed) {
        perf_solo = cx.solo0;
        cx.soloed = 0;
    }
    if ((on & ~was) & CX_BIT(CX_TAPE) && !cx.tape && !sl_lent) {
        sl_lent = 1;
        slicer_drop();
        memset(sl_buf[0], 0, 8u * sizeof sl_buf[0]);
        cx.tape = 1;
        cx.twr = 8u;                          /* (frames 0..3 quiet: the read starts 4 samples behind) */
        cx.trp = 4u << 16;
        cx.tn = 0;
        cx.todd = 0;
        cx.tlen = beat_samples();
        if (cx.tlen > CX_TAPE_MAX) cx.tlen = CX_TAPE_MAX;
        if (cx.tlen < 4096u) cx.tlen = 4096u;
        cx.tinv = (1u << 28) / cx.tlen;
    }
    if ((on & ~was) & CX_BIT(CX_HPR)) {
        cx.ht = 0;
        cx.hz[0] = cx.hz[1] = cx.hz[2] = cx.hz[3] = 0;
    }
    if ((on & ~was) & CX_BIT(CX_FREEZE))
        cx.ft = 0;
    if ((on & ~was) & CX_BIT(CX_NOISE)) {
        cx.nt = 0;
        cx.nz[0] = cx.nz[1] = 0;
    }
}

/* the bus offsets the next blocks (mod.c macro_master): THROW the delay's feedback and level up, FREEZE its feedback at
 * 100 (the loop holds), WASH the reverb larger; THROW's and WASH's stay a bar after the key goes (the tail rings) */
static __attribute__((noinline)) void cfx_offsets(uint32_t on, uint32_t n)
{
    int32_t fb = song.g[G_DFDBK], mx = song.g[G_DMIX], sz = song.g[G_RSIZE], fz = (on & CX_BIT(CX_FREEZE)) != 0u;
    uint32_t bar = 16u * div_samples(2);
    cx.tt = (on & CX_BIT(CX_THROW)) ? bar : cx.tt > n ? cx.tt - n : 0u;
    cx.wst = (on & CX_BIT(CX_WASH)) ? bar : cx.wst > n ? cx.wst - n : 0u;
    cfx_goff[GF_DFDBK] = (int16_t)(fz ? 100 - fb : cx.tt && fb < 88 ? 88 - fb : 0);
    cfx_goff[GF_DMIX] = (int16_t)((fz || cx.tt) && mx < 80 ? 80 - mx : 0);
    cfx_goff[GF_RSIZE] = (int16_t)(cx.wst && sz < 118 ? 118 - sz : 0);
}

/* TAPE STOP on one sample: recorded into sl_buf (22.05 kHz pairs), read slower and slower, quiet at the end */
static __attribute__((noinline)) void cfx_tape(int32_t *pl, int32_t *pr)
{
    int32_t l = *pl, r = *pr, yl, yr, g;
    uint32_t rate;
    if (cx.tn < cx.tlen + 64u) {              /* (recording while it moves) */
        if (!cx.todd) {
            cx.thl = l;
            cx.thr = r;
            cx.todd = 1;
        } else {
            uint32_t f = (cx.twr >> 1) & (PB_FRAMES - 1u);
            sl_buf[f][0] = (int16_t)clamp((cx.thl + l) >> 2, -32768, 32767);
            sl_buf[f][1] = (int16_t)clamp((cx.thr + r) >> 2, -32768, 32767);
            cx.todd = 0;
        }
        cx.twr++;
    }
    rate = cx.tn < cx.tlen ? ((cx.tlen - cx.tn) * cx.tinv) >> 12 : 0u;   /* 1 .. 0, Q16 (30000 x 65536: in 32 bits) */
    pb_ring_f(cx.trp, &yl, &yr);
    cx.trp += rate;
    cx.tn++;
    g = rate >= 16384u ? 32768 : (int32_t)(rate << 1);         /* (the last quarter fades: no DC held) */
    yl = mulq16(yl, (uint32_t)g << 1);
    yr = mulq16(yr, (uint32_t)g << 1);
    cx.tw = cx_ramp(cx.tw, cx.tape == 1u);
    *pl = l + mulq16(yl - l, (uint32_t)cx.tw << 1);
    *pr = r + mulq16(yr - r, (uint32_t)cx.tw << 1);
}

static uint32_t delay_samples(void);           /* (fx.c: the delay's time now, samples) */
/* fx.c lev_block, each block while an effect is at work or a share is still on: the mix (mix_l, mix_r, wet) in place */
static __attribute__((noinline)) void cfx_block(int32_t *ml, int32_t *mr, int32_t *wt, int32_t *sd, int32_t *sr, uint32_t n)
{
    uint32_t on = perf_kill ? 0u : cfx_held | cfx_latched | cfx_auto | cfx_home | cfx_lane_bits(n), was = cx.on, i, base, P, gp, bar4, hp, np, f2, gph;
    int32_t hf = 0, hd = 0, nf = 0, ng = 0, duck;
    uint32_t flen = (on & CX_BIT(CX_FREEZE)) ? delay_samples() : 0u;   /* (FREEZE: what it takes in) */
    int32_t it[3];                            /* the ISOLATOR's targets: its knobs, the kills held */
    int isw;
    it[0] = iso_target(iso_v[0], (on & CX_BIT(CX_LOWK)) != 0u);
    it[1] = iso_target(iso_v[1], (on & CX_BIT(CX_MIDK)) != 0u);
    it[2] = iso_target(iso_v[2], (on & CX_BIT(CX_HIK)) != 0u);
    isw = it[0] != 32768 || it[1] != 32768 || it[2] != 32768;
    if (on != was)
        cfx_edges(on, was);
    if (!(on & CX_BIT(CX_TAPE)) && cx.tape == 1u)
        cx.tape = 2u;                         /* (back to the live mix, then the buffer given back) */
    cx.on = on;
    cfx_offsets(on, n);
    P = div_samples(2);
    gp = (on & CX_BIT(CX_GATE16)) ? P : 2u * P;
    base = song.playing ? ms_clock - n : cx.ph;
    gph = base % gp;                          /* (the gate's place in its period: once a block, then counted) */
    cx.ph += n;
    bar4 = 64u * P;
    hp = cx.ht / ((bar4 >> 12) + 1u);         /* the risers' progress, Q12 (4 bars) */
    np = cx.nt / ((bar4 >> 12) + 1u);
    if (hp > 4096u) hp = 4096u;
    if (np > 4096u) np = 4096u;
    if (on & CX_BIT(CX_HPR) || cx.wh) {       /* HP RISER: 120 Hz .. ~6 kHz, slow then fast, the resonance growing (was
                                                * 40 Hz up: its first bar unheard, found on a device) */
        uint32_t q = (hp + ((hp * hp) >> 12)) >> 1, fc = (uint32_t)((120u * (uint64_t)cx_exp2(q * 1445u >> 12)) >> 16);
        hf = (int32_t)(fc * 2334u / 1000u);   /* (2 pi fc / fs, Q14) */
        hd = 16000 - (int32_t)(hp * 10000u >> 12);
        if (hf > 11500) hf = 11500;
    }
    if (on & CX_BIT(CX_NOISE) || cx.wn) {     /* NOISE RISER: a band 600 Hz .. 6 kHz, louder and louder (600: over the
                                                * bass from the start, not under it) */
        uint32_t fc = (uint32_t)((600u * (uint64_t)cx_exp2(np * 850u >> 12)) >> 16);
        nf = (int32_t)(fc * 2334u / 1000u);
        if (nf > 12000) nf = 12000;
        ng = (int32_t)((1700u * cx_exp2(np * 666u >> 12)) >> 16);   /* (Q12: 0.41 .. 2.5 on the band, as dB a straight
                                                * rise of 16: heard at once, ~-10 dBFS at the top; was np^2 x 1.5, -66 dB
                                                * under the mix for its first second, found on a device) */
    }
    for (i = 0; i < n; i++) {
        int32_t l = ml[i], r = mr[i], m = (l + r) >> 1;
        if ((on & CX_BIT(CX_THROW)) || cx.wt)
            cx.wt = cx_ramp(cx.wt, (on & CX_BIT(CX_THROW)) != 0u);
        if ((on & CX_BIT(CX_WASH)) || cx.ww)
            cx.ww = cx_ramp(cx.ww, (on & CX_BIT(CX_WASH)) != 0u);
        if (cx.wt)
            sd[i] += mulq16(m, (uint32_t)cx.wt << 1);
        if (cx.ww) {
            sr[i] += mulq16(m, (uint32_t)cx.ww << 1);
            duck = 32768 - (cx.ww * 21) / 32;   /* WASH: the dry back to ~35 % */
            l = mulq16(l, (uint32_t)duck << 1);
            r = mulq16(r, (uint32_t)duck << 1);
        }
        if (on & CX_BIT(CX_FREEZE)) {         /* FREEZE: the mix into the delay for one delay time, then nothing new
                                                * (an empty delay had nothing to hold: found on a device) */
            sd[i] = cx.ft < flen ? sd[i] + m : 0;
            cx.ft++;
        }
        l += wt[i];
        r += wt[i];
        wt[i] = 0;
        if ((on & CX_BIT(CX_NOISE)) || cx.wn)           /* (each stage: its share stepped only while it is in play) */
            cx.wn = cx_ramp(cx.wn, (on & CX_BIT(CX_NOISE)) != 0u);
        if (cx.wn) {
            int32_t z, b;
            cx.nrnd ^= cx.nrnd << 13;
            cx.nrnd ^= cx.nrnd >> 17;
            cx.nrnd ^= cx.nrnd << 5;
            z = (int32_t)(cx.nrnd >> 18) - 8192;
            cx_svf(z, cx.nz, nf, 9000);
            b = (cx.nz[1] * ng) >> 12;
            b = mulq16(b, (uint32_t)cx.wn << 1);
            l += b;
            r += b;
            sr[i] += b >> 1;
        }
        if (cx.tape)
            cfx_tape(&l, &r);
        if ((on & (CX_BIT(CX_GATE16) | CX_BIT(CX_GATE8))) || cx.gs)
            cx.gs = cx_ramp(cx.gs, (on & (CX_BIT(CX_GATE16) | CX_BIT(CX_GATE8))) != 0u);
        if (cx.gs) {                          /* GATE: open the first half of each 1/16 (1/8), 1.5 ms edges */
            int open = gph < gp / 2u;
            cx.gg += clamp((open ? 32768 : 0) - cx.gg, -512, 512);
            {
                int32_t g = 32768 - mulq16(32768 - cx.gg, (uint32_t)cx.gs << 1);
                l = mulq16(l, (uint32_t)g << 1);
                r = mulq16(r, (uint32_t)g << 1);
            }
        } else
            cx.gg = 32768;
        if (++gph >= gp) gph = 0;
        if ((on & CX_BIT(CX_DRIVE)) || cx.wd)
            cx.wd = cx_ramp(cx.wd, (on & CX_BIT(CX_DRIVE)) != 0u);
        if (cx.wd) {                          /* DRIVE: x3 into a cubic soft clip, back down a little */
            int32_t u = clamp(l * 3, -32767, 32767), v = clamp(r * 3, -32767, 32767), uu = (u * u) >> 15, vv = (v * v) >> 15;
            int32_t yl = (((3 * u - ((uu * u) >> 15)) >> 1) * 3) >> 2, yr = (((3 * v - ((vv * v) >> 15)) >> 1) * 3) >> 2;
            l += mulq16(yl - l, (uint32_t)cx.wd << 1);
            r += mulq16(yr - r, (uint32_t)cx.wd << 1);
        }
        if ((on & CX_BIT(CX_CRUSH)) || cx.wc)
            cx.wc = cx_ramp(cx.wc, (on & CX_BIT(CX_CRUSH)) != 0u);
        if (cx.wc) {                          /* CRUSH: 7 bits, every 4th sample held (~11 kHz) */
            if (++cx.cn >= 4u) {
                cx.cn = 0;
                cx.chl = ((l >> 9) << 9) + 256;
                cx.chr = ((r >> 9) << 9) + 256;
            }
            l += mulq16(cx.chl - l, (uint32_t)cx.wc << 1);
            r += mulq16(cx.chr - r, (uint32_t)cx.wc << 1);
        }
        if (isw || iso.w) {                   /* the ISOLATOR: its knobs, the kills held (cos_iso.c) */
            iso.w = cx_ramp(iso.w, isw);
            for (f2 = 0; f2 < 3u; f2++) iso.g[f2] += clamp(it[f2] - iso.g[f2], -ISO_SLOPE, ISO_SLOPE);
            if (iso.w) {
                /* (2^23: room for x 8 and the filters; not 2^22, whose -2^22 is 0xffc00000, a chip ROM address to
                 * tools/build.py's check) */
                int32_t yl = iso_ch(clamp(l, -(1 << 23), 1 << 23), iso.s[0], iso.g), yr = iso_ch(clamp(r, -(1 << 23), 1 << 23), iso.s[1], iso.g);
                l += mulq16(yl - l, (uint32_t)iso.w << 1);
                r += mulq16(yr - r, (uint32_t)iso.w << 1);
            } else {
                memset(iso.s, 0, sizeof iso.s);
                iso.g[0] = iso.g[1] = iso.g[2] = 32768;
            }
        }
        if ((on & CX_BIT(CX_HPR)) || cx.wh)
            cx.wh = cx_ramp(cx.wh, (on & CX_BIT(CX_HPR)) != 0u);
        if (cx.wh) {                          /* HP RISER (let go: the drop, in 2.9 ms) */
            int32_t yl = cx_svf(l >> 2, &cx.hz[0], hf, hd) << 2, yr = cx_svf(r >> 2, &cx.hz[2], hf, hd) << 2;
            l += mulq16(yl - l, (uint32_t)cx.wh << 1);
            r += mulq16(yr - r, (uint32_t)cx.wh << 1);
        }
        ml[i] = l;
        mr[i] = r;
    }
    if (on & CX_BIT(CX_HPR)) cx.ht += n;
    if (on & CX_BIT(CX_NOISE)) cx.nt += n;
    if (cx.tape == 2u && !cx.tw) {            /* (back: the buffer to the SLICER) */
        cx.tape = 0;
        sl_lent = 0;
        slicer_drop();
    }
    cx.busy = (uint8_t)(cx.wt || cx.ww || cx.wn || cx.tape || cx.gs || cx.wd || cx.wc || iso.w || cx.wh || cx.tt || cx.wst ||
                        cfx_goff[GF_DFDBK] || cfx_goff[GF_DMIX] || cfx_goff[GF_RSIZE]);
}

/* the lane's sixteenth now: recorded (REC with FX held: the keys held now), played back (its bits) */
static uint32_t cfx_lane_bits(uint32_t n)
{
    uint32_t p, pn, d;
    if (!song.playing) { perf_lane = 0; return 0; }
    d = div_samples(2);
    p = (ms_clock - n) / d % FXL_N;
    pn = ms_clock / d % FXL_N;                    /* (perform.c's: the next block's sixteenth, perf_begin takes them
                                                   * there: on the 1/16 as a key held would) */
    if (cfx_rec) {
        uint16_t v = (uint16_t)(cfx_held | cfx_latched);
        uint32_t pv = (perf_held | perf_latched) & CFX_PF_LANE;
        cfx_lane[p] = v;
        cfx_plane[p] = pv;
        if (v || pv) cfx_lane_on = 1;
        perf_lane = 0;
        return 0;
    }
    if (!cfx_lane_on) { perf_lane = 0; return 0; }
    perf_lane = cfx_plane[pn];
    return cfx_lane[p];
}
/* fx.c lev_block: the pad's effects, when any is on or still sounding */
static void live_tick(uint32_t n);             /* (cos_live.c, the end of seq.c) */
static void cfx_run(uint32_t n)
{
    live_tick(n);                                 /* (cursed-os) the live layer: mutes, fills, the BREAKDOWN on the grid */
    if (!cfx_lane_on || !song.playing) perf_lane = 0;
    if (cfx_held | cfx_latched | cfx_auto | cfx_home | cx.busy | cx.on | cfx_lane_on | cfx_rec | iso_v[0] | iso_v[1] | iso_v[2])
        cfx_block(mix_l, mix_r, wet, fxq_d, fxq_r, n);
}
