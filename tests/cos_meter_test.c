/* SPDX-License-Identifier: GPL-3.0-only
 * (cursedsound) firmware/src/cos_meter.c: its integer K-weighting against the float filters and BS.1770's reference
 * (a 997 Hz sine at 0 dBFS in one channel reads -3.01 LUFS, in both 0.0), the peak, the correlation sums, the rings.
 *   cc -O2 -w -Ifirmware/src -o build/t/cos_meter_test tests/cos_meter_test.c -lm && build/t/cos_meter_test */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
#include "cos_meter.c"
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } else { printf("ok: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int32_t buf[2 * 4410];
static double lufs_of(double fl, double fr, double al, double ar, int blocks)   /* a sine each side; LUFS of the last 4 blocks */
{
    int b, i; double e = 0; static uint32_t t;
    for (b = 0; b < blocks; b++) {
        for (i = 0; i < 4410; i++, t++) {
            buf[2 * i] = (int32_t)lrint(al * 32767.0 * sin(2 * M_PI * fl * t / 44100.0));
            buf[2 * i + 1] = (int32_t)lrint(ar * 32767.0 * sin(2 * M_PI * fr * t / 44100.0));
        }
        mt_block(buf, 4410);
    }
    for (b = 1; b <= 4; b++) e += (double)mt_ring[(mt_n - b) & 31u].kw / 1073741824.0;
    return -0.691 + 10 * log10(e / (4 * 4410.0));
}
int main(void)
{
    double v;
    v = lufs_of(997, 997, 1.0, 1.0, 20);
    CHECK(fabs(v) < 0.1, "997 Hz at 0 dBFS, both sides: %.2f LUFS (0.0)", v);
    v = lufs_of(997, 997, 0.1, 0.1, 10);
    CHECK(fabs(v + 20.0) < 0.1, "-20 dBFS: %.2f LUFS (-20.0)", v);
    v = lufs_of(997, 997, 0.1, 0.0, 10);
    CHECK(fabs(v + 23.01) < 0.1, "-20 dBFS one side: %.2f LUFS (-23.01, BS.1770's -3.01 at 0 dBFS)", v);
    v = lufs_of(10000, 10000, 0.1, 0.1, 10);
    CHECK(v > -20.0 + 3.0 && v < -20.0 + 5.0, "10 kHz: the shelf's +4 dB (%.2f)", v);
    v = lufs_of(30, 30, 0.1, 0.1, 30);
    CHECK(v < -20.0 - 2.0, "30 Hz: the high-pass under it (%.2f)", v);
    {
        const mt_blk_t *b = &mt_ring[(mt_n - 1u) & 31u];
        CHECK(b->pk >= 3270 && b->pk <= 3277, "the peak: 0.1 x 32767 (%d)", b->pk);
        CHECK(b->lr > 0 && llabs(b->lr - (int64_t)b->ll) < (int64_t)(b->ll / 1000u), "L R = L L for mono");
    }
    v = lufs_of(1000, 1000, 0.5, -0.5, 6);
    {
        const mt_blk_t *b = &mt_ring[(mt_n - 1u) & 31u];
        CHECK(b->lr < 0, "out of phase: L R below 0");
    }
    {   /* its cost here, a sample (x86 -O2: a hint of the device's) */
        clock_t c0 = clock(); int k;
        for (k = 0; k < 2000; k++) mt_block(buf, 4410);
        printf("  %.1f ns a sample\n", (double)(clock() - c0) / CLOCKS_PER_SEC / (2000.0 * 4410) * 1e9);
    }
    printf("cos_meter_test: %s (%d failures)\n", fails ? "FAILED" : "ok", fails);
    return fails != 0;
}
