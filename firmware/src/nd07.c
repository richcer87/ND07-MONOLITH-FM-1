/* SPDX-License-Identifier: GPL-3.0-only */
/* ND-07 // MONOLITH for the M-VAVE FM-1 (Noise Dungeon) - v0.6: a sampler and synth for game-audio SFX design.
 * The REAPER SFX workstation (Lua ReaImGui + JSFX) rebuilt for the FM-1 in integer C (JieLi AC79: no FPU, no libgcc).
 *
 *   01 OSC & SAMPLE      an oscillator (SINE TRI SAW SQUARE PULSE SUPERSAW NOISE + SUB-SAW FORMANT METAL FM PAD SCAN,
 *                        PolyBLEP) and an 8-slot sample bank (factory sounds in flash; record your own into a slot by
 *                        resampling the FM-1's output, kept in flash); OSC only, SAMPLE only, or MIX with a blend
 *   02 RESONATOR         6-mode modal bank (METAL GLASS WOOD STRING BELL DRUM TUBE): the source becomes a struck material
 *   03 AMP ENVELOPE      ADSR on a mono voice (held keys, last-note priority, legato glide), velocity, pitch, out
 *   04 TRANSIENT         transient shaper: ATTACK and SUSTAIN boost / cut (3 envelope followers), TIME, OUT
 *   05 RES MOD FILTER    ZDF SVF 12/24 dB, LP/BP/HP/NOTCH, mod by env follower / LFO / S&H / the amp envelope
 *   06 DESTROY           drive into SOFT / HARD / FOLD / RECTIFY, bit crusher, sample-rate reducer, ring mod, tone, mix
 *   07 GRAIN ENGINE      grain stream on the sample: position, size, density, spray, reverse, freeze
 *   08 OTT MULTIBAND     Ableton's OTT preset / Xfer OTT: LR4 crossovers (88.3 Hz, 2.5 kHz), RMS, 66.7:1 / inf:1 down and
 *                        1:4.17 up per band; DEPTH TIME UPWD DOWNWD IN OUT, band gains, THRESH, SOLO, PRE or POST the FX
 *   09 DELAY + FLANGER   1 s delay (smoothed time: tape glides) and a flanger, four routings
 *   10 MULTIBAND REVERB  predelay, diffusion, a 4-line FDN whose LOW / MID / HIGH bands decay separately
 *   11 LFO x2            shape, rate, mix and a target: any parameter, the other LFO and MORPH included
 *   12 MORPH + BATCH     two stored patches A and B and a MORPH knob between them (locks stay put); VARY: every note a
 *                        little different; BATCH: 4..16 hits in a row with silence between, for one take in REAPER
 *   13 CHAOS PAD + KEYS  an XY pad over MIDI (CC16 X, CC17 Y, CC18 touch: web/pad.html on a phone, or any XY app), each
 *                        axis on any parameter; key velocity and aftertouch from the FM-1's keys (beta, OFF by default)
 *   14 CHIP OSC          8-bit console voices (NES-style pulse 12.5 / 25 / 50 / 75 %, 32-step triangle, LFSR noise long /
 *                        93-step), and the console's 60 Hz frame on any source: pitch SWEEP, ARP, 16-step volume;
 *                        SEL makes a new sound of the PRESET kind (COIN, JUMP, LASER, HIT, POWERUP, EXPLODE, BLIP), sfxr-style
 *   + HAPPY ACCIDENT, parameter locks, 16-step undo, 4 themes, the session in flash.
 *   The chain (mono): SOURCE (osc / sample / grains) x ENV > RESONATOR > TRANSIENT > DESTROY > FILTER > OTT PRE > DLY+FLG >
 *   REVERB > OTT POST > out. Modules at rest cost nothing; after ~1 s of silence the whole chain sleeps.
 *   It runs in 64-sample blocks, module by module; the heavy stages run from RAM (.ram_text) and call nothing.
 *
 * Panel:
 *   HOME ENV EDIT ARP SEQ FX GLO LFO   the modules; press again: the next set of knobs (A/B/C/D), then the button's
 *                                     next module (HOME: 01 02 14, ENV: 03 04, EDIT: 05 06, LFO: 11 12 13)
 *   SELECT                            previous / next page
 *   K1..K4                            the page's parameters (hold SEL + turn: lock / unlock)
 *   ALGORITHM / PRESETS               oscillator wave / sample slot
 *   keys F3..G5                       play (held = gate); OCT- / OCT+ the octave. PLAY: a held C4
 *   REC                               HAPPY ACCIDENT            SEL + REC   UNDO
 *   SAVE                              the session (and a recorded sample) into flash      SEL + SAVE   clear all locks
 *   SEL tap                           the page's action: 01 REC ARM / STOP, 12 BATCH start / stop, the rest randomize
 *   SEL + OCT- / SEL + OCT+           reset the module / next theme (on 12: STORE A / STORE B)
 *   SEL + PLAY                        the slot back to its factory sound
 *   MIDI in                           notes (velocity), pitch bend +-2, CC1 LFO1 mix, CC11 MORPH, CC74 filter freq,
 *                                     CC16 / 17 / 18 the pad's X / Y / touch, channel and key pressure (aftertouch)
 *  * The engine reads every parameter in Q8, glided over ~6 ms (S8): knob turns and LFOs move without steps.
 * Integer only: no float, no 64-bit division (docs/RULES.md). Tables: tools/gen_nd07_tables.py; samples: tools/gen_samples.py */

#define APP_NAME "ND-07"
#define STORE_MAGIC 0x3730444Eu                 /* "ND07" */
#define CV_MAX (240u * 112u)
#if !defined(BOARD_SMK37) || !BOARD_SMK37
#define SLOT_SECT 8u                            /* 32 KiB sample slots: 1.48 s at 22.05 kHz mu-law */
#endif
#include <stdint.h>
static void app_idle(void);
#define LCD_IDLE app_idle
#if !defined(DINGHY_HOST) && !(defined(BOARD_SMK37) && BOARD_SMK37)
#define FM1_ANALOG 1                            /* the key velocity (fm1_input.h; nd07_sense.h): sampled only while KEY VEL is on */
static void sense_sample(uint32_t col, uint32_t rowbit, uint32_t v, uint32_t driven);
static int sense_drive(uint32_t col, uint32_t rowbit);
static int sense_led_ok(uint32_t col, uint32_t rowbit);
#define FM1_INPUT_ANALOG(col, rowbit, v, driven) sense_sample(col, rowbit, v, driven)
#define FM1_INPUT_DRIVE(col, rowbit) sense_drive(col, rowbit)
#define FM1_INPUT_LED_OK(col, rowbit) sense_led_ok(col, rowbit)
#endif
static int app_sysex(const uint8_t *p, uint32_t n);
#define APP_SYSEX app_sysex                     /* the web page's samples (web/index.html), before the updater sees them */
#include "platform.c"
#include "nd07_sense.h"
#include "nd07_tables.h"
#include "nd07_samples.h"

/* The audio engine runs from RAM (.ram_text): no fetches through the XIP flash cache in the hot loop. Code there may
 * call nothing (tools/build.py checks it), so every DSP helper is forced inline into the one RAM function, dsp_block. */
#define DSP static inline __attribute__((always_inline))
#ifdef DINGHY_HOST
#define DSP_RAM static __attribute__((noinline))
#else
#define DSP_RAM static __attribute__((section(".ram_text"), noinline, used))
#endif
/* Flash code (XIP, 0x02xxxxxx) cannot call RAM code (0x01C0xxxx) directly: it is beyond the 23-bit jump range
 * (the linker: "relocation truncated to fit: R_PI32V2_LONG_JUMP_23M2"). RAM_CALL goes through a pointer the compiler
 * cannot fold back into a direct call (as fm1_flash.h's FL_FAR); on the host it is a plain call. */
#ifdef DINGHY_HOST
#define RAM_CALL(fn) fn
#else
static inline void *ram_far(void *p) { void *volatile q = p; return q; }
#define RAM_CALL(fn) ((__typeof__(&fn))ram_far((void *)&fn))
#endif

#define DLY_N 44100u                            /* 1 s */
#define FLG_N 1024u
#define PRE_N 4096u                             /* the reverb runs at 22.05 kHz: 100 ms is 2205 */
#define RV_N 4096u                              /* an FDN line's ring (the longest line: 2393) */
#define SMP_MAX (SLOT_MAX - 4u)
#define ONE16 65536u
#define ENV_MAX (32767 << 8)

typedef struct { uint32_t len; uint8_t d[SMP_MAX]; } smprec_t;
_Static_assert(sizeof(smprec_t) <= SLOT_MAX, "a sample record fills one flash slot at most");
static int16_t dly_buf[DLY_N] __attribute__((section(".pool")));
static int16_t flg_buf[FLG_N] __attribute__((section(".pool")));
static int16_t rv_pre[PRE_N] __attribute__((section(".pool")));
static int16_t rv_line[4][RV_N] __attribute__((section(".pool")));
static int16_t rv_ap1[128] __attribute__((section(".pool")));
static int16_t rv_ap2[256] __attribute__((section(".pool")));
static int16_t smp16[SMP_MAX] __attribute__((section(".pool")));   /* the active slot when it holds a recording: 16-bit */
static smprec_t rec __attribute__((section(".pool")));       /* the recorder, and the flash staging (8-bit mu-law) */

/* ======================================================================== parameters */
enum { P_MODE, P_WAVE, P_SLOT, P_BLEND, P_TUNE, P_COLOR, P_STR, P_SLOOP,
       P_ATT, P_DEC, P_SUS, P_REL, P_GLIDE, P_VELO, P_PITCH, P_VOL,
       P_RFFREQ, P_RFRES, P_RFMODE, P_RFPOLE, P_RFDEP, P_RFRATE, P_RFSRC, P_RFDRV,
       P_GPOS, P_GSIZE, P_GDENS, P_GSPRAY, P_GREV, P_GMODE, P_GFRZ, P_RNDP,
       P_ODEPTH, P_OTIME, P_OUP, P_ODOWN, P_OIN, P_OOUT, P_OSTYLE, P_OPOS, P_OLOW, P_OMID, P_OHIGH, P_OTHR, P_OXLO, P_OXHI, P_OSOLO,
       P_DTIME, P_DFB, P_DMIX, P_DROUTE, P_FRATE, P_FDEPTH, P_FREGEN, P_FMIX,
       P_RSIZE, P_RDEC, P_RPRE, P_RMIX, P_RLOW, P_RHIGH, P_RXLO, P_RXHI,
       P_L1SHP, P_L1RATE, P_L1MIX, P_L1TGT, P_L2SHP, P_L2RATE, P_L2MIX, P_L2TGT,
       P_ZMAT, P_ZPITCH, P_ZDEC, P_ZMIX, P_ZDAMP, P_ZSPRD, P_ZHIT, P_ZTRK,
       P_TATK, P_TSUS, P_TTIME, P_TOUT,
       P_XDRV, P_XSHP, P_XBITS, P_XSR, P_XRING, P_XRMIX, P_XTONE, P_XMIX,
       P_MORPH, P_VARY, P_BMODE, P_BCNT, P_BSPC, P_BGATE,
       P_XTGT, P_XAMT, P_YTGT, P_YAMT, P_PADM, P_KVEL, P_KSENS, P_ATTGT, P_ATAMT,
       P_CDUTY, P_CSWEEP, P_CARP, P_CARPS, P_CSTEP, P_CNOISE, P_CPRE,
       P_THEME, NP };
#define NTGT_N (NP - 13)                        /* the mod targets: all but the slot, the 2 + 3 targets, the 13 page, the theme */
enum { F_INT, F_PCT, F_X100, F_ENUM, F_ATT, F_DEC, F_REL, F_GLIDE, F_RFHZ, F_RATE, F_DBH, F_XLO, F_XHI, F_PM, F_MS, F_DENS,
       F_SPR, F_SLOT, F_SEMI, F_BLEND, F_DTIME, F_FRATE, F_LRATE, F_RDEC, F_TGT, F_OXLO, F_OXHI, F_TSC,
       F_ZDEC, F_BITS, F_XSR, F_XRHZ, F_BSPC };
typedef struct { const char *name; int16_t lo, hi, def; uint8_t step, fmt; const char *const *en; } pdesc_t;
static const char *const E_ONOFF[] = {"OFF", "ON"};
static const char *const E_MODE[] = {"OSC", "SAMPLE", "MIX"};
static const char *const E_WAVE[] = {"SINE", "TRI", "SAW", "SQUARE", "PULSE", "SUPRSAW", "NOISE", "SUB-SAW", "FORMANT", "METALFM", "PADSCAN", "CHIP PLS", "CHIP TRI", "CHIP NSE"};
#define NWAVE 14
enum { W_PADSCAN = 10, W_CPLS, W_CTRI, W_CNSE };
static const char *const E_SLOOP[] = {"1-SHOT", "LOOP"};
static const char *const E_RFMODE[] = {"BYPASS", "LP", "BP", "HP", "NOTCH"};
static const char *const E_POLE[] = {"12dB", "24dB"};
static const char *const E_RFSRC[] = {"ENV FOL", "LFO", "S&H", "AMP ENV"};
static const char *const E_OSTY[] = {"BYPASS", "OTT", "SMOOTH", "PUNCH", "AGGR"};   /* OTT: Ableton's preset / Xfer; the rest: the ND-07 JSFX curves */
static const char *const E_OPOS[] = {"PRE FX", "POST FX"};
static const char *const E_OSOLO[] = {"OFF", "LOW", "MID", "HIGH"};
static const char *const E_ROUTE[] = {"PARALL.", "FLG>DLY", "DLY>FLG", "IN FDBK"};
static const char *const E_LSHP[] = {"SINE", "TRI", "SAW UP", "SAW DN", "SQUARE", "S&H", "DRIFT"};
static const char *const E_THEME[] = {"01 TEST", "02 PROD", "00PROTO", "MONO OG"};
static const char *const E_ZMAT[] = {"METAL", "GLASS", "WOOD", "STRING", "BELL", "DRUM", "TUBE"};
static const char *const E_XSHP[] = {"SOFT", "HARD", "FOLD", "RECTIFY"};
static const char *const E_BMODE[] = {"RANDOM", "A>B", "A>B+RND"};
static const char *const E_PADM[] = {"SPRING", "LATCH", "PLAY"};
static const char *const E_KVEL[] = {"OFF", "VEL", "VEL+AT"};
static const char *const E_CDUTY[] = {"12.5%", "25%", "50%", "75%"};
static const char *const E_CARP[] = {"OFF", "OCT", "5TH", "4TH", "MAJOR", "MINOR", "TRILL", "OCT DN", "UP 4TH", "UP 5TH", "UP OCT"};
#define NARP 11                                 /* (the last three step once and stay: the coin's jump) */
static const char *const E_CSTEP[] = {"OFF", "60 Hz", "30 Hz", "15 Hz"};
static const char *const E_CNOISE[] = {"LONG", "SHORT 93"};
static const char *const E_CPRE[] = {"COIN", "JUMP", "LASER", "HIT", "POWERUP", "EXPLODE", "BLIP"};
static const char *const E_BCNT[] = {"4", "8", "12", "16"};
static const pdesc_t PD[NP] = {
    [P_MODE] = {"SOURCE", 0, 2, 0, 1, F_ENUM, E_MODE},     [P_WAVE] = {"WAVE", 0, NWAVE - 1, 2, 1, F_ENUM, E_WAVE},
    [P_SLOT] = {"SAMPLE", 0, 7, 0, 1, F_SLOT, 0},          [P_BLEND] = {"BLEND", 0, 100, 50, 2, F_BLEND, 0},
    [P_TUNE] = {"TUNE", -24, 24, 0, 1, F_SEMI, 0},         [P_COLOR] = {"COLOR", 0, 100, 50, 1, F_PCT, 0},
    [P_STR] = {"STRETCH", 40, 250, 100, 2, F_X100, 0},     [P_SLOOP] = {"LOOP", 0, 1, 0, 1, F_ENUM, E_SLOOP},
    [P_ATT] = {"ATTACK", 0, 63, 9, 1, F_ATT, 0},           [P_DEC] = {"DECAY", 0, 63, 41, 1, F_DEC, 0},
    [P_SUS] = {"SUSTAIN", 0, 100, 70, 1, F_PCT, 0},        [P_REL] = {"RELEASE", 0, 63, 35, 1, F_REL, 0},
    [P_GLIDE] = {"GLIDE", 0, 63, 0, 1, F_GLIDE, 0},        [P_VELO] = {"VELO", 0, 100, 50, 1, F_PCT, 0},
    [P_PITCH] = {"PITCH", 50, 150, 100, 1, F_X100, 0},     [P_VOL] = {"OUT", 0, 100, 80, 1, F_PCT, 0},
    [P_RFFREQ] = {"FREQ", 0, 63, 45, 1, F_RFHZ, 0},        [P_RFRES] = {"RES", 0, 96, 30, 1, F_PCT, 0},
    [P_RFMODE] = {"MODE", 0, 4, 1, 1, F_ENUM, E_RFMODE},   [P_RFPOLE] = {"SLOPE", 0, 1, 1, 1, F_ENUM, E_POLE},
    [P_RFDEP] = {"DEPTH", -100, 100, 30, 2, F_PM, 0},      [P_RFRATE] = {"RATE", 0, 63, 39, 1, F_RATE, 0},
    [P_RFSRC] = {"MOD SRC", 0, 3, 3, 1, F_ENUM, E_RFSRC},  [P_RFDRV] = {"ANALOG", 0, 100, 20, 1, F_PCT, 0},
    [P_GPOS] = {"POS", 5, 90, 30, 1, F_PCT, 0},            [P_GSIZE] = {"SIZE", 20, 220, 80, 2, F_MS, 0},
    [P_GDENS] = {"DENS", 5, 42, 18, 1, F_DENS, 0},         [P_GSPRAY] = {"SPRAY", 0, 50, 12, 1, F_SPR, 0},
    [P_GREV] = {"REV %", 0, 100, 25, 1, F_PCT, 0},         [P_GMODE] = {"GRAIN", 0, 1, 0, 1, F_ENUM, E_ONOFF},
    [P_GFRZ] = {"FREEZE", 0, 1, 0, 1, F_ENUM, E_ONOFF},    [P_RNDP] = {"RND PIT", 0, 1, 0, 1, F_ENUM, E_ONOFF},
    [P_ODEPTH] = {"DEPTH", 0, 100, 30, 1, F_PCT, 0},       [P_OTIME] = {"TIME", 0, 100, 50, 1, F_TSC, 0},
    [P_OUP] = {"UPWD", 0, 100, 100, 1, F_PCT, 0},          [P_ODOWN] = {"DOWNWD", 0, 100, 100, 1, F_PCT, 0},
    [P_OIN] = {"IN", -48, 48, 0, 1, F_DBH, 0},             [P_OOUT] = {"OUT", -48, 48, 0, 1, F_DBH, 0},
    [P_OSTYLE] = {"STYLE", 0, 4, 1, 1, F_ENUM, E_OSTY},    [P_OPOS] = {"PLACE", 0, 1, 0, 1, F_ENUM, E_OPOS},
    [P_OLOW] = {"LOW", -24, 24, 0, 1, F_DBH, 0},           [P_OMID] = {"MID", -24, 24, 0, 1, F_DBH, 0},
    [P_OHIGH] = {"HIGH", -24, 24, 0, 1, F_DBH, 0},         [P_OTHR] = {"THRESH", -48, 48, 0, 1, F_DBH, 0},
    [P_OXLO] = {"X-LO", 0, 31, 10, 1, F_OXLO, 0},          [P_OXHI] = {"X-HI", 0, 31, 12, 1, F_OXHI, 0},
    [P_OSOLO] = {"SOLO", 0, 3, 0, 1, F_ENUM, E_OSOLO},
    [P_DTIME] = {"TIME", 0, 63, 50, 1, F_DTIME, 0},        [P_DFB] = {"FDBK", 0, 95, 35, 1, F_PCT, 0},
    [P_DMIX] = {"DLY MIX", 0, 100, 15, 1, F_PCT, 0},       [P_DROUTE] = {"ROUTE", 0, 3, 0, 1, F_ENUM, E_ROUTE},
    [P_FRATE] = {"RATE", 0, 63, 21, 1, F_FRATE, 0},        [P_FDEPTH] = {"DEPTH", 0, 100, 50, 1, F_PCT, 0},
    [P_FREGEN] = {"REGEN", -95, 95, 40, 1, F_PM, 0},       [P_FMIX] = {"FLG MIX", 0, 100, 0, 1, F_PCT, 0},
    [P_RSIZE] = {"SIZE", 0, 100, 60, 1, F_PCT, 0},         [P_RDEC] = {"DECAY", 0, 63, 35, 1, F_RDEC, 0},
    [P_RPRE] = {"PRE-DLY", 0, 100, 20, 1, F_MS, 0},        [P_RMIX] = {"REV MIX", 0, 100, 18, 1, F_PCT, 0},
    [P_RLOW] = {"LOW x", 25, 200, 100, 5, F_PCT, 0},       [P_RHIGH] = {"HIGH x", 25, 200, 60, 5, F_PCT, 0},
    [P_RXLO] = {"X-LO", 0, 31, 14, 1, F_XLO, 0},           [P_RXHI] = {"X-HI", 0, 31, 15, 1, F_XHI, 0},
    [P_L1SHP] = {"SHAPE", 0, 6, 0, 1, F_ENUM, E_LSHP},     [P_L1RATE] = {"RATE", 0, 63, 43, 1, F_LRATE, 0},
    [P_L1MIX] = {"MIX", 0, 100, 0, 1, F_PCT, 0},           [P_L1TGT] = {"TARGET", 0, NTGT_N - 1, 13, 1, F_TGT, 0},
    [P_L2SHP] = {"SHAPE", 0, 6, 1, 1, F_ENUM, E_LSHP},     [P_L2RATE] = {"RATE", 0, 63, 25, 1, F_LRATE, 0},
    [P_L2MIX] = {"MIX", 0, 100, 0, 1, F_PCT, 0},           [P_L2TGT] = {"TARGET", 0, NTGT_N - 1, 15, 1, F_TGT, 0},
    [P_ZMAT] = {"MATERL", 0, 6, 0, 1, F_ENUM, E_ZMAT},     [P_ZPITCH] = {"PITCH", -24, 24, 0, 1, F_SEMI, 0},
    [P_ZDEC] = {"DECAY", 0, 63, 34, 1, F_ZDEC, 0},         [P_ZMIX] = {"RES MIX", 0, 100, 0, 1, F_PCT, 0},
    [P_ZDAMP] = {"DAMP", 0, 100, 35, 1, F_PCT, 0},         [P_ZSPRD] = {"SPREAD", 0, 100, 50, 1, F_PCT, 0},
    [P_ZHIT] = {"HIT POS", 2, 50, 13, 1, F_PCT, 0},        [P_ZTRK] = {"KEYTRK", 0, 1, 1, 1, F_ENUM, E_ONOFF},
    [P_TATK] = {"ATTACK", -100, 100, 0, 2, F_PM, 0},       [P_TSUS] = {"SUSTAIN", -100, 100, 0, 2, F_PM, 0},
    [P_TTIME] = {"TIME", 0, 100, 50, 1, F_TSC, 0},         [P_TOUT] = {"OUT", -48, 48, 0, 1, F_DBH, 0},
    [P_XDRV] = {"DRIVE", 0, 72, 0, 1, F_DBH, 0},           [P_XSHP] = {"SHAPE", 0, 3, 0, 1, F_ENUM, E_XSHP},
    [P_XBITS] = {"BITS", 2, 16, 16, 1, F_BITS, 0},         [P_XSR] = {"RATE", 0, 63, 0, 1, F_XSR, 0},
    [P_XRING] = {"RING HZ", 0, 63, 30, 1, F_XRHZ, 0},      [P_XRMIX] = {"RNG MIX", 0, 100, 0, 1, F_PCT, 0},
    [P_XTONE] = {"TONE", 0, 63, 63, 1, F_RFHZ, 0},         [P_XMIX] = {"MIX", 0, 100, 100, 1, F_PCT, 0},
    [P_MORPH] = {"MORPH", 0, 100, 0, 1, F_PCT, 0},         [P_VARY] = {"VARY", 0, 100, 0, 1, F_PCT, 0},
    [P_BMODE] = {"BATCH", 0, 2, 0, 1, F_ENUM, E_BMODE},    [P_BCNT] = {"COUNT", 0, 3, 1, 1, F_ENUM, E_BCNT},
    [P_BSPC] = {"SPACE", 0, 15, 0, 1, F_BSPC, 0},          [P_BGATE] = {"GATE", 5, 90, 25, 1, F_PCT, 0},
    [P_XTGT] = {"X TGT", 0, NTGT_N - 1, 15, 1, F_TGT, 0},  [P_XAMT] = {"X AMT", -100, 100, 100, 2, F_PM, 0},
    [P_YTGT] = {"Y TGT", 0, NTGT_N - 1, 16, 1, F_TGT, 0},  [P_YAMT] = {"Y AMT", -100, 100, 60, 2, F_PM, 0},
    [P_PADM] = {"PAD", 0, 2, 0, 1, F_ENUM, E_PADM},        [P_KVEL] = {"KEY VEL", 0, 2, 0, 1, F_ENUM, E_KVEL},
    [P_KSENS] = {"SENS", 0, 100, 50, 1, F_PCT, 0},         [P_ATTGT] = {"AT TGT", 0, NTGT_N - 1, 15, 1, F_TGT, 0},
    [P_ATAMT] = {"AT AMT", -100, 100, 0, 2, F_PM, 0},
    [P_CDUTY] = {"DUTY", 0, 3, 2, 1, F_ENUM, E_CDUTY},     [P_CSWEEP] = {"SWEEP", -100, 100, 0, 2, F_PM, 0},
    [P_CARP] = {"ARP", 0, NARP - 1, 0, 1, F_ENUM, E_CARP},        [P_CARPS] = {"ARP SPD", 1, 12, 4, 1, F_INT, 0},
    [P_CSTEP] = {"STEPS", 0, 3, 0, 1, F_ENUM, E_CSTEP},    [P_CNOISE] = {"NOISE", 0, 1, 0, 1, F_ENUM, E_CNOISE},
    [P_CPRE] = {"PRESET", 0, 6, 0, 1, F_ENUM, E_CPRE},
    [P_THEME] = {"THEME", 0, 3, 0, 1, F_ENUM, E_THEME},
};
static volatile int16_t P[NP];                  /* as set */
static volatile int16_t E[NP];                  /* as heard: P plus the LFOs, rounded (for the screen) */
static volatile int32_t S8[NP];                 /* as heard, Q8 and smoothed: the engine reads this (no zipper, fine LFO steps) */
static uint8_t CONT[NP];                        /* a continuous parameter: smoothed (enums jump) */
static uint32_t locks[4];                       /* (NP <= 128) bit p: locked (mutations and MORPH leave it alone) */
static int lock_get(uint32_t p) { return (int)(locks[p >> 5] >> (p & 31u) & 1u); }

