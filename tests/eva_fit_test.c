/* SPDX-License-Identifier: GPL-3.0-only
 * EVA-01 theme: every knob name, every value of every parameter and every page name fits its place (no cut text).
 * cc -DDINGHY_HOST -Ifirmware/hal -Ifirmware/src -Ifirmware/gen tests/eva_fit_test.c */
#define main host_main
#include "../firmware/src/nd07.c"
#undef main
int main(void)
{
    static const char *const MODE[] = {"D", "M+u", "M", "TGT", "M name", "TERM", "CUT", "D word"};
    uint32_t p, m, k, q, cnt[8] = {0}, bad = 0;
    int32_t v;
    char s[40], nb[16];
    const char *u;
    for (p = 0; p < NP; p++) for (m = 0; m < NPAGE; m++) for (k = 0; k < 4; k++) for (q = 0; q < 4; q++) if (PAGE_P[m][k][q] == p) PMOD[p] = (uint8_t)m;
    NTGT = 0;
    for (p = 0; p < NP; p++) if (p != P_SLOT && p != P_L1TGT && p != P_L2TGT && p != P_THEME && PMOD[p] != PG_PAD) TGT[NTGT++] = (uint8_t)p;
    for (m = 0; m < NPAGE; m++) {
        int32_t w = text_w(&FONT_D, PAGE_NAME[m]) <= 194 ? text_w(&FONT_D, PAGE_NAME[m]) : text_w(&FONT_M, PAGE_NAME[m]);
        if (w > 194) { printf("page %u '%s' %d px > 194\n", m + 1, PAGE_NAME[m], w); bad++; }
    }
    for (m = 0; m < NPAGE; m++) for (k = 0; k < 4; k++) for (q = 0; q < 4; q++) {
        p = PAGE_P[m][k][q];
        if (p == NO) continue;
        if (text_w(&FONT_A, PD[p].name) > 55) { printf("label '%s' %d px > 55\n", PD[p].name, text_w(&FONT_A, PD[p].name)); bad++; }
        for (v = PD[p].lo; v <= PD[p].hi; v++) {
            int r;
            fmt_param(p, v, s);
            r = eva_plan(p, s, 56, nb, &u);
            cnt[r]++;
            if (r == 6) { if (bad < 40) printf("value cut: %s = '%s'\n", PD[p].name, s); bad++; }
        }
    }
    for (k = 0; k < 8; k++) printf("%s %u  ", MODE[k], cnt[k]);
    printf("\n%s: %u problems\n", bad ? "FAIL" : "ok", bad);
    return bad != 0;
}
