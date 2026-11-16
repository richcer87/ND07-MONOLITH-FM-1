/* SPDX-License-Identifier: GPL-3.0-only */
/* ND-07 // MONOLITH: key velocity and aftertouch from the FM-1's carbon pills (beta: needs a real FM-1 to tune).
 *
 * The method is Velo / MONK-1's (Charles Vestal, github.com/charlesvestal/fm1-monk, app/sense.c, GPL-3.0-only),
 * cut down to what ND-07 needs, and with no per-key learning: fm1_input.h samples each key of rows PA7 / PA8 every
 * ~2.2 ms, 1023 open, lower the harder the pill is pressed. Pulled-up samples run the state machine (TOUCH, then two
 * samples above RELEASE: off); right after the touch, every other sample is taken with the row driven high, which
 * spreads the seated range. The velocity is the lowest driven reading of the first SN_WAIT driven samples, on a log
 * scale between the floor (a firm press) and SN_CEIL (the lightest press that seats): conductive rubber's resistance
 * (reading - SN_DBASE) falls as a power of the force, so its log is linear in the log of the force; then
 * 1 + 126 (1 - t)^0.7 (measured by Velo on 830 presses: regular playing lands at 45 / 63 / 80). Aftertouch: pressing
 * harder than the note landed, 0 at the landing reading, 127 SN_ATRANGE counts below it.
 * Pure integer, no division by 64-bit, no libcalls: tests/sense_test.c runs it on a computer. */
#pragma once
#include <stdint.h>

#define SN_TOUCH 900u       /* pulled-up: the pill starts to conduct */
#define SN_RELEASE 600u     /* pulled-up: two samples above it, the key is off */
#define SN_WAIT 2u          /* driven samples before the velocity (~6.6 ms after the touch) */
#define SN_DBASE 185u       /* driven, no pill resistance: the diode and the 595 */
#define SN_CEIL 896u        /* driven: the lightest press that seats */
#define SN_FLOOR 450u       /* driven: a firm press (Velo's default at drive strength 1); KSENS moves it */
#define SN_ATRANGE 64u
enum { SN_OPEN, SN_TOUCHED, SN_HELD };
typedef struct {
    uint8_t st, nd, rel, drv_next;    /* state; driven samples so far; samples above RELEASE; drive the next one */
    uint8_t vel, at, fresh;           /* the velocity (1..127), the aftertouch (0..127); a velocity not taken yet */
    uint16_t dmin, land;              /* the lowest driven reading since the touch; the landing */
} sn_key_t;

static const uint8_t SN_POW07[17] = {0, 18, 29, 39, 48, 56, 63, 71, 78, 84, 91, 97, 103, 109, 115, 120, 126};

static uint32_t sn_lg2(uint32_t x)    /* log2 in Q8 (x >= 1), the mantissa linear: within 0.09 */
{
    uint32_t m = 0;
    while (m < 31u && (x >> (m + 1u)) != 0u) m++;
    return m * 256u + (m >= 8u ? (x >> (m - 8u)) & 255u : (x << (8u - m)) & 255u);
}
/* sens 0..100: 50 = Velo's floor; higher = a lighter touch reaches 127 */
static uint32_t sn_floor(uint32_t sens) { return SN_FLOOR - 120u + (sens > 100u ? 100u : sens) * 24u / 10u; }
static uint32_t sn_velocity(uint32_t dmin, uint32_t floor)
{
    uint32_t r = dmin > SN_DBASE ? dmin - SN_DBASE : 1u, lr = sn_lg2(r), lf = sn_lg2(floor - SN_DBASE),
             lc = sn_lg2(SN_CEIL - SN_DBASE), t, u, i;
    t = lr <= lf ? 0u : lr >= lc ? 256u : (lr - lf) * 256u / (lc - lf);
    u = 256u - t;
    i = u >> 4;
    if (i >= 16u) return 1u + SN_POW07[16];
    return 1u + SN_POW07[i] + (((uint32_t)(SN_POW07[i + 1u] - SN_POW07[i]) * (u & 15u)) >> 4);
}
/* one sample of one key (from the scan's timer interrupt) */
static void sn_sample(sn_key_t *k, uint32_t v, uint32_t driven, uint32_t floor)
{
    if (!driven) {
        if (k->st == SN_OPEN) {
            if (v < SN_TOUCH) {
                k->st = SN_TOUCHED; k->nd = 0; k->rel = 0; k->dmin = 1023; k->fresh = 0; k->at = 0;
                k->drv_next = 1;
            }
            return;
        }
        k->drv_next = 1;                          /* held: driven and pulled-up samples take turns */
        if (v > SN_RELEASE) {
            if (++k->rel >= 2u) { k->st = SN_OPEN; k->at = 0; k->drv_next = 0; }
        } else {
            k->rel = 0;
        }
        return;
    }
    k->drv_next = 0;
    if (k->st == SN_TOUCHED) {
        if (v < k->dmin) k->dmin = (uint16_t)v;
        if (++k->nd >= SN_WAIT) {
            k->vel = (uint8_t)sn_velocity(k->dmin, floor);
            k->land = k->dmin; k->st = SN_HELD; k->fresh = 1;
        }
    } else if (k->st == SN_HELD) {
        uint32_t a = v < k->land ? (k->land - v) * 127u / SN_ATRANGE : 0u;
        k->at = (uint8_t)(a > 127u ? 127u : a);
    }
}
