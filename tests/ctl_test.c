/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) cursed-os 0.13.0: GLO > MIDI 2 KNOBS: +MIDI turns the sound and sends its value as a CC (CTRL CH + the
 * mode, CC 20 + page x 4 + knob); MIDI sends a relative CC and leaves the sound.
 *   cc -O1 -w -DCOS_SLIM=1 -Ibuild/gen -Ifirmware/src -Itests -o build/t/ctl_test tests/ctl_test.c -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static void frame16(void) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); ui_draw(); }
static void enc(uint32_t role, int32_t s) { host_enc[panel.enc[role]] += s * panel.dir[role]; frame16(); }

int main(void)
{
    ui_power_on(); load_depth++; fm6_init(); power_on_parts(); load_depth--;
    cz.flip = 1; song.playing = 0; frame16();
    track_select(3);
    CHECK(cos_ctl_mode() == 0u && cos_ctl_ch() == 6u, "defaults: KNOBS FM-1, CTRL CH 7");
    cz.mode = CM_AMP; cz.tab[CM_AMP] = 0; frame16();     /* (ENV's first page: knob 1 a track's parameter) */
    cos_cput(CC_CTLM, 1);
    cos_ctl_last = 0;
    {
        int16_t *vp = 0, v0;
        const param_desc_t *d = cos_knob(cos_page(), 0, &vp);
        v0 = *vp;
        enc(EN_K1, 3);
        CHECK(*vp != v0, "+MIDI: the knob still turns its parameter (%s %d -> %d)", d->label, v0, *vp);
        CHECK(cos_ctl_last == (1u << 16 | (6u + CM_AMP) % 16u << 8 | 20u), "a CC: CH %u CC %u (%x)", (6u + CM_AMP) % 16u + 1u, 20u, cos_ctl_last);
        cz.tab[CM_AMP] = 1; frame16();
        enc(EN_K1 + 2, 1);
        CHECK((cos_ctl_last & 127u) == 20u + 4u + 2u, "page 2, knob 3: CC 26 (%u)", cos_ctl_last & 127u);
    }
    {
        int16_t *vp = 0, v0;
        cz.tab[CM_AMP] = 0; frame16();
        cos_cput(CC_CTLM, 2);
        cos_knob(cos_page(), 0, &vp);
        v0 = *vp;
        cos_ctl_last = 0;
        enc(EN_K1, 2);
        CHECK(*vp == v0 && cos_ctl_last, "MIDI: the sound stays, a CC goes (%d %d %x)", v0, *vp, cos_ctl_last);
        cos_cput(CC_CTLCH, 3);
        enc(EN_K1, -1);
        CHECK(((cos_ctl_last >> 8) & 15u) == (2u + CM_AMP) % 16u, "CTRL CH 3: + the mode (%u)", (cos_ctl_last >> 8) & 15u);
        cos_cput(CC_CTLM, 0);
        cos_cput(CC_CTLCH, 7);
    }
    {   /* the ACID played by another instrument: SOUND EXT, MIDI OUT ALL: its notes out (an accent 127, a slide's notes
         * overlapping: legato), nothing heard here */
        static int32_t o[2 * CTL];
        track_t *t = &trk[2];
        uint32_t b, i, w0, ons = 0, acc = 0, legato = 0, last_on = 0, open = 0;
        double e = 0;
        for (i = 0; i < NTRK; i++) trk[i].p[P_MUTE] = (int16_t)(i != 2u);
        for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
        t->p[P_SLEN] = 16;
        t->step[0].time = ST_NOTE; t->step[0].n = 1; t->step[0].note[0] = 36; t->step[0].vel = 100; t->step[0].flags = SF_SLIDE;
        t->step[1].time = ST_NOTE; t->step[1].n = 1; t->step[1].note[0] = 39; t->step[1].vel = 100;
        t->step[2].time = ST_NOTE; t->step[2].n = 1; t->step[2].note[0] = 43; t->step[2].vel = 100; t->step[2].flags = SF_ACCENT;
        track_select(2);
        cos_cput(CC_EXT, 1);
        cos_cput(CC_MOUT, 2);
        usb.config = 1;
        song.g[G_BPM] = 120;
        frame16();
        w0 = mo_w;
        transport_req = 1; frame16();
        for (b = 0; b < FS / CTL; b++) {
            mix_block(o, CTL);
            if (b % 22u == 0u) frame16();
            for (i = 0; i < 2u * CTL; i++) e += (double)o[i] * o[i];
        }
        transport_req = 2; frame16();
        for (i = w0; i != mo_w; i++) {
            uint32_t pk = midi_out_q[i % MQ], st = (pk >> 8) & 0xFFu, d1 = (pk >> 16) & 0xFFu, d2 = pk >> 24;
            if ((st & 15u) != trk_midi_ch(2)) continue;
            if ((st & 0xF0u) == 0x90u) { ons++; acc += d2 == 127u && d1 == 43u; legato += open && d1 == 39u; open = 1; last_on = d1; }
            if ((st & 0xF0u) == 0x80u && d1 == last_on) open = 0;
        }
        CHECK(ons >= 3u && acc == 1u && legato == 1u, "ACID out: %u notes, the accent 127 (%u), the slide legato (%u)", ons, acc, legato);
        CHECK(e < 1e6, "EXT: not heard here (%.3g)", e);
        cos_cput(CC_EXT, 0);
        cos_cput(CC_MOUT, 0);
        for (i = 0; i < NTRK; i++) trk[i].p[P_MUTE] = 0;
    }
    printf("ctl_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
