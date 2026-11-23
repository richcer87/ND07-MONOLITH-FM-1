/* SPDX-License-Identifier: GPL-3.0-only
 * EVA-01 theme: every knob name, every value of every parameter and every page name fits its place (no cut text).
 * cc -DDINGHY_HOST -Ifirmware/hal -Ifirmware/src -Ifirmware/gen tests/eva_fit_test.c */
#define main host_main
#include "../firmware/src/nd07.c"
#undef main
#ifndef FONT_D_CAP
#error "firmware/src/nd07.c is older than this test (v1.1): it needs the v1.1 nd07.c, gfx.c and firmware/gen/eva_font.h"
#endif
int main(void)
{
    static const char *const MODE[] = {"D", "M+u", "M", "TGT", "M name", "TERM", "CUT", "D word"};
    uint32_t p, m, k, q, g, cnt[8] = {0}, bad = 0;
    int32_t v;
    char s[40], nb[16];
    const char *u;
    for (p = 0; p < NP; p++) for (m = NPAGE; m-- > 0;) for (k = 0; k < 4; k++) for (q = 0; q < 4; q++) if (PAGE_P[m][k][q] == p) PMOD[p] = (uint8_t)m;
    NTGT = 0;
    for (p = 0; p < NP; p++) if (p != P_SLOT && p != P_L1TGT && p != P_L2TGT && p != P_THEME && p != P_ASSET && PMOD[p] != PG_PAD) TGT[NTGT++] = (uint8_t)p;
    for (m = 0; m < NPAGE; m++) {
        int32_t w = text_w(&FONT_D, PAGE_NAME[m]) <= 194 ? text_w(&FONT_D, PAGE_NAME[m]) : text_w(&FONT_M, PAGE_NAME[m]);
        if (w > 194) { printf("page %u '%s' %d px > 194\n", m + 1, PAGE_NAME[m], w); bad++; }
    }
    for (g = 0; g < NENG; g++) {                 /* (CHIP ENGINE's seven: their names and values, engine by engine) */
        P[P_ENG] = (int16_t)g;
        for (p = 0; p < NP; p++) eva_big[p] = 0;
        for (m = 0; m < NPAGE; m++) for (k = 0; k < 4; k++) for (q = 0; q < 4; q++) {
            p = PAGE_P[m][k][q];
            if (p == NO || (g && (p < P_E1 || p > P_E7))) continue;
            if (text_w(&FONT_A, pname(p)) > 55) { printf("label '%s' %d px > 55\n", pname(p), text_w(&FONT_A, pname(p))); bad++; }
            for (v = PD[p].lo; v <= p_hi(p); v++) {
                int r;
                fmt_param(p, v, s);
                r = eva_plan(p, s, 56, nb, &u);
                cnt[r]++;
                if (r == 6) { if (bad < 40) printf("value cut: %s = '%s'\n", pname(p), s); bad++; }
            }
        }
    }
    P[P_ENG] = 0;
    for (k = 0; k < 8; k++) printf("%s %u  ", MODE[k], cnt[k]);
    printf("\n%s: %u problems\n", bad ? "FAIL" : "ok", bad);
    return bad != 0;
}