enum { PG_SRC, PG_RES, PG_ENV, PG_TRN, PG_FLT, PG_DST, PG_GRN, PG_OTT, PG_DLY, PG_REV, PG_LFO, PG_MRP, PG_PAD, PG_CHIP, NPAGE };
#define NO 255u                                 /* (an empty knob) */
static const uint8_t PAGE_P[NPAGE][4][4] = {    /* [module][A B C D][knob] */
    {{P_MODE, P_WAVE, P_SLOT, P_BLEND}, {P_TUNE, P_COLOR, P_STR, P_SLOOP}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_ZMAT, P_ZPITCH, P_ZDEC, P_ZMIX}, {P_ZDAMP, P_ZSPRD, P_ZHIT, P_ZTRK}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_ATT, P_DEC, P_SUS, P_REL}, {P_GLIDE, P_VELO, P_PITCH, P_VOL}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_TATK, P_TSUS, P_TTIME, P_TOUT}, {NO, NO, NO, NO}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_RFFREQ, P_RFRES, P_RFMODE, P_RFPOLE}, {P_RFDEP, P_RFRATE, P_RFSRC, P_RFDRV}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_XDRV, P_XSHP, P_XBITS, P_XSR}, {P_XRING, P_XRMIX, P_XTONE, P_XMIX}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_GPOS, P_GSIZE, P_GDENS, P_GSPRAY}, {P_GREV, P_GMODE, P_GFRZ, P_RNDP}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_ODEPTH, P_OTIME, P_OUP, P_ODOWN}, {P_OIN, P_OOUT, P_OSTYLE, P_OPOS}, {P_OLOW, P_OMID, P_OHIGH, P_OTHR}, {P_OXLO, P_OXHI, P_OSOLO, NO}},
    {{P_DTIME, P_DFB, P_DMIX, P_DROUTE}, {P_FRATE, P_FDEPTH, P_FREGEN, P_FMIX}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_RSIZE, P_RDEC, P_RPRE, P_RMIX}, {P_RLOW, P_RHIGH, P_RXLO, P_RXHI}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_L1SHP, P_L1RATE, P_L1MIX, P_L1TGT}, {P_L2SHP, P_L2RATE, P_L2MIX, P_L2TGT}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_MORPH, P_VARY, P_BMODE, P_BCNT}, {P_BSPC, P_BGATE, NO, NO}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_XTGT, P_XAMT, P_YTGT, P_YAMT}, {P_PADM, P_KVEL, P_KSENS, NO}, {P_ATTGT, P_ATAMT, NO, NO}, {NO, NO, NO, NO}},
    {{P_CDUTY, P_CSWEEP, P_CARP, P_CARPS}, {P_CSTEP, P_CNOISE, P_CPRE, NO}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
};
static uint8_t NSUB[NPAGE];                     /* how many of A B C D a page has */
static const char *const PAGE_NAME[NPAGE] = {"OSC & SAMPLE", "RESONATOR", "AMP ENVELOPE", "TRANSIENT", "RES MOD FILTER",
                                             "DESTROY", "GRAIN ENGINE", "OTT MULTIBAND", "DELAY + FLANGER",
                                             "MULTIBAND REVERB", "LFO x2", "MORPH + BATCH", "CHAOS PAD + KEYS", "CHIP OSC"};
#define NGRP 8
static const uint8_t GRP_BTN[NGRP] = {BTN_HOME, BTN_ENV, BTN_EDIT, BTN_ARP, BTN_SEQ, BTN_FX, BTN_GLO, BTN_LFO};
static const uint8_t PAGE_GRP[NPAGE] = {0, 0, 1, 1, 2, 2, 3, 4, 5, 6, 7, 7, 7, 0};   /* the button each module is under */
static uint8_t PMOD[NP];                        /* the page of each parameter */
static uint8_t TGT[NP], NTGT;                   /* the mod targets (LFOs, pad, aftertouch): see NTGT_N */
/* 13 the CHAOS PAD (MIDI CC16 / 17 / 18) and the aftertouch (the keys, MIDI pressure): mod sources, 0..127 */
static volatile uint8_t pad_x = 64, pad_y = 64, pad_on, pad_held, at_keys, at_midi;

static uint32_t rng_main = 0x9E3779B9u, rng_isr = 0x6C8E9CF5u;
DSP uint32_t xrand(uint32_t *s) { uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return *s = x; }
static int32_t rnd_range(int32_t lo, int32_t hi) { return hi <= lo ? lo : lo + (int32_t)(xrand(&rng_main) % (uint32_t)(hi - lo + 1)); }
DSP int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
DSP int32_t absi(int32_t v) { return v < 0 ? -v : v; }
DSP int16_t sat16(int32_t v) { return (int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v); }

/* ======================================================================== DSP helpers */
DSP int32_t isin(uint32_t ph)
{
    uint32_t i = ph >> 22, fr = (ph >> 6) & 0xFFFFu;
    int32_t a = T_SIN[i], b = T_SIN[i + 1u];
    return a + (((b - a) * (int32_t)fr) >> 16);
}
DSP int32_t tab_odd(const uint16_t *t, int32_t x, uint32_t shift)
{
    uint32_t ax = (uint32_t)absi(x), i = ax >> shift, fr;
    int32_t y;
    if (i >= 1024u) y = t[1024];
    else { fr = ax & ((1u << shift) - 1u); y = t[i] + (int32_t)((((int32_t)t[i + 1u] - (int32_t)t[i]) * (int32_t)fr) >> shift); }
    return x < 0 ? -y : y;
}
DSP int32_t tanh_q(int32_t x) { return tab_odd(T_TANH, x, 7); }
DSP int32_t softk_q(int32_t x) { return tab_odd(T_SOFTK, x, 7); }
DSP int32_t herm16(const int16_t *b, int32_t i1, uint32_t fr, int32_t len)   /* 4-point Hermite, frac Q12 */
{
    int32_t i0 = i1 > 0 ? i1 - 1 : 0, i2 = i1 + 1 < len ? i1 + 1 : len - 1, i3 = i1 + 2 < len ? i1 + 2 : len - 1;
    int32_t y0, y1, y2, y3, c1, c2, c3, f = (int32_t)fr;
    if (i1 < 0) i1 = 0;
    if (i1 > len - 1) i1 = len - 1;
    y0 = b[i0]; y1 = b[i1]; y2 = b[i2]; y3 = b[i3];
    c1 = (y2 - y0) >> 1;
    c2 = y0 - ((5 * y1) >> 1) + 2 * y2 - (y3 >> 1);
    c3 = ((y3 - y0) >> 1) + ((3 * (y1 - y2)) >> 1);
    return ((((((c3 * f) >> 12) + c2) * f >> 12) + c1) * f >> 12) + y1;
}
DSP int32_t ilog2_q8(uint32_t v)              /* log2, Q8 (a binary search: no loop of 31) */
{
    int32_t n = 0;
    uint32_t m, x = v;
    if (!v) return 0;
    if (x >> 16) { x >>= 16; n += 16; }
    if (x >> 8) { x >>= 8; n += 8; }
    if (x >> 4) { x >>= 4; n += 4; }
    if (x >> 2) { x >>= 2; n += 2; }
    if (x >> 1) n += 1;
    m = n >= 8 ? (v >> (n - 8)) & 255u : (v << (8 - n)) & 255u;
    return n * 256 + T_LOG2F[m];
}
static uint32_t isqrt32(uint32_t v)             /* floor(sqrt v), bit by bit */
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1;
        b >>= 2;
    }
    return r;
}
static int32_t sin_q30(uint32_t ph)             /* sin, Q30, for 0 <= ph < 2^31; small angles exact (the resonator's lows) */
{
    if (ph < 0x08000000u) {                     /* < pi/16: x - x^3/6 */
        int32_t x = (int32_t)(((uint64_t)ph * 1686629713u) >> 30), x3 = (int32_t)(((int64_t)(((int64_t)x * x) >> 30) * x) >> 30);
        return x - x3 / 6;
    }
    return isin(ph) << 15;
}
DSP int32_t onepole(int32_t *st, int32_t x, uint32_t c16) { *st += (int32_t)(((int64_t)c16 * (x - *st)) >> 16); return *st; }
DSP int32_t dcblock(int32_t x, int32_t *x1, int32_t *y8)   /* 18 Hz high-pass, its state in Q8 and rounded: a plain
                                                                 >> floors, and the state then sticks at a negative value */
{
    int32_t y = ((x - *x1) << 8) + (int32_t)(((int64_t)*y8 * DC_C_Q15) >> 15);
    *x1 = x;
    *y8 = y;
    return (y + 128) >> 8;
}
static uint32_t alpha_ms_x10(uint32_t t)        /* t in 0.1 ms -> one-pole alpha Q16 */
{
    int32_t i;
    if (t < 5u) t = 5u;
    i = ((ilog2_q8(t) - ilog2_q8(5u)) * 16) >> 8;
    return T_ALPHA[clampi(i, 0, 255)];
}
DSP uint8_t ulaw_enc(int32_t v)
{
    uint32_t sign = v < 0 ? 0x80u : 0u, u, ex = 7u, mask = 0x4000u;
    u = (uint32_t)(v < 0 ? -v : v);
    if (u > 32635u) u = 32635u;
    u += 0x84u;
    while (ex > 0u && !(u & mask)) { ex--; mask >>= 1; }
    return (uint8_t)~(sign | ex << 4 | ((u >> (ex + 3u)) & 15u));
}

/* ======================================================================== the sample bank */
typedef struct { uint8_t user, dirty; uint16_t len; } slotinfo_t;
static slotinfo_t SL[8];
static const int16_t *volatile smp_ptr = FSMP_0;
static volatile uint32_t smp_len = sizeof FSMP_0 / 2u;
static volatile uint8_t smp_busy;
static uint8_t smp_view[228];                   /* the active sample as peak columns */
static int32_t cur_slot = -1;

static void smp_overview(void)
{
    uint32_t col, i, len = smp_len;
    for (col = 0; col < 228u; col++) {
        uint32_t a = col * len / 228u, b = (col + 1u) * len / 228u, pk = 0;
        for (i = a; i < b; i++) { uint32_t v = (uint32_t)absi(smp_ptr[i]); if (v > pk) pk = v; }
        smp_view[col] = (uint8_t)(pk >> 8);
    }
}
static void say(const char *s);
static int rec_state_busy(void);
static void slot_flush(int32_t i)               /* a recording into its flash slot, as mu-law (the sound pauses ~0.4 s) */
{
    uint32_t k;
    if (i < 0 || !SL[i].dirty || rec_state_busy()) return;
    for (k = 0; k < SL[i].len; k++) rec.d[k] = ulaw_enc(smp16[k]);
    rec.len = SL[i].len;
    say(slot_save((uint32_t)i, &rec, sizeof rec) ? "SAMPLE SAVE FAILED" : "SAMPLE SAVED TO FLASH");
    SL[i].dirty = 0;
}
static void slot_select(int32_t i)
{
    uint32_t k;
    slot_flush(cur_slot);
    smp_busy = 1;
    if (SL[i].user && !rec_state_busy() && slot_load((uint32_t)i, &rec, sizeof rec) == 0 && rec.len >= 256u && rec.len <= SMP_MAX) {
        for (k = 0; k < rec.len; k++) smp16[k] = T_ULAW[rec.d[k]];
        smp_ptr = smp16; smp_len = rec.len; SL[i].len = (uint16_t)rec.len;
    } else {
        if (!rec_state_busy()) SL[i].user = 0;
        smp_ptr = FSMP[i]; smp_len = FSMP_LEN[i];
    }
    smp_busy = 0;
    cur_slot = i;
    smp_overview();
}

/* ======================================================================== coefficients (k_update, from E) */
static struct {
    uint8_t mode, wave, sloop, unity, gmode, gfrz, rfmode, rfpole, rfsrc, mbcrv, route;
    uint8_t c_on, c_arp, c_arps, c_step, c_short;  /* 14 CHIP: the frame clock runs; ARP; frames a step; frames a volume step */
    int32_t c_sw;                               /* SWEEP: 1/4096 octave a frame */
    uint32_t c_pw;                              /* the pulse's duty */
    int32_t wo, ws, color;                      /* blend weights, COLOR Q15 */
    uint32_t tr_ratio, glide_a, sc_step;
    int32_t a_inc, sus23, velg;
    uint32_t d_a, r_a;
    uint32_t g_len, dens_q16, gspray, grev, gpos;
    int32_t rf_li, rf_k, rf_dep, rf_drv1, rf_drv2, rf_envk;
    uint32_t rf_lfo_inc;
    uint8_t ott, opost, osolo;
    int32_t ox[2][3];                           /* the two crossovers: a1 a2 a3 (Butterworth, LR4 by two in a row) */
    int32_t o_dep, o_up, o_down, o_in, o_thr, o_band[3], o_outlin, o_dnT[3], o_upT[3], o_dnK[3], o_upK[3], o_inG, o_outG[3], o_maxup;
    uint32_t o_att[3], o_rel[3], o_rms;
    uint32_t d_tq8, f_inc, f_sw;
    int32_t dfb, dmix, fregen, fmix;
    uint32_t rv_len[4], rv_pre, rv_cl, rv_ch;
    int32_t rv_g[4][3], rv_wet, rv_dry, gain;
    uint8_t zn, t_on, x_on, x_shp, x_bits;   /* 02 resonator: active modes; 04, 06 on; 06 shape, bits */
    int32_t za1[6], za2[6], zg[6], zmix;         /* 02: r cos w, r^2 (Q30), input gain (Q15) */
    uint32_t z_ratio;                           /* (the pitch the modes were tuned for) */
    int32_t t_atk, t_sus, t_out;                /* 04 */
    uint32_t t_fa, t_fr, t_sa, t_sr;
    int32_t x_drv, x_rmix, x_mix;               /* 06 */
    uint32_t x_srinc, x_rinc, x_tc;
} K;
static volatile int32_t v_note = 60, v_gate, v_vel = 25800;
static volatile uint32_t v_seq, v_pseq;
static volatile uint32_t rnd_q16 = ONE16, bend_q16 = ONE16;
static const uint16_t RV_BASE[4] = {797, 1039, 1343, 1709};   /* half-rate samples */

static int32_t rv_gain(uint32_t len, uint32_t t60ms)   /* the per-pass gain of a line for a T60: 2^(-3 log2(10) len / (fs T60)) */
{
    uint32_t x = 40820u * len / (t60ms * 441u / 10u + 1u), n = x >> 12, g;
    if (n >= 15u) return 0;
    g = (0x80000000u / T_EXP2[(x & 4095u) >> 4]) >> n;
    return (int32_t)(g > 32440u ? 32440u : g);
}
static uint32_t exp2_q16(int32_t e)             /* 2^(e / 256), Q16 */
{
    int32_t n = e >> 8;
    uint32_t r = T_EXP2[e & 255];
    return n >= 0 ? r << (n > 14 ? 14 : n) : r >> (-n > 31 ? 31 : -n);
}
static int32_t ei(uint32_t p) { return S8[p] >> 8; }   /* the parameter, whole */
static int32_t e8(uint32_t p) { return S8[p]; }        /* the parameter, Q8 */
static int32_t tab8(const uint16_t *t, uint32_t p)    /* a table at a fractional index (the parameter, 0..hi) */
{
    int32_t v = S8[p] < 0 ? 0 : S8[p], i = v >> 8, f = v & 255;
    if (i >= PD[p].hi) return t[PD[p].hi];
    return t[i] + ((((int32_t)t[i + 1] - (int32_t)t[i]) * f) >> 8);
}
static void bw_coef(int32_t *c, uint32_t li)   /* a Butterworth (k = sqrt 2) ZDF SVF at a log-frequency index */
{
    int32_t g = (int32_t)T_G[li], den = 65536 + (int32_t)(((int64_t)g * (g + SQRT2_Q16)) >> 16);
    c[0] = (int32_t)(0xFFFFFFFFu / (uint32_t)den);
    c[1] = (int32_t)(((int64_t)g * c[0]) >> 16);
    c[2] = (int32_t)(((int64_t)g * c[1]) >> 16);
}
typedef struct { int16_t dnT[3], upT[3]; uint16_t dnK[3], upK[3]; int16_t inG, outG[3], maxup; uint16_t att[3], rel[3]; } ostyle_t;
static const ostyle_t OSTY[4] = {               /* dB Q8, (1 - 1/ratio) Q8, times in 0.1 ms; bands L M H */
    /* OTT: Ableton's preset (Xfer OTT): RMS, 66.7:1 / inf:1 down, 1:4.17 up, in +5.2 dB, out +10.3 / +5.7 / +10.3 dB */
    {{-8653, -7757, -9088}, {-10445, -10701, -10445}, {252, 252, 256}, {195, 195, 195}, 1331, {2637, 1459, 2637}, 7680, {478, 224, 135}, {2820, 2820, 1320}},
    /* SMOOTH, PUNCH, AGGR: the ND-07 JSFX's own curves */
    {{-3584, -3840, -4096}, {-9216, -9472, -9728}, {183, 183, 189}, {140, 145, 152}, 0, {348, 410, 471}, 2816, {140, 90, 40}, {1300, 850, 450}},
    {{-3584, -3840, -4096}, {-8704, -8960, -9216}, {209, 209, 214}, {181, 184, 189}, 0, {609, 717, 824}, 3968, {140, 90, 40}, {1300, 850, 450}},
    {{-3584, -3840, -4096}, {-8064, -8320, -8576}, {226, 226, 229}, {203, 205, 208}, 0, {870, 1024, 1178}, 5120, {140, 90, 40}, {1300, 850, 450}},
};
/* 02 RESONATOR: the materials, 6 modes each: frequency ratio (Q10), level (Q15); and how much faster their highs die */
static const uint16_t ZRAT[7][6] = {
    {1024, 2822, 5534, 9147, 13660, 19087},     /* METAL: a free bar */
    {1024, 2376, 4352, 6789, 9605, 12718},      /* GLASS */
    {1024, 2632, 4106, 5591, 7188, 8888},       /* WOOD */
    {1024, 2048, 3072, 4096, 5120, 6144},       /* STRING: harmonic */
    {512, 1024, 1211, 1542, 2048, 2574},        /* BELL: hum, prime, minor third, fifth, nominal, ... */
    {1024, 1632, 2187, 2351, 2717, 2988},       /* DRUM: a circular membrane */
    {1024, 3072, 5120, 7168, 9216, 11264},      /* TUBE: odd harmonics */
};
static const uint16_t ZAMP[7][6] = {
    {32767, 22938, 16384, 11469, 8192, 5898}, {32767, 26214, 19661, 14746, 9830, 6554}, {32767, 18022, 13107, 8192, 4915, 3277},
    {32767, 16384, 10813, 8192, 6554, 5571}, {19661, 32767, 26214, 19661, 16384, 11469}, {32767, 19661, 14746, 13107, 9830, 8192},
    {32767, 16384, 11469, 8192, 6554, 4915},
};
static const uint8_t ZDMP[7] = {77, 38, 255, 51, 64, 154, 102};   /* Q8 */
static uint32_t zdec_ms(int32_t v8) { return (20u * exp2_q16((v8 * 35127) >> 18)) >> 16; }   /* 0..63 -> 20 ms .. 8 s */
static void reso_coef(void)
{
    uint32_t mat = (uint32_t)clampi(ei(P_ZMAT), 0, 6), k, base, inc0, spr, q, d;
    int32_t pz8 = e8(P_ZPITCH), si = clampi(pz8 >> 8, -48, 47), sf = pz8 & 255, hit = e8(P_ZHIT);
    uint32_t semi = T_SEMI[si + 48] + (((T_SEMI[si + 49] - T_SEMI[si + 48]) * (uint32_t)sf) >> 8);
    base = ei(P_ZTRK) ? (uint32_t)(((uint64_t)C4_INC * K.tr_ratio) >> 16) : C4_INC >> 1;   /* the note, or C3 */
    inc0 = (uint32_t)(((uint64_t)base * semi) >> 16);
    spr = 128u + (uint32_t)e8(P_ZSPRD) / 100u;  /* SPREAD: the distance from 1, x0.5 .. x1.5 (Q8) */
    d = 168190000u / (zdec_ms(e8(P_ZDEC)) + 1u);   /* 1 - r, Q30: 6.908 / (fs T60) */
    q = (uint32_t)e8(P_ZDAMP) * ZDMP[mat] / 25600u;   /* Q8 */
    K.z_ratio = K.tr_ratio;
    K.zn = 0;
    for (k = 0; k < 6u; k++) {
        int32_t rq = 1024 + (((int32_t)ZRAT[mat][k] - 1024) * (int32_t)spr >> 8), s, s2, cw;
        uint64_t inc = (uint64_t)inc0 * (uint32_t)(rq < 64 ? 64 : rq) >> 10;
        uint32_t dk, r, sq, sw, amp, hw;
        if (inc >= 1750000000u) break;          /* above ~18 kHz: no more modes */
        dk = d + (uint32_t)(((uint64_t)d * q * (uint32_t)(rq > 1024 ? rq - 1024 : 0)) >> 18);   /* DAMP: the highs die sooner */
        if (dk > (1u << 27)) dk = 1u << 27;
        r = (1u << 30) - dk;
        s = sin_q30((uint32_t)inc >> 1);        /* cos w = 1 - 2 sin^2(w/2): exact for the low modes */
        s2 = (int32_t)(((int64_t)s * s) >> 30);
        cw = (int32_t)(1u << 30) - 2 * s2;
        K.za1[k] = (int32_t)(((int64_t)r * cw) >> 30);
        K.za2[k] = (int32_t)(((uint64_t)r * r) >> 30);
        sq = isqrt32(dk);                       /* sqrt(1 - r), Q15: the same loudness for any decay (noise in) */
        sw = (uint32_t)isin((uint32_t)inc);
        hw = (uint32_t)absi(isin((k + 1u) * (uint32_t)hit * 83886u));   /* HIT POS: sin(pi (k+1) pos) (hit: % Q8) */
        amp = (uint32_t)ZAMP[mat][k] * (9830u + (hw * 22937u >> 15)) >> 15;
        K.zg[k] = (int32_t)((((sq * sw) >> 15) * amp) >> 14);
        K.zn = (uint8_t)(k + 1u);
    }
}
enum { KG_SRC = 1u << PG_SRC, KG_ENV = 1u << PG_ENV, KG_RES = 1u << PG_RES, KG_TRN = 1u << PG_TRN, KG_FLT = 1u << PG_FLT,
       KG_DST = 1u << PG_DST, KG_GRN = 1u << PG_GRN, KG_OTT = 1u << PG_OTT, KG_DLY = 1u << PG_DLY, KG_REV = 1u << PG_REV,
       KG_CHIP = 1u << PG_CHIP, KG_NOTE = 1u << 16, KG_ALL = 0xFFFFFFFFu };
