/* SPDX-License-Identifier: GPL-3.0-only
 * Adapted from MIDI clock contributions by ChanceTheMaker and keremimo (2026).
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Audio ISR state. Input timestamps keep USB/TRS jitter outside the render loop. */
static struct {
    uint32_t pos, rendered, last_ms, start_ms, rem, interval_ms;
    uint32_t pulse_samples, interp_q8, tempo_ms;
    uint8_t mode, have_pulse, tempo_valid, tempo_n;
    uint32_t tempo_q;                              /* (cursed-os 0.14.1) the beat, samples x 16, smoothed */
} midi_clock;

static __attribute__((noinline)) void midi_clock_transport(uint32_t status, uint32_t ms)
{
    if (status == 0xFAu) {                         /* Start: step zero */
        midi_clock.pos = midi_clock.rendered = midi_clock.rem = 0;
        midi_clock.have_pulse = midi_clock.tempo_valid = 0;
        midi_clock.interval_ms = 0;
        midi_clock.start_ms = ms;
        seq_start();
    } else if (status == 0xFBu) {                  /* Continue: preserve the step */
        midi_clock.pos = midi_clock.rendered;
        midi_clock.rem = 0;
        midi_clock.have_pulse = midi_clock.tempo_valid = 0;
        midi_clock.interval_ms = 0;
        midi_clock.start_ms = ms;
        if (!song.playing)
            motion_begin();
        song.playing = 1;
    } else if (status == 0xFCu) {
        seq_stop();
    }
}

static __attribute__((noinline)) void midi_clock_pulse(uint32_t ms)
{
    uint32_t q;
    if (!midi_clock.tempo_valid) {
        midi_clock.tempo_valid = 1;
        midi_clock.tempo_ms = ms;
        midi_clock.tempo_n = 0;
    } else if (++midi_clock.tempo_n == 24u) {
        uint32_t dt = ms - midi_clock.tempo_ms;
        midi_clock.tempo_ms = ms;
        midi_clock.tempo_n = 0;
        /* (cursed-os 0.14.1, GG: Ableton at 135, the FM-1 134.7 .. 135 all the time) A whole beat (24 clocks), not a
         * quarter: a millisecond's error 0.2 %, not 0.9 %; smoothed (a quarter of the way a beat), a jump of 2 % or more
         * taken at once. The tempo (delay times, the steps' lengths) no longer wobbles with USB's timing. Reject gaps
         * and corrupt bursts */
        if (dt >= 250u && dt <= 1500u) {
            uint32_t old_beat = beat_samples(), new_beat, m = (uint32_t)FS * dt / 1000u * 16u + ((uint32_t)FS * dt % 1000u) * 16u / 1000u;
            if (!midi_clock.tempo_q || m > midi_clock.tempo_q + midi_clock.tempo_q / 50u || m + midi_clock.tempo_q / 50u < midi_clock.tempo_q)
                midi_clock.tempo_q = m;
            else
                midi_clock.tempo_q = (uint32_t)((int32_t)midi_clock.tempo_q + ((int32_t)m - (int32_t)midi_clock.tempo_q) / 4);
            new_beat = (midi_clock.tempo_q + 8u) >> 4;
            dt = (new_beat * 1000u + (uint32_t)FS / 2u) / (uint32_t)FS;   /* (the beat smoothed, ms: the BPM shown) */
            if (song.playing && new_beat != old_beat) {
                uint32_t i, ratio = (new_beat << 12) / old_beat;
                /* Preserve each track's fractional step phase when the master
                 * changes tempo. The next pulse then lands on its new boundary. */
                for (i = 0; i < NTRK; i++) {
                    if (trk[i].seq_pos < (uint32_t)FS * 2u)
                        trk[i].seq_pos = (uint32_t)(((uint64_t)trk[i].seq_pos * ratio + 2048u) >> 12);
                    if (trk[i].seq_off)
                        trk[i].seq_off = (uint32_t)(((uint64_t)trk[i].seq_off * ratio + 2048u) >> 12);
                }
                if (clk_pos < (uint32_t)FS * 2u)   /* (the metronome's beat as the steps) */
                    clk_pos = (uint32_t)(((uint64_t)clk_pos * ratio + 2048u) >> 12);
            }
            midi_beat_samples = new_beat;
            song.g[G_BPM] = (int16_t)clamp((int32_t)((60000u + dt / 2u) / dt), 40, 240);
        }
    }
    if (song.playing) {
        if (midi_clock.have_pulse) {
            uint32_t interval = ms - midi_clock.last_ms;
            if (interval >= 8u && interval <= 80u)
                midi_clock.interval_ms = interval;
            q = beat_samples() + midi_clock.rem;
            midi_clock.pos += q / 24u;
            midi_clock.rem = q % 24u;
        }
        midi_clock.have_pulse = 1;
    }
    midi_clock.pulse_samples = beat_samples() / 24u;
    if (!midi_clock.interval_ms)
        midi_clock.interval_ms = beat_samples() * 1000u / ((uint32_t)FS * 24u);
    midi_clock.interp_q8 = midi_clock.pulse_samples * 256u / midi_clock.interval_ms;
    midi_clock.last_ms = ms;
}

static uint32_t midi_clock_advance(uint32_t now)
{
    uint32_t target, elapsed, offset, n;
    if (!midi_clock.have_pulse)
        return 0;
    elapsed = now - midi_clock.last_ms;
    /* Interpolate to the next pulse, never across it before it arrives. */
    if (elapsed > midi_clock.interval_ms)
        elapsed = midi_clock.interval_ms;
    offset = elapsed * midi_clock.interp_q8 >> 8;
    if (offset >= midi_clock.pulse_samples)
        offset = midi_clock.pulse_samples - 1u;
    target = midi_clock.pos + offset;
    n = (int32_t)(target - midi_clock.rendered) > 0 ? target - midi_clock.rendered : 0u;
    if (n > (uint32_t)FS / 8u)
        n = (uint32_t)FS / 8u;
    midi_clock.rendered += n;
    return n;
}

