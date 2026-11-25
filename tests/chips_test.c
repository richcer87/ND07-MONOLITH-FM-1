/* SPDX-License-Identifier: GPL-3.0-only
 * 01 ENGINE: the chips' own arithmetic held to the hardware's numbers.
 *   YM2149  a note on its 12-bit tone period at 2 MHz / 16 (C7 is 2083.3 Hz, not 2093); its measured DAC
 *   DMG     a note on its 11-bit period, 131072 / (2048 - x) (C7 is 2080.5 Hz); the noise's clock one of its dividers
 *   YM2612  the two ROMs (the first and last words as decapped), an operator's 6 dB at 64 steps of attenuation, the
 *           DAC's ladder (no step between -1 and +1)
 *   S-DSP   the Gaussian's four taps add up to 2048 (+-1) wherever it reads; BRR filter 1 decodes 15/16 of the last;
 *           a block of a sine coded and decoded comes back within its 4 bits; its 16 kHz output filter at -3 dB
 *   1.9.1   the YM2149's three outputs tied (three channels up: 1.89 x one, MAME's resistor model); the Mega Drive's
 *           model 1 low-pass at ~3.9 kHz; the YM2612's envelope as attenuation (half the amplitude: 64 steps)
 * cc -DDINGHY_HOST -Ifirmware/hal -Ifirmware/src -Ifirmware/gen tests/chips_test.c */
#define main host_main
#include "../firmware/src/nd07.c"
#undef main
#include <math.h>
static int bad;
static void near(const char *what, double got, double want, double tol)
{
    int ok = got >= want - tol && got <= want + tol;
    printf("%s %-44s %.3f (want %.3f)\n", ok ? "ok  " : "FAIL", what, got, want);
    if (!ok) bad++;
}
static double hz(uint32_t inc) { return inc * 44100.0 / 4294967296.0; }
static uint32_t inc_of(double f) { return (uint32_t)(f * 4294967296.0 / 44100.0 + 0.5); }
int main(void)
{
    int32_t k, d, h[4], in[16], out[16], q1 = 0, q2 = 0, worst = 2048;
    double c7 = 2093.005;
    uint32_t tp = grid_div(YM_CLK16, inc_of(c7), 4095u), x = grid_div(DMG_CLK16, inc_of(c7), 2048u);
    near("YM2149 C7: tone period", tp, 60, 0);
    near("YM2149 C7: what it plays (Hz)", hz(YM_CLK16 / tp << 4), 125000.0 / 60, 0.2);
    near("YM2149 lowest: period 4095 (Hz)", hz(YM_CLK16 / grid_div(YM_CLK16, inc_of(10.0), 4095u) << 4), 125000.0 / 4095, 0.01);
    near("YM2149 DAC: step 31 / step 15", YM_LOG[31] / (double)YM_LOG[15], 1.0 / 0.0777752346075, 0.05);
    near("DMG C7: 2048 - x", x, 63, 0);
    near("DMG C7: what it plays (Hz)", hz(DMG_CLK16 / x << 4), 131072.0 / 63, 0.2);
    near("DMG noise at C4: steps a second", dmg_noise(inc_of(261.63)) * 44100.0 / 65536, 524288.0 / 64, 60);
    near("YM2612 logsin[0] (0x859)", OPN_LOGSIN[0], 0x859, 0);
    near("YM2612 exp[255] (0x3FA)", OPN_EXP[255], 0x3FA, 0);
    near("YM2612 operator full", opn_op(0x40000000u, 0, 0), 8168, 0);
    near("YM2612 operator -6 dB (64 steps)", opn_op(0x40000000u, 0, 64), 4084, 0);
    near("YM2612 operator, the other half", opn_op(0xC0000000u, 0, 0), -8168, 0);
    for (d = 0; d < 256; d++) {
        int32_t s = SPC_GAUSS[255 - d] + SPC_GAUSS[511 - d] + SPC_GAUSS[256 + d] + SPC_GAUSS[d];
        if (absi(s - 2048) > absi(worst - 2048)) worst = s;
    }
    near("S-DSP Gaussian: the four taps' sum, worst", worst, 2048, 1);
    h[0] = h[1] = h[2] = h[3] = 10000;
    near("S-DSP Gaussian: a flat line stays flat", spc_gauss(h, 12345), 10000, 12);
    near("S-DSP BRR filter 1: 15/16 of the last", brr_pred(1, 2 * 1600, 0), 1500, 0);
    for (k = 0; k < 16; k++) in[k] = (int32_t)(12000.0 * sin(k * 0.4));
    brr_block(in, out, &q1, &q2, 0);
    for (k = 0, d = 0; k < 16; k++) if (absi(out[k] - in[k]) > d) d = absi(out[k] - in[k]);
    near("S-DSP BRR: a sine block's worst error", d, 0, 12000 / 8);
    {                                           /* the YM2149's resistor network: one channel against three */
        int32_t one = YM_G16[31] - YM_G16[0], y1, y3;
        y1 = (int32_t)(((((uint32_t)one * 16306u) >> 15) << 15) / (uint32_t)(YM_S0 + YM_G0 + one));
        y3 = (int32_t)(((((uint32_t)(3 * one) * 16306u) >> 15) << 15) / (uint32_t)(YM_S0 + YM_G0 + 3 * one));
        near("YM2149 tied outputs: three channels / one", y3 / (double)y1, 1.889, 0.01);
    }
    {                                           /* two filters measured by their gain at one frequency */
        int32_t z[4] = {0, 0, 0, 0}, lp = 0, n;
        double pk = 0, pm = 0;
        for (n = 0; n < 4410; n++) {
            int32_t x16 = (int32_t)(10000.0 * sin(2 * M_PI * 16000.0 * n / 44100.0)), x4 = (int32_t)(10000.0 * sin(2 * M_PI * 3902.0 * n / 44100.0));
            int32_t y16 = snes_lp(z, x16);
            lp += (int32_t)(((int64_t)(x4 - lp) * MD_LP) >> 15);
            if (n > 2000) { if (absi(y16) > pk) pk = absi(y16); if (absi(lp) > pm) pm = absi(lp); }
        }
        near("SNES output filter at 16 kHz (dB)", 20 * log10(pk / 10000.0), -3.0, 0.6);
        near("Mega Drive model 1 low-pass at 3.9 kHz (dB)", 20 * log10(pm / 10000.0), -3.0, 0.8);
    }
    near("YM2612 envelope: full amplitude, attenuation", opn_att(32767), 0, 0);
    near("YM2612 envelope: half amplitude, attenuation", opn_att(16384), 64, 0);
    {                                           /* 1.9.3: an ARP (MAJOR, 3 frames a note) and STEPS become the macros */
        static int16_t q[NP];
        int32_t j, okp = 1;
        static const int8_t W[9] = {0, 0, 0, 4, 4, 4, 7, 7, 7};
        q[P_CARP] = 4; q[P_CARPS] = 3; q[P_CSTEP] = 1;
        drv_macros(q);
        for (j = 0; j < 9; j++) if (q[P_MP0 + j] != W[j]) okp = 0;
        near("driver: ARP MAJOR x3 as PITCH steps", okp, 1, 0);
        near("driver: its length, looped (16 + 9)", q[P_MLP], 25, 0);
        near("driver: STEPS as a one-step VOL", q[P_MLV], 1, 0);
    }
    printf("bad=%d\n", bad);
    return bad != 0;
}