static void k_update(uint32_t chg)              /* the coefficients; the costly groups only when their knobs moved */
{
    uint32_t str = (uint32_t)ei(P_STR), i, gl, mid, sc, old_ratio = K.tr_ratio;
    int32_t semi8 = (v_note - 60) * 256 + e8(P_TUNE), si = clampi(semi8 >> 8, -48, 47), sf = semi8 & 255;
    uint64_t r;
    if (chg & KG_CHIP) {
        int32_t sw = ei(P_CSWEEP);
        K.c_sw = sw * (sw < 0 ? -sw : sw) * 16 / 150;   /* squared: fine near 0, up to 2/3 octave a frame (Q4) */
        K.c_arp = (uint8_t)(ei(P_CARP) < NARP ? ei(P_CARP) : 0); K.c_arps = (uint8_t)ei(P_CARPS);
        K.c_step = (uint8_t)(ei(P_CSTEP) ? 1u << (ei(P_CSTEP) - 1) : 0u);
        K.c_short = (uint8_t)ei(P_CNOISE);
        K.c_pw = 0x20000000u << ei(P_CDUTY);    /* 1/8, 1/4, 1/2 */
        if (ei(P_CDUTY) == 3) K.c_pw = 0xC0000000u;   /* 3/4: the 1/4 upside down (the NES's fourth) */
        K.c_on = K.c_sw || K.c_arp || K.c_step;
    }
    if (chg & KG_SRC) {
        K.mode = (uint8_t)ei(P_MODE); K.wave = (uint8_t)ei(P_WAVE); K.sloop = (uint8_t)ei(P_SLOOP);
        K.color = e8(P_COLOR) * 327 >> 8;
        K.ws = K.mode == 0 ? 0 : K.mode == 1 ? 32767 : e8(P_BLEND) * 327 >> 8;
        K.wo = K.mode == 1 ? 0 : 32767 - (K.mode == 2 ? K.ws : 0);
        K.unity = str >= 98u && str <= 102u;
        K.sc_step = 204800u * 256u / (uint32_t)(e8(P_STR) < 40 * 256 ? 40 * 256 : e8(P_STR));
    }
    r = T_SEMI[si + 48] + (((T_SEMI[si + 49] - T_SEMI[si + 48]) * (uint32_t)sf) >> 8);   /* tune: fractional semitones */
    r = r * ((uint32_t)e8(P_PITCH) * 256u / 100u) >> 16;                                  /* pitch, Q8 % */
    r = (r * rnd_q16 >> 16) * bend_q16 >> 16;
    K.tr_ratio = (uint32_t)(r > 0x7FFFFFu ? 0x7FFFFFu : r);
    if (K.tr_ratio != old_ratio) chg |= KG_NOTE;
    if (chg & (KG_ENV | KG_NOTE)) K.velg = 32767 - ((e8(P_VELO) >> 8) * (32767 - v_vel)) / 100;
    if (chg & KG_ENV) {
        gl = (uint32_t)tab8(T_GLIDE_MS, P_GLIDE);
        K.glide_a = gl ? alpha_ms_x10(gl * 10u / 3u) : 0u;
        K.a_inc = ENV_MAX / (int32_t)((uint32_t)tab8(T_ATT_MS, P_ATT) * 441u / 10u + 1u);
        if (K.a_inc < 1) K.a_inc = 1;
        K.d_a = alpha_ms_x10((uint32_t)tab8(T_DEC_MS, P_DEC) * 10u / 4u);
        K.r_a = alpha_ms_x10((uint32_t)tab8(T_REL_MS, P_REL) * 10u / 4u);
        K.sus23 = e8(P_SUS) * 327;
        K.gain = e8(P_VOL) * 327 >> 8;
    }
    i = (uint32_t)K.zmix;
    K.zmix = e8(P_ZMIX) * 327 >> 8;
    if (K.zmix && ((chg & (KG_RES | KG_NOTE)) || !i)) reso_coef();   /* (off: nothing to tune) */
    /* 04 TRANSIENT */
    if (chg & KG_TRN) {
        K.t_atk = ei(P_TATK); K.t_sus = ei(P_TSUS);
        K.t_out = e8(P_TOUT) >> 1;
        K.t_on = K.t_atk || K.t_sus || K.t_out;
        sc = exp2_q16(((e8(P_TTIME) - 50 * 256) * 17) >> 8);   /* TIME: 0.1x .. 10x */
        K.t_fa = alpha_ms_x10(3u);              /* 0.3 ms: the fast attack */
        K.t_sa = alpha_ms_x10((uint32_t)((200u * sc) >> 16));   /* 20 ms */
        K.t_fr = alpha_ms_x10((uint32_t)((400u * sc) >> 16));   /* 40 ms */
        K.t_sr = alpha_ms_x10((uint32_t)((3000u * sc) >> 16));  /* 300 ms */
    }
    /* 06 DESTROY */
    if (chg & KG_DST) {
        K.x_drv = (int32_t)T_DB2LIN[clampi(((e8(P_XDRV) >> 1) + 72 * 256) >> 6, 0, 480)] >> 4;   /* Q8 */
        K.x_shp = (uint8_t)ei(P_XSHP);
        K.x_bits = (uint8_t)clampi(ei(P_XBITS), 2, 16);
        K.x_srinc = exp2_q16(-((e8(P_XSR) * 31) >> 8));   /* 1 .. 1/190: 44.1 kHz .. 230 Hz */
        K.x_rinc = (uint32_t)(((uint64_t)(20u * exp2_q16((e8(P_XRING) * 32) >> 8)) * PH_PER_HZ) >> 16);   /* 20 Hz .. 4.7 kHz */
        K.x_rmix = e8(P_XRMIX) * 327 >> 8;
        K.x_tc = ei(P_XTONE) >= 63 ? 0u : T_CUT[tab8(T_RF_LI, P_XTONE)];
        K.x_mix = e8(P_XMIX) * 327 >> 8;
        K.x_on = K.x_mix && (e8(P_XDRV) > 0 || K.x_shp == 3u || K.x_bits < 16u || K.x_srinc < 65535u || K.x_rmix || K.x_tc);
    }
    /* 07 GRAIN */
    if (chg & (KG_GRN | KG_NOTE)) {
        K.gmode = (uint8_t)ei(P_GMODE); K.gfrz = (uint8_t)ei(P_GFRZ);
        K.g_len = (uint32_t)e8(P_GSIZE) * 2205u / 25600u; if (K.g_len < 200u) K.g_len = 200u;
        K.dens_q16 = (uint32_t)((uint64_t)(K.tr_ratio >> 1) * (36044u + (uint32_t)e8(P_GDENS) * 58982u / 10752u) >> 16);
        K.gspray = (uint32_t)e8(P_GSPRAY) * 327u >> 8;
        K.grev = (uint32_t)e8(P_GREV) * 655u >> 8;
        K.gpos = (uint32_t)e8(P_GPOS) * 327u >> 8;
    }
    /* 05 FILTER */
    if (chg & KG_FLT) {
        K.rfmode = (uint8_t)ei(P_RFMODE); K.rfpole = (uint8_t)ei(P_RFPOLE); K.rfsrc = (uint8_t)ei(P_RFSRC);
        K.rf_li = tab8(T_RF_LI, P_RFFREQ);
        K.rf_k = 131072 - (e8(P_RFRES) * 1271 >> 8);
        K.rf_dep = ei(P_RFDEP);
        K.rf_drv1 = 256 + (e8(P_RFDRV) * 1152 / 100 >> 8);
        K.rf_drv2 = 256 + (e8(P_RFDRV) * 384 / 100 >> 8);
        K.rf_lfo_inc = (uint32_t)tab8(T_RATE_CHZ, P_RFRATE) * LFO_PH_PER_CHZ;
        K.rf_envk = 640 + tab8(T_RATE_CHZ, P_RFRATE) * 90 / 100;
    }
    /* 08 OTT */
    i = (uint32_t)ei(P_OSTYLE);
    sc = K.ott;
    K.ott = i != 0u && e8(P_ODEPTH) > 0;        /* (DEPTH 0: only the dry: nothing to compute) */
    if (K.ott && ((chg & KG_OTT) || !sc)) {
        const ostyle_t *st = &OSTY[i - 1u];
        uint32_t b;
        int32_t e = ((e8(P_OTIME) - 50 * 256) * 17) >> 8;   /* TIME: 0.1x .. 10x, 50 % = the preset's times */
        sc = exp2_q16(e);
        K.opost = (uint8_t)ei(P_OPOS); K.osolo = (uint8_t)ei(P_OSOLO);
        bw_coef(K.ox[0], (uint32_t)tab8(T_OXLO_LI, P_OXLO));
        bw_coef(K.ox[1], (uint32_t)tab8(T_OXHI_LI, P_OXHI));
        K.o_dep = e8(P_ODEPTH) * 327 >> 8;
        K.o_up = e8(P_OUP) * 327 >> 8;
        K.o_down = e8(P_ODOWN) * 327 >> 8;
        K.o_in = e8(P_OIN) >> 1;                /* half dB, Q8 -> dB Q8 */
        K.o_thr = e8(P_OTHR) >> 1;
        K.o_band[0] = e8(P_OLOW) >> 1; K.o_band[1] = e8(P_OMID) >> 1; K.o_band[2] = e8(P_OHIGH) >> 1;
        K.o_outlin = (int32_t)T_DB2LIN[clampi(((e8(P_OOUT) >> 1) + 72 * 256) >> 6, 0, 480)];
        K.o_inG = st->inG; K.o_maxup = st->maxup;
        for (b = 0; b < 3u; b++) {
            K.o_dnT[b] = st->dnT[b] + K.o_thr; K.o_upT[b] = st->upT[b] + K.o_thr;
            K.o_dnK[b] = st->dnK[b]; K.o_upK[b] = st->upK[b]; K.o_outG[b] = st->outG[b];
            K.o_att[b] = alpha_ms_x10((uint32_t)(((uint64_t)st->att[b] * sc) >> 16) / 16u);   /* per 16-sample block */
            K.o_rel[b] = alpha_ms_x10((uint32_t)(((uint64_t)st->rel[b] * sc) >> 16) / 16u);
        }
        K.o_rms = alpha_ms_x10(12u);            /* RMS over ~5 ms, updated every 4th sample */
    }
    /* 09 DELAY + FLANGER */
    if (chg & KG_DLY) {
        K.d_tq8 = (uint32_t)tab8(T_DTIME_MS, P_DTIME) * 441u / 10u << 8;
        if (K.d_tq8 > (DLY_N - 4u) << 8) K.d_tq8 = (DLY_N - 4u) << 8;
        K.dfb = e8(P_DFB) * 327 >> 8; K.dmix = e8(P_DMIX) * 327 >> 8; K.route = (uint8_t)ei(P_DROUTE);
        K.f_inc = (uint32_t)tab8(T_FRATE_CHZ, P_FRATE) * LFO_PH_PER_CHZ;
        K.f_sw = (uint32_t)e8(P_FDEPTH) * 3u;
        K.fregen = e8(P_FREGEN) * 327 >> 8; K.fmix = e8(P_FMIX) * 327 >> 8;
    }
    /* 10 REVERB */
    if (chg & KG_REV) {
        mid = (uint32_t)tab8(T_RDEC_MS, P_RDEC);
        for (i = 0; i < 4u; i++) {
            uint32_t len = (uint32_t)RV_BASE[i] * (102u + (uint32_t)e8(P_RSIZE) / 100u) >> 8;
            K.rv_len[i] = len;
            int32_t g0 = rv_gain(2u * len, mid * (uint32_t)ei(P_RLOW) / 100u) >> 2;   /* (2x: in samples at 44.1 kHz) */
            int32_t g1 = rv_gain(2u * len, mid) >> 2, g2 = rv_gain(2u * len, mid * (uint32_t)ei(P_RHIGH) / 100u) >> 2;
            K.rv_g[i][0] = g2; K.rv_g[i][1] = g0 - g1; K.rv_g[i][2] = g1 - g2;   /* Q13, as y g2 + lo (g0-g1) + hi (g1-g2) */
        }
        K.rv_pre = (uint32_t)e8(P_RPRE) * 441u / 5120u + 1u;
        K.rv_cl = T_CUT[tab8(T_XLO_LI, P_RXLO) + 103];   /* one octave up the table: the same Hz at half the rate */
        K.rv_ch = T_CUT[tab8(T_XHI_LI, P_RXHI) + 103];
        i = (uint32_t)e8(P_RMIX);               /* Q8 % */
        K.rv_wet = i >= 50u * 256u ? 32767 : (int32_t)(i * 655u >> 8);
        K.rv_dry = i <= 50u * 256u ? 32767 : (int32_t)((100u * 256u - i) * 655u >> 8);
    }
}

/* ======================================================================== the LFOs (11): E = P + modulation */
static uint32_t l_ph[2];
static int32_t l_sh[2], l_dr[2];
static volatile int32_t tm_lfo[2];
static int32_t lfo_val(uint32_t i, uint32_t shp, uint32_t chz, uint32_t n)
{
    uint32_t old = l_ph[i], t;
    l_ph[i] += chz * LFO_PH_PER_CHZ * n;
    if (l_ph[i] < old) l_sh[i] = (int32_t)(xrand(&rng_isr) >> 16) - 32768;
    t = l_ph[i] >> 16;
    switch (shp) {
    case 0: return isin(l_ph[i]);
    case 1: return (int32_t)(t < 32768u ? t * 2u : (65535u - t) * 2u) - 32768;
    case 2: return (int32_t)t - 32768;
    case 3: return 32767 - (int32_t)t;
    case 4: return t < 32768u ? 32767 : -32767;
    case 5: return l_sh[i];
    default: l_dr[i] += (l_sh[i] - l_dr[i]) >> 4; return l_dr[i];
    }
}
static void lfo_apply(int32_t *t8, uint32_t t, int32_t v, int32_t mix)   /* Q8: the LFO moves the parameter in fine steps */
{
    int32_t lo = PD[t].lo, hi = PD[t].hi, off = (((v * mix) / 100) * (hi - lo)) >> 8;
    t8[t] = clampi(t8[t] + off, lo * 256, hi * 256);
}
static int32_t tabv(const uint16_t *t, int32_t v8, int32_t hi)
{
    int32_t i = v8 >> 8, f = v8 & 255;
    if (v8 < 0) return t[0];
    if (i >= hi) return t[hi];
    return t[i] + ((((int32_t)t[i + 1] - (int32_t)t[i]) * f) >> 8);
}
/* 12 MORPH: A and B, stored patches; while morphing, every unlocked parameter but these sits between them */
static int16_t snapA[NP], snapB[NP];
static volatile uint8_t has_ab, morph_on;      /* has_ab: bit 0 A, bit 1 B */
static uint8_t NOMORPH[NP];                     /* the slot, the theme, the MORPH page itself */
/* VARY: on each note, a random nudge to these (in steps of the parameter, at VARY 100 %) */
static const uint8_t VTGT[8] = {P_PITCH, P_RFFREQ, P_DEC, P_ZDEC, P_ZPITCH, P_XDRV, P_VOL, P_GPOS};
static const uint8_t VAMT[8] = {5, 4, 4, 4, 1, 6, 4, 6};
static int32_t vr[8];                           /* this note's nudges, Q15 */
static volatile uint8_t vary_mute;              /* (a BATCH A>B: the steps only, no randomness) */
static uint32_t lfo_block(uint32_t n)           /* every 64 samples: MORPH, VARY, the LFOs; then every parameter glides to
                                                   where it goes. Returns the modules whose parameters moved */
{
    int32_t t8[NP], v1, v2, m1 = P[P_L1MIX], m2;
    uint32_t i, g1 = TGT[P[P_L1TGT]], g2 = TGT[P[P_L2TGT]], chg = 0;
    for (i = 0; i < NP; i++) t8[i] = P[i] * 256;
    v1 = lfo_val(0, (uint32_t)P[P_L1SHP], (uint32_t)tabv(T_LRATE_CHZ, t8[P_L1RATE], 63), n);
    tm_lfo[0] = v1;
    if (m1 && g1 == P_L2MIX) lfo_apply(t8, g1, v1, m1);   /* (LFO 1 on LFO 2's mix) */
    m2 = t8[P_L2MIX] >> 8;
    v2 = lfo_val(1, (uint32_t)(t8[P_L2SHP] >> 8), (uint32_t)tabv(T_LRATE_CHZ, t8[P_L2RATE], 63), n);
    tm_lfo[1] = v2;
    if (m1 && g1 == P_MORPH) lfo_apply(t8, P_MORPH, v1, m1);
    if (m2 && g2 == P_MORPH) lfo_apply(t8, P_MORPH, v2, m2);
    if (morph_on) {
        int32_t f16 = t8[P_MORPH] * 655 >> 8;  /* 0 .. 65500 */
        for (i = 0; i < NP; i++) {
            int32_t a = snapA[i], b = snapB[i];
            if (NOMORPH[i] || lock_get(i)) continue;
            t8[i] = CONT[i] ? a * 256 + (((b - a) * f16) >> 8) : (f16 >= 32768 ? b : a) * 256;
        }
    }
    if (P[P_VARY])
        for (i = 0; i < 8u; i++) {
            uint32_t t = VTGT[i];
            t8[t] = clampi(t8[t] + ((vr[i] * (int32_t)VAMT[i] * P[P_VARY] / 100) >> 7), PD[t].lo * 256, PD[t].hi * 256);
        }
    if (m1 && g1 != P_MORPH && g1 != P_L2MIX) lfo_apply(t8, g1, v1, m1);
    if (m2 && g2 != P_MORPH) lfo_apply(t8, g2, v2, m2);
    if (pad_on || pad_held) {                   /* the pad: each axis moves its parameter around where it is set */
        if (P[P_XAMT]) lfo_apply(t8, TGT[P[P_XTGT]], ((int32_t)pad_x - 64) * 512, P[P_XAMT]);
        if (P[P_YAMT]) lfo_apply(t8, TGT[P[P_YTGT]], ((int32_t)pad_y - 64) * 512, P[P_YAMT]);
    }
    {
        int32_t at = at_keys > at_midi ? at_keys : at_midi;
        if (at && P[P_ATAMT]) lfo_apply(t8, TGT[P[P_ATTGT]], at * 258, P[P_ATAMT]);
    }
    for (i = 0; i < NP; i++) {
        int32_t sv = S8[i], nv;
        nv = CONT[i] ? sv + ((t8[i] - sv + (t8[i] > sv ? 3 : 0)) >> 2) : t8[i];   /* ~6 ms glide */
        if (nv != sv) { S8[i] = nv; chg |= 1u << PMOD[i]; }
        E[i] = (int16_t)((nv + 128) >> 8);
    }
    return chg;
}

/* ======================================================================== the voice */
static struct {
    uint8_t stage, sp_on, is_rev;
    int32_t env;                                /* Q23 */
    uint32_t ratio, seq, pseq;
    uint32_t oph[5], spos, cpos, tb[2], tg[2], g_phase, g_start, g_win, g_wstep;
    int32_t nz, dc_x, dc_y, ol_x, ol_y, ddc;
    int32_t s1a, s1b, s2a, s2b, rf_env, rf_sh, rf_mod, rf_cnt, rf_a1, rf_a2, rf_a3, rf_k;
    uint32_t rf_lfo, rfz;
    int32_t ox[2][2][2];                        /* OTT: [crossover][stage][ic1 ic2] */
    uint32_t oms[3], ozero;
    int32_t ogdb[3], og[3], ocnt;
    uint32_t odet;
    uint8_t rv_off;
    uint32_t dw, dcur, fw, fph, pw, fcnt, fdl;
    int32_t dlp;
    uint32_t rw, aw1, aw2, rodd, rm1, rm2, rof[2];
    int32_t rlo[4], rhi[4], rdc[4], racc, rcl, rpl;
    int32_t lim, lg;
    uint32_t lcnt;
    int32_t zy1[6], zy2[6];                     /* 02 the modes (Q25) */
    uint32_t zq;
    int32_t te_f, te_s, te_r, t_g, t_cnt;       /* 04 the followers (Q23), the gain (Q12) */
    int32_t x_hold, x_dc, x_lp1, x_lp2;         /* 06 */
    uint32_t x_acc, x_rph;
    uint32_t quiet, pquiet;                     /* samples of silence: out (past ~1 s the chain sleeps); the voice's chain */
    int32_t c_fr, c_sw, c_vq;                   /* 14 CHIP: samples to the next frame; the sweep (1/4096 oct); the volume */
    uint32_t c_mul, c_n, c_fn, c_ai, c_lfsr, c_nph;   /* the pitch factor (Q16); frames: this arp step, all; the arp step;
                                                     the noise */
} V;
enum { ST_A, ST_D, ST_R, ST_OFF };
static volatile uint32_t tm_play, tm_awake, tm_rf_li, tm_gi, tm_peak, tm_lvl, tm_cpu, tm_rv;
static volatile int32_t tm_mod, tm_gd[3], tm_benv[3], tm_gr, tm_env, tm_stage = ST_OFF, tm_fd, tm_tg, tm_z[6];
static volatile uint8_t tm_sleep;
static volatile uint16_t tm_gpos[16];
static volatile uint8_t tm_grev[16];
/* the recorder: armed, it starts at the next note and stops at 1.48 s or after 0.6 s of silence */
enum { REC_IDLE, REC_ARMED, REC_ON, REC_DONE, REC_STOP, REC_USB };   /* REC_USB: a sample coming in over SysEx */
static volatile uint8_t rec_state;
static int rec_state_busy(void) { return rec_state != 0u; }
static volatile uint32_t rec_n;
static uint32_t rec_quiet;
static int32_t rec_prev, rec_pp;
static uint32_t rec_odd;

static void fx_clear(void)
{
    uint32_t i, j;
    for (i = 0; i < DLY_N; i++) dly_buf[i] = 0;
    for (i = 0; i < FLG_N; i++) flg_buf[i] = 0;
    for (i = 0; i < PRE_N; i++) rv_pre[i] = 0;
    for (j = 0; j < 4u; j++) for (i = 0; i < RV_N; i++) rv_line[j][i] = 0;
    for (i = 0; i < 128u; i++) rv_ap1[i] = 0;
    for (i = 0; i < 256u; i++) rv_ap2[i] = 0;
    for (i = 0; i < 3u; i++) { V.og[i] = 4096; V.ogdb[i] = 0; }
    for (i = 0; i < 6u; i++) V.zy1[i] = V.zy2[i] = 0;
    V.t_g = 4096;
    V.lim = LIM_CEIL;
    V.stage = ST_OFF;
    V.ratio = ONE16;
    V.dcur = K.d_tq8;
}

