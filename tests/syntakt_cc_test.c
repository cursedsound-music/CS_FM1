/* SPDX-License-Identifier: GPL-3.0-only OR MIT
 * Copyright (C) 2026 cursedsound
 * (cursed-os 0.19.0, GG: "this midi cc list ... can help verify if everything works and also good for automated
 * tests") Every CC an Elektron Syntakt sends, from Pencil Research's MIDI CC list (tests/data/syntakt_cc.csv, its
 * github.com/pencilresearch/midi Elektron/Syntakt.csv, GPL-3.0), into the FM-1 on a track's channel, as when one of the
 * Syntakt's own tracks shares a channel with an FM-1 track (its CC OUT on) or a Syntakt MIDI track's CC slot is set to it:
 *   each at its lowest, highest and middle value: only that track's parameter the FM-1's CC map names for it moves
 *   (midi_control.c MIDI_CC_MAP, mod.c MIDI_CC_ENG), every other CC moves nothing (no parameter of any track, no global);
 *   notes held while every CC comes (the Syntakt's sustain among them) still end at their note-off / pedal up, and
 *   nothing sounds after;
 *   GLO > LINK 2's maps send what the list names: ST TRACK the filter's frequency, resonance, delay and reverb sends,
 *   ST FX the FX track's filter frequency and resonance, the delay's feedback, the reverb's decay.
 * It prints which Syntakt parameters move which FM-1 parameter (the guide's table).
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/syntakt_cc_test tests/syntakt_cc_test.c -lm
 *   build/t/syntakt_cc_test tests/data/syntakt_cc.csv */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); } else printf("ok: "); printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { char sect[40], name[64]; int cc, lo, hi; } row_t;
static row_t rows[600];
static uint32_t nrows;

static void csv_field(const char **s, char *out, size_t n)   /* one field (quotes allowed), *s past its comma */
{
    size_t k = 0;
    int q = **s == '"';
    if (q) (*s)++;
    while (**s && (q ? !(**s == '"' && (*s)[1] != '"') : **s != ',' && **s != '\n' && **s != '\r')) {
        if (q && **s == '"') (*s)++;
        if (k + 1 < n) out[k++] = **s;
        (*s)++;
    }
    if (q && **s == '"') (*s)++;
    if (**s == ',') (*s)++;
    out[k] = 0;
}
static int load(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[2048], fld[18][256];
    if (!f) return 0;
    if (!fgets(line, sizeof line, f)) { fclose(f); return 0; }   /* (the header) */
    while (fgets(line, sizeof line, f) && nrows < NELEM(rows)) {
        const char *s = line;
        uint32_t i;
        for (i = 0; i < 18u; i++) csv_field(&s, fld[i], sizeof fld[i]);
        if (!fld[5][0]) continue;                        /* (an NRPN only) */
        snprintf(rows[nrows].sect, sizeof rows[nrows].sect, "%s", fld[2]);
        snprintf(rows[nrows].name, sizeof rows[nrows].name, "%s", fld[3]);
        rows[nrows].cc = atoi(fld[5]);
        rows[nrows].lo = fld[7][0] ? atoi(fld[7]) : 0;
        rows[nrows].hi = fld[8][0] ? atoi(fld[8]) : 127;
        nrows++;
    }
    fclose(f);
    return nrows > 0;
}
static int csv_cc(const char *name)                     /* the list's CC of a parameter, -1 none */
{
    uint32_t i;
    for (i = 0; i < nrows; i++) if (str_eq(rows[i].name, name)) return rows[i].cc;
    return -1;
}
/* the FM-1 parameter CC cc sets on track t (the map, the engine's brightness / resonance), P_COUNT none */
static uint32_t mapped(const track_t *t, uint32_t cc)
{
    uint32_t i, id = P_COUNT, e;
    for (i = 0; i < NELEM(MIDI_CC_MAP); i++) if (MIDI_CC_MAP[i][0] == cc) id = MIDI_CC_MAP[i][1];
    if (id == MCC_CUT || id == MCC_RES) {
        e = MIDI_CC_ENG[eng_idx(t->eng_req)] >> (id == MCC_CUT ? 4 : 0) & 15u;
        id = e ? P_E0 + e - 1u : P_COUNT;
    }
    return id;
}
static void blk(void)
{
    static int32_t o[2u * CTL];
    events_block(CTL);
    song.master_q12 = 4096;
    mix_block(o, CTL);
}
static uint32_t sounding(void)
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NPART; p++) for (i = 0; i < NVOICE; i++) n += trk[p].v[i].active && trk[p].v[i].stage != 4u;
    return n;
}

