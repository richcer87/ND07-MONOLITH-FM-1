/* SPDX-License-Identifier: GPL-3.0-only */
/* ND-07: the key velocity on a computer, with synthetic presses (the readings Velo measured on an FM-1).
 *   cc -O2 -Wall -Wextra -o /tmp/sense_test tests/sense_test.c && /tmp/sense_test */
#include <stdio.h>
#include "../firmware/src/nd07_sense.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

/* a press: pulled-up readings land at `pulled`, driven ones at `driven`; held `hold` samples; then released */
static int press(sn_key_t *k, uint32_t pulled, uint32_t driven, int hold, uint32_t floor, uint8_t *at_peak)
{
    int i, vel = -1;
    *at_peak = 0;
    sn_sample(k, 1023, 0, floor);                     /* open */
    for (i = 0; i < hold; i++) {
        uint32_t d = k->drv_next;
        uint32_t v = d ? driven - (uint32_t)(i > 8 ? 20 : 0) : pulled;   /* later: leaning in (aftertouch) */
        sn_sample(k, v, d, floor);
        if (k->fresh) { vel = k->vel; k->fresh = 0; }
        if (k->at > *at_peak) *at_peak = k->at;
    }
    for (i = 0; i < 4; i++) sn_sample(k, 1023, k->drv_next, floor);
    return vel;
}

int main(void)
{
    sn_key_t k = {0};
    uint32_t f = sn_floor(50), d, last = 200;
    uint8_t at;
    int v_hard, v_mid, v_soft, i;
    CHECK(f == SN_FLOOR, "floor at sens 50: %u", f);
    v_hard = press(&k, 190, 430, 20, f, &at);
    v_mid = press(&k, 280, 620, 20, f, &at);
    v_soft = press(&k, 350, 860, 20, f, &at);
    printf("velocity: hard %d  regular %d  soft %d  (aftertouch peak %u)\n", v_hard, v_mid, v_soft, at);
    CHECK(v_hard == 127, "a hard hit is 127 (%d)", v_hard);
    CHECK(v_mid > 20 && v_mid < 110, "a regular press in the middle (%d)", v_mid);
    CHECK(v_soft >= 1 && v_soft < v_mid, "a soft press below it (%d)", v_soft);
    CHECK(k.st == SN_OPEN, "released");
    CHECK(at > 0, "leaning in gives aftertouch");
    for (d = SN_DBASE; d <= 1023u; d += 7u) {          /* monotonic: a deeper reading is never softer */
        uint32_t v = sn_velocity(d, f);
        CHECK(v >= 1u && v <= 127u, "range at %u: %u", d, v);
        CHECK(v <= last, "monotonic at %u: %u > %u", d, v, last);
        last = v;
    }
    /* a graze: touches but never seats below RELEASE for long, then lifts: no stuck key */
    sn_sample(&k, 850, 0, f);
    for (i = 0; i < 6; i++) sn_sample(&k, 1000, k.drv_next, f);
    CHECK(k.st == SN_OPEN, "a graze lets go");
    CHECK(sn_floor(0) < sn_floor(100), "sens moves the floor");
    printf(fails ? "sense: %d FAILED\n" : "sense: all passed\n", fails);
    return fails != 0;
}