DSP int32_t blep(uint32_t ph, uint32_t inc)  /* PolyBLEP correction, Q15 */
{
    int32_t t = (int32_t)(ph >> 16), dt = (int32_t)(inc >> 16), x;
    if (dt < 1) dt = 1;
    if (dt > 16384) dt = 16384;
    if (t < dt) { x = (t << 15) / dt; return x + x - ((x * x) >> 15) - 32768; }
    if (t > 65535 - dt) { x = ((t - 65536) << 15) / dt; return ((x * x) >> 15) + x + x + 32768; }
    return 0;
}
DSP int32_t saw_b(uint32_t ph, uint32_t inc) { return ((int32_t)(ph >> 16) - 32768) - blep(ph, inc); }
DSP int32_t osc_tick(uint32_t inc)           /* 01 OSC: one sample, Q15 */
{
    uint32_t ph = (V.oph[0] += inc);
    int32_t m = K.color, s;
    switch (K.wave) {
    case 0: return isin(ph);
    case 1: { int32_t t = (int32_t)(ph >> 16); return (t < 32768 ? t * 2 : (65535 - t) * 2) - 32768; }
    case 2: return saw_b(ph, inc);
    case 3: return (saw_b(ph, inc) - saw_b(ph + 0x80000000u, inc)) >> 1;
    case 4: { uint32_t pw = 0x0CCCCCCCu + (uint32_t)m * 108000u; return (saw_b(ph, inc) - saw_b(ph + pw, inc)) >> 1; }   /* 5..95 % */
    case 5: {                                    /* five saws, COLOR = detune */
        static const int8_t DT[5] = {0, 7, -7, 17, -17};
        uint32_t k;
        s = saw_b(ph, inc);
        for (k = 1; k < 5u; k++) {
            uint32_t ik = inc + (uint32_t)(int32_t)(((int64_t)inc * DT[k] * m) >> 25);   /* up to +-1.7 % */
            s += saw_b(V.oph[k] += ik, ik);
        }
        return s * 7 >> 4;
    }
    case 6: {                                    /* COLOR = brightness */
        int32_t n = (int32_t)(xrand(&rng_isr) >> 16) - 32768;
        V.nz += (int32_t)(((int64_t)(n - V.nz) * (2000 + m * 2)) >> 16);
        return m > 32000 ? n : V.nz + (V.nz >> 1);
    }
    case 7:                                      /* SUB-SAW: COLOR = morph */
        return ((((int32_t)(ph >> 16) - 32768) * (11469 + (m * 14746 >> 15))) >> 15) + (isin(ph) * 18022 >> 15)
             + ((isin(ph << 1) * m >> 15) * 11469 >> 15);
    case 8: {                                    /* FORMANT */
        uint32_t p2 = (uint32_t)(((uint64_t)ph * (98304u + (uint32_t)m * 13u)) >> 16);
        return (isin(p2) * (int32_t)T_HANN[ph >> 24] >> 15) + (isin(ph) >> 1);
    }
    case 9: {                                    /* METAL FM */
        uint32_t n = 2u + (uint32_t)(m * 6 >> 15), idx = 273426110u + (uint32_t)m * 75098u;
        return isin(ph + (uint32_t)(int32_t)(((int64_t)isin(ph * n) * idx) >> 15));
    }
    case W_PADSCAN: {                            /* PAD SCAN: cycles cut from the sample, COLOR = where */
        int32_t len = (int32_t)smp_len, off;
        if (smp_busy || len < 1100) return 0;
        off = (int32_t)(((int64_t)m * (len - 520)) >> 15);
        s = herm16(smp_ptr + off, (int32_t)(ph >> 23), (ph >> 11) & 4095u, 512);
        return s * (int32_t)T_SQSIN[ph >> 24] >> 15;
    }
    case W_CPLS:                                 /* 14 CHIP: the console's pulse, its 4 duties (band-limited: no */
        return (saw_b(ph, inc) - saw_b(ph + K.c_pw, inc)) >> 1;   /* fizz at 44.1 kHz; the grit is the volume steps) */
    case W_CTRI: {                               /* 32 steps of 4 bits, down and up (no volume control on a NES) */
        int32_t st = (int32_t)(ph >> 27);
        return ((st < 16 ? 15 - st : st - 16) * 2 - 15) * 2184;
    }
    case W_CNSE: {                               /* a 15-bit LFSR clocked at 8..64 x the note (COLOR); SHORT: 93 steps */
        uint32_t ni = inc << (3u + (uint32_t)(m >> 13)), o = V.c_nph;
        if (ni < inc) ni = 0xFFFFFFFFu;         /* (past 44.1 kHz: a step every sample) */
        V.c_nph += ni;
        if (V.c_nph < o || ni == 0xFFFFFFFFu) {
            uint32_t r = V.c_lfsr ? V.c_lfsr : 1u, fb = (r ^ (r >> (K.c_short ? 6u : 1u))) & 1u;
            V.c_lfsr = (r >> 1) | (fb << 14);
        }
        return V.c_lfsr & 1u ? -26000 : 26000;
    }
    default: return 0;
    }
}

DSP int32_t smp_tick(uint32_t step)          /* 01 SAMPLE: one sample at the voice's pitch (22.05 kHz source) */
{
    const int16_t *b = smp_ptr;
    int32_t len = (int32_t)smp_len, s, i;
    if (!V.sp_on || smp_busy || len < 256) return 0;
    if (K.unity) {
        i = (int32_t)(V.spos >> 12);
        s = herm16(b, i, V.spos & 4095u, len);
        if (K.sloop) {                          /* loop the last 3/4 with a crossfade into its start */
            int32_t ls = len >> 2, le = len - 64, span = le - ls, xf = span / 2 > 1024 ? 1024 : span / 2;
            if (i >= le - xf) {
                int32_t w = (i - (le - xf)) * 32767 / xf, s2 = herm16(b, i - span, V.spos & 4095u, len);
                s = (s * (32767 - w) + s2 * w) >> 15;
            }
            V.spos += step;
            if ((int32_t)(V.spos >> 12) >= le) V.spos -= (uint32_t)span << 12;
        } else {
            V.spos += step;
            if (i >= len - 4) { V.sp_on = 0; return 0; }
            if (i > len - 260) s = s * (len - 4 - i) >> 8;
        }
        tm_play = (uint32_t)i * 32767u / (uint32_t)len;
        return s;
    }
    {                                           /* STRETCH: two Hann taps of 1024 output samples, 50 % apart */
        uint32_t k;
        int32_t end = len << 12;
        s = 0;
        for (k = 0; k < 2u; k++) {
            int32_t rd;
            if (++V.tg[k] >= 1024u) { V.tg[k] = 0; V.tb[k] = V.cpos; }
            rd = (int32_t)(V.tb[k] + V.tg[k] * step);
            if (rd < end - 8192) s += herm16(b, rd >> 12, (uint32_t)rd & 4095u, len) * (int32_t)T_HANN[V.tg[k] >> 2] >> 15;
        }
        V.cpos += K.sc_step;
        if ((int32_t)V.cpos >= end - 4096) {
            if (K.sloop) V.cpos = (uint32_t)(len >> 2) << 12;
            else { V.sp_on = 0; return 0; }
        }
        tm_play = (V.cpos >> 12) * 32767u / (uint32_t)len;
        return s;
    }
}
DSP int32_t grain_tick(void)                 /* 07 GRAIN ENGINE on the sample */
{
    const int16_t *b = smp_ptr;
    uint32_t gl = K.g_len, gp;
    int32_t len = (int32_t)smp_len, rd, w;
    if (smp_busy || len < 256) return 0;
    if (gl > (uint32_t)len / 2u) gl = (uint32_t)len / 2u;
    V.g_phase += K.dens_q16;
    if (V.g_phase >= (gl << 16) || V.g_phase < K.dens_q16) {
        int32_t jit = (int32_t)((((int64_t)((int32_t)(xrand(&rng_isr) >> 17) - 16384)) * (int32_t)K.gspray) >> 15), pos;
        V.g_phase = 0;
        V.is_rev = (xrand(&rng_isr) & 0xFFFFu) < K.grev;
        pos = clampi((int32_t)K.gpos + jit, 655, 30801);
        V.g_start = (uint32_t)(((int64_t)pos * (len - (int32_t)gl)) >> 15);
        V.g_win = 0;
        V.g_wstep = ((K.dens_q16 << 4) / gl) << 4;
        tm_gpos[tm_gi & 15u] = (uint16_t)pos;
        tm_grev[tm_gi & 15u] = V.is_rev;
        tm_gi++;
        tm_play = (uint32_t)pos;
    }
    gp = V.g_phase >> 4;
    rd = V.is_rev ? (int32_t)(((V.g_start + gl) << 12) - gp) : (int32_t)((V.g_start << 12) + gp);
    rd = clampi(rd, 0, (len - 2) << 12);
    w = T_HANN[V.g_win >> 16 > 256u ? 256u : V.g_win >> 16];
    V.g_win += V.g_wstep;
    return herm16(b, rd >> 12, (uint32_t)rd & 4095u, len) * w >> 15;
}

DSP int32_t svf_stage(int32_t v0, int32_t *ic1, int32_t *ic2, uint32_t mode, int lin)
{
    int32_t c2 = lin ? *ic2 : tanh_q(*ic2), v3 = v0 - c2, v1, v2;   /* (ANALOG 0: a clean, linear stage) */
    v1 = (int32_t)(((int64_t)V.rf_a1 * *ic1 + (int64_t)V.rf_a2 * v3 + 32768) >> 16);   /* rounded: a plain >> floors, and */
    v2 = c2 + (int32_t)(((int64_t)V.rf_a2 * *ic1 + (int64_t)V.rf_a3 * v3 + 32768) >> 16);   /* the silence then latches a DC */
    *ic1 = clampi(2 * v1 - *ic1, -3000000, 3000000);   /* (+-4e6 would be a ROM-looking constant) */
    *ic2 = clampi(2 * v2 - c2, -3000000, 3000000);
    if (mode == 1u) return v2;
    if (mode == 2u) return v1;
    if (mode == 3u) return v0 - (int32_t)(((int64_t)V.rf_k * v1) >> 16) - v2;
    return v0 - (int32_t)(((int64_t)V.rf_k * v1) >> 16);
}
DSP int32_t rf_tick(int32_t x, int32_t env15)   /* 05 RES MOD FILTER */
{
    int32_t ain = absi(x) << 8, out;
    uint32_t old = V.rf_lfo;
    V.rfz = x ? 0u : V.rfz + 1u;
    V.rf_env += (int32_t)(((int64_t)(ain - V.rf_env) * (ain > V.rf_env ? RF_ATT_Q16 : RF_REL_Q16)) >> 16);
    V.rf_lfo += K.rf_lfo_inc;
    if (V.rf_lfo < old) V.rf_sh = (int32_t)(xrand(&rng_isr) >> 16) - 32768;
    if (--V.rf_cnt <= 0) {
        int32_t li, g, den;
        V.rf_cnt = 16;
        if (V.rfz >= 512u && absi(V.s1a) + absi(V.s1b) + absi(V.s2a) + absi(V.s2b) < 256)   /* silence in: let go of the */
            V.s1a = V.s1b = V.s2a = V.s2b = 0;  /* last few LSBs (the integrators' rounding holds them as a DC otherwise) */
        V.rf_mod = K.rfsrc == 0 ? clampi(((V.rf_env >> 8) * K.rf_envk) >> 8, 0, 32767)
                 : K.rfsrc == 1 ? isin(V.rf_lfo) : K.rfsrc == 2 ? V.rf_sh : env15;
        li = clampi(K.rf_li + ((K.rf_dep * V.rf_mod * LI_DEPTH_K) >> 20), LI_55, LI_MAX);
        tm_rf_li = (uint32_t)li;
        tm_mod = V.rf_mod;
        g = (int32_t)T_G[li];
        den = 65536 + (int32_t)(((int64_t)g * (g + K.rf_k)) >> 16);
        V.rf_a1 = (int32_t)(0xFFFFFFFFu / (uint32_t)den);
        V.rf_a2 = (int32_t)(((int64_t)g * V.rf_a1) >> 16);
        V.rf_a3 = (int32_t)(((int64_t)g * V.rf_a2) >> 16);
        V.rf_k = K.rf_k;
    }
    if (K.rf_drv1 == 256) {                     /* ANALOG 0: no saturation anywhere (4 tanh fewer a sample) */
        out = svf_stage(x, &V.s1a, &V.s1b, K.rfmode, 1);
        return K.rfpole ? svf_stage(out, &V.s2a, &V.s2b, K.rfmode, 1) : out;
    }
    out = svf_stage(tanh_q(x * K.rf_drv1 >> 8), &V.s1a, &V.s1b, K.rfmode, 0);
    if (K.rfpole) out = svf_stage(tanh_q(out * K.rf_drv2 >> 8), &V.s2a, &V.s2b, K.rfmode, 0);
    return out;
}

/* 02 RESONATOR: six two-pole resonators y = 2 r cos(w) y1 - r^2 y2 + g x (Q30 coefficients, Q25 states) */
DSP int32_t reso_tick(int32_t x)
{
    int32_t sum = 0, wet;
    uint32_t k;
    if (x) V.zq = 0;
    else if (++V.zq >= 64u) {                   /* silence in: below ~-70 dB a mode is let go (the rounding of a */
        uint32_t big = 0;                       /* two-pole loop can otherwise hum on in the last LSBs forever) */
        V.zq = 0;
        for (k = 0; k < K.zn; k++) big |= (uint32_t)(absi(V.zy1[k]) | absi(V.zy2[k]));
        if (big < (1u << 14)) { for (k = 0; k < 6u; k++) V.zy1[k] = V.zy2[k] = 0; return 0; }
    }
    for (k = 0; k < K.zn; k++) {
        int32_t y = (int32_t)((((int64_t)K.za1[k] * V.zy1[k]) >> 29) - (((int64_t)K.za2[k] * V.zy2[k]) >> 30)) + ((K.zg[k] * x) >> 5);
        y = clampi(y, -1000000000, 1000000000);
        V.zy2[k] = V.zy1[k]; V.zy1[k] = y;
        sum += y >> 10;                         /* Q25 -> Q15, the modes summed */
    }
    wet = tanh_q(sum);
    return x + (int32_t)(((int64_t)(wet - x) * K.zmix) >> 15);
}

/* 04 TRANSIENT: fast and slow followers; ATTACK scales the fast-over-slow-attack rise, SUSTAIN the slow-over-fast tail */
DSP int32_t trans_tick(int32_t x)
{
    int32_t a = absi(x) << 8;
    V.te_f += (int32_t)(((int64_t)(a - V.te_f) * (a > V.te_f ? K.t_fa : K.t_fr)) >> 16);
    V.te_s += (int32_t)(((int64_t)(a - V.te_s) * (a > V.te_s ? K.t_sa : K.t_fr)) >> 16);
    V.te_r += (int32_t)(((int64_t)(a - V.te_r) * (a > V.te_r ? K.t_fa : K.t_sr)) >> 16);
    if (--V.t_cnt <= 0) {                       /* the gain, every 4th sample */
        int32_t lf = ilog2_q8((uint32_t)V.te_f + 1u), ls = ilog2_q8((uint32_t)V.te_s + 1u), lr = ilog2_q8((uint32_t)V.te_r + 1u);
        int32_t at = lf - ls - 128, su = lr - lf - 32, db, gate;   /* (a 3 dB dead zone: the ripple of a steady sound) */
        V.t_cnt = 4;
        if (at < 0) at = 0;
        if (su < 0) su = 0;
        db = ((at * K.t_atk * 3 / 2 + su * K.t_sus) * 1541 / 100) >> 8;   /* log2 Q8 -> dB Q8 (x6.02), x amount (attack x1.5) */
        gate = clampi((lf - 10 * 256) / 4, 0, 256);   /* below ~-70 dB: no gain (the noise floor stays put) */
        db = clampi((db * gate >> 8), -30 * 256, 24 * 256);
        tm_tg = db;
        db = clampi(db + K.t_out, -72 * 256, 48 * 256);
        V.t_g += ((int32_t)T_DB2LIN[(db + 72 * 256) >> 6] - V.t_g) >> 1;
    }
    return (int32_t)(((int64_t)x * V.t_g) >> 12);
}

/* 06 DESTROY: drive > shape > bits > sample rate > ring > tone, mixed with the dry */
DSP int32_t destroy_tick(int32_t x)
{
    int32_t y = (x * K.x_drv) >> 8;
    switch (K.x_shp) {
    case 0: y = tanh_q(y); break;
    case 1: y = clampi(y, -30000, 30000); break;
    case 2: { uint32_t u = (uint32_t)(y + 32768) & 0x1FFFFu; y = u < 65536u ? (int32_t)u - 32768 : 98303 - (int32_t)u; break; }   /* fold */
    default: y = absi(tanh_q(y)) * 2; V.x_dc += (y - V.x_dc) >> 8; y -= V.x_dc; break;   /* rectify: up an octave */
    }
    if (K.x_bits < 16u) { uint32_t sh = 16u - K.x_bits; y = ((y + (1 << (sh - 1u))) >> sh) << sh; }
    if (K.x_srinc < 65535u) {
        V.x_acc += K.x_srinc;
        if (V.x_acc >= 65536u) { V.x_acc -= 65536u; V.x_hold = y; }
        y = V.x_hold;
    }
    if (K.x_rmix) { int32_t c = isin(V.x_rph += K.x_rinc); y += (int32_t)(((int64_t)((y * c >> 15) - y) * K.x_rmix) >> 15); }
    if (K.x_tc) { y = onepole(&V.x_lp1, y, K.x_tc); y = onepole(&V.x_lp2, y, K.x_tc); }
    return x + (int32_t)(((int64_t)(y - x) * K.x_mix) >> 15);
}

/* 08 OTT MULTIBAND: Linkwitz-Riley 24 dB/oct crossovers, RMS detection, upward and downward compression per band */
DSP void svf2(int32_t *st, const int32_t *c, int32_t x, int32_t *lp, int32_t *hp)   /* one Butterworth stage: LP and HP */
{
    int32_t v3 = x - st[1], v1, v2;
    v1 = (int32_t)(((int64_t)c[0] * st[0] + (int64_t)c[1] * v3) >> 16);
    v2 = st[1] + (int32_t)(((int64_t)c[1] * st[0] + (int64_t)c[2] * v3) >> 16);
    st[0] = 2 * v1 - st[0];
    st[1] = 2 * v2 - st[1];
    if (lp) *lp = v2;
    if (hp) *hp = x - (int32_t)(((int64_t)SQRT2_Q16 * v1) >> 16) - v2;
}
DSP void lr4(int32_t (*st)[2], const int32_t *c, int32_t x, int32_t *lo, int32_t *hi)
{                                               /* LR4: LP = Butterworth LP twice; and since LP^2 + HP^2 is the Butterworth
                                                   allpass (x - 2k BP), HP = allpass - LP: two stages, not three */
    int32_t v3 = x - st[0][1], v1, l1;
    v1 = (int32_t)(((int64_t)c[0] * st[0][0] + (int64_t)c[1] * v3) >> 16);
    l1 = st[0][1] + (int32_t)(((int64_t)c[1] * st[0][0] + (int64_t)c[2] * v3) >> 16);
    st[0][0] = 2 * v1 - st[0][0];
    st[0][1] = 2 * l1 - st[0][1];
    svf2(st[1], c, l1, lo, 0);
    *hi = x - (int32_t)(((int64_t)(2 * SQRT2_Q16) * v1) >> 16) - *lo;
}
DSP void ott_gains(void)                     /* every 16 samples: the gain computer, per band */
{
    uint32_t b;
    for (b = 0; b < 3u; b++) {
        int32_t db = ((ilog2_q8(V.oms[b] + 1u) - 26 * 256) * 771) >> 8;   /* RMS, dB Q8 (0 dB: full-scale square) */
        int32_t dbi = db + K.o_inG + K.o_in, t = 0, tot;
        if (dbi > K.o_dnT[b]) t = -((((dbi - K.o_dnT[b]) * K.o_dnK[b]) >> 8) * K.o_down >> 15);
        else if (dbi < K.o_upT[b]) {
            int32_t up = ((K.o_upT[b] - dbi) * K.o_upK[b]) >> 8, gate = clampi((dbi + 78 * 256) / 12, 0, 256);
            if (up > K.o_maxup) up = K.o_maxup;
            t = ((up * gate) >> 8) * K.o_up >> 15; /* (the gate: digital silence stays silent) */
        }
        V.ogdb[b] += (int32_t)(((int64_t)(t - V.ogdb[b]) * (t < V.ogdb[b] ? K.o_att[b] : K.o_rel[b])) >> 16);
        tm_gd[b] = V.ogdb[b];
        tm_benv[b] = db;
        tot = clampi(V.ogdb[b] + K.o_inG + K.o_in + K.o_outG[b] + K.o_band[b], -72 * 256, 42 * 256);
        V.og[b] = (K.osolo && K.osolo != b + 1u) ? 0 : (int32_t)T_DB2LIN[(tot + 72 * 256) >> 6];
    }
}
DSP int32_t ott_tick(int32_t x)              /* split, detect, apply, mix (mono) */
{
    int32_t bd[3], rest, wet, dry;
    uint32_t b, q, j;
    if (x == 0) {                               /* silence in: after ~6 ms let go of the crossovers' last LSBs */
        if (++V.ozero == 256u) {                /* (their rounding holds them as a DC, and the upward gain lifts it) */
            uint32_t sum = 0;
            for (q = 0; q < 2u; q++) for (j = 0; j < 2u; j++) sum += (uint32_t)absi(V.ox[q][j][0]) + (uint32_t)absi(V.ox[q][j][1]);
            if (sum < 65536u) for (q = 0; q < 2u; q++) for (j = 0; j < 2u; j++) V.ox[q][j][0] = V.ox[q][j][1] = 0;
            else V.ozero = 0;
        }
    } else V.ozero = 0;
    lr4(V.ox[0], K.ox[0], x << 6, &bd[0], &rest);   /* << 6: room for the 88 Hz filters' small steps */
    lr4(V.ox[1], K.ox[1], rest, &bd[1], &bd[2]);
    if (!(++V.odet & 3u))                        /* the level: every 4th sample is plenty for a 5 ms RMS */
        for (b = 0; b < 3u; b++) {
            int32_t lv = bd[b] >> 8;            /* Q15 / 4 */
            uint32_t al = (uint32_t)(lv < 0 ? -lv : lv), sq;
            if (al > 65535u) al = 65535u;      /* (a band driven far past full scale: the square stays in 32 bits) */
            sq = al * al;
            V.oms[b] += (uint32_t)(int32_t)(((int64_t)((int64_t)sq - V.oms[b]) * K.o_rms) >> 16);
        }
    if (--V.ocnt <= 0) { V.ocnt = 16; ott_gains(); }
    dry = K.osolo ? bd[K.osolo - 1u] : bd[0] + bd[1] + bd[2];   /* (a soloed band: the only one, dry too) */
    wet = (int32_t)(((int64_t)bd[0] * V.og[0] + (int64_t)bd[1] * V.og[1] + (int64_t)bd[2] * V.og[2]) >> 12);
    wet = dry + (int32_t)(((int64_t)(wet - dry) * K.o_dep) >> 15);   /* DEPTH: dry/wet, the dry phase-aligned */
    return (int32_t)(((int64_t)wet * K.o_outlin) >> 18);           /* OUT, and back from << 6 */
}

/* 09 DELAY + FLANGER */
DSP int32_t dly_read(void)
{
    uint32_t d = V.dcur >> 8, fr = V.dcur & 255u, a = V.dw + DLY_N - d - 1u, b;
    if (a >= DLY_N) a -= DLY_N;
    b = a == 0u ? DLY_N - 1u : a - 1u;
    return dly_buf[a] + (((dly_buf[b] - dly_buf[a]) * (int32_t)fr) >> 8);
}
DSP void dly_write(int32_t v) { dly_buf[V.dw] = sat16(v); if (++V.dw >= DLY_N) V.dw = 0; }
DSP int32_t flg_tap(uint32_t dq8)
{
    uint32_t a = (V.fw - (dq8 >> 8) - 1u) & (FLG_N - 1u), b = (a - 1u) & (FLG_N - 1u);
    return flg_buf[a] + (((flg_buf[b] - flg_buf[a]) * (int32_t)(dq8 & 255u)) >> 8);
}
DSP int32_t flanger(int32_t x)                /* the delayed (comb) signal */
{
    int32_t y;
    if (!K.fmix) { flg_buf[V.fw] = sat16(x); V.fw = (V.fw + 1u) & (FLG_N - 1u); return x; }   /* off: keep the line fed */
    V.fph += K.f_inc;
    if (!(++V.fcnt & 7u)) {                     /* the sweep, every 8 samples (it moves at most 10 Hz) */
        V.fdl = (22u << 8) + (((uint32_t)(isin(V.fph) + 32768) >> 1) * K.f_sw >> 15);
        tm_fd = (int32_t)V.fdl;
    }
    y = flg_tap(V.fdl);
    flg_buf[V.fw] = sat16(x + (int32_t)(((int64_t)y * K.fregen) >> 15));
    V.fw = (V.fw + 1u) & (FLG_N - 1u);
    return y;
}
DSP int32_t fmixf(int32_t in, int32_t y) { return in + (int32_t)(((int64_t)(((in + y) >> 1) - in) * K.fmix) >> 15); }   /* dry -> (in + comb) / 2 */
DSP int32_t dfb(int32_t v)                  /* the delay's feedback: damped, saturated, no DC */
{
    V.dlp += (v - V.dlp) >> 2;
    V.ddc += (V.dlp - V.ddc + 512) >> 10;
    return (int32_t)(((int64_t)tanh_q(V.dlp - V.ddc) * K.dfb) >> 15);
}
DSP int32_t dlyflg(int32_t x)                   /* (one flanger, its input chosen by the route: inlined once) */
{
    int32_t d, fin, f;
    V.dcur += (uint32_t)(((int32_t)K.d_tq8 - (int32_t)V.dcur) >> 12);   /* time glides like tape */
    if (!K.dmix) { dly_write(x); return fmixf(x, flanger(x)); }   /* no delay heard: its line fed with the dry */
    d = dly_read();
    fin = K.route == 2u ? x + (int32_t)(((int64_t)d * K.dmix) >> 15) : K.route == 3u ? d : x;
    f = fmixf(fin, flanger(fin));
    switch (K.route) {
    case 0: dly_write(x + dfb(d)); return f + (int32_t)(((int64_t)d * K.dmix) >> 15);   /* PARALLEL */
    case 1: dly_write(f + dfb(d)); return f + (int32_t)(((int64_t)d * K.dmix) >> 15);   /* FLG > DLY */
    case 2: dly_write(x + dfb(d)); return f;                                            /* DLY > FLG */
    default: dly_write(x + dfb(f)); return x + (int32_t)(((int64_t)f * K.dmix) >> 15);  /* FLG IN FDBK: every repeat */
    }                                                                                   /* through the flanger again */
}