int main(int argc, char **argv)
{
    static int16_t p0[NTRK][P_COUNT], g0[G_COUNT];
    uint32_t i, k, j, ch = 2u, stray = 0, wrong = 0, moved = 0, hits = 0;
    const char *path = argc > 1 ? argv[1] : "tests/data/syntakt_cc.csv";
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1;
    if (!load(path)) { printf("FAIL: no CC list at %s\n", path); return 1; }
    CHECK(nrows > 300u, "the Syntakt's CC list: %u parameters with a CC (%s)", nrows, path);
    /* GLO > LINK 2's maps against the list */
    CHECK(MC_MAP[0][0] == csv_cc("Filter: Frequency") && MC_MAP[0][1] == csv_cc("Filter: Resonance") &&
          MC_MAP[0][2] == csv_cc("Amp: Delay Send") && MC_MAP[0][3] == csv_cc("Amp: Reverb Send"),
          "LINK 2 ST TRACK: the filter's frequency %d, resonance %d, delay %d and reverb %d sends, as the list names them",
          MC_MAP[0][0], MC_MAP[0][1], MC_MAP[0][2], MC_MAP[0][3]);
    CHECK(MC_MAP[1][0] == csv_cc("AFX Filter: Frequency") && MC_MAP[1][1] == csv_cc("AFX Filter: Resonance") &&
          MC_MAP[1][2] == csv_cc("Delay: Feedback") && MC_MAP[1][3] == csv_cc("Reverb: Decay Time"),
          "LINK 2 ST FX: the FX track's filter %d / %d, the delay's feedback %d, the reverb's decay %d", MC_MAP[1][0],
          MC_MAP[1][1], MC_MAP[1][2], MC_MAP[1][3]);
    /* every CC on T3's channel, notes held through it */
    seq_stop();
    for (j = 0; j < 64u; j++) blk();
    midi_event(0x90u, ch, 60u, 100u); midi_event(0x90u, ch, 64u, 100u); midi_event(0x90u, ch, 67u, 100u);
    for (j = 0; j < 4u; j++) blk();
    printf("  Syntakt parameter (CC) -> what it moves on an FM-1 track sharing its channel:\n");
    for (i = 0; i < nrows; i++) {
        const int vals[3] = {rows[i].lo, rows[i].hi, (rows[i].lo + rows[i].hi) / 2};
        uint32_t want = mapped(&trk[ch], (uint32_t)rows[i].cc), any = 0, t;
        if (rows[i].cc < 0 || rows[i].cc > 127) continue;
        for (t = 0; t < NTRK; t++) memcpy(p0[t], trk[t].p, sizeof p0[t]);
        memcpy(g0, song.g, sizeof g0);
        for (k = 0; k < 3u; k++) {
            midi_event(0xB0u, ch, (uint32_t)rows[i].cc, (uint32_t)clamp(vals[k], 0, 127));
            blk();
            for (t = 0; t < NTRK; t++)
                for (j = 0; j < P_COUNT; j++)
                    if (trk[t].p[j] != p0[t][j]) {
                        if (t != ch || j != want) { wrong++; printf("  WRONG: %s (CC %d = %d) moved T%u's parameter %u\n", rows[i].name, rows[i].cc, vals[k], t + 1u, j); }
                        any = 1;
                    }
            for (j = 0; j < G_COUNT; j++)
                if (song.g[j] != g0[j]) { stray++; printf("  STRAY: %s (CC %d) moved global %u\n", rows[i].name, rows[i].cc, j); }
        }
        if (want < P_COUNT) {
            hits++;
            moved += any || p0[ch][want] == trk[ch].p[want];   /* (moved, or already where the CC put it) */
            printf("    %-34s CC %3d -> %s\n", rows[i].name, rows[i].cc, want >= P_E0 ? track_desc(&trk[ch], want)->label : TP[want].label);
        }
        for (t = 0; t < NTRK; t++) memcpy(trk[t].p, p0[t], sizeof p0[t]);   /* (each CC from the same sound) */
        memcpy(song.g, g0, sizeof g0);
    }
    CHECK(!wrong && !stray, "every CC of the list: only the parameter the FM-1's map names moves, on that track only (%u wrong, %u globals)", wrong, stray);
    CHECK(hits && moved == hits, "the list's CCs the FM-1 maps: %u parameters, each moves its FM-1 parameter", hits);
    midi_event(0xB0u, ch, 64u, 0u);                      /* (the list's sustain: up) */
    midi_event(0x80u, ch, 60u, 0); midi_event(0x80u, ch, 64u, 0); midi_event(0x80u, ch, 67u, 0);
    for (j = 0; j < (uint32_t)(FS * 4 / CTL); j++) blk();
    CHECK(!sounding() && !midi_owners[ch], "after every CC and the note-offs: nothing sounds, nothing held (%u voices)", sounding());
    printf(fails ? "syntakt_cc: %d FAILED\n" : "syntakt_cc: all ok\n", fails);
    return fails != 0;
}
