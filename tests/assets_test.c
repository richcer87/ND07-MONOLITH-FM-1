/* SPDX-License-Identifier: GPL-3.0-only
 * 15 ASSETS: each asset rendered as SEL on its page plays it (4 s, C4): build/host/assets/NN_NAME.wav, and checked:
 * loud enough and not too loud (peak, the loudest 300 ms: about as loud as each other), and as loud again the second time
 * (not bit for bit: filter and smoothing state carries over from the sound before, as it does on the unit).
 * cc -DDINGHY_HOST -Ifirmware/hal -Ifirmware/src -Ifirmware/gen tests/assets_test.c -lm */
#include <math.h>
#define main host_main
#include "../firmware/src/nd07.c"
#undef main
static void wav_out(const char *fn, const int16_t *d, uint32_t n)
{
    FILE *f = fopen(fn, "wb");
    uint32_t sr = 44100, b = n * 2, t;
    uint16_t fmt = 1, ch = 1, ba = 2, bits = 16;
    if (!f) return;
    fwrite("RIFF", 1, 4, f); t = 36 + b; fwrite(&t, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); t = 16; fwrite(&t, 4, 1, f);
    fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); t = sr * 2; fwrite(&t, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&b, 4, 1, f); fwrite(d, 2, n, f); fclose(f);
}
static double measure(const int16_t *out, uint32_t N, double *pkdb);
static uint32_t render(uint32_t k, int16_t *out, uint32_t N)
{
    static int32_t blk[512];
    uint32_t n, i, hold = asset_hold_ms(k), t0;
    asset_load(k);
    for (i = 0; i < NP; i++) { E[i] = P[i]; S8[i] = P[i] * 256; }
    memset(&V, 0, sizeof V); rng_isr = 0x6C8E9CF5u; rng_main = 0x9E3779B9u; rng_hit = 0x2545F491u; k_update(KG_ALL); fx_clear();
    t0 = fm1_ms;
    if (P[P_PMODE]) phys_start(60, 32767, 999u); else note_on(60, 32767);
    for (n = 0; n < N; n += 256) {
        memset(blk, 0, sizeof blk);
        fm1_ms = t0 + n * 1000u / 44100u;
        if (P[P_PMODE]) phys_poll(); else if (hold && fm1_ms - t0 >= hold && v_gate) note_off(60);
        app_audio(blk, 256);
        for (i = 0; i < 256; i++) out[n + i] = (int16_t)clampi(blk[2 * i], -32768, 32767);
    }
    phys_stop(); all_off();
    for (n = 0; n < 44100u * 6u; n += 256) { memset(blk, 0, sizeof blk); app_audio(blk, 256); }   /* (the tails die) */
    return 0;
}
static double measure(const int16_t *out, uint32_t N, double *pkdb)   /* loudest 300 ms of the sounding part, dB */
{
    double pk = 0, best = 0, acc = 0, e = 0;
    uint32_t i, end = 0, len, w = 13230;
    for (i = 0; i < N; i++) { double v = fabs(out[i] / 32768.0); if (v > pk) pk = v; }
    for (i = 0; i < N; i++) { e += fabs(out[i] / 32768.0); if (i >= 441) e -= fabs(out[i - 441] / 32768.0); if (e / 441 > 0.01) end = i; }
    len = end < 2205 ? 2205 : end;   /* (to where a 10 ms envelope falls under -40 dB for good; a sound under 300 ms is its own mean) */
    if (len < w) { for (i = 0; i < len; i++) { double v = out[i] / 32768.0; acc += v * v; } best = acc / len; }
    else for (i = 0; i < len; i++) { double v = out[i] / 32768.0; acc += v * v; if (i >= w) { double o = out[i - w] / 32768.0; acc -= o * o; } if (i >= w - 1 && acc / w > best) best = acc / w; }
    *pkdb = 20 * log10(pk + 1e-9);
    return 10 * log10(best + 1e-12);
}
int main(void)
{
    static int16_t out[44100 * 4];
    uint32_t k, N = 44100 * 4, bad = 0;
    double ld1[NASSET];
    app_init();
    if (system("mkdir -p build/host/assets")) {}
    for (k = 0; k < NASSET; k++) {
        double pkdb, lddb;
        char fn[80], *c;
        render(k, out, N);
        lddb = ld1[k] = measure(out, N, &pkdb);
        snprintf(fn, sizeof fn, "build/host/assets/%02u_%s.wav", k, E_ASSET[k]);
        for (c = fn + 18; *c; c++) if (*c == ' ') *c = '_';
        wav_out(fn, out, N);
        printf("%-8s peak %6.1f dB  loudest 300 ms %6.1f dB  OUT %u %%\n", E_ASSET[k], pkdb, lddb, ASSET_VOL[k]);
        if (pkdb > -1.0 || pkdb < -14.0 || lddb > -13.0 || lddb < -19.0) { printf("  ^ out of range\n"); bad++; }
    }
    for (k = 0; k < NASSET; k++) {
        double pkdb, d;
        render(k, out, N);
        d = measure(out, N, &pkdb) - ld1[k];
        if (d > 0.5 || d < -0.5) { printf("%s: %.1f dB off the second time\n", E_ASSET[k], d); bad++; }
    }
    printf("%s: %u problems\n", bad ? "FAIL" : "ok", bad);
    return bad != 0;
}