/* 10 MULTIBAND REVERB: at half rate (22.05 kHz: a tail needs no more, and it halves the cost), power-of-two rings */
DSP int32_t allpass(int16_t *buf, uint32_t mask, uint32_t len, uint32_t *w, int32_t x)
{
    int32_t d = buf[(*w - len) & mask], v = x + (d * 20480 >> 15), y = d - (v * 20480 >> 15);
    buf[*w & mask] = sat16(v);
    (*w)++;
    return y;
}
DSP int32_t reverb_step(int32_t in)          /* one half-rate step of the FDN; out: the four lines summed */
{
    int32_t p, d, o[4], h[4], i;
    rv_pre[V.pw & (PRE_N - 1u)] = sat16(in);
    p = rv_pre[(V.pw - K.rv_pre) & (PRE_N - 1u)];
    V.pw++;
    d = allpass(rv_ap2, 255u, 191u, &V.aw2, allpass(rv_ap1, 127u, 73u, &V.aw1, p)) >> 1;
    if (!(V.rw & 7u)) {                         /* two lines wander +-6 samples (0.6, 0.8 Hz): no metallic ringing */
        V.rm1 += 121700u * 8u; V.rm2 += 167300u * 8u;   /* (every 8th step: it moves that slowly) */
        V.rof[0] = 1536u + (uint32_t)((isin(V.rm1) * 1535) >> 15);
        V.rof[1] = 1536u + (uint32_t)((isin(V.rm2) * 1535) >> 15);
    }
    for (i = 0; i < 4; i++) {
        if (i == 0 || i == 2) {
            uint32_t off = V.rof[i >> 1], pos = V.rw - K.rv_len[i] - (off >> 8);
            int32_t a = rv_line[i][pos & (RV_N - 1u)], b = rv_line[i][(pos - 1u) & (RV_N - 1u)];
            o[i] = a + (((b - a) * (int32_t)(off & 255u)) >> 8);
        } else o[i] = rv_line[i][(V.rw - K.rv_len[i]) & (RV_N - 1u)];
    }
    h[0] = (o[0] + o[1] + o[2] + o[3] + 1) >> 1;   /* Hadamard / 2 (rounded: see the DC trap) */
    h[1] = (o[0] - o[1] + o[2] - o[3] + 1) >> 1;
    h[2] = (o[0] + o[1] - o[2] - o[3] + 1) >> 1;
    h[3] = (o[0] - o[1] - o[2] + o[3] + 1) >> 1;
    for (i = 0; i < 4; i++) {                    /* each line: LOW / MID / HIGH decay on their own */
        int32_t y = h[i], lo, hi, fb;
        V.rdc[i] += (y - V.rdc[i] + 256) >> 9;  /* a ~7 Hz DC trap: the loop gain is near 1, integer bias would build up */
        y -= V.rdc[i];
        lo = onepole(&V.rlo[i], y, K.rv_cl);
        hi = onepole(&V.rhi[i], y, K.rv_ch);
        fb = (y * K.rv_g[i][0] + lo * K.rv_g[i][1] + hi * K.rv_g[i][2] + 4096) >> 13;   /* 32-bit (no overflow: 3 x 2^16 x 2^13) */
        rv_line[i][V.rw & (RV_N - 1u)] = sat16(fb + ((i & 1) ? -d : d));
    }
    V.rw++;
    return (o[0] + o[1] + o[2] + o[3]) >> 2;
}
DSP int32_t reverb(int32_t x)
{
    int32_t w;
    if (V.rv_off) {                             /* back on: no frozen tail from before */
        uint32_t i, j;
        for (j = 0; j < 4u; j++) for (i = 0; i < RV_N; i++) rv_line[j][i] = 0;
        for (i = 0; i < PRE_N; i++) rv_pre[i] = 0;
        V.rcl = V.rpl = 0;
        V.rv_off = 0;
    }
    if (!V.rodd) { V.racc = x; V.rodd = 1; w = V.rcl; }
    else {
        V.rpl = V.rcl;
        V.rcl = reverb_step((V.racc + x) >> 1);
        V.rodd = 0;
        w = (V.rpl + V.rcl) >> 1;               /* back to 44.1 kHz: halfway */
    }
    tm_rv = (tm_rv * 255u + (uint32_t)absi(w)) >> 8;
    return (int32_t)(((int64_t)x * K.rv_dry + (int64_t)w * K.rv_wet) >> 15);
}

static void voice_events(uint32_t chg)          /* note-ons, glides and gate from the main loop */
{
    if (V.seq != v_seq) {
        int legato = V.stage <= ST_D && v_gate && K.glide_a && V.seq != 0u;
        uint32_t k, start = 0;
        V.seq = v_seq;
        V.pseq = v_pseq;
        rnd_q16 = P[P_RNDP] ? 55050u + xrand(&rng_isr) % 20972u : ONE16;
        if (!legato) {                          /* VARY: this note's nudges, and up to 1 % into the sample */
            if (P[P_VARY] && !vary_mute) {
                for (k = 0; k < 8u; k++) vr[k] = ((int32_t)(xrand(&rng_isr) >> 17) - 16384) * 2;
                start = (xrand(&rng_isr) >> 16) * (smp_len * (uint32_t)P[P_VARY] / 10000u) >> 16;
            } else for (k = 0; k < 8u; k++) vr[k] = 0;
        }
        k_update(chg | KG_NOTE);
        if (!legato) {
            V.stage = ST_A;
            V.sp_on = 1; V.spos = start << 12; V.cpos = start << 12; V.tg[0] = 1023; V.tg[1] = 511; V.tb[0] = V.tb[1] = V.cpos;
            V.g_phase = 0xFFFFFFFFu;
            if (!K.glide_a || V.ratio == 0u) V.ratio = K.tr_ratio;
            V.c_fr = 0; V.c_sw = -K.c_sw; V.c_n = 0; V.c_fn = 0; V.c_ai = 0; V.c_vq = 0; V.c_mul = 65536u;   /* 14: the first frame */
            V.c_lfsr = 1u;                                                                    /* now, from the note */
        }
        V.quiet = 0; V.pquiet = 0;
        if (rec_state == REC_ARMED) { rec_state = REC_ON; rec_n = 0; rec_quiet = 0; rec_odd = 0; }
    } else {
        k_update(chg);
        if (V.pseq != v_pseq) V.pseq = v_pseq;
    }
    if (!v_gate && V.stage <= ST_D) V.stage = ST_R;
}

#define BLK 64u                                 /* a block: the control rate (LFOs, coefficients, note events) */
/* The chain runs over one block at a time, module by module: each loop keeps its state in registers and tests its
 * switches once per block, not once per sample. The stages marked DSP_RAM run from RAM (no fetches through the XIP
 * flash cache) and call nothing (tools/build.py checks it): every DSP helper above is inlined into them. */
static int32_t blk_b[BLK], blk_e[BLK];          /* the block's signal, the amp envelope (05's AMP ENV source) */
/* 14 CHIP: the console's 60 Hz frame. Each frame the SWEEP moves the pitch, the ARP steps, and (STEPS) the volume takes
 * its new 4-bit level: on a NES the music driver did all of it once a frame, which is half of the 8-bit sound. */
#define CHIP_FRAME 735                          /* 44100 / 60 */
static const int8_t ARP_T[NARP][3] = {{0}, {0, 12}, {0, 7}, {0, 5}, {0, 4, 7}, {0, 3, 7}, {0, 1}, {0, -12}, {0, 5}, {0, 7}, {0, 12}};
static const uint8_t ARP_N[NARP] = {1, 2, 2, 2, 3, 3, 2, 2, 2, 2, 2};
static uint32_t arp_next(uint32_t arp, uint32_t i)   /* the next step (the UP ones stay on their last) */
{
    return arp >= 8u ? (i + 1u < ARP_N[arp] ? i + 1u : i) : (i + 1u) % ARP_N[arp];
}
static void chip_frame(void)
{
    int32_t off;
    V.c_sw = clampi(V.c_sw + K.c_sw, -5 * 4096, 4 * 4096);   /* (5 octaves down, 4 up: then it holds) */
    off = (V.c_sw >> 4) + (K.c_arp ? ARP_T[K.c_arp][V.c_ai % ARP_N[K.c_arp]] * 256 / 12 : 0);
    V.c_mul = off ? exp2_q16(off) : 65536u;
    if (K.c_arp && ++V.c_n >= K.c_arps) { V.c_n = 0; V.c_ai = arp_next(K.c_arp, V.c_ai); }
    V.c_fn++;
}
static void stage_src(uint32_t n)               /* the envelope, the glide, 01 the sources (in flash: 11 waves, big) */
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t env15, o = 0, s = 0, sig = 0;
        switch (V.stage) {
        case ST_A: V.env += K.a_inc; if (V.env >= ENV_MAX) { V.env = ENV_MAX; V.stage = ST_D; } break;
        case ST_D: V.env += (int32_t)(((int64_t)(K.sus23 - V.env) * K.d_a) >> 16); break;
        case ST_R: V.env -= (int32_t)(((int64_t)V.env * K.r_a) >> 16) + 1; if (V.env <= 0) { V.env = 0; V.stage = ST_OFF; } break;
        default: break;
        }
        env15 = V.env >> 8;
        if (K.glide_a) V.ratio += (uint32_t)(int32_t)(((int64_t)((int32_t)K.tr_ratio - (int32_t)V.ratio) * K.glide_a) >> 16);
        else V.ratio = K.tr_ratio;
        if (V.stage != ST_OFF || K.gfrz) {
            uint32_t ratio = V.ratio;
            if (K.c_on) {                       /* 14 CHIP: the frame clock */
                if (--V.c_fr <= 0) {
                    V.c_fr = CHIP_FRAME;
                    chip_frame();
                    if (K.c_step && !(V.c_fn % K.c_step)) V.c_vq = 0;   /* (a new volume step: taken just below) */
                }
                if (K.c_step) {                 /* 16 levels, held a frame (up at once while attacking: no late hits) */
                    int32_t q = (env15 * 15 + 16384) >> 15;
                    if (!V.c_vq || (V.stage == ST_A && q * 2184 > V.c_vq)) V.c_vq = q ? q * 2184 : 1;
                    env15 = V.c_vq > 1 ? V.c_vq : 0;
                }
                ratio = (uint32_t)(((uint64_t)ratio * V.c_mul) >> 16);
            }
            if (K.wo) o = osc_tick((uint32_t)(((uint64_t)C4_INC * ratio) >> 16));
            if (K.ws) s = (K.gfrz || K.gmode) ? grain_tick() : smp_tick(ratio >> 5);
            sig = (int32_t)(((int64_t)o * K.wo + (int64_t)s * K.ws) >> 15);
            sig = (int32_t)(((int64_t)sig * (K.gfrz && V.stage == ST_OFF ? 32767 : env15) >> 15) * K.velg >> 15);
        }
        blk_e[i] = env15;
        blk_b[i] = sig;
    }
}
DSP_RAM void stage_pre(uint32_t n)              /* 02 RESONATOR, 04 TRANSIENT, 06 DESTROY */
{
    uint32_t i, lvl = tm_lvl;
    int32_t *b = blk_b;
    for (i = 0; i < n; i++) b[i] = dcblock(b[i], &V.dc_x, &V.dc_y);
    if (K.zmix) for (i = 0; i < n; i++) b[i] = reso_tick(b[i]);
    if (K.t_on) for (i = 0; i < n; i++) b[i] = trans_tick(b[i]);
    if (K.x_on) for (i = 0; i < n; i++) b[i] = destroy_tick(b[i]);
    for (i = 0; i < n; i++) lvl = (lvl * 31u + (uint32_t)absi(b[i])) >> 5;
    tm_lvl = lvl;
}
DSP_RAM void stage_flt(uint32_t n)              /* 05 RES MOD FILTER */
{
    uint32_t i;
    for (i = 0; i < n; i++) blk_b[i] = rf_tick(blk_b[i], blk_e[i]);
}
DSP_RAM void stage_ott(uint32_t n)              /* 08 OTT (PRE or POST) */
{
    uint32_t i;
    for (i = 0; i < n; i++) blk_b[i] = ott_tick(blk_b[i]);
}
DSP_RAM void stage_fx(uint32_t n)               /* 09 DELAY + FLANGER, 10 REVERB */
{
    uint32_t i;
    for (i = 0; i < n; i++) blk_b[i] = dlyflg(blk_b[i]);
    if (K.rv_wet) for (i = 0; i < n; i++) blk_b[i] = reverb(blk_b[i]);
}
DSP_RAM void stage_out(int32_t *out, uint32_t n)   /* no DC (before the limiter: a high-pass after it would overshoot
                                                   the ceiling), gain, soft knee, a peak limiter; mono to L and R */
{
    int32_t lim = V.lim, lg = V.lg, gain = K.gain, v, pk;
    uint32_t i, lcnt = V.lcnt, quiet = V.quiet, peak = tm_peak, still = V.stage == ST_OFF && !K.gfrz;
    for (i = 0; i < n; i++) {
        v = dcblock(blk_b[i], &V.ol_x, &V.ol_y);
        v = softk_q((int32_t)(((int64_t)v * gain) >> 15));
        pk = absi(v);
        if (pk > lim) { lim = pk; lcnt = 0; }
        else lim = LIM_CEIL + (int32_t)(((int64_t)(lim - LIM_CEIL) * LIM_REL_Q16) >> 16);
        if (lim > LIM_CEIL) {                   /* (the division: on a new peak, then every 4th sample of the release) */
            if (!(lcnt++ & 3u)) lg = (int32_t)((uint32_t)LIM_CEIL * 32768u / (uint32_t)lim);
            v = v * lg >> 15;
        }
        v = clampi(v, -LIM_CEIL, LIM_CEIL);
        peak = (uint32_t)pk > peak ? (uint32_t)pk : (peak * 4031u) >> 12;
        quiet = still && pk <= 2 ? quiet + 1u : 0u;
        blk_b[i] = v;
        out[2u * i] += v;
        out[2u * i + 1u] += v;
    }
    tm_gr = lim > LIM_CEIL ? lg : 32768;
    V.lim = lim; V.lg = lg; V.lcnt = lcnt; V.quiet = quiet; tm_peak = peak;
}
static void dsp_block(int32_t *out, uint32_t n) /* one block of the chain */
{
    uint32_t i, on = V.stage != ST_OFF || K.gfrz;
    int32_t v;
    if (V.quiet > 48000u && !on && rec_state != REC_ON) {   /* ~1.1 s of silence: the chain sleeps */
        tm_sleep = 1; tm_peak = 0; tm_lvl = 0;
        return;
    }
    tm_sleep = 0;
    stage_src(n);
    if (V.pquiet < 4096u || on || V.stage != ST_OFF) {   /* the voice's own chain; ~90 ms after it fell silent only */
        uint32_t pq = V.pquiet;                         /* the FX tails are left to compute */
        RAM_CALL(stage_pre)(n);
        if (K.rfmode) RAM_CALL(stage_flt)(n);
        if (K.ott && !K.opost) RAM_CALL(stage_ott)(n);
        if (V.stage == ST_OFF && !K.gfrz) { for (i = 0; i < n; i++) pq = absi(blk_b[i]) <= 1 ? pq + 1u : 0u; }
        else pq = 0;
        V.pquiet = pq;
    } else { for (i = 0; i < n; i++) blk_b[i] = 0; tm_lvl = 0; }
    RAM_CALL(stage_fx)(n);
    if (!K.rv_wet) V.rv_off = 1;
    if (K.ott && K.opost) RAM_CALL(stage_ott)(n);         /* POST FX: on the tails too */
    if (!K.ott) tm_gd[0] = tm_gd[1] = tm_gd[2] = 0;
    RAM_CALL(stage_out)(out, n);
    if (rec_state == REC_ON)                    /* the recorder: to 22.05 kHz through a [1 2 1] / 4 low-pass, mu-law */
        for (i = 0; i < n && rec_state == REC_ON; i++) {
            if (rec_odd ^= 1u) { rec_prev = blk_b[i]; continue; }
            v = (rec_pp + 2 * rec_prev + blk_b[i]) >> 2;
            rec_pp = blk_b[i];
            rec.d[rec_n++] = ulaw_enc(v);
            rec_quiet = (V.stage == ST_OFF || V.stage == ST_R) && absi(v) < 96 ? rec_quiet + 1u : 0u;
            if (rec_n >= SMP_MAX || rec_quiet > 13230u) rec_state = REC_DONE;
        }
}

/* ======================================================================== the audio interrupt */
static void app_audio(int32_t *out, uint32_t n)  /* (in flash: the control between the blocks; the blocks in RAM) */
{
    uint32_t i, m;
#ifndef DINGHY_HOST
    uint32_t t0 = fm1_ticks();
#endif
    for (i = 0; i < n; i += m) {
        m = n - i < BLK ? n - i : BLK;
        voice_events(lfo_block(m));
        dsp_block(out + 2u * i, m);
    }
    if (rec_state == REC_STOP) rec_state = REC_DONE;
    if (K.zmix) for (i = 0; i < 6u; i++) {      /* the modes ringing, for the screen */
        int32_t z = absi(V.zy1[i] >> 10), o = tm_z[i] - (tm_z[i] >> 3);
        tm_z[i] = z > o ? z : o;
    }
    tm_env = V.env >> 8;
    tm_stage = V.stage;
    tm_awake = V.stage != ST_OFF;
#ifndef DINGHY_HOST
    tm_cpu = (tm_cpu * 7u + (fm1_ticks() - t0) / FM1_TICKS_PER_US * 100u / 5805u) >> 3;
#endif
}

/* ======================================================================== notes: a mono stack, last note wins */
static int8_t stk[16];
static uint32_t nstk;
static void note_on(int32_t note, int32_t vel15)
{
    uint32_t i, j;
    for (i = j = 0; i < nstk; i++) if (stk[i] != note) stk[j++] = stk[i];
    nstk = j;
    if (nstk < 16u) stk[nstk++] = (int8_t)note;
    v_note = note; v_vel = vel15; v_gate = 1;
    v_seq++;
}
static void note_off(int32_t note)
{
    uint32_t i, j;
    for (i = j = 0; i < nstk; i++) if (stk[i] != note) stk[j++] = stk[i];
    nstk = j;
    if (!nstk) v_gate = 0;
    else if (v_note != stk[nstk - 1u]) { v_note = stk[nstk - 1u]; v_pseq++; }   /* back to the held one: legato */
}
static void all_off(void) { nstk = 0; v_gate = 0; }

/* ======================================================================== actions */
static uint8_t page, sub, mutant, session_dirty;
static uint8_t ui_dirty = 0xFF;                 /* 1 header 2 vis 4 knobs 8 footer */
static uint32_t t_toast, mutant_t;
static char toast[40];
static int16_t undo_buf[16][NP];
static uint32_t undo_n;
static int32_t oct_shift;

static void cat(char *s, const char *a);
static void say(const char *s)
{
    uint32_t i = 0;
    while (s[i] && i < sizeof toast - 1u) { toast[i] = s[i]; i++; }
    toast[i] = 0;
    t_toast = fm1_ms;
    ui_dirty |= 8;
}
static void touched(void) { ui_dirty |= 4 | 2 | 1; session_dirty = 1; }
static void push_undo(void)
{
    uint32_t i, p;
    if (undo_n == 16u) { for (i = 1; i < 16u; i++) for (p = 0; p < NP; p++) undo_buf[i - 1u][p] = undo_buf[i][p]; undo_n = 15; }
    for (p = 0; p < NP; p++) undo_buf[undo_n][p] = P[p];
    undo_n++;
}
static void set_unlocked(uint32_t p, int32_t v) { if (!lock_get(p)) P[p] = (int16_t)clampi(v, PD[p].lo, PD[p].hi); }
/* 14 CHIP: a new sound of one kind, the way sfxr (DrPetter) makes them: each kind a recipe, every value rolled in its
 * range. Played at C4 (TUNE moves it), through the source and the envelope only: the other modules stay as they are. */
static void chip_preset(uint32_t kind)
{
    static const struct { int8_t w0, w1, t0, t1, s0, s1, a0, a1, ar0, ar1, d0, d1, st; } R[7] = {
        /* wave lo hi (11 PLS, 12 TRI, 13 NSE), TUNE, SWEEP, ARP, ARP SPD, DECAY (the release: the same time), STEPS */
        { 11,  11,  12,  24,   0,   0,   8,   9,   3,   5,  28,  36,   1}, /* COIN: a blip that jumps up a 4th / 5th */
        { 11,  11,   0,  12,  35,  60,   0,   0,   1,   1,  26,  34,   1}, /* JUMP: up */
        { 11,  12,  12,  24, -90, -55,   0,   0,   1,   1,  22,  32,   1}, /* LASER: falling fast */
        { 13,  13, -12,  12, -60, -30,   0,   0,   1,   1,   6,  18,   1}, /* HIT: a noise burst, dropping */
        { 11,  12,   0,   7,  10,  25,   1,   4,   2,   3,  32,  42,   1}, /* POWERUP: rising arpeggio */
        { 13,  13, -24,  -8, -30, -10,   0,   0,   1,   1,  34,  46,   2}, /* EXPLODE: long falling noise, 30 Hz steps */
        { 11,  12,  12,  24,   0,   0,   0,   0,   1,   1,   4,  12,   1}, /* BLIP: a menu tick */
    };
    uint32_t k = kind % 7u, i;
    int32_t d;
    i = (uint32_t)rnd_range(R[k].w0, R[k].w1);
    set_unlocked(P_MODE, 0); set_unlocked(P_WAVE, (int32_t)i);
    set_unlocked(P_CDUTY, rnd_range(0, 2)); set_unlocked(P_CNOISE, k == 3u ? rnd_range(0, 1) : 0);
    set_unlocked(P_COLOR, i == W_CNSE ? rnd_range(10, 90) : 50);
    set_unlocked(P_TUNE, rnd_range(R[k].t0, R[k].t1)); set_unlocked(P_CSWEEP, rnd_range(R[k].s0, R[k].s1));
    set_unlocked(P_CARP, rnd_range(R[k].a0, R[k].a1)); set_unlocked(P_CARPS, rnd_range(R[k].ar0, R[k].ar1));
    set_unlocked(P_CSTEP, R[k].st);
    d = rnd_range(R[k].d0, R[k].d1);              /* SUSTAIN 0: a short tap and a held key both decay the same */
    set_unlocked(P_ATT, 0); set_unlocked(P_DEC, d); set_unlocked(P_SUS, 0);
    set_unlocked(P_REL, (d * 27 >> 5) + 6); set_unlocked(P_GLIDE, 0); set_unlocked(P_PITCH, 100);
}
static uint32_t aud_t;                          /* the audition's note-off time (0: none) */
static void audition(void)                      /* a C4 held 150 ms (the gate released at once would skip the attack) */
{
    if (v_gate) return;
    note_on(60, 32767);
    aud_t = fm1_ms + 150u; if (!aud_t) aud_t = 1;
}
static void rnd_module(uint32_t m)
{
    switch (m) {
    case PG_SRC: set_unlocked(P_WAVE, rnd_range(0, NWAVE - 1)); set_unlocked(P_BLEND, rnd_range(10, 90)); set_unlocked(P_COLOR, rnd_range(0, 100));
            set_unlocked(P_STR, rnd_range(50, 220)); break;
    case PG_RES: set_unlocked(P_ZMAT, rnd_range(0, 6)); set_unlocked(P_ZPITCH, rnd_range(-12, 12)); set_unlocked(P_ZDEC, rnd_range(15, 50));
            set_unlocked(P_ZMIX, rnd_range(35, 100)); set_unlocked(P_ZDAMP, rnd_range(0, 80)); set_unlocked(P_ZSPRD, rnd_range(25, 80));
            set_unlocked(P_ZHIT, rnd_range(4, 48)); break;
    case PG_ENV: set_unlocked(P_ATT, rnd_range(0, 40)); set_unlocked(P_DEC, rnd_range(15, 55)); set_unlocked(P_SUS, rnd_range(0, 100));
            set_unlocked(P_REL, rnd_range(10, 55)); set_unlocked(P_GLIDE, rnd_range(0, 99) > 70 ? rnd_range(10, 40) : 0); break;
    case PG_TRN: set_unlocked(P_TATK, rnd_range(-40, 100)); set_unlocked(P_TSUS, rnd_range(-100, 60)); set_unlocked(P_TTIME, rnd_range(25, 75));
            set_unlocked(P_TOUT, 0); break;
    case PG_FLT: set_unlocked(P_RFMODE, rnd_range(1, 4)); set_unlocked(P_RFPOLE, rnd_range(0, 1)); set_unlocked(P_RFSRC, rnd_range(0, 3));
            set_unlocked(P_RFFREQ, rnd_range(8, 55)); set_unlocked(P_RFRES, rnd_range(20, 92)); set_unlocked(P_RFDEP, rnd_range(-85, 85));
            set_unlocked(P_RFRATE, rnd_range(13, 58)); set_unlocked(P_RFDRV, rnd_range(0, 75)); break;
    case PG_DST: set_unlocked(P_XDRV, rnd_range(0, 48)); set_unlocked(P_XSHP, rnd_range(0, 3)); set_unlocked(P_XBITS, rnd_range(0, 99) > 50 ? rnd_range(4, 12) : 16);
            set_unlocked(P_XSR, rnd_range(0, 99) > 50 ? rnd_range(10, 50) : 0); set_unlocked(P_XRING, rnd_range(5, 60));
            set_unlocked(P_XRMIX, rnd_range(0, 99) > 60 ? rnd_range(20, 100) : 0); set_unlocked(P_XTONE, rnd_range(25, 63));
            set_unlocked(P_XMIX, rnd_range(40, 100)); break;
    case PG_GRN: set_unlocked(P_GPOS, rnd_range(8, 86)); set_unlocked(P_GSIZE, rnd_range(24, 195)); set_unlocked(P_GDENS, rnd_range(7, 40));
            set_unlocked(P_GSPRAY, rnd_range(0, 46)); set_unlocked(P_GREV, rnd_range(0, 85)); break;
    case PG_OTT: set_unlocked(P_OSTYLE, rnd_range(0, 99) > 30 ? 1 : rnd_range(2, 4)); set_unlocked(P_ODEPTH, rnd_range(20, 100));
            set_unlocked(P_OTIME, rnd_range(15, 85)); set_unlocked(P_OUP, rnd_range(20, 100)); set_unlocked(P_ODOWN, rnd_range(20, 100));
            set_unlocked(P_OIN, rnd_range(-12, 18)); set_unlocked(P_OTHR, rnd_range(-16, 16)); set_unlocked(P_OPOS, rnd_range(0, 99) > 70);
            set_unlocked(P_OLOW, rnd_range(-12, 8)); set_unlocked(P_OMID, rnd_range(-10, 8)); set_unlocked(P_OHIGH, rnd_range(-12, 8));
            set_unlocked(P_OOUT, rnd_range(-16, 0)); break;
    case PG_DLY: set_unlocked(P_DTIME, rnd_range(20, 60)); set_unlocked(P_DFB, rnd_range(10, 80)); set_unlocked(P_DMIX, rnd_range(0, 60));
            set_unlocked(P_DROUTE, rnd_range(0, 3)); set_unlocked(P_FRATE, rnd_range(5, 50)); set_unlocked(P_FDEPTH, rnd_range(20, 100));
            set_unlocked(P_FREGEN, rnd_range(-85, 85)); set_unlocked(P_FMIX, rnd_range(0, 80)); break;
    case PG_REV: set_unlocked(P_RSIZE, rnd_range(10, 100)); set_unlocked(P_RDEC, rnd_range(10, 55)); set_unlocked(P_RPRE, rnd_range(0, 80));
            set_unlocked(P_RMIX, rnd_range(5, 55)); set_unlocked(P_RLOW, rnd_range(10, 40) * 5); set_unlocked(P_RHIGH, rnd_range(5, 30) * 5);
            set_unlocked(P_RXLO, rnd_range(0, 31)); set_unlocked(P_RXHI, rnd_range(0, 31)); break;
    case PG_LFO: set_unlocked(P_L1SHP, rnd_range(0, 6)); set_unlocked(P_L1RATE, rnd_range(10, 55)); set_unlocked(P_L1MIX, rnd_range(5, 45));
            set_unlocked(P_L1TGT, rnd_range(0, NTGT - 1)); set_unlocked(P_L2SHP, rnd_range(0, 6)); set_unlocked(P_L2RATE, rnd_range(5, 50));
            set_unlocked(P_L2MIX, rnd_range(5, 45)); set_unlocked(P_L2TGT, rnd_range(0, NTGT - 1)); break;
    case PG_CHIP: chip_preset((uint32_t)rnd_range(0, 6)); break;
    case PG_PAD: set_unlocked(P_XTGT, rnd_range(0, NTGT - 1)); set_unlocked(P_YTGT, rnd_range(0, NTGT - 1));
            set_unlocked(P_XAMT, rnd_range(-100, 100)); set_unlocked(P_YAMT, rnd_range(-100, 100)); break;
    default: set_unlocked(P_VARY, rnd_range(10, 70)); break;   /* MORPH: only the variation */
    }
}
static void happy_accident(void)
{
    uint32_t m;
    push_undo();
    morph_on = 0;
    for (m = 0; m < NPAGE; m++) {
        if (m == PG_MRP || m == PG_PAD || m == PG_CHIP || (m == PG_LFO && rnd_range(0, 99) < 50)) continue;
        if (m == PG_RES && rnd_range(0, 99) < 65) { set_unlocked(P_ZMIX, 0); continue; }   /* the extra modules: sometimes */
        if (m == PG_DST && rnd_range(0, 99) < 60) { set_unlocked(P_XMIX, 0); continue; }
        if (m == PG_TRN && rnd_range(0, 99) < 50) { set_unlocked(P_TATK, 0); set_unlocked(P_TSUS, 0); continue; }
        rnd_module(m);
    }
    set_unlocked(P_MODE, rnd_range(0, 2));
    set_unlocked(P_GMODE, rnd_range(0, 99) > 70); set_unlocked(P_GFRZ, 0);
    set_unlocked(P_PITCH, rnd_range(70, 130)); set_unlocked(P_RNDP, rnd_range(0, 99) > 70);
    mutant = 1; mutant_t = fm1_ms;
    touched();
    say("HAPPY ACCIDENT");
    audition();
}
static void undo(void)
{
    uint32_t i;
    if (!undo_n) { say("NOTHING TO UNDO"); return; }
    undo_n--;
    morph_on = 0;
    for (i = 0; i < NP; i++) if (i != P_SLOT && i != P_THEME) P[i] = undo_buf[undo_n][i];
    touched();
    say("UNDO");
    audition();
}
static void reset_module(uint32_t m)
{
    uint32_t s, k, kept = 0;
    char t[24] = "RESET ", n[4];
    push_undo();
    morph_on = 0;
    for (s = 0; s < 4u; s++) for (k = 0; k < 4u; k++) {
        uint32_t p = PAGE_P[m][s][k];
        if (p == NO || p == P_SLOT) continue;
        if (lock_get(p)) kept++; else P[p] = PD[p].def;
    }
    n[0] = (char)('0' + (m + 1u) / 10u); n[1] = (char)('0' + (m + 1u) % 10u); n[2] = 0;
    cat(t, n);
    if (kept) { cat(t, " L"); n[0] = (char)('0' + kept % 10u); n[1] = 0; cat(t, n); }
    touched();
    say(t);
}
/* 12 MORPH: store A / B; BATCH: N hits, SPACE apart, each with its own VARY (and, A>B, a step from A to B) */
static void morph_set(int32_t v)                /* the MORPH knob: the panel shows the mix (unlocked parameters) */
{
    uint32_t i;
    P[P_MORPH] = (int16_t)clampi(v, 0, 100);
    if (has_ab != 3u) return;
    for (i = 0; i < NP; i++) {
        int32_t a = snapA[i], b = snapB[i];
        if (NOMORPH[i] || lock_get(i)) continue;
        P[i] = (int16_t)(CONT[i] ? a + ((b - a) * P[P_MORPH] + (b > a ? 50 : -50)) / 100 : P[P_MORPH] >= 50 ? b : a);
    }
    morph_on = 1;
}
static void morph_store(uint32_t which)
{
    uint32_t i;
    int16_t *d = which ? snapB : snapA;
    morph_on = 0;
    for (i = 0; i < NP; i++) d[i] = P[i];
    has_ab |= (uint8_t)(1u << which);
    session_dirty = 1;
    say(which ? (has_ab == 3u ? "STORED B: TURN MORPH" : "STORED B (NOW STORE A)") : (has_ab == 3u ? "STORED A: TURN MORPH" : "STORED A (NOW STORE B)"));
    ui_dirty |= 2 | 4 | 8;
}
static uint32_t batch_n, batch_i, batch_t0, batch_on, batch_ph, batch_qt, last_note = 60;
static const uint8_t BCNT[4] = {4, 8, 12, 16};
static void batch_start(void)
{
    if (batch_on) { batch_on = 0; vary_mute = 0; all_off(); say("BATCH STOPPED"); ui_dirty |= 2; return; }
    if (P[P_BMODE] && has_ab != 3u) { say("A>B: STORE A AND B FIRST"); return; }
    if (!P[P_BMODE] && !P[P_VARY]) say("BATCH (VARY 0: ALL THE SAME)");
    else say("BATCH: RECORD IT IN REAPER");
    batch_n = BCNT[P[P_BCNT] & 3]; batch_i = 0; batch_t0 = fm1_ms + 300u; batch_on = 1; batch_ph = 0;
    vary_mute = P[P_BMODE] == 1;
    ui_dirty |= 2;
}
static void batch_poll(void)                    /* from the main loop: hit, hold for GATE, then wait for SPACE (AUTO: */
{                                               /* until the tails are under -60 dB for 0.3 s), next */
    uint32_t sp = (uint32_t)P[P_BSPC] * 500u, gate = (sp ? sp : 2000u) * (uint32_t)P[P_BGATE] / 100u, t;
    if (!batch_on || (int32_t)(fm1_ms - batch_t0) < 0) return;
    t = fm1_ms - batch_t0;
    switch (batch_ph) {
    case 0:                                     /* the hit */
        if (batch_i >= batch_n) { batch_on = 0; vary_mute = 0; say("BATCH DONE"); ui_dirty |= 2; return; }
        if (P[P_BMODE]) morph_set((int32_t)(batch_n > 1u ? batch_i * 100u / (batch_n - 1u) : 0u));
        note_on((int32_t)last_note, 32767);
        batch_i++; batch_ph = 1; batch_t0 = fm1_ms; batch_qt = 0;
        ui_dirty |= 2 | 4;
        break;
    case 1:                                     /* held */
        if (t >= gate) { note_off((int32_t)last_note); batch_ph = 2; }
        break;
    default:                                    /* the space */
        if (sp) { if (t >= sp) batch_ph = 0; break; }
        if (tm_peak > 33u) batch_qt = 0;        /* (-60 dBFS) */
        else if (!batch_qt) batch_qt = fm1_ms;
        else if (fm1_ms - batch_qt >= 300u) batch_ph = 0;
        if (t > 20000u) batch_ph = 0;           /* (a tail that never ends: 20 s and on) */
        break;
    }
}
static void page_action(void)
{
    if (page == PG_SRC) {                       /* REC ARM / STOP */
        if (rec_state == REC_IDLE) { rec_state = REC_ARMED; say("REC ARMED: PLAY A NOTE"); }
        else if (rec_state == REC_ARMED) { rec_state = REC_IDLE; say("REC CANCELLED"); }
        else if (rec_state == REC_ON) rec_state = REC_STOP;
        ui_dirty |= 2 | 8;
        return;
    }
    if (page == PG_MRP) { batch_start(); return; }
    if (page == PG_CHIP) {                      /* a new sound of the PRESET's kind */
        char t[24] = "NEW ";
        push_undo();
        chip_preset((uint32_t)P[P_CPRE]);
        touched();
        cat(t, E_CPRE[P[P_CPRE] % 7]);
        say(t);
        audition();
        return;
    }
    push_undo();
    rnd_module(page);
    touched();
    say(page == PG_LFO ? "RND LFOs" : page == PG_PAD ? "RND PAD TARGETS" : "RANDOMIZED MODULE");
    audition();
}
static void rec_commit(void)                    /* the recording, trimmed and normalized, into the active slot (16-bit) */
{
    uint32_t n = rec_n, i, pk = 1, g;
    char t[32] = "RECORDED SLOT x (SAVE KEEPS)";
    while (n > 256u && absi(T_ULAW[rec.d[n - 1u]]) < 200) n--;
    if (n < 512u) { say("REC: NOTHING HEARD"); rec_state = REC_IDLE; return; }
    for (i = 0; i < n; i++) { uint32_t v = (uint32_t)absi(T_ULAW[rec.d[i]]); if (v > pk) pk = v; }
    g = 31000u * 4096u / pk;
    smp_busy = 1;
    for (i = 0; i < n; i++) smp16[i] = sat16((int32_t)(((int64_t)T_ULAW[rec.d[i]] * g) >> 12));
    for (i = n - 64u; i < n; i++) smp16[i] = (int16_t)(smp16[i] * (int32_t)(n - i) / 64);
    SL[cur_slot].user = 1; SL[cur_slot].dirty = 1; SL[cur_slot].len = (uint16_t)n;
    smp_ptr = smp16; smp_len = n;
    smp_busy = 0;
    smp_overview();
    rec_state = REC_IDLE;
    t[14] = (char)('1' + cur_slot);
    say(t);
    ui_dirty |= 2 | 4;
}
static void slot_factory(void)                  /* SEL + PLAY: the factory sound back */
{
    if (!SL[cur_slot].user) { say("ALREADY FACTORY"); return; }
    smp_busy = 1;
    smp_ptr = FSMP[cur_slot]; smp_len = FSMP_LEN[cur_slot];
    smp_busy = 0;
    rec.len = 0;
    slot_save((uint32_t)cur_slot, &rec, sizeof rec);
    SL[cur_slot].user = 0; SL[cur_slot].dirty = 0;
    smp_overview();
    say("FACTORY SAMPLE RESTORED");
    ui_dirty |= 2 | 4;
}

/* ======================================================================== the screen */
typedef struct { uint16_t acc, cyan, mut, haz, panel, box, border; } theme_t;
static const theme_t THEMES[4] = {
    {RGB(189, 112, 255), RGB(82, 250, 133), RGB(158, 255, 51), RGB(255, 138, 41), RGB(17, 17, 20), RGB(26, 26, 31), RGB(50, 48, 60)},
    {RGB(250, 71, 66), RGB(255, 199, 51), RGB(250, 97, 209), RGB(255, 214, 61), RGB(20, 16, 17), RGB(30, 24, 26), RGB(60, 43, 46)},
    {RGB(255, 173, 41), RGB(77, 209, 255), RGB(140, 250, 255), RGB(255, 122, 46), RGB(19, 18, 18), RGB(28, 28, 27), RGB(54, 51, 48)},
    {RGB(230, 125, 201), RGB(89, 224, 245), RGB(140, 255, 56), RGB(255, 194, 26), RGB(18, 18, 21), RGB(27, 27, 32), RGB(47, 47, 57)},
};
#define TH (THEMES[P[P_THEME] & 3])
#define C_TXT RGB(232, 236, 245)
#define C_MUTE RGB(140, 145, 165)
#define C_DK RGB(46, 46, 56)

static void fmt_num(char *s, int32_t v, int sign)
{
    char t[12];
    uint32_t i = 0, j = 0, u = (uint32_t)(v < 0 ? -v : v);
    if (v < 0) s[j++] = '-'; else if (sign) s[j++] = '+';
    do { t[i++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    while (i) s[j++] = t[--i];
    s[j] = 0;
}
static void cat(char *s, const char *a) { while (*s) s++; while (*a) *s++ = *a++; *s = 0; }
static void fmt_dec(char *s, int32_t v, uint32_t div, uint32_t places, int sign)
{
    int32_t whole = absi(v) / (int32_t)div, fr = absi(v) % (int32_t)div;
    char f[8];
    uint32_t k;
    fmt_num(s, v < 0 ? -whole : whole, sign);
    if (v < 0 && whole == 0) { s[0] = '-'; s[1] = '0'; s[2] = 0; }
    if (!places) return;
    cat(s, ".");
    for (k = 0; k < places; k++) { fr *= 10; f[k] = (char)('0' + fr / (int32_t)div); fr %= (int32_t)div; }
    f[places] = 0;
    cat(s, f);
}
static void fmt_hz(char *s, uint32_t hz)
{
    if (hz >= 1000u) { fmt_dec(s, (int32_t)hz, 1000u, hz >= 10000u ? 1u : 2u, 0); cat(s, "k"); }
    else { fmt_num(s, (int32_t)hz, 0); cat(s, "Hz"); }
}
static void fmt_ms(char *s, uint32_t ms)
{
    if (ms >= 1000u) { fmt_dec(s, (int32_t)ms, 1000u, ms >= 10000u ? 0u : 1u, 0); cat(s, "s"); }
    else { fmt_num(s, (int32_t)ms, 0); cat(s, "ms"); }
}
static void fmt_chz(char *s, uint32_t chz) { fmt_dec(s, (int32_t)chz, 100u, chz < 100u ? 2u : chz < 1000u ? 1u : 0u, 0); cat(s, "Hz"); }
static void slot_name(char *s, uint32_t i)
{
    s[0] = 0;
    if (SL[i].user) { cat(s, "USER "); s[5] = (char)('1' + i); s[6] = 0; }
    else cat(s, FSMP_NAME[i]);
}
static void fmt_param(uint32_t p, int32_t v, char *s)
{
    s[0] = 0;
    switch (PD[p].fmt) {
    case F_ENUM: cat(s, PD[p].en[clampi(v, PD[p].lo, PD[p].hi)]); break;
    case F_PCT: fmt_num(s, v, 0); cat(s, "%"); break;
    case F_SPR: fmt_num(s, v * 2, 0); cat(s, "%"); break;
    case F_PM: fmt_num(s, v, 1); cat(s, "%"); break;
    case F_X100: fmt_dec(s, v, 100u, 2u, 0); cat(s, "x"); break;
    case F_ATT: fmt_ms(s, T_ATT_MS[v]); break;
    case F_DEC: fmt_ms(s, T_DEC_MS[v]); break;
    case F_REL: fmt_ms(s, T_REL_MS[v]); break;
    case F_GLIDE: if (v) fmt_ms(s, T_GLIDE_MS[v]); else cat(s, "OFF"); break;
    case F_DTIME: fmt_ms(s, T_DTIME_MS[v]); break;
    case F_RDEC: fmt_ms(s, T_RDEC_MS[v]); break;
    case F_MS: fmt_num(s, v, 0); cat(s, "ms"); break;
    case F_DENS: fmt_num(s, v, 0); cat(s, "/s"); break;
    case F_RFHZ: fmt_hz(s, T_HZ[T_RF_LI[v]]); break;
    case F_XLO: fmt_hz(s, T_HZ[T_XLO_LI[v]]); break;
    case F_XHI: fmt_hz(s, T_HZ[T_XHI_LI[v]]); break;
    case F_RATE: fmt_chz(s, T_RATE_CHZ[v]); break;
    case F_FRATE: fmt_chz(s, T_FRATE_CHZ[v]); break;
    case F_LRATE: fmt_chz(s, T_LRATE_CHZ[v]); break;
    case F_DBH: fmt_dec(s, v * 5, 10u, 1u, 1); cat(s, "dB"); break;
    case F_SEMI: fmt_num(s, v, 1); cat(s, "st"); break;
    case F_SLOT: slot_name(s, (uint32_t)v); break;
    case F_OXLO: fmt_hz(s, T_HZ[T_OXLO_LI[v]]); break;
    case F_OXHI: fmt_hz(s, T_HZ[T_OXHI_LI[v]]); break;
    case F_TSC: fmt_dec(s, (int32_t)((100u * exp2_q16(((v - 50) * 256 * 17) >> 8)) >> 16), 100u, 2u, 0); cat(s, "x"); break;
    case F_BLEND: cat(s, "S"); fmt_num(s + 1, v, 0); cat(s, "%"); break;
    case F_TGT: { uint32_t t = TGT[clampi(v, 0, NTGT - 1)]; fmt_num(s, PMOD[t] + 1, 0); cat(s, " "); cat(s, PD[t].name); break; }
    case F_ZDEC: fmt_ms(s, zdec_ms(v * 256)); break;
    case F_BITS: if (v >= 16) cat(s, "OFF"); else { fmt_num(s, v, 0); cat(s, "bit"); } break;
    case F_XSR: if (!v) cat(s, "OFF"); else fmt_hz(s, (uint32_t)((44100u * exp2_q16(-(v * 31)) + 32768u) >> 16)); break;
    case F_XRHZ: fmt_hz(s, (20u * exp2_q16(v * 32)) >> 16); break;
    case F_BSPC: if (!v) cat(s, "AUTO"); else fmt_ms(s, (uint32_t)v * 500u); break;
    default: fmt_num(s, v, 0); break;
    }
}
static void cv_text_fit(int32_t x, int32_t y, int32_t w, const char *s, uint16_t c, int align)
{
    char t[40];
    uint32_t n = 0;
    while (s[n] && n < sizeof t - 1u) { t[n] = s[n]; n++; }
    t[n] = 0;
    while (n > 1u && text_w(&FONT_S, t) > w) t[--n] = 0;
    if (align == 1) x += (w - text_w(&FONT_S, t)) / 2;
    else if (align == 2) x += w - text_w(&FONT_S, t);
    cv_text(x, y, &FONT_S, t, c);
}
static void cv_frame(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    cv_rect(x, y, w, 1, c); cv_rect(x, y + h - 1, w, 1, c); cv_rect(x, y, 1, h, c); cv_rect(x + w - 1, y, 1, h, c);
}

static void draw_header(void)
{
    const theme_t *th = &TH;
    char s[8];
    cv_begin(240, 40, th->panel);
    cv_rect(4, 4, 7, 14, RGB(8, 8, 10)); cv_rect(5, 6, 2, 10, th->acc); cv_rect(8, 9, 2, 7, th->cyan);
    cv_text(15, 2, &FONT_S, "ND-07", mutant ? th->mut : th->acc);
    cv_text(63, 2, &FONT_S, "// MONOLITH", C_TXT);
    cv_text_fit(180, 2, 56, morph_on ? "MORPH" : E_MODE[P[P_MODE]], morph_on ? th->mut : th->cyan, 2);
    cv_rect(0, 20, 240, 1, th->border);
    s[0] = (char)('0' + (page + 1u) / 10u); s[1] = (char)('0' + (page + 1u) % 10u); s[2] = 0;
    cv_text(4, 22, &FONT_S, s, mutant ? th->mut : th->acc);
    cv_text_fit(26, 22, 236 - 19 * (int32_t)NSUB[page] - 26, PAGE_NAME[page], C_TXT, 0);
    {
        uint32_t k;
        for (k = 0; k < NSUB[page]; k++) {     /* A B (C D): which knob set */
            int32_t x = 240 - (int32_t)(NSUB[page] - k) * 19;
            char l[2];
            l[0] = (char)('A' + k); l[1] = 0;
            cv_rect(x, 24, 16, 13, sub == k ? th->acc : C_DK);
            cv_text(x + 4, 22, &FONT_S, l, sub == k ? RGB(10, 10, 12) : C_MUTE);
        }
    }
    cv_rect(0, 39, 240, 1, th->border);
    cv_blit(0, 0);
}
static void vis_crt(const theme_t *th)
{
    int32_t y;
    cv_begin(240, 112, RGB(5, 5, 7));
    for (y = 6; y < 110; y += 4) cv_rect(3, y, 234, 1, RGB(10, 10, 13));
    cv_frame(1, 1, 238, 110, RGB(12, 12, 14));
    cv_frame(0, 0, 240, 112, th->border);
}
static void vis_smp(uint16_t c, int32_t x0, int32_t w, int32_t y0, int32_t h)
{
    int32_t i, mid = y0 + h / 2;
    for (i = 0; i < w; i++) {
        int32_t a = smp_view[i * 228 / w] * h / 2 / 128;
        if (a) cv_rect(x0 + i, mid - a, 1, 2 * a, c);
    }
}
static int32_t meter_px(uint32_t v, int32_t h)
{
    int32_t db = v ? ((ilog2_q8(v) - 15 * 256) * 1541) >> 8 : -48 * 256;
    return clampi((db + 48 * 256) * h / (48 * 256), 0, h);
}
static int32_t wave_shape(uint32_t w, uint32_t ph, int32_t m)   /* the oscillator drawn (no band limiting) */
{
    int32_t t = (int32_t)(ph >> 16);
    switch (w) {
    case 0: return isin(ph);
    case 1: return (t < 32768 ? t * 2 : (65535 - t) * 2) - 32768;
    case 2: case 5: return t - 32768;
    case 3: return t < 32768 ? 30000 : -30000;
    case 4: return ph < 0x0CCCCCCCu + (uint32_t)m * 108000u ? 30000 : -30000;
    case 6: return (int32_t)(xrand(&rng_main) >> 16) - 32768;
    case 7: return (((t - 32768) * (11469 + (m * 14746 >> 15))) >> 15) + (isin(ph) * 18022 >> 15) + ((isin(ph << 1) * m >> 15) * 11469 >> 15);
    case 8: return (isin((uint32_t)(((uint64_t)ph * (98304u + (uint32_t)m * 13u)) >> 16)) * (int32_t)T_HANN[ph >> 24] >> 15) + (isin(ph) >> 1);
    case 9: return isin(ph + (uint32_t)(int32_t)(((int64_t)isin(ph * (2u + (uint32_t)(m * 6 >> 15))) * (273426110u + (uint32_t)m * 75098u)) >> 15));
    case W_CPLS: return ph < (P[P_CDUTY] == 3 ? 0xC0000000u : 0x20000000u << P[P_CDUTY]) ? 30000 : -30000;
    case W_CTRI: { int32_t st = (int32_t)(ph >> 27); return ((st < 16 ? 15 - st : st - 16) * 2 - 15) * 2184; }
    case W_CNSE: return (xrand(&rng_main) >> 31) ? 26000 : -26000;
    default: return isin(ph) >> 1;
    }
}
static void draw_vis(void)
{
    const theme_t *th = &TH;
    char s[40], t[16];
    int32_t i, k;
    vis_crt(th);
    switch (page) {
    case PG_SRC: {                              /* the oscillator | the sample, the blend underneath */
        int32_t md = P[P_MODE], px = 0, py = 0, on_o = md != 1, on_s = md != 0;
        uint16_t co = on_o ? th->acc : C_DK, cs = on_s ? th->cyan : C_DK;
        for (i = 0; i <= 108; i++) {
            int32_t y = 58 - wave_shape((uint32_t)P[P_WAVE], (uint32_t)i * 39768216u, P[P_COLOR] * 327) * 34 / 32768;
            if (i) cv_line(6 + px, py, 6 + i, y, co);
            px = i; py = y;
        }
        cv_rect(119, 16, 1, 84, C_DK);
        vis_smp(cs, 124, 110, 24, 68);
        if (on_s && tm_awake && !P[P_GMODE]) cv_rect(124 + (int32_t)tm_play * 110 / 32768, 18, 1, 80, C_TXT);
        cv_text_fit(8, 0, 108, E_WAVE[P[P_WAVE]], co, 0);
        slot_name(t, (uint32_t)P[P_SLOT]);
        s[0] = (char)('1' + P[P_SLOT]); s[1] = ' '; s[2] = 0; cat(s, t);
        cv_text_fit(124, 0, 110, s, cs, 0);
        cv_rect(6, 102, 228, 6, C_DK);
        if (md == 2) { k = P[P_BLEND] * 228 / 100; cv_rect(6, 102, 228 - k, 6, th->acc); cv_rect(234 - k, 102, k, 6, th->cyan); }
        else cv_rect(md ? 120 : 6, 102, 114, 6, md ? th->cyan : th->acc);
        if (rec_state) {
            s[0] = 0; cat(s, rec_state == REC_ARMED ? "ARMED" : rec_state == REC_USB ? "USB " : "REC ");
            if (rec_state != REC_ARMED) { fmt_dec(t, (int32_t)(rec_n * 100u / 22050u), 100u, 1u, 0); cat(s, t); cat(s, "s"); }
            cv_rect(150, 84, 84, 16, RGB(120, 16, 30));
            cv_text_fit(152, 84, 80, s, C_TXT, 1);
        }
        break;
    }
    case PG_ENV: {                              /* ADSR and where the voice is on it */
        int32_t a = 6 + P[P_ATT] * 50 / 63, d = a + 6 + P[P_DEC] * 50 / 63, r0 = 170, r1 = r0 + 6 + P[P_REL] * 56 / 63;
        int32_t top = 22, bot = 104, sy = bot - P[P_SUS] * (bot - top) / 100, ey;
        cv_line(6, bot, a, top, th->acc); cv_line(a, top, d, sy, th->acc); cv_line(d, sy, r0, sy, th->acc); cv_line(r0, sy, r1, bot, th->acc);
        cv_text(8, 0, &FONT_S, "ADSR", th->acc);
        fmt_param(P_GLIDE, P[P_GLIDE], t); s[0] = 0; cat(s, "GLIDE "); cat(s, t); cv_text_fit(110, 0, 124, s, C_MUTE, 2);
        if (tm_awake) {
            ey = bot - tm_env * (bot - top) / 32768;
            k = tm_stage == ST_A ? a : tm_stage == ST_D ? d + (r0 - d) / 2 : r0 + (r1 - r0) / 2;
            cv_rect(k - 3, ey - 3, 7, 7, th->cyan);
        }
        break;
    }
    case PG_FLT: {                              /* the response, the cutoff moving */
        int32_t li = P[P_RFMODE] ? (int32_t)tm_rf_li : T_RF_LI[P[P_RFFREQ]], cut = (li - LI_55) * 228 / (LI_MAX - LI_55);
        int32_t px = 0, py = 0, mode = P[P_RFMODE], res = P[P_RFRES], steep = P[P_RFPOLE] ? 14 : 8, mid = 64;
        for (i = 0; i <= 57; i++) {
            int32_t x = i * 4, d = x - cut, resp, y, bump = 0, dd = d * d;
            if (dd < 900) bump = res * (900 - dd) / 900 * 34 / 96;
            resp = mode == 1 ? (d <= 0 ? 0 : -d * steep / 20) : mode == 2 ? -absi(d) * steep * 23 / 400 : mode == 3 ? (d >= 0 ? 0 : d * steep / 20)
                 : mode == 4 ? -(dd < 400 ? (400 - dd) * 60 / 400 : 0) : 0;
            if (mode && mode != 4) resp += bump;
            y = clampi(mid - resp, 18, 106);
            cv_rect(6 + x, y, 4, 106 - y, mode ? RGB(14, 40, 30) : RGB(26, 26, 30));
            if (i) cv_line(6 + px, py, 6 + x, y, mode ? th->cyan : C_MUTE);
            px = x; py = y;
        }
        if (mode) cv_rect(6 + cut, 16, 1, 92, th->acc);
        cv_text(8, 0, &FONT_S, P[P_RFPOLE] ? "ZDF 24dB SVF" : "ZDF 12dB SVF", th->acc);
        fmt_hz(s, T_HZ[clampi(li, 0, 1023)]); cv_text_fit(150, 0, 84, s, th->cyan, 2);
        break;
    }
    case PG_GRN:                                /* the cloud on the sample */
        vis_smp(RGB(60, 40, 90), 6, 228, 18, 88);
        k = 6 + P[P_GPOS] * 228 / 100; i = P[P_GSPRAY] * 228 / 100;
        cv_rect(k - i / 2 < 6 ? 6 : k - i / 2, 18, i ? i : 1, 88, RGB(40, 28, 64));
        cv_rect(k, 16, 1, 92, th->acc);
        for (i = 0; i < 16; i++) {
            uint32_t gp = tm_gpos[i];
            if (gp) cv_rect(6 + (int32_t)gp * 228 / 32768 - 2, 22 + (i * 19) % 80, 4, 4, tm_grev[i] ? C_TXT : th->cyan);
        }
        cv_text(8, 0, &FONT_S, P[P_GFRZ] ? "FREEZE" : P[P_GMODE] ? "GRAIN ON" : "GRAIN OFF", P[P_GFRZ] ? th->cyan : th->acc);
        if (P[P_MODE] == 0) cv_text_fit(110, 0, 124, "(SOURCE: OSC)", C_MUTE, 2);
        break;
    case PG_OTT: {                              /* OTT: per band, the level and what the compressor does to it */
        static const char *const B[3] = {"L", "M", "H"};
        int32_t on = P[P_OSTYLE] != 0;
        s[0] = 0; cat(s, E_OSTY[P[P_OSTYLE]]);
        if (on) { cat(s, P[P_OPOS] ? " POST" : " PRE"); }
        cv_text_fit(8, 0, 110, s, on ? th->acc : C_MUTE, 0);
        fmt_hz(s, T_HZ[T_OXLO_LI[P[P_OXLO]]]); cat(s, "|"); fmt_hz(t, T_HZ[T_OXHI_LI[P[P_OXHI]]]); cat(s, t);
        cv_text_fit(110, 0, 124, s, th->cyan, 2);
        for (k = 0; k < 3; k++) {
            int32_t y = 22 + (2 - k) * 29, gd = on ? tm_gd[k] : 0, w = clampi(gd * 90 / (24 * 256), -90, 90);
            int32_t lv = clampi((tm_benv[k] + 60 * 256) * 180 / (60 * 256), 0, 180);
            int32_t muted = P[P_OSOLO] && P[P_OSOLO] != k + 1;
            cv_text(8, y + 2, &FONT_S, B[k], muted ? C_DK : C_TXT);
            cv_rect(26, y, 180, 22, RGB(16, 16, 20));
            if (on) cv_rect(26, y, lv, 22, RGB(30, 30, 38));
            if (!muted) { if (w >= 0) cv_rect(116, y + 4, w, 14, th->cyan); else cv_rect(116 + w, y + 4, -w, 14, th->acc); }
            cv_rect(116, y - 1, 1, 24, C_TXT);
            if (absi(gd) >= 10 * 256) fmt_num(s, gd / 256, 1); else fmt_dec(s, gd * 10 / 256, 10u, 1u, 1);
            cv_text_fit(206, y + 2, 32, muted ? "SOLO" : s, C_MUTE, 2);
        }
        break;
    }
    case PG_DLY: {                              /* the repeats over 2 s, the flanger's comb */
        int32_t tms = T_DTIME_MS[P[P_DTIME]], h = 60 * P[P_DMIX] / 100 + 6, x, comb;
        cv_text_fit(8, 0, 140, E_ROUTE[P[P_DROUTE]], th->acc, 0);
        fmt_ms(t, (uint32_t)tms); cv_text_fit(150, 0, 84, t, th->cyan, 2);
        for (x = tms, k = 0; x <= 2000 && k < 40 && h > 1; x += tms, k++) {
            int32_t px = 8 + x * 224 / 2000;
            cv_rect(px - 1, 70 - h, 3, h, th->cyan);
            h = h * P[P_DFB] / 100;
        }
        cv_rect(8, 70, 224, 1, C_DK);
        comb = (tm_fd >> 8) + 1;                /* notches every fs / delay */
        for (k = 1; k < 24; k++) {
            int32_t f = (2 * k - 1) * 22050 / comb, px;
            if (f > 20000) break;
            px = 8 + (ilog2_q8((uint32_t)f) - ilog2_q8(40u)) * 224 / (ilog2_q8(20000u) - ilog2_q8(40u));
            if (px >= 8) cv_rect(px, 78, 2, 26, P[P_FMIX] ? th->acc : C_DK);
        }
        cv_text(8, 92, &FONT_S, "FLG", P[P_FMIX] ? th->acc : C_MUTE);
        break;
    }
    case PG_REV: {                              /* LOW / MID / HIGH decay, the tail */
        static const char *const B[3] = {"LOW", "MID", "HIGH"};
        uint32_t mid = T_RDEC_MS[P[P_RDEC]], tt[3];
        tt[0] = mid * (uint32_t)P[P_RLOW] / 100u; tt[1] = mid; tt[2] = mid * (uint32_t)P[P_RHIGH] / 100u;
        cv_text(8, 0, &FONT_S, "FDN x4 // 3-BAND", th->acc);
        for (k = 0; k < 3; k++) {
            int32_t y = 22 + k * 24, w = clampi((ilog2_q8(tt[k]) - ilog2_q8(100u)) * 150 / (ilog2_q8(40000u) - ilog2_q8(100u)), 2, 150);
            cv_text(8, y + 1, &FONT_S, B[k], C_TXT);
            cv_rect(52, y + 3, 150, 14, RGB(16, 16, 20));
            cv_rect(52, y + 3, w, 14, k == 1 ? th->acc : th->cyan);
            fmt_ms(s, tt[k]); cv_text_fit(204, y + 1, 32, s, C_MUTE, 2);
        }
        k = meter_px(tm_rv, 220);
        cv_rect(8, 98, 224, 6, C_DK); cv_rect(8, 98, k, 6, th->cyan);
        break;
    }
    case PG_LFO: {                              /* the two LFOs, their targets */
        uint32_t l;
        for (l = 0; l < 2u; l++) {
            int32_t y0 = 6 + (int32_t)l * 52, shp = P[l ? P_L2SHP : P_L1SHP], mix = P[l ? P_L2MIX : P_L1MIX], px = 0, py = 0;
            uint32_t tg = TGT[P[l ? P_L2TGT : P_L1TGT]];
            uint16_t c = mix ? (l ? th->cyan : th->acc) : C_DK;
            for (i = 0; i <= 96; i++) {
                uint32_t ph = (uint32_t)i * 44739243u;
                int32_t t2 = (int32_t)(ph >> 16), v;
                v = shp == 0 ? isin(ph) : shp == 1 ? (t2 < 32768 ? t2 * 2 : (65535 - t2) * 2) - 32768 : shp == 2 ? t2 - 32768
                  : shp == 3 ? 32767 - t2 : shp == 4 ? (t2 < 32768 ? 30000 : -30000) : ((i / 12) * 7919 % 60000) - 30000;
                v = y0 + 26 - v * 18 / 32768;
                if (i) cv_line(8 + px, py, 8 + i, v, c);
                px = i; py = v;
            }
            cv_rect(8 + 96 * (int32_t)(tm_lfo[l] + 32768) / 65536, y0 + 4, 2, 44, mix ? C_TXT : C_DK);
            s[0] = 'L'; s[1] = (char)('1' + l); s[2] = ' '; s[3] = 0; cat(s, PD[tg].name);
            cv_text_fit(112, y0 + 4, 122, s, c, 0);
            fmt_param(tg, E[tg], t);
            s[0] = 0; cat(s, "= "); cat(s, t);
            cv_text_fit(112, y0 + 24, 122, s, mix ? C_TXT : C_DK, 0);
        }
        break;
    }
    case PG_RES: {                              /* the modes: where they sit (log Hz), how loud, ringing now */
        uint32_t mat = (uint32_t)P[P_ZMAT], spr = 128u + (uint32_t)P[P_ZSPRD] * 256u / 100u;
        int32_t on = P[P_ZMIX] != 0, f0 = P[P_ZTRK] ? 262 : 131;
        cv_text_fit(8, 0, 100, E_ZMAT[mat], on ? th->acc : C_MUTE, 0);
        fmt_param(P_ZDEC, P[P_ZDEC], t); s[0] = 0; cat(s, P[P_ZTRK] ? "KEY " : "C3 "); cat(s, t);
        cv_text_fit(110, 0, 124, s, th->cyan, 2);
        for (i = 0; i < 6; i++) {             /* 30 Hz .. 16 kHz across */
            int32_t rq = 1024 + (((int32_t)ZRAT[mat][i] - 1024) * (int32_t)spr >> 8), f = f0 * rq / 1024, x, h, z;
            if (f > 16000) break;
            x = 10 + (ilog2_q8((uint32_t)(f < 30 ? 30 : f)) - ilog2_q8(30u)) * 216 / (ilog2_q8(16000u) - ilog2_q8(30u));
            h = (int32_t)ZAMP[mat][i] * 70 / 32768;
            z = clampi(tm_z[i] * 70 / 6000, 0, 80);
            cv_rect(x - 4, 102 - h, 9, h, on ? RGB(30, 30, 44) : RGB(22, 22, 26));
            if (on && z) cv_rect(x - 3, 102 - z, 7, z, th->cyan);
            cv_rect(x, 102 - h, 1, h, on ? th->acc : C_DK);
        }
        cv_rect(6, 103, 228, 1, C_DK);
        break;
    }
    case PG_TRN: {                              /* a hit's envelope, and the same shaped; the gain now */
        int32_t px = 0, py = 0, qy = 0, ga = P[P_TATK], gs = P[P_TSUS], gl;
        cv_text(8, 0, &FONT_S, "TRANSIENT SHAPER", ga || gs ? th->acc : C_MUTE);
        for (i = 0; i <= 150; i++) {
            int32_t e = i < 6 ? i * 5461 : (32767 * (150 - i) / 144) * (150 - i) / 150, at = i < 18 ? (18 - i) * 32767 / 18 : 0;
            int32_t su = i > 12 ? (i - 12) * 32767 / 138 : 0, g, y0, y1;
            g = 32768 + ga * at / 140 + gs * su / 140;
            y0 = 100 - e * 76 / 32768;
            y1 = 100 - clampi((int32_t)((int64_t)e * g >> 15), 0, 40000) * 76 / 40000;
            if (i) { cv_line(6 + px, py, 6 + i, y0, C_DK); cv_line(6 + px, qy, 6 + i, y1, th->cyan); }
            px = i; py = y0; qy = y1;
        }
        gl = clampi(tm_tg * 40 / (24 * 256), -40, 40);
        cv_rect(196, 20, 24, 84, RGB(16, 16, 20)); cv_rect(194, 62, 28, 1, C_TXT);
        if (gl >= 0) cv_rect(198, 62 - gl, 20, gl, th->acc); else cv_rect(198, 62, 20, -gl, th->cyan);
        fmt_dec(s, tm_tg * 10 / 256, 10u, 1u, 1); cv_text_fit(166, 0, 70, s, C_MUTE, 2);
        break;
    }
    case PG_DST: {                              /* the shaper's curve | a cycle through bits and rate */
        int32_t px = 0, py = 0, drv = (int32_t)T_DB2LIN[clampi(P[P_XDRV] * 2 + 288, 0, 480)] >> 4, sh = 16 - P[P_XBITS];
        uint32_t sri = exp2_q16(-(P[P_XSR] * 31)), acc = 0;
        int32_t hold = 0, on = P[P_XMIX] != 0;
        cv_text_fit(8, 0, 110, E_XSHP[P[P_XSHP]], on ? th->acc : C_MUTE, 0);
        for (i = 0; i <= 100; i++) {
            int32_t x = (i - 50) * 655, y = (x * drv) >> 8, u;
            switch (P[P_XSHP]) {
            case 0: y = tanh_q(y); break;
            case 1: y = clampi(y, -30000, 30000); break;
            case 2: u = (y + 32768) & 0x1FFFF; y = u < 65536 ? u - 32768 : 98303 - u; break;
            default: y = absi(tanh_q(y)) * 2 - 32767; break;
            }
            y = 60 - y * 38 / 32768;
            if (i) cv_line(8 + px, py, 8 + i, y, on ? th->acc : C_DK);
            px = i; py = y;
        }
        cv_rect(8, 60, 101, 1, C_DK); cv_rect(58, 20, 1, 80, C_DK);
        px = 0; py = 0;
        for (i = 0; i <= 110; i++) {           /* a 1 kHz sine (44 samples a cycle), through BITS and RATE */
            int32_t y = isin((uint32_t)i * 39045157u) * 30000 >> 15;
            if (sh > 0) y = ((y + (1 << (sh - 1))) >> sh) << sh;
            if (sri < 65535u) { acc += sri * 2u / 5u; if (acc >= 65536u || i == 0) { acc &= 65535u; hold = y; } y = hold; }
            y = 60 - y * 36 / 32768;
            if (i) cv_line(122 + px, py, 122 + i, y, on ? th->cyan : C_DK);
            px = i; py = y;
        }
        s[0] = 0; fmt_param(P_XBITS, P[P_XBITS], t); cat(s, t); cat(s, " "); fmt_param(P_XSR, P[P_XSR], t); cat(s, t);
        cv_text_fit(122, 0, 112, s, th->cyan, 2);
        if (P[P_XRMIX]) { fmt_param(P_XRING, P[P_XRING], t); s[0] = 0; cat(s, "RING "); cat(s, t); cv_text_fit(122, 96, 112, s, th->mut, 2); }
        break;
    }
    case PG_MRP: {                              /* A and B, the morph between them, the batch */
        uint32_t nd = 0, q;
        int32_t x = 20 + P[P_MORPH] * 200 / 100;
        for (q = 0; q < NP; q++) if (!NOMORPH[q] && snapA[q] != snapB[q]) nd++;
        cv_rect(8, 18, 40, 30, has_ab & 1u ? th->acc : C_DK); cv_text(22, 24, &FONT_S, "A", has_ab & 1u ? RGB(10, 10, 12) : C_MUTE);
        cv_rect(192, 18, 40, 30, has_ab & 2u ? th->cyan : C_DK); cv_text(206, 24, &FONT_S, "B", has_ab & 2u ? RGB(10, 10, 12) : C_MUTE);
        cv_rect(52, 32, 136, 2, C_DK);
        cv_rect(52, 32, P[P_MORPH] * 136 / 100, 2, morph_on ? th->mut : C_MUTE);
        x = 52 + clampi(E[P_MORPH], 0, 100) * 136 / 100;
        cv_rect(x - 2, 26, 5, 14, morph_on ? C_TXT : C_MUTE);
        if (has_ab == 3u) { fmt_num(t, (int32_t)nd, 0); s[0] = 0; cat(s, t); cat(s, " PARAMS DIFFER"); }
        else { s[0] = 0; cat(s, "SEL+OCT- STORE A  SEL+OCT+ B"); }
        cv_text_fit(8, 0, 226, s, C_MUTE, 1);
        if (batch_on) {                         /* the hits so far */
            uint32_t w = 220u / batch_n;
            for (q = 0; q < batch_n; q++) cv_rect(10 + (int32_t)(q * w), 60, (int32_t)w - 3, 18, q < batch_i ? th->acc : C_DK);
            fmt_num(t, (int32_t)batch_i, 0); s[0] = 0; cat(s, "BATCH "); cat(s, t); cat(s, "/"); fmt_num(t, (int32_t)batch_n, 0); cat(s, t);
            cv_text_fit(8, 84, 226, s, th->mut, 1);
        } else {
            s[0] = 0; cat(s, "SEL: BATCH x"); cat(s, E_BCNT[P[P_BCNT] & 3]); cat(s, " "); cat(s, E_BMODE[P[P_BMODE]]);
            cv_text_fit(8, 64, 226, s, C_TXT, 1);
            fmt_param(P_BSPC, P[P_BSPC], t); s[0] = 0; cat(s, "SPACE "); cat(s, t); cat(s, "  VARY "); fmt_num(t, P[P_VARY], 0); cat(s, t); cat(s, "%");
            cv_text_fit(8, 84, 226, s, C_MUTE, 1);
        }
        break;
    }
    case PG_CHIP: {                             /* the pitch over the first 48 frames (0.8 s): SWEEP and ARP, as played */
        int32_t sw = 0, csw = P[P_CSWEEP] * (P[P_CSWEEP] < 0 ? -P[P_CSWEEP] : P[P_CSWEEP]) * 16 / 150, ai = 0, n = 0;
        int32_t ar = P[P_CARP] % NARP, prev = -1, w = P[P_WAVE] >= W_CPLS;
        cv_rect(8, 4, 98, 1, C_DK); cv_rect(8, 54, 98, 1, C_DK); cv_rect(8, 104, 98, 1, C_DK);
        for (i = 0; i < 48; i++) {               /* 1 octave = 25 px; the middle line = the note */
            int32_t off = (sw >> 4) + (ar ? ARP_T[ar][ai % ARP_N[ar]] * 256 / 12 : 0), y = clampi(54 - off * 25 / 256, 4, 104);
            cv_rect(8 + i * 2, y - 1, 2, 3, th->acc);
            if (prev >= 0 && prev != y) cv_rect(8 + i * 2, prev < y ? prev : y, 1, absi(prev - y), th->cyan);
            prev = y;
            sw = clampi(sw + csw, -5 * 4096, 4 * 4096);
            if (ar && ++n >= P[P_CARPS]) { n = 0; ai = (int32_t)arp_next((uint32_t)ar, (uint32_t)ai); }
        }
        cv_text_fit(116, 0, 118, E_CPRE[P[P_CPRE] % 7], th->acc, 2);
        cv_text_fit(116, 26, 118, "SEL = NEW", C_MUTE, 1);
        cv_text_fit(116, 46, 118, w ? E_WAVE[P[P_WAVE]] : "WAVE: ANY", w ? th->cyan : C_MUTE, 1);
        s[0] = 0; cat(s, "VOL "); cat(s, E_CSTEP[P[P_CSTEP] & 3]);
        cv_text_fit(116, 64, 118, s, P[P_CSTEP] ? C_TXT : C_MUTE, 1);
        cv_text_fit(116, 84, 118, "0.8 s", C_MUTE, 1);
        break;
    }
    case PG_PAD: {                              /* the XY field, where the finger is; the routings; the keys */
        int32_t on = pad_on || pad_held, px = 10 + pad_x * 100 / 127, py = 104 - pad_y * 100 / 127;
        cv_rect(8, 2, 104, 104, C_DK);
        cv_rect(10, 4, 100, 100, RGB(10, 10, 12));
        for (i = 1; i < 4; i++) { cv_rect(10 + i * 25, 4, 1, 100, C_DK); cv_rect(10, 4 + i * 25, 100, 1, C_DK); }
        cv_rect(px - 1, 4, 2, 100, on ? th->acc : C_DK);
        cv_rect(10, py - 1, 100, 2, on ? th->cyan : C_DK);
        cv_rect(px - 4, py - 4, 9, 9, on ? C_TXT : C_MUTE);
        for (k = 0; k < 2; k++) {
            uint32_t a = k ? P_YAMT : P_XAMT;
            s[0] = 0; cat(s, k ? "Y " : "X "); fmt_param(a - 1u, P[a - 1u], t); cat(s, t);
            cv_text_fit(120, k * 24, 114, s, k ? th->cyan : th->acc, 1);
            fmt_param(a, P[a], t); cv_text_fit(120, k * 24 + 12, 114, t, C_MUTE, 1);
        }
        s[0] = 0; cat(s, "PAD "); cat(s, E_PADM[P[P_PADM] % 3]); cat(s, on ? " *" : "");
        cv_text_fit(120, 52, 114, s, on ? C_TXT : C_MUTE, 1);
        s[0] = 0; cat(s, "KEY VEL "); cat(s, E_KVEL[P[P_KVEL] % 3]);
        cv_text_fit(120, 70, 114, s, P[P_KVEL] ? th->mut : C_MUTE, 1);
        k = at_keys > at_midi ? at_keys : at_midi;
        cv_rect(120, 88, 114, 6, C_DK);
        cv_rect(120, 88, k * 114 / 127, 6, P[P_ATAMT] ? th->haz : C_MUTE);
        cv_text_fit(120, 98, 114, "AT", C_MUTE, 1);
        break;
    }
    }
    cv_blit(0, 40);
}
static uint32_t knob_ph(int32_t norm) { return ((uint32_t)norm + 225u) * 11930465u; }   /* degrees -> phase */
static void draw_knobs(void)
{
    const theme_t *th = &TH;
    uint32_t k;
    char s[24];
    cv_begin(240, 64, th->panel);
    for (k = 0; k < 4u; k++) {
        uint32_t p = PAGE_P[page][sub][k], a;
        int32_t x = 2 + (int32_t)k * 60, cx = x + 28, cy = 16, lo, rng;
        if (p == NO) { cv_rect(x, 0, 56, 63, RGB(14, 14, 17)); continue; }
        lo = PD[p].lo; rng = PD[p].hi - lo ? PD[p].hi - lo : 1;
        int32_t norm = ((P[p] - lo) * 270) / rng, en = ((E[p] - lo) * 270) / rng;
        int modded = (P[P_L1MIX] && TGT[P[P_L1TGT]] == p) || (P[P_L2MIX] && TGT[P[P_L2TGT]] == p);
        int locked = lock_get(p);
        uint16_t c = locked ? th->haz : mutant ? th->mut : (k & 1u) ? th->cyan : th->acc;
        cv_rect(x, 0, 56, 63, modded ? RGB(30, 30, 44) : th->box);
        for (a = 0; a <= 270u; a += 10u) {
            uint32_t ph = knob_ph((int32_t)a);
            cv_rect(cx + (isin(ph) * 13 >> 15) - 1, cy - (isin(ph + 0x40000000u) * 13 >> 15) - 1, 2, 2, (int32_t)a <= norm ? c : C_DK);
        }
        { uint32_t ph = knob_ph(norm); cv_line(cx, cy, cx + (isin(ph) * 9 >> 15), cy - (isin(ph + 0x40000000u) * 9 >> 15), locked ? th->haz : C_TXT); }
        if (modded && en != norm) { uint32_t ph = knob_ph(en); cv_rect(cx + (isin(ph) * 13 >> 15) - 2, cy - (isin(ph + 0x40000000u) * 13 >> 15) - 2, 4, 4, C_TXT); }
        cv_text_fit(x - 1, 30, 58, PD[p].name, locked ? th->haz : C_TXT, 1);   /* 7 characters */
        fmt_param(p, P[p], s);
        cv_text_fit(x - 1, 46, 58, s, locked ? th->haz : C_MUTE, 1);
        if (locked) cv_rect(x + 48, 2, 6, 6, th->haz);
    }
    cv_blit(0, 152);
}
static void draw_footer(void)
{
    const theme_t *th = &TH;
    char s[48], t[12];
    uint32_t nl = 0, p;
    cv_begin(240, 24, RGB(10, 10, 12));
    cv_rect(0, 0, 240, 1, th->border);
    if (toast[0]) cv_text_fit(4, 4, 232, toast, th->mut, 0);
    else {
        for (p = 0; p < NP; p++) nl += (uint32_t)lock_get(p);
        s[0] = 0; cat(s, "L:"); fmt_num(t, (int32_t)nl, 0); cat(s, t);
        cat(s, " U:"); fmt_num(t, (int32_t)undo_n, 0); cat(s, t);
        cat(s, " OCT"); fmt_num(t, oct_shift, 1); cat(s, t);
        if (SL[cur_slot].dirty) cat(s, " *SMP");
#ifndef DINGHY_HOST
        cat(s, " CPU"); fmt_num(t, (int32_t)tm_cpu, 0); cat(s, t); cat(s, "%");
#endif
        cv_text_fit(4, 4, 232, s, nl ? th->haz : C_MUTE, 0);
    }
    cv_blit(0, 216);
}

/* ======================================================================== the session in flash */
typedef struct { uint32_t ver; int16_t p[NP]; uint32_t lk[4]; int16_t a[NP], b[NP]; uint8_t ab, mon; } session_t;
_Static_assert(NP <= 4 * 32, "the locks: 4 words of bits, one a parameter");
_Static_assert(sizeof(session_t) <= SECTOR_MAX, "the session must fit the settings sector");
_Static_assert(NPAGE <= 16, "a page's change flag (1 << PMOD) must stay below KG_NOTE");
#define SESSION_VER (0x0D070600u + NP)
_Static_assert(sizeof(session_t) <= SECTOR_MAX, "the session: one settings sector");
static void session_save(void)
{
    session_t ss;
    uint32_t i;
    ss.ver = SESSION_VER;
    for (i = 0; i < NP; i++) { ss.p[i] = P[i]; ss.a[i] = snapA[i]; ss.b[i] = snapB[i]; }
    for (i = 0; i < 4u; i++) ss.lk[i] = locks[i];
    ss.ab = has_ab; ss.mon = morph_on;
    all_off();
    slot_flush(cur_slot);
    say(settings_save(&ss, sizeof ss) ? "SAVE FAILED" : "SESSION SAVED");
    session_dirty = 0;
}
static void session_load(void)
{
    session_t ss;
    uint32_t i;
    for (i = 0; i < NP; i++) P[i] = PD[i].def;
    if (settings_load(&ss, sizeof ss) || ss.ver != SESSION_VER) return;
    for (i = 0; i < NP; i++) {
        P[i] = (int16_t)clampi(ss.p[i], PD[i].lo, PD[i].hi);
        snapA[i] = (int16_t)clampi(ss.a[i], PD[i].lo, PD[i].hi); snapB[i] = (int16_t)clampi(ss.b[i], PD[i].lo, PD[i].hi);
    }
    for (i = 0; i < 4u; i++) locks[i] = ss.lk[i];
    has_ab = ss.ab & 3u; morph_on = has_ab == 3u && ss.mon;
}

/* ======================================================================== input */
static uint32_t idle_ok;
static void pad_touch(int on)                   /* SPRING: the offset while touched; LATCH: it stays; PLAY: and a note */
{
    if (on == (int)pad_on) return;
    pad_on = (uint8_t)on;
    pad_held = P[P_PADM] == 1 && !on;
    if (P[P_PADM] == 2) { if (on) note_on((int32_t)last_note, 25800); else note_off((int32_t)last_note); }
    if (page == PG_PAD) ui_dirty |= 2;
}
/* the keys' velocity (FM-1, rows PA7 / PA8: 20 of the 27 keys; the top row has no converter) */
static sn_key_t sn[2 * 11];
static uint8_t sn_cell[27];                     /* key -> its cell in sn[], 255 = none */
static uint32_t sn_fl = SN_FLOOR;
#ifdef FM1_ANALOG
_Static_assert(FM1_NCOL == 11u, "sn[]: 2 rows of 11 columns");
static void sense_sample(uint32_t col, uint32_t rowbit, uint32_t v, uint32_t driven)
{
    sn_sample(&sn[(rowbit - 3u) * 11u + col], v, driven, sn_fl);
}
static int sense_drive(uint32_t col, uint32_t rowbit) { return sn[(rowbit - 3u) * 11u + col].drv_next; }
static int sense_led_ok(uint32_t col, uint32_t rowbit) { return sn[(rowbit - 3u) * 11u + col].st == SN_OPEN; }
#endif
static void sense_setup(void)                   /* which key sits in which cell; the scan's sampling on or off */
{
    uint32_t i;
    for (i = 0; i < 27u; i++) sn_cell[i] = 255;
#ifdef FM1_ANALOG
    {
        uint32_t r, c;
        for (r = 3; r <= 4u; r++) for (c = 0; c < 11u; c++)
            if (FM1_KEYMAP[r][c] >= 14) sn_cell[FM1_KEYMAP[r][c] - 14] = (uint8_t)((r - 3u) * 11u + c);
    }
#endif
}
static void sense_poll(void)                    /* main loop: KEY VEL on / off, the sensitivity, the aftertouch */
{
    uint32_t i, a = 0;
    sn_fl = sn_floor((uint32_t)P[P_KSENS]);
#ifdef FM1_ANALOG
    if (P[P_KVEL] && !fm1_an.con) {             /* the pot's last value first: the scan reads it from now on */
        int32_t m = fm1_adc_read(FM1_ADC_MASTER);
        fm1_an.master = (uint16_t)(m < 0 ? 0 : m);
        fm1_an.drive = 2;                       /* strength 1: Velo's setting */
        fm1_an.con = (uint16_t)FM1_ADC_CON_SCAN;
    } else if (!P[P_KVEL] && fm1_an.con) {
        fm1_an.con = 0;
        for (i = 0; i < 22u; i++) sn[i].st = SN_OPEN;
    }
#endif
    if (P[P_KVEL] == 2) for (i = 0; i < 22u; i++) if (sn[i].at > a) a = sn[i].at;
    at_keys = (uint8_t)a;
}
static int32_t key_vel15(uint32_t k)            /* a pressed key's velocity, Q15 (no sample: the default) */
{
    uint32_t c = sn_cell[k];
    if (!P[P_KVEL] || c == 255u || !sn[c].vel) return 25800;
    sn[c].fresh = 0;
    return sn[c].vel * 258;
}
static int key_waits(uint32_t k)                /* wait (up to ~12 ms) for this key's velocity? */
{
    uint32_t c = sn_cell[k];
    return P[P_KVEL] && c != 255u && !sn[c].fresh;
}
/* SysEx from web/index.html (Web MIDI): F0 7D 'N' '7' cmd .. F7 (7D: non-commercial). A sample comes in as a recording
 * would (rec, mu-law at 22.05 kHz), packed 7 bits per byte as the updater packs (ota_unpack7), and goes the same way:
 * trimmed, normalized, into the slot (SAVE keeps it; END with save = 1 saves at once). Every message is answered:
 * F0 7D 'N' '7' (0x40 | cmd) status a0 a1 a2 F7 (status 0 ok; a = the samples so far / the most a slot holds). */
enum { SX_HELLO = 1, SX_BEGIN = 2, SX_DATA = 3, SX_END = 4, SX_ABORT = 5 };
static uint32_t sx_len, sx_next;
static void sx_reply(uint32_t cmd, uint32_t st, uint32_t a)
{
    uint8_t m[10];
    m[0] = 0xF0; m[1] = 0x7D; m[2] = 'N'; m[3] = '7'; m[4] = (uint8_t)(0x40u | cmd); m[5] = (uint8_t)st;
    m[6] = (uint8_t)(a & 127u); m[7] = (uint8_t)(a >> 7 & 127u); m[8] = (uint8_t)(a >> 14 & 127u); m[9] = 0xF7;
    ota_wire_send(m, sizeof m);
}
static int app_sysex(const uint8_t *p, uint32_t n)   /* p: the bytes between F0 and F7. 0: not ours */
{
    uint32_t cmd, st = 0;
    if (n < 4u || p[0] != 0x7Du || p[1] != 'N' || p[2] != '7') return 0;
    cmd = p[3];
    switch (cmd) {
    case SX_HELLO: sx_reply(cmd, 0, SMP_MAX); return 1;
    case SX_BEGIN: {                            /* slot, length (3 x 7 bit) */
        uint32_t slot = n > 4u ? p[4] & 7u : 0u, len = n > 7u ? (uint32_t)p[5] | (uint32_t)p[6] << 7 | (uint32_t)p[7] << 14 : 0u;
        if (rec_state != REC_IDLE && rec_state != REC_USB) st = 1;   /* busy recording */
        else if (len < 512u || len > SMP_MAX) st = 2;
        else {
            all_off();
            P[P_SLOT] = (int16_t)slot;
            if (cur_slot != (int32_t)slot) slot_select((int32_t)slot);
            rec_state = REC_USB; sx_len = len; sx_next = 0;
            ui_dirty |= 2 | 4;
        }
        break;
    }
    case SX_DATA:
        if (rec_state != REC_USB) { st = 3; break; }
        sx_next += ota_unpack7(p + 4, n - 4u, rec.d + sx_next, sx_len - sx_next);
        if (sx_next > sx_len) sx_next = sx_len;
        rec_n = sx_next;                        /* (the screen shows the progress as a recording's) */
        break;
    case SX_END:
        if (rec_state != REC_USB || sx_next != sx_len) { st = 4; break; }
        rec_n = sx_len;
        rec_state = REC_DONE;
        rec_commit();
        if (n > 4u && p[4]) session_save();
        break;
    case SX_ABORT: if (rec_state == REC_USB) rec_state = REC_IDLE; break;
    default: st = 5;
    }
    sx_reply(cmd, st, sx_next);
    return 1;
}
static void midi_in(void)
{
    uint8_t st, d1, d2;
    while (midi_get(&st, &d1, &d2)) {
        uint32_t ty = st & 0xF0u;
        if (ty == 0x90u && d2) { note_on(d1, d2 * 258); last_note = d1; }
        else if (ty == 0x80u || ty == 0x90u) note_off(d1);
        else if (ty == 0xE0u) {
            int32_t sq8 = (int32_t)(((uint32_t)d2 << 7 | d1) >> 4) - 512, s = sq8 >> 8, fr = sq8 & 255;   /* +-2 semitones, Q8 (written so
                                                                   that no 0xFFC00000 constant folds out of it) */
            uint32_t r = T_SEMI[clampi(s, -48, 47) + 48];
            bend_q16 = r + (uint32_t)(((uint64_t)(T_SEMI[clampi(s, -48, 47) + 49] - r) * (uint32_t)fr) >> 8);
        } else if (ty == 0xB0u && d1 == 1u) { P[P_L1MIX] = (int16_t)(d2 * 100 / 127); ui_dirty |= 4; }
        else if (ty == 0xB0u && d1 == 74u) { P[P_RFFREQ] = (int16_t)(d2 * 63 / 127); ui_dirty |= 4; }
        else if (ty == 0xB0u && d1 == 11u) { morph_set(d2 * 100 / 127); ui_dirty |= 4; }
        else if (ty == 0xB0u && (d1 == 16u || d1 == 17u)) {   /* the pad: a move is a touch */
            if (d1 == 16u) pad_x = d2; else pad_y = d2;
            if (!pad_on) pad_touch(1);
        } else if (ty == 0xB0u && d1 == 18u) pad_touch(d2 >= 64u);
        else if (ty == 0xD0u) at_midi = d1;    /* channel pressure */
        else if (ty == 0xA0u) at_midi = d2;    /* key pressure (the last one) */
        else if (ty == 0xB0u && d1 == 123u) all_off();
    }
}
static void app_idle(void) { if (idle_ok) midi_in(); }
static void turn(uint32_t knob, int32_t e, int sel_held)
{
    uint32_t p = PAGE_P[page][sub][knob];
    int32_t step;
    if (p == NO) return;
    step = PD[p].step * (absi(e) > 1 ? 3 : 1);
    if (sel_held) {
        locks[p >> 5] ^= 1u << (p & 31u);
        say(lock_get(p) ? "LOCKED" : "UNLOCKED");
        ui_dirty |= 4 | 8;
        return;
    }
    if (p == P_THEME) { P[p] = (int16_t)((P[p] + (e > 0 ? 1 : 3)) & 3); ui_dirty = 0xFF; return; }
    if (p == P_MORPH) {
        morph_set(P[p] + e * step);
        if (has_ab != 3u) say(has_ab ? "STORE B: SEL + OCT+" : "STORE A / B: SEL + OCT- / OCT+");
        touched();
        return;
    }
    if (!NOMORPH[p]) morph_on = 0;              /* an edit: the panel is yours again (it holds the mix) */
    P[p] = (int16_t)clampi(P[p] + e * step, PD[p].lo, PD[p].hi);
    touched();
}

static void app_main(void)
{
    uint32_t prev_keys = 0, t_vis = 0, t_knob = 0, sel_used = 0, sel_down = 0, kpend = 0, i;
    uint16_t kpend_t[27] = {0};
    session_load();
    sense_setup();
    for (i = 0; i < NP; i++) {                  /* the page of each parameter, the LFO targets */
        uint32_t m, s, k;
        for (m = 0; m < NPAGE; m++) for (s = 0; s < 4u; s++) for (k = 0; k < 4u; k++) if (PAGE_P[m][s][k] == i) PMOD[i] = (uint8_t)m;
        if (i != P_SLOT && i != P_L1TGT && i != P_L2TGT && i != P_THEME && PMOD[i] != PG_PAD) TGT[NTGT++] = (uint8_t)i;
        NOMORPH[i] = i == P_SLOT || i == P_THEME || PMOD[i] == PG_MRP || PMOD[i] == PG_PAD;
        CONT[i] = PD[i].fmt != F_ENUM && PD[i].fmt != F_SLOT && PD[i].fmt != F_TGT && PD[i].hi - PD[i].lo >= 10;
        E[i] = P[i];
        S8[i] = P[i] * 256;
    }
    if (NTGT != NTGT_N) say("BUG: NTGT_N");    /* (a new parameter: NTGT_N must count the mod targets) */
    for (i = 0; i < NPAGE; i++) {               /* how many knob sets each page has */
        uint32_t s;
        for (s = 0; s < 4u; s++) if (PAGE_P[i][s][0] != NO) NSUB[i] = (uint8_t)(s + 1u);
    }
    _Static_assert(P_PITCH - 1 == 13 && P_RFFREQ - 1 == 15, "the LFO default targets: PITCH, FREQ (the list skips the slot)");
    _Static_assert(NP <= 128, "the locks: 4 words");
    for (i = 0; i < 8u; i++)                    /* which slots hold a recording */
        if (slot_load(i, &rec, sizeof rec) == 0 && rec.len >= 256u && rec.len <= SMP_MAX) { SL[i].user = 1; SL[i].len = (uint16_t)rec.len; }
    slot_select(P[P_SLOT]);
    oct_shift = 0;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    k_update(KG_ALL);
    fx_clear();
    for (;;) {
        uint32_t keys, down, up, b, rel, k;
        int32_t e;
        int sel = (int)(fm1_in.buttons >> BTN_SEL & 1u);
        platform_poll();
        if (P[P_SLOT] != cur_slot) { slot_select(P[P_SLOT]); ui_dirty |= 2 | 4; }
        if (rec_state == REC_DONE) rec_commit();
        /* buttons */
        b = fm1_input_edges(&rel);
        if (b & (1u << BTN_SEL)) { sel_down = 1; sel_used = 0; }
        for (k = 0; k < NGRP; k++)
            if (b & (1u << GRP_BTN[k])) {           /* its modules in turn, each one's knob sets in turn */
                if (PAGE_GRP[page] != k) { uint32_t q = 0; while (PAGE_GRP[q] != k) q++; page = (uint8_t)q; sub = 0; }
                else if (sub + 1u < NSUB[page]) sub++;
                else { uint32_t q = page + 1u; if (q >= NPAGE || PAGE_GRP[q] != k) while (q > 0u && PAGE_GRP[q - 1u] == k) q--; page = (uint8_t)q; sub = 0; }
                ui_dirty = 0xFF;
            }
        if (b & (1u << BTN_PLAY)) { if (sel) { slot_factory(); sel_used = 1; } else note_on(60, 32767); }
        if (rel & (1u << BTN_PLAY)) note_off(60);
        if (b & (1u << BTN_REC)) { if (sel) { undo(); sel_used = 1; } else happy_accident(); }
        if (b & (1u << BTN_SAVE)) { if (sel) { locks[0] = locks[1] = locks[2] = locks[3] = 0; say("ALL LOCKS CLEARED"); ui_dirty |= 4; sel_used = 1; } else session_save(); }
        if (b & (1u << BTN_OCT_DOWN)) {
            if (sel) { if (page == PG_MRP) morph_store(0); else reset_module(page); sel_used = 1; }
            else if (oct_shift > -3) { oct_shift--; ui_dirty |= 8; }
        }
        if (b & (1u << BTN_OCT_UP)) {
            if (sel && page == PG_MRP) { morph_store(1); sel_used = 1; }
            else if (sel) { P[P_THEME] = (int16_t)((P[P_THEME] + 1) & 3); ui_dirty = 0xFF; sel_used = 1; say(E_THEME[P[P_THEME]]); }
            else if (oct_shift < 3) { oct_shift++; ui_dirty |= 8; }
        }
        if (rel & (1u << BTN_SEL)) { if (sel_down && !sel_used) page_action(); sel_down = 0; }
        /* encoders */
        if ((e = fm1_enc_take(ENC_SELECT)) != 0) { page = (uint8_t)((page + (e > 0 ? 1u : NPAGE - 1u)) % NPAGE); sub = 0; ui_dirty = 0xFF; }
        if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) { P[P_WAVE] = (int16_t)clampi(P[P_WAVE] + e, 0, NWAVE - 1); touched(); }
        if ((e = fm1_enc_take(ENC_PRESETS)) != 0) { P[P_SLOT] = (int16_t)clampi(P[P_SLOT] + e, 0, 7); touched(); }
        {
            static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
            for (k = 0; k < 4u; k++) if ((e = fm1_enc_take(KN[k])) != 0) { turn(k, e, sel); if (sel) sel_used = 1; }
        }
        if (aud_t && (int32_t)(fm1_ms - aud_t) >= 0) { aud_t = 0; note_off(60); }
        /* keys: F3..G5, chromatic, held = gate; with KEY VEL a press waits for its velocity (a few ms) */
        sense_poll();
        keys = fm1_in.notes;
        down = (keys & ~prev_keys) | (fm1_input_note_edges() & ~prev_keys);
        up = prev_keys & ~keys;
        prev_keys = keys;
        for (k = 0; k < 27u; k++) {
            int32_t note = 53 + (int32_t)k + oct_shift * 12;
            if (up >> k & 1u) { if (kpend >> k & 1u) kpend &= ~(1u << k); else note_off(note); }
            if (down >> k & 1u) { if (key_waits(k)) { kpend |= 1u << k; kpend_t[k] = (uint16_t)fm1_ms; } else { note_on(note, key_vel15(k)); last_note = (uint32_t)note; } }
            if ((kpend >> k & 1u) && (!key_waits(k) || (uint16_t)((uint16_t)fm1_ms - kpend_t[k]) >= 12u)) {
                kpend &= ~(1u << k);
                note_on(note, key_vel15(k)); last_note = (uint32_t)note;
            }
        }
        batch_poll();
        idle_ok = 1;
        midi_in();
        if (mutant && fm1_ms - mutant_t > 700u) { mutant = 0; ui_dirty |= 1 | 4; }
        /* LEDs */
        for (k = 0; k < NGRP; k++) fm1_led_key(GRP_BTN[k], PAGE_GRP[page] == k);
        fm1_led_key(BTN_PLAY, (int)tm_awake);
        fm1_led_key(BTN_REC, mutant || rec_state == REC_ON || (rec_state == REC_ARMED && (fm1_ms & 256u)));
        fm1_led_key(BTN_SEL, sel || (batch_on && (fm1_ms & 128u)));
        fm1_led_key(BTN_SAVE, session_dirty || SL[cur_slot].dirty);
        fm1_led_key(BTN_OCT_DOWN, oct_shift < 0);
        fm1_led_key(BTN_OCT_UP, oct_shift > 0);
        for (k = 0; k < 27u; k++) fm1_led_key(KEY_LED(k), (int)(keys >> k & 1u));
        /* the screen */
        if (ui_dirty & 1) { draw_header(); ui_dirty &= (uint8_t)~1u; }
        if ((ui_dirty & 2) || ((tm_awake || page == PG_GRN || page == PG_FLT || page == PG_LFO || page == PG_RES || page == PG_MRP || page == PG_PAD || page == PG_CHIP || rec_state)
                               && fm1_ms - t_vis > 50u)) { draw_vis(); ui_dirty &= (uint8_t)~2u; t_vis = fm1_ms; }
        if ((ui_dirty & 4) || ((P[P_L1MIX] || P[P_L2MIX]) && fm1_ms - t_knob > 80u)) { draw_knobs(); ui_dirty &= (uint8_t)~4u; t_knob = fm1_ms; }
        if (toast[0] && fm1_ms - t_toast >= 1800u) { toast[0] = 0; ui_dirty |= 8; }
        if (ui_dirty & 8) { draw_footer(); ui_dirty &= (uint8_t)~8u; }
        idle_ok = 0;
    }
}
