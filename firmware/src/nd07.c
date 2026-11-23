/* SPDX-License-Identifier: GPL-3.0-only */
/* ND-07 // MONOLITH for the M-VAVE FM-1: a sampler and synth for game-audio sound effects.
 * The REAPER workstation (Lua ReaImGui + JSFX) rebuilt for the FM-1 in integer C (JieLi AC79: no FPU, no libgcc).
 *
 * 21 pages, in the order of the work; the eight buttons each hold one step of it:
 *   HOME  01 ASSETS           19 starting points (EXPLODE ... SYNTH): a whole patch, loaded and played; B, C: SOUND
 *                             CHIPS, the result through a console's chip (YM2149, YM2612, S-DSP, SCSP, DMG)
 *         02 OSC & SAMPLE     15 waves (PolyBLEP, CHIP voices, SHEPARD) and 8 sample slots; OSC, SAMPLE or a blend;
 *                             loop / sustain points, up to 3 layered voices; record your own into a slot
 *         03 CHIP OSC         NES-style pulse, triangle and LFSR noise; the 60 Hz frame: SWEEP, ARP, 16-step volume
 *   ARP   04 GRAIN ENGINE     grains on the sample: position, size, density, spray, reverse, freeze
 *         05 SAMPLE LAB       reverse, crop, fades, half / double speed, normalize, revert, factory sound
 *   ENV   06 ENVELOPES        ADSR, glide, velocity, pitch, out; a modulation envelope to any target
 *         07 TRANSIENT        attack and sustain boost / cut
 *   EDIT  08 RESONATOR        6 modes: METAL GLASS WOOD STRING BELL DRUM TUBE
 *         09 DESTROY          drive (SOFT HARD FOLD RECTIFY), bits, rate, ring mod, tone
 *         10 VOWEL + SHIFT    formants U O A E I, a frequency shifter with a feedback spiral, growl
 *         11 RES MOD FILTER   ZDF SVF 12 / 24 dB, LP BP HP NOTCH, moved by envelope, LFO, S&H or the amp envelope
 *   GLO   12 OTT MULTIBAND    three-band up / down compression (the OTT preset), before or after the effects
 *   FX    13 DELAY + FLANGER  1 s delay and a flanger, four routings
 *         14 MULTIBAND REVERB a 4-line FDN with separate LOW / MID / HIGH decay
 *         15 SPACE + MOTION   distance, occlusion, a pass-by with Doppler
 *   LFO   16 LFO 1 + 2        shape, rate, mix and any target
 *         17 RTPC             one game value swept or set by hand, on three targets, with a curve
 *         18 CHAOS PAD + KEYS an XY pad over MIDI, key velocity and aftertouch, a recorded gesture
 *   SEQ   19 VARIATION + MORPH a seed of variations on the keys; two patches and a MORPH between them
 *         20 PHYSICS TRIGGER  one key, many hits: BOUNCE DEBRIS ROLL STUTTER SCATTER
 *         21 BATCH + FIRE TEST a run of varied hits to record, or fired as a game would, level matched
 *   Also: HAPPY ACCIDENT, parameter locks, 16 undo steps, 5 themes (EVA-01 by default), the session in flash.
 *
 * The chain (mono): SOURCE x ENV > RESONATOR > TRANSIENT > DESTROY > VOWEL + SHIFT > FILTER > OTT (pre) > DELAY +
 * FLANGER > REVERB > OTT (post) > SOUND CHIPS > SPACE > limiter > out. A module at rest costs nothing; after about a
 * second of silence the whole chain sleeps. It runs in 64-sample blocks; the heavy stages run from RAM (.ram_text)
 * and call nothing.
 *
 * The panel follows one rule: a control alone does its job; with SEL held it does its second job, the same on
 * every page.
 *   HOME ARP ENV EDIT GLO FX LFO SEQ  a step of the work; again: the next knob set (A B C D), then the next page
 *   SELECT       previous / next page              SEL + SELECT      the knob set
 *   K1..K4       the page's parameters             SEL + K           lock / unlock
 *   ALGORITHM    oscillator wave                   SEL + ALGORITHM   the theme
 *   PRESETS      sample slot                       SEL + PRESETS     the next asset, loaded and played
 *   PLAY         a held C4                         SEL + PLAY        stop everything
 *   REC          HAPPY ACCIDENT                    SEL + REC         undo
 *   SAVE         the session into flash            SEL + SAVE        clear all locks
 *   OCT- / OCT+  the octave                        SEL + OCT- / +    reset / randomize this page
 *   SEL tap      the page's own action (the footer names it); SEL held: the second jobs, one by one, on the footer
 *   MIDI in      notes with velocity, pitch bend +-2, CC1 LFO 1 mix, CC11 MORPH, CC74 filter, CC16/17/18 the pad,
 *                channel and key pressure
 *
 * Every parameter reaches the engine in Q8, glided over ~6 ms (S8), so knobs and LFOs move without steps.
 * Integer only, no 64-bit division (docs/RULES.md). Tables: tools/gen_nd07_tables.py; samples: tools/gen_samples.py */

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
#define SH_N 4096u                              /* 09 SHIFT's feedback line: 93 ms */
static int16_t sh_buf[SH_N] __attribute__((section(".pool")));
#define EM_N 16384u                             /* SOUND CHIPS' echo line: 371 ms */
static int16_t em_buf[EM_N] __attribute__((section(".pool")));
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
       P_RECM, P_LPOS, P_LLEN, P_LXF, P_VOICES, P_LPIT, P_LDET, P_LLVL, P_LMATCH, P_LTGT,
       P_SDIST, P_SOCCL, P_SPASS, P_SDOPP,
       P_VSEED, P_VKEYS, P_VLOCK, P_HAAMT, P_EVOLVE,
       P_TRATE, P_TPOOL, P_TORDER,
       P_QINT, P_QSWP, P_QCRV, P_Q1T, P_Q1A, P_Q2T, P_Q2A, P_Q3T, P_Q3A,
       P_MATK, P_MDEC, P_MAMT, P_MTGT,
       P_PMODE, P_PHITS, P_PTIME, P_PFALL, P_PPIT, P_PJIT,
       P_STOOL, P_SAMT, P_RECIPE, P_GEST,
       P_VOWEL, P_VSIZE, P_VRES, P_VMIX, P_SHIFT, P_SFB, P_STIME, P_SMIX, P_GROWL, P_GRATE,
       P_THEME, P_ASSET,
       P_EMU, P_ERATE, P_EGRIT, P_EMIX, P_EECHO, P_ETIME, P_ETONE, NP };
#define NP_V13 (P_ASSET + 1)                    /* the parameters of a v1.2 / v1.3 session */
#define NTGT_N (NP_V13 - 48)                    /* the mod targets: all but the slot, the theme, the asset, the target pickers, the PAD page, SOUND CHIPS */
enum { F_VOWEL, F_SHZ, F_CENT, F_SEC10, F_RPM, F_INT, F_PCT, F_X100, F_ENUM, F_ATT, F_DEC, F_REL, F_GLIDE, F_RFHZ, F_RATE, F_DBH, F_XLO, F_XHI, F_PM, F_MS, F_DENS,
       F_SPR, F_SLOT, F_SEMI, F_BLEND, F_DTIME, F_FRATE, F_LRATE, F_RDEC, F_TGT, F_OXLO, F_OXHI, F_TSC,
       F_ZDEC, F_BITS, F_XSR, F_XRHZ, F_BSPC, F_ERATE, F_ETIME, F_ETONE };
typedef struct { const char *name; int16_t lo, hi, def; uint8_t step, fmt; const char *const *en; } pdesc_t;
static const char *const E_ONOFF[] = {"OFF", "ON"};
static const char *const E_MODE[] = {"OSC", "SAMPLE", "MIX"};
static const char *const E_WAVE[] = {"SINE", "TRI", "SAW", "SQUARE", "PULSE", "SUPRSAW", "NOISE", "SUB-SAW", "FORMANT", "METALFM", "PADSCAN", "CHIP PLS", "CHIP TRI", "CHIP NSE", "SHEPARD"};
#define NWAVE 15
enum { W_PADSCAN = 10, W_CPLS, W_CTRI, W_CNSE, W_SHEP };
static const char *const E_SLOOP[] = {"1-SHOT", "LOOP", "SUSTAIN"};
static const char *const E_RECM[] = {"NEW", "OVERDUB", "BOUNCE"};
static const char *const E_VOICES[] = {"MONO", "LAYER 2", "LAYER 3"};
static const char *const E_VKEYS[] = {"NOTES", "VARIANT"};
static const char *const E_VLOCK[] = {"NEVER", "BATCH"};
static const char *const E_TORDER[] = {"RANDOM", "NO REPT", "SHUFFLE"};
static const char *const E_QCRV[] = {"LINEAR", "EXP", "LOG", "S-CURVE"};
static const char *const E_RFMODE[] = {"BYPASS", "LP", "BP", "HP", "NOTCH"};
static const char *const E_POLE[] = {"12dB", "24dB"};
static const char *const E_RFSRC[] = {"ENV FOL", "LFO", "S&H", "AMP ENV"};
static const char *const E_OSTY[] = {"BYPASS", "OTT", "SMOOTH", "PUNCH", "AGGR"};   /* OTT: Ableton's preset / Xfer; the rest: the ND-07 JSFX curves */
static const char *const E_OPOS[] = {"PRE FX", "POST FX"};
static const char *const E_OSOLO[] = {"OFF", "LOW", "MID", "HIGH"};
static const char *const E_ROUTE[] = {"PARALL.", "FLG>DLY", "DLY>FLG", "IN FDBK"};
static const char *const E_LSHP[] = {"SINE", "TRI", "SAW UP", "SAW DN", "SQUARE", "S&H", "DRIFT", "HIT RND", "AMP ENV"};
static const char *const E_THEME[] = {"01 TEST", "02 PROD", "00PROTO", "MONO OG", "EVA-01"};
static const char *const E_ZMAT[] = {"METAL", "GLASS", "WOOD", "STRING", "BELL", "DRUM", "TUBE"};
static const char *const E_XSHP[] = {"SOFT", "HARD", "FOLD", "RECTIFY"};
static const char *const E_BMODE[] = {"RANDOM", "A>B", "A>B+RND"};
static const char *const E_PADM[] = {"SPRING", "LATCH", "PLAY"};
static const char *const E_PMODE[] = {"OFF", "BOUNCE", "DEBRIS", "ROLL", "STUTTER", "SCATTER"};
static const char *const E_STOOL[] = {"REVERSE", "CROP", "FADE IN", "FADEOUT", "1/2 SPD", "2x SPD", "MAX LVL", "REVERT", "FACTORY"};
static const char *const E_RECIPE[] = {"FREE", "IMPACT", "WHOOSH", "UI", "MAGIC", "MONSTER", "WEAPON", "AMBIENT"};
static const char *const E_GEST[] = {"OFF", "1-SHOT", "LOOP"};
static const char *const E_KVEL[] = {"OFF", "VEL", "VEL+AT"};
static const char *const E_CDUTY[] = {"12.5%", "25%", "50%", "75%"};
static const char *const E_CARP[] = {"OFF", "OCT", "5TH", "4TH", "MAJOR", "MINOR", "TRILL", "OCT DN", "UP 4TH", "UP 5TH", "UP OCT"};
#define NARP 11                                 /* (the last three step once and stay: the coin's jump) */
static const char *const E_CSTEP[] = {"OFF", "60 Hz", "30 Hz", "15 Hz"};
static const char *const E_CNOISE[] = {"LONG", "SHORT 93"};
static const char *const E_CPRE[] = {"COIN", "JUMP", "LASER", "HIT", "POWERUP", "EXPLODE", "BLIP"};
static const char *const E_BCNT[] = {"4", "8", "12", "16"};
/* SOUND CHIPS: the sound printed through a console's chip; each with the machines it sounds like */
static const char *const E_EMU[] = {"OFF", "YM2149", "YM2612", "S-DSP", "SCSP", "DMG"};
static const char *const EMU_HOST[] = {"", "ATARI ST / MSX", "MEGA DRIVE", "SUPER NES", "SATURN", "GAME BOY"};
/* 15 ASSETS: base sounds for game audio, three to a row (the screen's grid): hits, weapons, motion, UI, rewards, beings */
static const char *const E_ASSET[] = {"EXPLODE", "IMPACT", "HIT", "DEBRIS", "GUNSHOT", "LASER", "ZAP", "WHOOSH", "RISER",
                                      "UI OK", "UI ERR", "UI TICK", "PICKUP", "POWERUP", "MAGIC", "MONSTER", "ALARM", "ENGINE", "SYNTH"};
#define NASSET 19
static const pdesc_t PD[NP] = {
    [P_MODE] = {"SOURCE", 0, 2, 0, 1, F_ENUM, E_MODE},     [P_WAVE] = {"WAVE", 0, NWAVE - 1, 2, 1, F_ENUM, E_WAVE},
    [P_SLOT] = {"SAMPLE", 0, 7, 0, 1, F_SLOT, 0},          [P_BLEND] = {"BLEND", 0, 100, 50, 2, F_BLEND, 0},
    [P_TUNE] = {"TUNE", -24, 24, 0, 1, F_SEMI, 0},         [P_COLOR] = {"COLOR", 0, 100, 50, 1, F_PCT, 0},
    [P_STR] = {"STRETCH", 40, 250, 100, 2, F_X100, 0},     [P_SLOOP] = {"LOOP", 0, 2, 0, 1, F_ENUM, E_SLOOP},
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
    [P_GREV] = {"REVERSE", 0, 100, 25, 1, F_PCT, 0},         [P_GMODE] = {"GRAIN", 0, 1, 0, 1, F_ENUM, E_ONOFF},
    [P_GFRZ] = {"FREEZE", 0, 1, 0, 1, F_ENUM, E_ONOFF},    [P_RNDP] = {"RND PIT", 0, 1, 0, 1, F_ENUM, E_ONOFF},
    [P_ODEPTH] = {"DEPTH", 0, 100, 30, 1, F_PCT, 0},       [P_OTIME] = {"TIME", 0, 100, 50, 1, F_TSC, 0},
    [P_OUP] = {"UP", 0, 100, 100, 1, F_PCT, 0},          [P_ODOWN] = {"DOWN", 0, 100, 100, 1, F_PCT, 0},
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
    [P_L1SHP] = {"SHAPE", 0, 8, 0, 1, F_ENUM, E_LSHP},     [P_L1RATE] = {"RATE", 0, 63, 43, 1, F_LRATE, 0},
    [P_L1MIX] = {"MIX", 0, 100, 0, 1, F_PCT, 0},           [P_L1TGT] = {"TARGET", 0, NTGT_N - 1, 13, 1, F_TGT, 0},
    [P_L2SHP] = {"SHAPE", 0, 8, 1, 1, F_ENUM, E_LSHP},     [P_L2RATE] = {"RATE", 0, 63, 25, 1, F_LRATE, 0},
    [P_L2MIX] = {"MIX", 0, 100, 0, 1, F_PCT, 0},           [P_L2TGT] = {"TARGET", 0, NTGT_N - 1, 15, 1, F_TGT, 0},
    [P_ZMAT] = {"BODY", 0, 6, 0, 1, F_ENUM, E_ZMAT},     [P_ZPITCH] = {"PITCH", -24, 24, 0, 1, F_SEMI, 0},
    [P_ZDEC] = {"DECAY", 0, 63, 34, 1, F_ZDEC, 0},         [P_ZMIX] = {"RES MIX", 0, 100, 0, 1, F_PCT, 0},
    [P_ZDAMP] = {"DAMP", 0, 100, 35, 1, F_PCT, 0},         [P_ZSPRD] = {"SPREAD", 0, 100, 50, 1, F_PCT, 0},
    [P_ZHIT] = {"HIT POS", 2, 50, 13, 1, F_PCT, 0},        [P_ZTRK] = {"KEY TRK", 0, 1, 1, 1, F_ENUM, E_ONOFF},
    [P_TATK] = {"ATTACK", -100, 100, 0, 2, F_PM, 0},       [P_TSUS] = {"SUSTAIN", -100, 100, 0, 2, F_PM, 0},
    [P_TTIME] = {"TIME", 0, 100, 50, 1, F_TSC, 0},         [P_TOUT] = {"OUT", -48, 48, 0, 1, F_DBH, 0},
    [P_XDRV] = {"DRIVE", 0, 72, 0, 1, F_DBH, 0},           [P_XSHP] = {"SHAPE", 0, 3, 0, 1, F_ENUM, E_XSHP},
    [P_XBITS] = {"BITS", 2, 16, 16, 1, F_BITS, 0},         [P_XSR] = {"RATE", 0, 63, 0, 1, F_XSR, 0},
    [P_XRING] = {"RING HZ", 0, 63, 30, 1, F_XRHZ, 0},      [P_XRMIX] = {"RING MIX", 0, 100, 0, 1, F_PCT, 0},
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
    [P_RECM] = {"REC", 0, 2, 0, 1, F_ENUM, E_RECM},        [P_LPOS] = {"L POS", 0, 100, 40, 1, F_PCT, 0},
    [P_LLEN] = {"L LEN", 5, 95, 40, 1, F_PCT, 0},           [P_LXF] = {"XFADE", 0, 50, 20, 1, F_PCT, 0},
    [P_VOICES] = {"VOICES", 0, 2, 0, 1, F_ENUM, E_VOICES},
    [P_LPIT] = {"L PITCH", -24, 24, -12, 1, F_SEMI, 0},     [P_LDET] = {"DETUNE", 0, 50, 12, 1, F_CENT, 0},
    [P_LLVL] = {"L LEVEL", 0, 100, 70, 1, F_PCT, 0},
    [P_LMATCH] = {"MATCH", 0, 1, 0, 1, F_ENUM, E_ONOFF},    [P_LTGT] = {"TARGET", -48, 0, -6, 1, F_DBH, 0},
    [P_SDIST] = {"DIST", 0, 100, 0, 1, F_PCT, 0},           [P_SOCCL] = {"OCCLUDE", 0, 100, 0, 1, F_PCT, 0},
    [P_SPASS] = {"PASS-BY", 0, 60, 0, 1, F_SEC10, 0},       [P_SDOPP] = {"DOPPLER", 0, 24, 7, 1, F_SEMI, 0},
    [P_VSEED] = {"SEED", 1, 999, 1, 1, F_INT, 0},           [P_VKEYS] = {"KEYS", 0, 1, 0, 1, F_ENUM, E_VKEYS},
    [P_VLOCK] = {"RESEED", 0, 1, 0, 1, F_ENUM, E_VLOCK},      [P_HAAMT] = {"HA AMT", 10, 100, 100, 5, F_PCT, 0},
    [P_EVOLVE] = {"EVOLVE", 0, 100, 0, 1, F_PCT, 0},
    [P_TRATE] = {"RATE", 30, 1200, 300, 10, F_RPM, 0},     [P_TPOOL] = {"POOL", 2, 16, 6, 1, F_INT, 0},
    [P_TORDER] = {"ORDER", 0, 2, 1, 1, F_ENUM, E_TORDER},
    [P_QINT] = {"VALUE", 0, 100, 0, 1, F_PCT, 0},          [P_QSWP] = {"SWEEP", 5, 200, 30, 1, F_SEC10, 0},
    [P_QCRV] = {"CURVE", 0, 3, 0, 1, F_ENUM, E_QCRV},
    [P_Q1T] = {"TARGET 1", 0, NTGT_N - 1, 15, 1, F_TGT, 0}, [P_Q1A] = {"AMT 1", -100, 100, 60, 2, F_PM, 0},
    [P_Q2T] = {"TARGET 2", 0, NTGT_N - 1, 14, 1, F_TGT, 0}, [P_Q2A] = {"AMT 2", -100, 100, 0, 2, F_PM, 0},
    [P_Q3T] = {"TARGET 3", 0, NTGT_N - 1, 16, 1, F_TGT, 0}, [P_Q3A] = {"AMT 3", -100, 100, 0, 2, F_PM, 0},
    [P_MATK] = {"M ATK", 0, 63, 0, 1, F_ATT, 0},            [P_MDEC] = {"M DEC", 0, 63, 30, 1, F_DEC, 0},
    [P_MAMT] = {"M AMT", -100, 100, 0, 2, F_PM, 0},         [P_MTGT] = {"M TGT", 0, NTGT_N - 1, 13, 1, F_TGT, 0},
    [P_PMODE] = {"TRIGGER", 0, 5, 0, 1, F_ENUM, E_PMODE},   [P_PHITS] = {"HITS", 2, 32, 8, 1, F_INT, 0},
    [P_PTIME] = {"TIME", 2, 80, 15, 1, F_SEC10, 0},         [P_PFALL] = {"FALL", 0, 100, 70, 1, F_PCT, 0},
    [P_PPIT] = {"PITCH", -12, 12, 0, 1, F_SEMI, 0},         [P_PJIT] = {"JITTER", 0, 100, 20, 1, F_PCT, 0},
    [P_STOOL] = {"TOOL", 0, 8, 0, 1, F_ENUM, E_STOOL},      [P_SAMT] = {"AMOUNT", 1, 100, 25, 1, F_PCT, 0},
    [P_RECIPE] = {"HA KIND", 0, 7, 0, 1, F_ENUM, E_RECIPE}, [P_GEST] = {"GESTURE", 0, 2, 0, 1, F_ENUM, E_GEST},
    [P_VOWEL] = {"VOWEL", 0, 100, 50, 1, F_VOWEL, 0},       [P_VSIZE] = {"SIZE", -12, 12, 0, 1, F_SEMI, 0},
    [P_VRES] = {"RES", 0, 100, 50, 1, F_PCT, 0},            [P_VMIX] = {"V MIX", 0, 100, 0, 1, F_PCT, 0},
    [P_SHIFT] = {"SHIFT", -100, 100, 15, 1, F_SHZ, 0},      [P_SFB] = {"FDBK", 0, 95, 0, 1, F_PCT, 0},
    [P_STIME] = {"S TIME", 1, 90, 20, 1, F_MS, 0},          [P_SMIX] = {"S MIX", 0, 100, 0, 1, F_PCT, 0},
    [P_GROWL] = {"GROWL", 0, 100, 0, 1, F_PCT, 0},          [P_GRATE] = {"G RATE", 15, 120, 45, 1, F_INT, 0},
    [P_THEME] = {"THEME", 0, 4, 4, 1, F_ENUM, E_THEME},
    [P_ASSET] = {"ASSET", 0, NASSET - 1, 0, 1, F_ENUM, E_ASSET},
    [P_EMU] = {"CHIP", 0, 5, 0, 1, F_ENUM, E_EMU},          [P_ERATE] = {"RATE", 0, 100, 50, 1, F_ERATE, 0},
    [P_EGRIT] = {"GRIT", 0, 100, 30, 1, F_PCT, 0},          [P_EMIX] = {"MIX", 0, 100, 100, 1, F_PCT, 0},
    [P_EECHO] = {"ECHO", 0, 100, 0, 1, F_PCT, 0},           [P_ETIME] = {"E TIME", 1, 15, 5, 1, F_ETIME, 0},
    [P_ETONE] = {"TONE", 0, 100, 60, 1, F_ETONE, 0},
};
static volatile int16_t P[NP];                  /* as set */
static volatile int16_t E[NP];                  /* as heard: P plus the LFOs, rounded (for the screen) */
static volatile int32_t S8[NP];                 /* as heard, Q8 and smoothed: the engine reads this (no zipper, fine LFO steps) */
static uint8_t CONT[NP];                        /* a continuous parameter: smoothed (enums jump) */
#define NLOCKW 8u
static uint32_t locks[NLOCKW];                  /* (NP <= 256) bit p: locked (mutations and MORPH leave it alone) */
static int lock_get(uint32_t p) { return (int)(locks[p >> 5] >> (p & 31u) & 1u); }

/* The pages follow the work, one button a step: start from an ASSET (and the console it will sound on) and make the
 * source (HOME, ARP), shape it (ENV), colour it (EDIT), glue it (GLO), place it (FX), move it (LFO: what the game
 * drives), then vary it and deliver it (SEQ). The audio chain keeps its own order (see the audio). Each button holds its
 * pages one after the other (PAGE_GRP), so pressing it walks them in that order. */
enum { PG_ASSET, PG_SRC, PG_CHIP, PG_GRN, PG_LAB, PG_ENV, PG_TRN, PG_RES, PG_DST, PG_VOX, PG_FLT, PG_OTT, PG_DLY, PG_REV,
       PG_SPC, PG_LFO, PG_RTPC, PG_PAD, PG_VAR, PG_PHY, PG_BAT, NPAGE };
#define NO 255u                                 /* (an empty knob) */
static const uint8_t PAGE_P[NPAGE][4][4] = {    /* [module][A B C D][knob] */
    {{P_ASSET, P_VARY, P_RECIPE, P_VOL}, {P_EMU, P_ERATE, P_EGRIT, P_EMIX}, {P_EECHO, P_ETIME, P_ETONE, NO}, {NO, NO, NO, NO}},   /* (VARY,
                                                   HA KIND, OUT: here too; B, C: the SOUND CHIP, chosen with the asset) */
    {{P_MODE, P_WAVE, P_SLOT, P_BLEND}, {P_TUNE, P_COLOR, P_STR, P_RECM}, {P_SLOOP, P_LPOS, P_LLEN, P_LXF}, {P_VOICES, P_LPIT, P_LDET, P_LLVL}},
    {{P_CDUTY, P_CSWEEP, P_CARP, P_CARPS}, {P_CSTEP, P_CNOISE, P_CPRE, NO}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_GPOS, P_GSIZE, P_GDENS, P_GSPRAY}, {P_GREV, P_GMODE, P_GFRZ, P_RNDP}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_STOOL, P_SAMT, NO, NO}, {NO, NO, NO, NO}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_ATT, P_DEC, P_SUS, P_REL}, {P_GLIDE, P_VELO, P_PITCH, P_VOL}, {P_MATK, P_MDEC, P_MAMT, P_MTGT}, {NO, NO, NO, NO}},
    {{P_TATK, P_TSUS, P_TTIME, P_TOUT}, {NO, NO, NO, NO}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_ZMAT, P_ZPITCH, P_ZDEC, P_ZMIX}, {P_ZDAMP, P_ZSPRD, P_ZHIT, P_ZTRK}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_XDRV, P_XSHP, P_XBITS, P_XSR}, {P_XRING, P_XRMIX, P_XTONE, P_XMIX}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_VOWEL, P_VSIZE, P_VRES, P_VMIX}, {P_SHIFT, P_SFB, P_STIME, P_SMIX}, {P_GROWL, P_GRATE, NO, NO}, {NO, NO, NO, NO}},
    {{P_RFFREQ, P_RFRES, P_RFMODE, P_RFPOLE}, {P_RFDEP, P_RFRATE, P_RFSRC, P_RFDRV}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_ODEPTH, P_OTIME, P_OUP, P_ODOWN}, {P_OIN, P_OOUT, P_OSTYLE, P_OPOS}, {P_OLOW, P_OMID, P_OHIGH, P_OTHR}, {P_OXLO, P_OXHI, P_OSOLO, NO}},
    {{P_DTIME, P_DFB, P_DMIX, P_DROUTE}, {P_FRATE, P_FDEPTH, P_FREGEN, P_FMIX}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_RSIZE, P_RDEC, P_RPRE, P_RMIX}, {P_RLOW, P_RHIGH, P_RXLO, P_RXHI}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_SDIST, P_SOCCL, P_SPASS, P_SDOPP}, {NO, NO, NO, NO}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_L1SHP, P_L1RATE, P_L1MIX, P_L1TGT}, {P_L2SHP, P_L2RATE, P_L2MIX, P_L2TGT}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_QINT, P_QSWP, P_QCRV, NO}, {P_Q1T, P_Q1A, P_Q2T, P_Q2A}, {P_Q3T, P_Q3A, NO, NO}, {NO, NO, NO, NO}},
    {{P_XTGT, P_XAMT, P_YTGT, P_YAMT}, {P_PADM, P_GEST, P_KVEL, P_KSENS}, {P_ATTGT, P_ATAMT, NO, NO}, {NO, NO, NO, NO}},
    {{P_VARY, P_VSEED, P_VKEYS, P_VLOCK}, {P_MORPH, P_HAAMT, P_EVOLVE, P_RECIPE}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_PMODE, P_PHITS, P_PTIME, P_PFALL}, {P_PPIT, P_PJIT, NO, NO}, {NO, NO, NO, NO}, {NO, NO, NO, NO}},
    {{P_BMODE, P_BCNT, P_BSPC, P_BGATE}, {P_TRATE, P_TPOOL, P_TORDER, NO}, {P_LMATCH, P_LTGT, NO, NO}, {NO, NO, NO, NO}},
};
static uint8_t NSUB[NPAGE];                     /* how many of A B C D a page has */
static const char *const PAGE_NAME[NPAGE] = {"ASSETS", "OSC & SAMPLE", "CHIP OSC", "GRAIN ENGINE", "SAMPLE LAB", "ENVELOPES",
                                             "TRANSIENT", "RESONATOR", "DESTROY", "VOWEL + SHIFT", "RES MOD FILTER", "OTT MULTIBAND",
                                             "DELAY + FLANGER", "MULTIBAND REVERB", "SPACE + MOTION", "LFO 1 + 2", "RTPC", "CHAOS PAD + KEYS",
                                             "VARIATION + MORPH", "PHYSICS TRIGGER", "BATCH + FIRE TEST"};
#define NGRP 8
static const uint8_t GRP_BTN[NGRP] = {BTN_HOME, BTN_ARP, BTN_ENV, BTN_EDIT, BTN_GLO, BTN_FX, BTN_LFO, BTN_SEQ};
static const uint8_t PAGE_GRP[NPAGE] = {0, 0, 0, 1, 1, 2, 2, 3, 3, 3, 3, 4, 5, 5, 5, 6, 6, 6, 7, 7, 7};   /* the button each module is under */
static uint8_t PMOD[NP];                        /* the page of each parameter */
static uint8_t TGT[NP], NTGT;                   /* the mod targets (LFOs, pad, aftertouch): see NTGT_N */
/* 13 the CHAOS PAD (MIDI CC16 / 17 / 18) and the aftertouch (the keys, MIDI pressure): mod sources, 0..127 */
static volatile uint8_t pad_x = 64, pad_y = 64, pad_on, pad_held, at_keys, at_midi;

static uint32_t rng_main = 0x9E3779B9u, rng_isr = 0x6C8E9CF5u, rng_hit = 0x2545F491u;
static int32_t hit_rnd[2], amp_env15;           /* the LFO shapes HIT RND (each note's own values) and AMP ENV */
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
    int32_t i0, i2, i3, y0, y1, y2, y3, c1, c2, c3, f = (int32_t)fr;
    if (i1 < 0) i1 = 0;
    if (i1 > len - 1) i1 = len - 1;         /* (first: a read past the end, as a loop wraps, stays in the sample) */
    i0 = i1 > 0 ? i1 - 1 : 0; i2 = i1 + 1 < len ? i1 + 1 : len - 1; i3 = i1 + 2 < len ? i1 + 2 : len - 1;
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
static const int16_t *volatile smp_ptr = smp16;
static volatile uint32_t smp_len = 256u;
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
static void fmt_num(char *s, int32_t v, int sign);
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
static void fsmp_unpack(uint32_t i)            /* factory sound i into smp16, exactly as it was made (tools/gen_samples.py:
                                                   per 128 samples a predictor and a Rice code of what it misses) */
{
    const uint8_t *d = FSMP[i];
    uint32_t n = FSMP_LEN[i], pos = 0, j = 0;
    int32_t a = 0, b = 0;
#define FS_BIT() ((uint32_t)(d[pos >> 3] >> (7u - (pos & 7u))) & 1u)
    while (j < n) {
        uint32_t order = 0, k = 0, m, c;
        for (c = 0; c < 2u; c++, pos++) order = order << 1 | FS_BIT();
        for (c = 0; c < 5u; c++, pos++) k = k << 1 | FS_BIT();
        for (m = n - j < 128u ? n - j : 128u; m; m--) {
            uint32_t q = 0, u = 0, nb;
            int32_t e, x;
            while (q < 24u && FS_BIT()) { q++; pos++; }
            if (q < 24u) pos++;                 /* (the 0 that ends the count) */
            nb = q < 24u ? k : 18u;
            for (c = 0; c < nb; c++, pos++) u = u << 1 | FS_BIT();
            if (q < 24u) u |= q << k;
            e = (int32_t)(u >> 1) ^ -(int32_t)(u & 1u);
            x = (order == 0u ? 0 : order == 1u ? a : 2 * a - b) + e;
            b = a; a = x;
            smp16[j++] = (int16_t)x;
        }
    }
#undef FS_BIT
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
        fsmp_unpack((uint32_t)i);
        smp_ptr = smp16; smp_len = FSMP_LEN[i];
    }
    smp_busy = 0;
    cur_slot = i;
    smp_overview();
}

/* ======================================================================== coefficients (k_update, from E) */
static struct {
    uint8_t mode, wave, sloop, unity, gmode, gfrz, rfmode, rfpole, rfsrc, mbcrv, route;
    uint8_t c_on, c_arp, c_arps, c_step, c_short;  /* 14 CHIP: the frame clock runs; ARP; frames a step; frames a volume step */
    uint8_t ly_n;                               /* 03 LAYERS: how many more voices; their pitches (Q16), level, the sum's trim */
    uint32_t ly_mul[2];
    int32_t ly_g, ly_norm;
    int32_t sh_inc;                             /* 01 SHEPARD: the glide, octaves (Q24) a sample */
    uint8_t sp_on;                              /* 12 SPACE: at work (lfo_block sets sp_g, sp_a, pass_mul each block) */
    int32_t sp_g, sp_a;                         /* its gain and low-pass coefficient, Q15 */
    uint32_t pass_mul;                          /* the PASS-BY Doppler, a pitch factor (Q16) */
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
    uint8_t vx_on, vf_on, sh_on, gr_on;         /* 09 VOWEL + SHIFT: at work; its three parts */
    int32_t vf[3][3], vk, vmix, vli[3];         /* the formants' band-passes (a1 a2 a3, Q16), their damping, the mix; where */
    int32_t sh_fb, smix, gr_dep;
    uint32_t fs_inc, sh_d, gr_inc;
    uint8_t em, em_n, em_bits;                  /* SOUND CHIPS: which chip (0 off); YM2149 levels; bits */
    uint32_t em_inc, em_d;                      /* its sample clock (a phase step a sample); the echo's delay */
    int32_t em_lv[32], em_lad, em_sh, em_mix, em_ev, em_fb, em_lp;   /* YM2149's log DAC; the 2612's ladder; BRR's extra
                                                   shift / DMG's volume shift; mix, echo level and feedback, the tone (Q15) */
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
static uint32_t emu_rate(int32_t chip, int32_t v8)   /* SOUND CHIPS: RATE (Q8 %) as the chip's sample clock, Hz, on a log scale
                                                   over what its games used */
{
    static const uint16_t LO[6] = {0, 4000, 4000, 8000, 11025, 2048}, HI[6] = {0, 22050, 26000, 32000, 44100, 16384};
    int32_t c = clampi(chip, 1, 5), oct = ilog2_q8(HI[c]) - ilog2_q8(LO[c]);
    return (uint32_t)(((uint64_t)LO[c] * exp2_q16(v8 / 100 * oct / 256)) >> 16);
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
       KG_DST = 1u << PG_DST, KG_VOX = 1u << PG_VOX, KG_GRN = 1u << PG_GRN, KG_OTT = 1u << PG_OTT, KG_DLY = 1u << PG_DLY, KG_REV = 1u << PG_REV,
       KG_CHIP = 1u << PG_CHIP, KG_EMU = 1u << PG_ASSET, KG_NOTE = 1u << 24, KG_ALL = 0xFFFFFFFFu };
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
    {                                           /* 03 LAYERS, 01 SHEPARD */
        int32_t dt = ei(P_LDET), lv = ei(P_LLVL);
        K.ly_n = (uint8_t)ei(P_VOICES);
        K.ly_mul[0] = exp2_q16((ei(P_LPIT) * 100 + dt) * 256 / 1200);
        K.ly_mul[1] = exp2_q16(-dt * 256 / 1200);
        K.ly_g = lv * 327;
        K.ly_norm = (int32_t)(32767u * 1000u / (1000u + (uint32_t)(K.ly_n * lv * 5)));
        K.sh_inc = (K.color - 16384) * 190 / 16384;   /* COLOR 50 %: still; up: rising for ever, down: falling */
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
    /* 09 VOWEL + SHIFT */
    if (chg & KG_VOX) {
        static const int16_t VLI[5][3] = {      /* the formants of U O A E I (a log-frequency index, as the filter's) */
            {401, 559, 699}, {496, 554, 710}, {533, 592, 712}, {486, 670, 714}, {386, 702, 743}};
        int32_t pos = clampi(e8(P_VOWEL), 0, 100 * 256) * 4 / 100, vi = pos >> 8, vf = pos & 255, sz = e8(P_VSIZE) * 2190 >> 16;
        uint32_t f;
        int64_t hz;
        if (vi >= 4) { vi = 3; vf = 256; }
        K.vmix = e8(P_VMIX) * 327 >> 8;
        K.vk = (int32_t)(65536u * 10u / (25u + (uint32_t)clampi(e8(P_VRES), 0, 25600) * 125u / 25600u));   /* Q 2.5 .. 15 */
        for (f = 0; f < 3u; f++) {
            int32_t li = VLI[vi][f] + (((VLI[vi + 1][f] - VLI[vi][f]) * vf) >> 8) + sz, g, den;
            li = clampi(li, LI_55, 960);
            K.vli[f] = li;
            g = (int32_t)T_G[li];
            den = 65536 + (int32_t)(((int64_t)g * (g + K.vk)) >> 16);
            K.vf[f][0] = (int32_t)(0xFFFFFFFFu / (uint32_t)den);
            K.vf[f][1] = (int32_t)(((int64_t)g * K.vf[f][0]) >> 16);
            K.vf[f][2] = (int32_t)(((int64_t)g * K.vf[f][1]) >> 16);
        }
        K.vf_on = K.vmix > 0;
        hz = (int64_t)e8(P_SHIFT) * e8(P_SHIFT) * 20 >> 16;   /* 0.01 Hz: 0.2 Hz .. 2 kHz, finer near 0 */
        K.fs_inc = (uint32_t)(hz * 974);
        if (e8(P_SHIFT) < 0) K.fs_inc = 0u - K.fs_inc;
        K.sh_fb = e8(P_SFB) * 327 >> 8;
        K.sh_d = (uint32_t)clampi(e8(P_STIME) * 441 / 2560, 32, (int32_t)SH_N - 2);
        K.smix = e8(P_SMIX) * 327 >> 8;
        K.sh_on = K.smix > 0;
        K.gr_dep = e8(P_GROWL) * 327 >> 8;
        K.gr_inc = (uint32_t)e8(P_GRATE) * (PH_PER_HZ >> 8);
        K.gr_on = K.gr_dep > 0;
        K.vx_on = K.vf_on || K.sh_on || K.gr_on;
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
    /* SOUND CHIPS */
    if (chg & KG_EMU) {
        int32_t g = ei(P_EGRIT), m = ei(P_EMU);
        uint32_t j, fc;
        K.em = (uint8_t)(ei(P_EMIX) ? m : 0);
        K.em_inc = emu_rate(m, e8(P_ERATE)) * 97391u;   /* (2^32 / 44100 a Hz) */
        K.em_n = (uint8_t)(g < 50 ? 32 : 16);   /* YM2149: 32 steps of its envelope DAC; AY-style 16 from half way */
        {
            int32_t st = (150 + g * 9 / 2) * 256 / 602;   /* 1.5 .. 6 dB a step, in 1/256 octave */
            K.em_lv[0] = 0;
            for (j = 1; j < K.em_n; j++) K.em_lv[j] = (int32_t)((65535u * exp2_q16(-(int32_t)(K.em_n - 1u - j) * st)) >> 16);
        }
        K.em_bits = (uint8_t)(m == 2 ? 9 - g * 3 / 100 : 16 - g * 8 / 100);   /* YM2612: 9 .. 6 bits; SCSP: 16 .. 8 */
        K.em_lad = g * 512 / 100;               /* YM2612: the ladder's step at zero, up to 4 of its 9-bit steps */
        K.em_sh = m == 5 ? g * 3 / 101 : g / 40;   /* DMG: wave volume 100 / 50 / 25 %; S-DSP: BRR 0 .. 2 shifts coarser */
        K.em_mix = e8(P_EMIX) * 327 >> 8;
        K.em_ev = (m == 3 || m == 4) ? e8(P_EECHO) * 327 >> 8 : 0;
        K.em_fb = K.em_ev * 45 >> 6;
        K.em_d = (uint32_t)ei(P_ETIME) * 706u;  /* 16 ms steps, as the S-DSP's echo */
        fc = (1500u * exp2_q16(e8(P_ETONE) * 957 / 25600)) >> 16;   /* TONE: 1.5 .. 20 kHz */
        {
            int32_t w = (int32_t)fc * 4668 / 1000;
            K.em_lp = (int32_t)((int64_t)w * 32768 / (32768 + w));
        }
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
    case 6: l_dr[i] += (l_sh[i] - l_dr[i]) >> 4; return l_dr[i];
    case 7: return hit_rnd[i];                  /* a new value at each note (each variation its own) */
    default: return amp_env15 * 2 - 32767;      /* the amp envelope: -100 % silent .. +100 % full */
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
/* 16 RTPC: one value (0..10000) through a curve onto three parameters, as a game engine's RTPC drives a sound; SEL
 * sweeps it 0 -> 100 % over SWEEP with the last note held (an engine revving up, a charge filling) */
static volatile uint32_t rt_on, rt_cur, tm_rtpc;
static int32_t fc_coef(int32_t hz)              /* a one-pole low-pass at hz: 1 - e^(-2 pi hz / 44100), Q15 */
{
    return 32768 - (int32_t)(exp2_q16(-(hz * 5262 / 100000)) >> 1);
}
/* 12 SPACE + MOTION: DIST (level, air absorption, more reverb), OCCLUDE (a wall: muffled, quieter), PASS-BY (each note
 * flies past: closest halfway through PASS-BY, the pitch falling by DOPPLER as it goes, the level and the air with it) */
static uint32_t ps_t, ps_seq;
/* 06 ENVELOPES, set C: the MOD ENV, one shot from each note: up in M ATK, down in M DEC (a curve), M AMT of the way
 * (+-) onto any parameter: an impact's pitch drop, a laser's sweep, a filter opening */
static uint32_t ns_t = 0x7FFFFFFFu, ns_seq;     /* samples since the note began */
static volatile int32_t tm_menv;
static int32_t menv_at(uint32_t t, uint32_t am, uint32_t dm)   /* 0..32767 at t samples; am, dm: the times in ms */
{
    uint32_t A = am * 44u + 1u, D = dm * 44u + 1u, sh = 0;
    int32_t x;
    if (t < A) { while ((A >> sh) > 60000u) sh++; return (int32_t)((t >> sh) * 32767u / ((A >> sh) + 1u)); }
    t -= A;
    if (t >= D) return 0;
    while ((D >> sh) > 60000u) sh++;
    x = 32767 - (int32_t)((t >> sh) * 32767u / ((D >> sh) + 1u));
    return x * x >> 15;
}
/* 19 the pad's GESTURE: a move of the pad recorded (50 points a second, 4 s), played again by every note */
#define GEST_MAX 200u
static uint8_t g_x[GEST_MAX], g_y[GEST_MAX];
static volatile uint32_t g_n, g_act, g_pos;
static volatile uint32_t tm_awake;
static volatile int32_t tm_px, tm_sg, tm_sfc = 20000;           /* (the screen: the source's place, -1024 .. 1024, and the gain) */
static void space_block(int32_t *t8, uint32_t n)
{
    int32_t d = t8[P_SDIST] >> 8, o = t8[P_SOCCL] >> 8, pt = P[P_SPASS], g, fc, f2, x = 0;
    uint32_t pm = 65536u;
    K.sp_on = (uint8_t)(d || o || pt);
    if (!K.sp_on) { K.pass_mul = 65536u; tm_sg = 32768; tm_sfc = 20000; return; }
    t8[P_RMIX] = clampi(t8[P_RMIX] + d * 90 + o * 38, 0, 100 * 256);   /* (far: more of the room) */
    g = (int32_t)(exp2_q16(-(d * 102 + o * 51) / 10) >> 1);           /* -24 dB far, -12 dB behind a wall */
    fc = (int32_t)((20000u * exp2_q16(-d * 768 / 100)) >> 16);        /* air: 20 kHz near .. 2.5 kHz far */
    f2 = (int32_t)((20000u * exp2_q16(-o * 1434 / 100)) >> 16);       /* the wall: .. 400 Hz */
    if (f2 < fc) fc = f2;
    if (pt) {
        uint32_t T = (uint32_t)pt * 4410u;
        int32_t h = 256, r, pg;
        if (v_seq != ps_seq) { ps_seq = v_seq; ps_t = 0; }
        x = ((int32_t)(2u * ps_t) - (int32_t)T) / (int32_t)(T >> 10);  /* -1024 coming .. 0 closest .. 1024 gone */
        if (x > 3072) x = 3072;
        if (ps_t < 8u * T) ps_t += n;
        r = (int32_t)isqrt32((uint32_t)(x * x + h * h));
        pm = exp2_q16(-(P[P_SDOPP] * x / r) * 256 / 12);              /* coming: up; going: down */
        pg = h * h * 512 / (x * x + h * h) * 64;   /* (32-bit: no 64-bit division on the chip) */
        g = (int32_t)((int64_t)g * pg >> 15);
        f2 = 1500 + (int32_t)((int64_t)16000 * pg >> 15);
        if (f2 < fc) fc = f2;
    }
    K.pass_mul = pm; K.sp_g = g; K.sp_a = fc_coef(fc);
    tm_px = x; tm_sg = g; tm_sfc = fc;
}
static void rtpc_block(int32_t *t8)
{
    int32_t r = rt_on ? (int32_t)rt_cur : t8[P_QINT] * 100 / 256, c, v;
    r = clampi(r, 0, 10000);
    switch (P[P_QCRV]) {
    case 1: c = r * r / 10000; break;                                  /* EXP: slow, then fast */
    case 2: c = 10000 - (10000 - r) * (10000 - r) / 10000; break;     /* LOG: fast, then slow */
    case 3: c = r * r / 10000 * (30000 - 2 * r) / 10000; break;       /* S-CURVE */
    default: c = r;
    }
    tm_rtpc = (uint32_t)c;
    v = c * 6553 / 1000;                        /* 100 %: the full range of the parameter */
    if (P[P_Q1A]) lfo_apply(t8, TGT[P[P_Q1T]], v, P[P_Q1A]);
    if (P[P_Q2A]) lfo_apply(t8, TGT[P[P_Q2T]], v, P[P_Q2A]);
    if (P[P_Q3A]) lfo_apply(t8, TGT[P[P_Q3T]], v, P[P_Q3A]);
}
static volatile uint16_t v_var;                 /* the note's variation: 0 free, 1.. the seed's variation n (13 VARIATION) */
static uint32_t var_hash(uint32_t seed, uint32_t n)   /* a variation's random stream: the same seed and n, the same sound */
{
    uint32_t h = seed * 0x9E3779B1u ^ (n + 1u) * 0x85EBCA77u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
    return h ? h : 1u;
}
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
    if (v_seq != ns_seq) { ns_seq = v_seq; ns_t = 0; }
    {                                           /* the pad: each axis moves its parameter around where it is set; no
                                                   finger on it: the GESTURE, from the note's start */
        uint32_t gi = ns_t / 882u, gn = g_n;
        int32_t px = pad_x, py = pad_y, on = pad_on || pad_held;
        g_act = 0;
        if (!on && P[P_GEST] && gn && tm_awake && (P[P_GEST] == 2 || gi < gn)) {
            gi %= gn; px = g_x[gi]; py = g_y[gi]; on = 1; g_act = 1; g_pos = gi;
        }
        if (on) {
            if (P[P_XAMT]) lfo_apply(t8, TGT[P[P_XTGT]], (px - 64) * 512, P[P_XAMT]);
            if (P[P_YAMT]) lfo_apply(t8, TGT[P[P_YTGT]], (py - 64) * 512, P[P_YAMT]);
        }
    }
    if (t8[P_MAMT] >> 8) {                      /* the MOD ENV */
        int32_t v = menv_at(ns_t, T_ATT_MS[P[P_MATK]], T_DEC_MS[P[P_MDEC]]);
        tm_menv = v;
        if (v) lfo_apply(t8, TGT[P[P_MTGT]], v * 2, t8[P_MAMT] >> 8);
    } else tm_menv = 0;
    if (ns_t < 0x70000000u) ns_t += n;
    {
        int32_t at = at_keys > at_midi ? at_keys : at_midi;
        if (at && P[P_ATAMT]) lfo_apply(t8, TGT[P[P_ATTGT]], at * 258, P[P_ATAMT]);
    }
    if (rt_on || P[P_QINT]) rtpc_block(t8); else tm_rtpc = 0;
    space_block(t8, n);
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
    int32_t sp_y;                               /* 12 SPACE: its low-pass */
    int32_t vs[3][2];                           /* 09 the formants' integrators */
    int32_t hx[8][2], hy[8][2], hb1;            /* 09 the Hilbert pair: inputs and outputs two back; path B's sample delay */
    uint32_t sh_ph, sh_w, gr_ph, gr_jit;
    int32_t sh_dx, sh_dy;                       /* (the feedback's DC blocker) */
    uint32_t lph[2][5], lsp[2], l_nph[2], l_lfsr[2]; /* 03 LAYERS: their oscillators, their places in the sample */
    uint8_t lon[2];
    uint32_t shp[6], sh_g, sh_mul, sh_cnt;      /* 01 SHEPARD: six octaves' phases, the glide (Q24 of an octave) */
    int32_t c_fr, c_sw, c_vq;                   /* 14 CHIP: samples to the next frame; the sweep (1/4096 oct); the volume */
    uint32_t c_mul, c_n, c_fn, c_ai, c_lfsr, c_nph;   /* the pitch factor (Q16); frames: this arp step, all; the arp step;
                                                     the noise */
} V;
enum { ST_A, ST_D, ST_R, ST_OFF };
static volatile uint32_t tm_play, tm_awake, tm_rf_li, tm_gi, tm_peak, tm_lvl, tm_cpu, tm_rv;
static int16_t tm_scope[1024];                  /* the output's last 1024 samples (stage_out), for the screen's motion layer */
static volatile uint32_t tm_sc_w;
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
    for (i = 0; i < EM_N; i++) em_buf[i] = 0;
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
    case W_SHEP: {                               /* six sines an octave apart under a bell: the endless glide */
        uint32_t k, g = V.sh_g;
        int32_t sum = 0;
        if (!V.sh_cnt--) { V.sh_cnt = 31; V.sh_mul = exp2_q16((int32_t)(g >> 16)); }
        for (k = 0; k < 6u; k++) {
            uint32_t ik = (uint32_t)(((uint64_t)(inc >> 3) * V.sh_mul) >> 16) << k;
            sum += isin(V.shp[k] += ik) * (int32_t)T_HANN[((k << 16) + (g >> 8)) / 6u >> 8] >> 15;
        }
        g += (uint32_t)K.sh_inc;
        if (g >= 1u << 24) {                     /* an octave on: each sine takes its neighbour's place */
            if ((int32_t)K.sh_inc > 0) { for (k = 5; k; k--) V.shp[k] = V.shp[k - 1u]; V.shp[0] = 0; }
            else { for (k = 0; k < 5u; k++) V.shp[k] = V.shp[k + 1u]; V.shp[5] = 0; }
            g &= (1u << 24) - 1u; V.sh_cnt = 0;
        }
        V.sh_g = g;
        return sum * 11000 >> 15;
    }
    default: return 0;
    }
}

/* 01 the sample's loop: LOOP repeats [lp_s, lp_e) (L POS, L LEN), each pass crossfading its last lp_xf samples into
 * the ones before lp_s; SUSTAIN does so while the key is held, then plays on from there to the end (START > LOOP > STOP,
 * as a game engine plays a held sound). Both points sit on rising zero crossings (loop_points, main loop). */
static volatile int32_t lp_s, lp_e, lp_xf;
DSP int32_t smp_tick(uint32_t step)          /* 01 SAMPLE: one sample at the voice's pitch (22.05 kHz source) */
{
    const int16_t *b = smp_ptr;
    int32_t len = (int32_t)smp_len, s, i;
    if (!V.sp_on || smp_busy || len < 256) return 0;
    if (K.unity) {
        i = (int32_t)(V.spos >> 12);
        s = herm16(b, i, V.spos & 4095u, len);
        if (K.sloop && (K.sloop == 1u || V.stage != ST_R) && i < lp_e) {   /* (SUSTAIN let go: on to the end) */
            int32_t ls = lp_s, le = lp_e, span = le - ls, xf = lp_xf;
            if (xf && i >= le - xf) {
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
        if (K.sloop && (K.sloop == 1u || V.stage != ST_R) && (int32_t)(V.cpos >> 12) < lp_e) {
            if ((int32_t)(V.cpos >> 12) >= lp_e - 2) V.cpos = (uint32_t)lp_s << 12;
        } else if ((int32_t)V.cpos >= end - 4096) { V.sp_on = 0; return 0; }
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
            uint32_t vs = v_var ? var_hash((uint32_t)P[P_VSEED], v_var) : 0u, *rs = v_var ? &vs : &rng_isr;
            if (v_var) {                        /* a variation: all of this note's chance from the seed, the LFOs from 0 */
                rng_isr = vs ^ 0x5BD1E995u; if (!rng_isr) rng_isr = 1u;
                l_ph[0] = l_ph[1] = 0u; l_sh[0] = l_sh[1] = l_dr[0] = l_dr[1] = 0;
            }
            if (P[P_VARY] && !vary_mute) {
                for (k = 0; k < 8u; k++) vr[k] = ((int32_t)(xrand(rs) >> 17) - 16384) * 2;
                start = (xrand(rs) >> 16) * (smp_len * (uint32_t)P[P_VARY] / 10000u) >> 16;
            } else for (k = 0; k < 8u; k++) vr[k] = 0;
            rs = v_var ? &vs : &rng_hit;        /* (its own stream: the other random draws stay as they were) */
            hit_rnd[0] = (int32_t)(xrand(rs) >> 16) - 32768; hit_rnd[1] = (int32_t)(xrand(rs) >> 16) - 32768;
        }
        k_update(chg | KG_NOTE);
        if (!legato) {
            V.stage = ST_A;
            V.lon[0] = V.lon[1] = 1; V.lsp[0] = V.lsp[1] = start << 12;
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
/* 03 LAYERS: the same source again, L PITCH away (and DETUNE), and a detuned double; each with its own oscillator
 * phases and its own place in the sample (one shot or the loop: no crossfade, no stretch, no grains) */
static int32_t __attribute__((noinline)) osc_layer(uint32_t k, uint32_t inc)
{
    uint32_t sv[5], j, nph = V.c_nph, lf = V.c_lfsr;
    int32_t o;
    for (j = 0; j < 5u; j++) { sv[j] = V.oph[j]; V.oph[j] = V.lph[k][j]; }
    V.c_nph = V.l_nph[k]; V.c_lfsr = V.l_lfsr[k] ? V.l_lfsr[k] : 1u;
    o = K.wave == W_SHEP ? 0 : osc_tick(inc);   /* (SHEPARD is a chord already) */
    for (j = 0; j < 5u; j++) { V.lph[k][j] = V.oph[j]; V.oph[j] = sv[j]; }
    V.l_nph[k] = V.c_nph; V.l_lfsr[k] = V.c_lfsr; V.c_nph = nph; V.c_lfsr = lf;
    return o;
}
static int32_t __attribute__((noinline)) smp_layer(uint32_t k, uint32_t step)
{
    const int16_t *b = smp_ptr;
    int32_t len = (int32_t)smp_len, i, s;
    if (!V.lon[k] || smp_busy || len < 256) return 0;
    i = (int32_t)(V.lsp[k] >> 12);
    if (i >= len - 4) { V.lon[k] = 0; return 0; }
    s = herm16(b, i, V.lsp[k] & 4095u, len);
    V.lsp[k] += step;
    if (K.sloop && (K.sloop == 1u || V.stage != ST_R) && i < lp_e) {
        if ((int32_t)(V.lsp[k] >> 12) >= lp_e) V.lsp[k] -= (uint32_t)(lp_e - lp_s) << 12;
    } else if (i > len - 260) s = s * (len - 4 - i) >> 8;
    return s;
}
static void stage_src(uint32_t n)               /* the envelope, the glide, 01 the sources (in flash: 11 waves, big) */
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t env15, o = 0, s = 0, sig = 0;
        switch (V.stage) {
        case ST_A: V.env += K.a_inc; if (V.env >= ENV_MAX) { V.env = ENV_MAX; V.stage = ST_D; } break;
        case ST_D: V.env += (int32_t)(((int64_t)(K.sus23 - V.env) * K.d_a) >> 16); break;
        case ST_R:                              /* (SUSTAIN on the sample alone: its STOP part ends the note, not RELEASE) */
            if (K.sloop == 2u && K.mode == 1u && V.sp_on && !K.gmode) break;
            V.env -= (int32_t)(((int64_t)V.env * K.r_a) >> 16) + 1; if (V.env <= 0) { V.env = 0; V.stage = ST_OFF; } break;
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
            if (K.pass_mul != 65536u) ratio = (uint32_t)(((uint64_t)ratio * K.pass_mul) >> 16);   /* (12 PASS-BY) */
            if (K.wo) o = osc_tick((uint32_t)(((uint64_t)C4_INC * ratio) >> 16));
            if (K.ws) s = (K.gfrz || K.gmode) ? grain_tick() : smp_tick(ratio >> 5);
            if (K.ly_n) {                       /* 03 LAYERS */
                uint32_t k;
                int32_t lo = 0, ls = 0;
                for (k = 0; k < K.ly_n; k++) {
                    uint32_t r2 = (uint32_t)(((uint64_t)ratio * K.ly_mul[k]) >> 16);
                    if (K.wo) lo += osc_layer(k, (uint32_t)(((uint64_t)C4_INC * r2) >> 16));
                    if (K.ws && K.unity && !K.gfrz && !K.gmode) ls += smp_layer(k, r2 >> 5);
                }
                o = (int32_t)(((int64_t)(o + (int32_t)(((int64_t)lo * K.ly_g) >> 15)) * K.ly_norm) >> 15);
                s = (int32_t)(((int64_t)(s + (int32_t)(((int64_t)ls * K.ly_g) >> 15)) * K.ly_norm) >> 15);
            }
            sig = (int32_t)(((int64_t)o * K.wo + (int64_t)s * K.ws) >> 15);
            sig = (int32_t)(((int64_t)sig * (K.gfrz && V.stage == ST_OFF ? 32767 : env15) >> 15) * K.velg >> 15);
        }
        blk_e[i] = env15;
        blk_b[i] = sig;
    }
    amp_env15 = n ? blk_e[n - 1u] : 0;          /* (the LFO shape AMP ENV, next block) */
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
    uint32_t i, lcnt = V.lcnt, quiet = V.quiet, peak = tm_peak, still = V.stage == ST_OFF && !K.gfrz, sw = tm_sc_w;
    int16_t *sc = tm_scope;
    if (K.sp_on) {                              /* 12 SPACE: air / wall low-pass, the distance's level */
        int32_t y = V.sp_y, a = K.sp_a, g = K.sp_g;
        for (i = 0; i < n; i++) { y += (int32_t)(((int64_t)(blk_b[i] - y) * a) >> 15); blk_b[i] = (int32_t)(((int64_t)y * g) >> 15); }
        V.sp_y = y;
    } else if (n) V.sp_y = blk_b[n - 1u];       /* (switched on later: no jump) */
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
        sc[(sw + i) & 1023u] = (int16_t)v;
        out[2u * i] += v;
        out[2u * i + 1u] += v;
    }
    tm_sc_w = sw + n;
    tm_gr = lim > LIM_CEIL ? lg : 32768;
    V.lim = lim; V.lg = lg; V.lcnt = lcnt; V.quiet = quiet; tm_peak = peak;
}
/* 09 VOWEL + SHIFT (in flash, as 01: off, it costs nothing). GROWL: a rough, unsteady tremolo (a throat); VOWEL: three
 * band-passes on the formants of U O A E I, morphed, SIZE moves them (down: bigger), RES narrows them; SHIFT: every
 * frequency moved by the same Hz (a Hilbert pair, Olli Niemitalo's), with FDBK through an S TIME line: the spiral */
static const int32_t HA_C[4] = {514752760, 940832443, 1048613677, 1071056671};   /* a^2, Q30 */
static const int32_t HB_C[4] = {173686865, 787083823, 1015061512, 1063647745};
static void stage_vox(uint32_t n)
{
    uint32_t i, k;
    int32_t *b = blk_b;
    if (K.gr_on) for (i = 0; i < n; i++) {
        uint32_t old = V.gr_ph;
        int32_t m;
        V.gr_ph += K.gr_inc + V.gr_jit;
        if (V.gr_ph < old) V.gr_jit = xrand(&rng_isr) % (K.gr_inc >> 1) << 0;   /* each cycle a little off: up to +50 % */
        m = 16384 + (isin(V.gr_ph) >> 1);       /* 0 .. 1 */
        m = m * m >> 15;                        /* (a narrower opening: rougher) */
        b[i] = (int32_t)(((int64_t)b[i] * (32768 - (((32768 - m) * K.gr_dep) >> 15))) >> 15);
    }
    if (K.vf_on) for (i = 0; i < n; i++) {
        static const int32_t VG[3] = {58000, 38000, 24000};   /* F1 F2 F3, Q15 (over 1: band-passes take a lot away) */
        int32_t x = b[i], y = 0;
        for (k = 0; k < 3u; k++) {
            int32_t *st = V.vs[k], v3 = x - st[1], v1, v2;
            v1 = (int32_t)(((int64_t)K.vf[k][0] * st[0] + (int64_t)K.vf[k][1] * v3 + 32768) >> 16);
            v2 = st[1] + (int32_t)(((int64_t)K.vf[k][1] * st[0] + (int64_t)K.vf[k][2] * v3 + 32768) >> 16);
            st[0] = clampi(2 * v1 - st[0], -3000000, 3000000);
            st[1] = clampi(2 * v2 - st[1], -3000000, 3000000);
            y += (int32_t)(((int64_t)v1 * K.vk >> 16) * VG[k] >> 15);   /* (k v1: a band-pass of unity peak) */
        }
        b[i] = x + (int32_t)(((int64_t)(y - x) * K.vmix) >> 15);
    }
    if (K.sh_on) for (i = 0; i < n; i++) {
        int32_t x = b[i], a = x, bb, fbv, y;
        fbv = sh_buf[(V.sh_w - K.sh_d) & (SH_N - 1u)];
        if (fbv < 24 && fbv > -24) fbv = 0;     /* (a gate at -62 dB: the loop's rounding can't hum on its own) */
        a = clampi(x + (int32_t)(((int64_t)fbv * K.sh_fb) >> 15), -98000, 98000);
        a <<= 8; bb = a;                        /* (8 more bits inside: the poles near 1 would turn rounding into a hum) */
        for (k = 0; k < 4u; k++) {              /* the two allpass chains: 90 degrees apart, 20 Hz .. 20 kHz */
            int32_t ya = (int32_t)(((int64_t)HA_C[k] * (a + V.hy[k][1]) + (1 << 29)) >> 30) - V.hx[k][1];
            int32_t yb = (int32_t)(((int64_t)HB_C[k] * (bb + V.hy[k + 4][1]) + (1 << 29)) >> 30) - V.hx[k + 4][1];
            V.hx[k][1] = V.hx[k][0]; V.hx[k][0] = a; V.hy[k][1] = V.hy[k][0]; V.hy[k][0] = ya; a = ya;
            V.hx[k + 4][1] = V.hx[k + 4][0]; V.hx[k + 4][0] = bb; V.hy[k + 4][1] = V.hy[k + 4][0]; V.hy[k + 4][0] = yb; bb = yb;
        }
        { int32_t t = V.hb1; V.hb1 = bb; bb = t; }   /* (path B a sample later) */
        V.sh_ph += K.fs_inc;
        y = (int32_t)(((int64_t)a * isin(V.sh_ph + 0x40000000u) + (int64_t)bb * isin(V.sh_ph) + (1 << 22)) >> 23);   /* (+: up) */
        sh_buf[V.sh_w] = sat16(dcblock(y, &V.sh_dx, &V.sh_dy));
        V.sh_w = (V.sh_w + 1u) & (SH_N - 1u);
        b[i] = x + (int32_t)(((int64_t)(y - x) * K.smix) >> 15);
    }
}
/* SOUND CHIPS (in flash, as 09: off, it costs nothing). The whole chain, printed through a console's sound chip: each
 * takes the sound at its own sample clock (RATE, no anti-alias filter, as the hardware) and through its own DAC:
 *   YM2149  the volume register as a DAC (the Atari ST's digi-drums): 32 log steps, 16 with GRIT; 1.5 .. 6 dB apart
 *   YM2612  the Mega Drive's 8-bit DAC channel, 9 .. 6 bits out, and the ladder's step at zero (crossover grit)
 *   S-DSP   the SNES: BRR (4-bit ADPCM, 16-sample blocks, filter 1) at 32 kHz or less, Gaussian-like interpolation,
 *           the echo with its 8-tap FIR and feedback (E TIME in 16 ms steps)
 *   SCSP    the Saturn: linear PCM 16 .. 8 bits, linear interpolation, its DSP as a short diffuse echo
 *   DMG     the Game Boy's wave channel: 4-bit samples, the volume shift, and the output capacitor's high-pass
 * TONE is the console's analog low-pass, MIX against the sound before it */
static struct { uint32_t ph; int32_t h[4], hp_x, hp_y, lp, fir[8], bprev, bin[16], bout[16]; uint32_t bi, w; uint8_t last; } EMV;
static int32_t emu_take(int32_t x)              /* one sample at the chip's clock: through its DAC */
{
    int32_t y, q;
    x = clampi(x, -32767, 32767);
    switch (K.em) {
    case 1: {                                   /* the nearest log step (a binary search) */
        int32_t u = x + 32768, lo = 0, hi = K.em_n - 1;
        while (hi - lo > 1) { int32_t mid = (lo + hi) >> 1; if (K.em_lv[mid] <= u) lo = mid; else hi = mid; }
        return (u - K.em_lv[lo] < K.em_lv[hi] - u ? K.em_lv[lo] : K.em_lv[hi]) - 32768;
    }
    case 2:
        q = (x >> 8) << 8;                      /* the DAC channel: 8 bits in */
        y = q >> (16 - K.em_bits) << (16 - K.em_bits);
        return y + (y < 0 ? -K.em_lad : K.em_lad);
    case 3: {                                   /* BRR: 16 in, then the block coded and played back, 16 out */
        uint32_t k;
        EMV.bin[EMV.bi] = x;
        y = EMV.bout[EMV.bi];
        if (++EMV.bi >= 16u) {
            int32_t m = 0, sh = 0, p = EMV.bprev;
            for (k = 0; k < 16u; k++) { int32_t r = absi(EMV.bin[k] - (p - (p >> 4))); if (r > m) m = r; p = EMV.bin[k]; }
            while (sh < 12 && (m >> sh) > 7) sh++;
            sh = clampi(sh + K.em_sh, 0, 12);
            p = EMV.bprev;
            for (k = 0; k < 16u; k++) {
                int32_t pr = p - (p >> 4), r = EMV.bin[k] - pr, n4 = clampi((r + (sh ? 1 << (sh - 1) : 0)) >> sh, -8, 7);
                p = clampi(pr + (n4 << sh), -32768, 32767);
                EMV.bout[k] = p;
            }
            EMV.bprev = p; EMV.bi = 0;
        }
        return y;
    }
    case 4: return x >> (16 - K.em_bits) << (16 - K.em_bits);
    default: q = (x + 32768) >> 12 >> K.em_sh << K.em_sh; return q * 4096 + (2048 << K.em_sh) - 32768;   /* DMG: 16 / 8 / 4 levels */
    }
}
static void stage_emu(uint32_t n)
{
    static const int8_t FIR[8] = {12, 33, 43, 43, 19, -2, -13, -7};   /* the S-DSP's echo filter, a usual setting (/128) */
    uint32_t i, k;
    int32_t *b = blk_b;
    if (K.em != EMV.last) { for (i = 0; i < EM_N; i++) em_buf[i] = 0; for (k = 0; k < 8u; k++) EMV.fir[k] = 0; EMV.last = K.em; }
    for (i = 0; i < n; i++) {
        int32_t x = b[i], y, t;
        uint32_t ph = EMV.ph + K.em_inc;
        if (ph < EMV.ph) { EMV.h[3] = EMV.h[2]; EMV.h[2] = EMV.h[1]; EMV.h[1] = EMV.h[0]; EMV.h[0] = emu_take(x); }
        EMV.ph = ph;
        t = (int32_t)(ph >> 17);                /* (where between the chip's samples: 0 .. 32767) */
        if (K.em == 3) {                        /* a cubic B-spline over four: smooth and a little dull, as the SNES */
            int32_t t2 = t * t >> 15, t3 = t2 * t >> 15, u = 32768 - t, u3 = ((u * u >> 15) * u) >> 15;
            int32_t w0 = u3 / 6, w3 = t3 / 6, w1 = (3 * t3 - 6 * t2 + 4 * 32768) / 6, w2 = 32768 - w0 - w1 - w3;
            y = (int32_t)(((int64_t)EMV.h[3] * w0 + (int64_t)EMV.h[2] * w1 + (int64_t)EMV.h[1] * w2 + (int64_t)EMV.h[0] * w3) >> 15);
        } else if (K.em == 4) y = EMV.h[1] + (int32_t)(((int64_t)(EMV.h[0] - EMV.h[1]) * t) >> 15);
        else y = EMV.h[0];
        if (K.em == 1 || K.em == 5) {           /* (the unipolar DACs: their DC out; the Game Boy's capacitor, stronger) */
            int32_t a = K.em == 5 ? 32640 : 32740, hy = y - EMV.hp_x + (int32_t)(((int64_t)EMV.hp_y * a) >> 15);
            EMV.hp_x = y; EMV.hp_y = hy; y = hy;
        }
        if (K.em_ev) {                          /* the echo */
            int32_t d = em_buf[(EMV.w - K.em_d) & (EM_N - 1u)], e = 0;
            if (K.em == 3) {
                for (k = 7; k > 0u; k--) EMV.fir[k] = EMV.fir[k - 1u];
                EMV.fir[0] = d;
                for (k = 0; k < 8u; k++) e += EMV.fir[k] * FIR[k];
                e >>= 7;
            } else e = (d + em_buf[(EMV.w - K.em_d * 3u / 4u) & (EM_N - 1u)] + em_buf[(EMV.w - K.em_d / 2u) & (EM_N - 1u)]) / 3;
            em_buf[EMV.w] = sat16(y + (int32_t)(((int64_t)e * K.em_fb) >> 15));
            EMV.w = (EMV.w + 1u) & (EM_N - 1u);
            y += (int32_t)(((int64_t)e * K.em_ev) >> 15);
        }
        EMV.lp += (int32_t)(((int64_t)(y - EMV.lp) * K.em_lp) >> 15);   /* TONE */
        b[i] = x + (int32_t)(((int64_t)(EMV.lp - x) * K.em_mix) >> 15);
    }
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
        if (K.vx_on) stage_vox(n);
        if (K.rfmode) RAM_CALL(stage_flt)(n);
        if (K.ott && !K.opost) RAM_CALL(stage_ott)(n);
        if (V.stage == ST_OFF && !K.gfrz) { for (i = 0; i < n; i++) pq = absi(blk_b[i]) <= 1 ? pq + 1u : 0u; }
        else pq = 0;
        V.pquiet = pq;
    } else { for (i = 0; i < n; i++) blk_b[i] = 0; tm_lvl = 0; }
    RAM_CALL(stage_fx)(n);
    if (!K.rv_wet) V.rv_off = 1;
    if (K.ott && K.opost) RAM_CALL(stage_ott)(n);         /* POST FX: on the tails too */
    if (K.em) stage_emu(n);                     /* SOUND CHIPS: last, the print */
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
static void note_on_var(int32_t note, int32_t vel15, uint32_t var)   /* a note; var: as variation var of the seed */
{
    uint32_t i, j;
    for (i = j = 0; i < nstk; i++) if (stk[i] != note) stk[j++] = stk[i];
    nstk = j;
    if (nstk < 16u) stk[nstk++] = (int8_t)note;
    v_note = note; v_vel = vel15; v_gate = 1; v_var = (uint16_t)var;
    v_seq++;                                    /* (last: the audio picks the note up by this) */
}
static void note_on(int32_t note, int32_t vel15) { note_on_var(note, vel15, 0); }
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
static uint32_t sel_t, help_n;                  /* SEL: when it went down, the help lines shown while held */
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
static void audition_ms(uint32_t ms)            /* a C4 held ms (the gate released at once would skip the attack) */
{
    if (v_gate) return;
    note_on(60, 32767);
    aud_t = fm1_ms + ms; if (!aud_t) aud_t = 1;
}
static void audition(void) { audition_ms(150u); }
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
    case PG_VOX: set_unlocked(P_VOWEL, rnd_range(0, 100)); set_unlocked(P_VSIZE, rnd_range(-10, 6)); set_unlocked(P_VRES, rnd_range(20, 85));
            set_unlocked(P_VMIX, rnd_range(0, 99) > 40 ? rnd_range(50, 100) : 0);
            set_unlocked(P_SHIFT, rnd_range(-60, 60)); set_unlocked(P_SFB, rnd_range(0, 99) > 60 ? rnd_range(20, 85) : 0);
            set_unlocked(P_STIME, rnd_range(5, 60)); set_unlocked(P_SMIX, rnd_range(0, 99) > 55 ? rnd_range(40, 100) : 0);
            set_unlocked(P_GROWL, rnd_range(0, 99) > 70 ? rnd_range(20, 80) : 0); set_unlocked(P_GRATE, rnd_range(20, 90)); break;
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
    case PG_ASSET: set_unlocked(P_EMU, rnd_range(1, 5)); set_unlocked(P_ERATE, rnd_range(10, 90)); set_unlocked(P_EGRIT, rnd_range(0, 100));
            set_unlocked(P_EECHO, rnd_range(0, 99) > 50 ? rnd_range(10, 60) : 0); set_unlocked(P_ETIME, rnd_range(2, 12)); set_unlocked(P_ETONE, rnd_range(30, 100)); break;
    case PG_PAD: set_unlocked(P_XTGT, rnd_range(0, NTGT - 1)); set_unlocked(P_YTGT, rnd_range(0, NTGT - 1));
            set_unlocked(P_XAMT, rnd_range(-100, 100)); set_unlocked(P_YAMT, rnd_range(-100, 100)); break;
    default: set_unlocked(P_VARY, rnd_range(10, 70)); break;   /* MORPH: only the variation */
    }
}
/* HA KIND (13 B): the HAPPY ACCIDENT inside a recipe, as 02 CHIP's presets are: the chain from its defaults, then each
 * kind's own ranges. Locks are respected; the sample slot stays. */
static int32_t tgt_of(uint32_t p) { uint32_t i; for (i = 0; i < NTGT; i++) if (TGT[i] == p) return (int32_t)i; return 0; }
static int32_t pick(int32_t a, int32_t b, int32_t c, int32_t d) { int32_t v[4]; v[0] = a; v[1] = b; v[2] = c; v[3] = d; return v[rnd_range(0, 3)]; }
#define RU(p, lo, hi) set_unlocked(p, rnd_range(lo, hi))
#define RS(p, v) set_unlocked(p, v)
static void recipe(uint32_t kind)
{
    uint32_t i;
    for (i = 0; i < NP; i++) {
        uint32_t m = PMOD[i];
        if (m == PG_LAB || m == PG_VAR || m == PG_BAT || m == PG_PHY || m == PG_PAD || m == PG_RTPC || m == PG_ASSET) continue;
        if (i == P_SLOT || i == P_THEME || i == P_RECM || i == P_VOL) continue;
        set_unlocked(i, PD[i].def);
    }
    switch (kind) {
    case 1:                                     /* IMPACT: a hit, its pitch dropping, a body ringing */
        RS(P_MODE, pick(0, 0, 2, 2)); RS(P_WAVE, pick(6, 0, 7, 9)); RU(P_COLOR, 20, 80); RU(P_TUNE, -19, -5);
        RS(P_ATT, 0); RU(P_DEC, 18, 34); RS(P_SUS, 0); RU(P_REL, 18, 32);
        RS(P_MATK, 0); RU(P_MDEC, 12, 26); RU(P_MAMT, 20, 60); RS(P_MTGT, tgt_of(P_TUNE));
        RU(P_TATK, 30, 80);
        if (rnd_range(0, 99) < 70) { RU(P_ZMIX, 30, 85); RU(P_ZMAT, 0, 6); RU(P_ZDEC, 18, 40); RU(P_ZPITCH, -12, 5); }
        RU(P_XDRV, 8, 36); RU(P_XSHP, 0, 1);
        RS(P_RFMODE, 1); RU(P_RFFREQ, 34, 50); RU(P_RFRES, 10, 40);
        RU(P_RMIX, 8, 28); RU(P_RDEC, 20, 40); RS(P_DMIX, 0);
        break;
    case 2:                                     /* WHOOSH: noise swelling through a filter that opens; sometimes passing by */
        RS(P_MODE, 0); RS(P_WAVE, 6); RU(P_COLOR, 45, 95);
        RU(P_ATT, 34, 48); RU(P_DEC, 38, 52); RS(P_SUS, 0); RU(P_REL, 30, 45);
        RS(P_RFMODE, pick(1, 2, 2, 2)); RU(P_RFFREQ, 28, 40); RU(P_RFRES, 25, 55); RS(P_RFDEP, 0);
        RU(P_MATK, 30, 46); RU(P_MDEC, 34, 50); RU(P_MAMT, 35, 75); RS(P_MTGT, tgt_of(P_RFFREQ));
        RU(P_FMIX, 20, 60); RU(P_FDEPTH, 40, 90); RU(P_DMIX, 0, 15); RU(P_RMIX, 12, 32);
        RU(P_ODEPTH, 55, 85); RU(P_OIN, 20, 30);   /* (a band of noise is quiet: the OTT brings it up) */
        if (rnd_range(0, 99) < 50) {            /* passing by: closest when the swell peaks */
            RS(P_SPASS, clampi((2 * T_ATT_MS[P[P_ATT]] + T_DEC_MS[P[P_DEC]] / 3) / 100, 5, 30)); RU(P_SDOPP, 3, 9); RU(P_SDIST, 0, 12);
        }
        break;
    case 3:                                     /* UI: short, high, clean; a chirp or a little arpeggio */
        RS(P_MODE, 0); RS(P_WAVE, pick(0, 1, W_CPLS, 8)); RU(P_TUNE, 7, 24);
        RU(P_ATT, 0, 3); RU(P_DEC, 12, 26); RS(P_SUS, 0); RU(P_REL, 12, 24);
        RS(P_RFMODE, 0); RS(P_OSTYLE, 0);
        RU(P_MDEC, 8, 20); RS(P_MAMT, rnd_range(10, 30) * (rnd_range(0, 1) ? 1 : -1)); RS(P_MTGT, tgt_of(P_PITCH));
        if (rnd_range(0, 99) < 40) { RU(P_CARP, 8, 10); RU(P_CARPS, 2, 5); }
        RU(P_DMIX, 0, 18); RU(P_DTIME, 20, 40); RU(P_DFB, 10, 35); RU(P_RMIX, 0, 14);
        break;
    case 4:                                     /* MAGIC: bright layers, glass or bells, long echoes */
        RS(P_MODE, 0); RS(P_WAVE, pick(8, 9, W_SHEP, W_PADSCAN)); RU(P_COLOR, 55, 95); RU(P_TUNE, 0, 19);
        RU(P_VOICES, 1, 2); RS(P_LPIT, pick(12, 7, -12, 12)); RU(P_LDET, 5, 25); RU(P_LLVL, 40, 80);
        RU(P_ATT, 4, 28); RU(P_DEC, 40, 54); RU(P_SUS, 20, 55); RU(P_REL, 40, 55);
        RU(P_ZMIX, 25, 70); RS(P_ZMAT, pick(1, 4, 1, 4)); RU(P_ZDEC, 34, 50);
        RS(P_L1SHP, pick(0, 5, 0, 5)); RU(P_L1RATE, 40, 58); RU(P_L1MIX, 5, 22); RS(P_L1TGT, tgt_of(P_PITCH));
        RU(P_DMIX, 20, 45); RU(P_DFB, 40, 72); RU(P_DTIME, 30, 55); RU(P_RMIX, 28, 50); RU(P_RSIZE, 70, 100); RU(P_RDEC, 38, 52);
        RS(P_RFMODE, pick(0, 3, 0, 3)); RU(P_RFFREQ, 20, 36);
        break;
    case 5:                                     /* CREATURE: low, a throat (a band-pass wandering), a growl */
        RS(P_MODE, pick(0, 0, 2, 0)); RS(P_WAVE, pick(7, 8, 2, 8)); RU(P_COLOR, 30, 90); RU(P_TUNE, -24, -7);
        RU(P_ATT, 8, 28); RU(P_DEC, 44, 54); RU(P_SUS, 45, 75); RU(P_REL, 34, 48);
        RS(P_RFMODE, 2); RU(P_RFFREQ, 26, 40); RU(P_RFRES, 50, 82);
        RS(P_L2SHP, 6); RU(P_L2RATE, 30, 50); RU(P_L2MIX, 30, 60); RS(P_L2TGT, tgt_of(P_RFFREQ));
        RS(P_L1SHP, pick(4, 2, 4, 3)); RU(P_L1RATE, 52, 63); RU(P_L1MIX, 15, 45); RS(P_L1TGT, tgt_of(P_VOL));
        RU(P_MDEC, 30, 46); RS(P_MAMT, rnd_range(20, 45) * (rnd_range(0, 1) ? 1 : -1)); RS(P_MTGT, tgt_of(P_PITCH));
        RU(P_XDRV, 12, 34); RS(P_XSHP, pick(0, 2, 0, 2)); RU(P_RMIX, 10, 25);
        break;
    case 6:                                     /* WEAPON: a shot, its pitch falling fast, driven and squashed */
        RS(P_MODE, 0); RS(P_WAVE, pick(2, 3, W_CNSE, 4)); RU(P_TUNE, 0, 12);
        RS(P_ATT, 0); RU(P_DEC, 20, 34); RS(P_SUS, 0); RU(P_REL, 16, 30);
        RS(P_MATK, 0); RU(P_MDEC, 14, 30); RU(P_MAMT, 35, 90); RS(P_MTGT, tgt_of(P_TUNE));
        RU(P_XDRV, 16, 44); RU(P_XSHP, 0, 2); RU(P_TATK, 30, 70);
        RS(P_OSTYLE, pick(1, 3, 4, 1)); RU(P_ODEPTH, 40, 85);
        RS(P_RFMODE, 1); RU(P_RFFREQ, 44, 58); RU(P_RFRES, 15, 45);
        RU(P_DMIX, 0, 12); RU(P_RMIX, 8, 22);
        break;
    default:                                    /* AMBIENT: grains of the sample, slow, far, looping */
        RS(P_MODE, pick(1, 2, 1, 2)); RS(P_WAVE, pick(6, 8, 0, 8)); RU(P_COLOR, 10, 50);
        RS(P_GMODE, 1); RU(P_GSIZE, 120, 220); RU(P_GDENS, 10, 24); RU(P_GSPRAY, 20, 50); RU(P_GREV, 20, 60);
        RU(P_ATT, 44, 58); RU(P_SUS, 85, 100); RU(P_REL, 50, 60); RS(P_SLOOP, 1);
        RS(P_L2SHP, 6); RU(P_L2RATE, 8, 25); RU(P_L2MIX, 15, 40); RS(P_L2TGT, tgt_of(P_RFFREQ));
        RS(P_RFMODE, 1); RU(P_RFFREQ, 30, 46); RU(P_RFRES, 15, 40);
        RU(P_RMIX, 35, 55); RU(P_RSIZE, 80, 100); RU(P_RDEC, 44, 58); RU(P_DMIX, 10, 30); RU(P_DFB, 45, 75);
    }
}
/* 15 ASSETS: a base sound for each common game asset, as a starting point (not a variation: the same every time).
 * Built from the defaults: the chain from scratch, then the asset's own values; locks are respected, the sample slot,
 * the theme and the VARIATION page stay. Each sets HA KIND to its family, so HAPPY ACCIDENT then mutates inside it,
 * and a VARY for BATCH. Only the oscillator: an asset sounds the same whatever is in the sample slots. */
static const uint8_t ASSET_VOL[NASSET] = {45, 68, 36, 77, 79, 86, 80, 100, 42, 100, 100, 100, 100, 75, 36, 91, 36, 53, 37};   /* (each asset about as loud as the others: tests/assets_test.c) */
static uint8_t asset_cur = 255;                 /* the one loaded (255: none) and the patch it left, to show EDITED */
static uint32_t asset_sum;
static uint32_t patch_sum(void) { uint32_t i, h = 2166136261u; for (i = 0; i < NP; i++) if (i != P_ASSET && i != P_THEME && i != P_VSEED) { h ^= (uint16_t)P[i]; h *= 16777619u; } return h; }
static uint16_t asset_hold_ms(uint32_t k)       /* how long its audition holds the key */
{
    static const uint16_t H[NASSET] = {150, 150, 150, 0, 150, 150, 400, 700, 1600, 150, 150, 150, 150, 150, 400, 900, 1200, 1200, 900};
    return H[k % NASSET];
}
static void asset_load(uint32_t k)
{
    uint32_t i;
    k %= NASSET;
    for (i = 0; i < NP; i++) {
        uint32_t m = PMOD[i];
        if (m == PG_LAB || m == PG_VAR || m == PG_BAT || m == PG_PAD || m == PG_ASSET) continue;
        if (i == P_SLOT || i == P_THEME || i == P_RECM) continue;
        set_unlocked(i, PD[i].def);
    }
    RS(P_MODE, 0); RS(P_RFMODE, 0); RS(P_RFDEP, 0); RS(P_OSTYLE, 0); RS(P_DMIX, 0); RS(P_RMIX, 0);
    switch (k) {
    case 0:                                     /* EXPLODE: noise through a closing low-pass, a drum body far down, a room */
        RS(P_WAVE, 6); RS(P_COLOR, 60); RS(P_ATT, 0); RS(P_DEC, 48); RS(P_SUS, 0); RS(P_REL, 46);
        RS(P_RFMODE, 1); RS(P_RFFREQ, 30); RS(P_RFRES, 12);
        RS(P_MTGT, tgt_of(P_RFFREQ)); RS(P_MATK, 0); RS(P_MDEC, 42); RS(P_MAMT, 46);
        RS(P_ZMIX, 45); RS(P_ZMAT, 5); RS(P_ZPITCH, -12); RS(P_ZDEC, 33); RS(P_ZTRK, 0); RS(P_ZDAMP, 40);
        RS(P_TATK, 60); RS(P_XDRV, 24); RS(P_XSHP, 0);
        RS(P_OSTYLE, 3); RS(P_ODEPTH, 50); RS(P_RMIX, 24); RS(P_RSIZE, 90); RS(P_RDEC, 27); RS(P_RPRE, 10);
        RS(P_RECIPE, 1); RS(P_VARY, 25); break;
    case 1:                                     /* IMPACT: a metal body struck, its pitch dropping */
        RS(P_WAVE, 6); RS(P_COLOR, 50); RS(P_TUNE, -12); RS(P_ATT, 0); RS(P_DEC, 27); RS(P_SUS, 0); RS(P_REL, 27);
        RS(P_MTGT, tgt_of(P_TUNE)); RS(P_MATK, 0); RS(P_MDEC, 20); RS(P_MAMT, 40);
        RS(P_ZMIX, 65); RS(P_ZMAT, 0); RS(P_ZDEC, 32); RS(P_ZPITCH, -5); RS(P_ZTRK, 1); RS(P_ZDAMP, 35);
        RS(P_TATK, 60); RS(P_XDRV, 20); RS(P_RFMODE, 1); RS(P_RFFREQ, 44); RS(P_RFRES, 25);
        RS(P_OSTYLE, 3); RS(P_ODEPTH, 40); RS(P_RMIX, 15); RS(P_RSIZE, 70); RS(P_RDEC, 30);
        RS(P_RECIPE, 1); RS(P_VARY, 25); break;
    case 2:                                     /* HIT: a punch, a sine kick dropping fast, driven */
        RS(P_WAVE, 0); RS(P_TUNE, -17); RS(P_ATT, 0); RS(P_DEC, 21); RS(P_SUS, 0); RS(P_REL, 18);
        RS(P_MTGT, tgt_of(P_TUNE)); RS(P_MATK, 0); RS(P_MDEC, 12); RS(P_MAMT, 55);
        RS(P_TATK, 70); RS(P_XDRV, 30); RS(P_OSTYLE, 3); RS(P_ODEPTH, 60); RS(P_RMIX, 6); RS(P_RDEC, 12);
        RS(P_RECIPE, 1); RS(P_VARY, 20); break;
    case 3:                                     /* DEBRIS: PHYSICS DEBRIS on short wooden knocks, each hit a variation */
        RS(P_WAVE, 6); RS(P_COLOR, 55); RS(P_ATT, 0); RS(P_DEC, 15); RS(P_SUS, 0); RS(P_REL, 15);
        RS(P_ZMIX, 55); RS(P_ZMAT, 2); RS(P_ZPITCH, 5); RS(P_ZDEC, 20); RS(P_ZTRK, 0);
        RS(P_TATK, 40); RS(P_RFMODE, 1); RS(P_RFFREQ, 48); RS(P_RMIX, 12); RS(P_RDEC, 20);
        RS(P_PMODE, 2); RS(P_PHITS, 14); RS(P_PTIME, 18); RS(P_PFALL, 70); RS(P_PPIT, -3); RS(P_PJIT, 50);
        RS(P_RECIPE, 1); RS(P_VARY, 40); break;
    case 4:                                     /* GUNSHOT: a bright noise crack, clipped and squashed, a slap back */
        RS(P_WAVE, 6); RS(P_COLOR, 85); RS(P_ATT, 0); RS(P_DEC, 21); RS(P_SUS, 0); RS(P_REL, 30);
        RS(P_TATK, 90); RS(P_XDRV, 40); RS(P_XSHP, 1);
        RS(P_ZMIX, 25); RS(P_ZMAT, 0); RS(P_ZDEC, 18); RS(P_ZTRK, 0);
        RS(P_RFMODE, 1); RS(P_RFFREQ, 52); RS(P_OSTYLE, 4); RS(P_ODEPTH, 70);
        RS(P_DMIX, 18); RS(P_DTIME, 30); RS(P_DFB, 15); RS(P_RMIX, 20); RS(P_RSIZE, 80); RS(P_RDEC, 24);
        RS(P_RECIPE, 6); RS(P_VARY, 25); break;
    case 5:                                     /* LASER: a saw falling two octaves, flanged, echoing */
        RS(P_WAVE, 2); RS(P_TUNE, 12); RS(P_ATT, 0); RS(P_DEC, 30); RS(P_SUS, 0); RS(P_REL, 24);
        RS(P_MTGT, tgt_of(P_TUNE)); RS(P_MATK, 0); RS(P_MDEC, 24); RS(P_MAMT, 60);
        RS(P_FMIX, 40); RS(P_FDEPTH, 70); RS(P_FRATE, 40); RS(P_XDRV, 10);
        RS(P_DMIX, 20); RS(P_DTIME, 27); RS(P_DFB, 35); RS(P_RMIX, 10);
        RS(P_RECIPE, 6); RS(P_VARY, 30); break;
    case 6:                                     /* ZAP: noise folded, crushed and ring-modulated, a band-pass jumping */
        RS(P_WAVE, 6); RS(P_COLOR, 95); RS(P_ATT, 0); RS(P_DEC, 36); RS(P_SUS, 40); RS(P_REL, 30);
        RS(P_XDRV, 30); RS(P_XSHP, 2); RS(P_XSR, 30); RS(P_XRING, 45); RS(P_XRMIX, 60); RS(P_XBITS, 8);
        RS(P_RFMODE, 2); RS(P_RFFREQ, 40); RS(P_RFRES, 60); RS(P_RFSRC, 2); RS(P_RFRATE, 54); RS(P_RFDEP, 80);
        RS(P_RMIX, 10); RS(P_RECIPE, 6); RS(P_VARY, 35); break;
    case 7:                                     /* WHOOSH: noise swelling through a band-pass that opens and closes */
        RS(P_WAVE, 6); RS(P_COLOR, 60); RS(P_ATT, 44); RS(P_DEC, 44); RS(P_SUS, 0); RS(P_REL, 38);
        RS(P_RFMODE, 2); RS(P_RFFREQ, 30); RS(P_RFRES, 40);
        RS(P_MTGT, tgt_of(P_RFFREQ)); RS(P_MATK, 42); RS(P_MDEC, 44); RS(P_MAMT, 45);
        RS(P_FMIX, 35); RS(P_FDEPTH, 60); RS(P_OSTYLE, 1); RS(P_ODEPTH, 70); RS(P_OIN, 36); RS(P_RMIX, 18);
        RS(P_RECIPE, 2); RS(P_VARY, 30); break;
    case 8:                                     /* RISER: the endless SHEPARD climb, a low-pass opening over it */
        RS(P_WAVE, W_SHEP); RS(P_COLOR, 80); RS(P_TUNE, -12); RS(P_ATT, 57); RS(P_DEC, 63); RS(P_SUS, 100); RS(P_REL, 45);
        RS(P_RFMODE, 1); RS(P_RFFREQ, 22); RS(P_RFRES, 35);
        RS(P_MTGT, tgt_of(P_RFFREQ)); RS(P_MATK, 57); RS(P_MDEC, 48); RS(P_MAMT, 60);
        RS(P_FMIX, 30); RS(P_FDEPTH, 50); RS(P_RMIX, 30); RS(P_RSIZE, 90); RS(P_RDEC, 33);
        RS(P_RECIPE, 2); RS(P_VARY, 15); break;
    case 9:                                     /* UI OK: a soft rise, up a fifth */
        RS(P_WAVE, 1); RS(P_TUNE, 12); RS(P_ATT, 0); RS(P_DEC, 30); RS(P_SUS, 0); RS(P_REL, 27);
        RS(P_CARP, 9); RS(P_CARPS, 4); RS(P_DMIX, 15); RS(P_DTIME, 24); RS(P_DFB, 20); RS(P_RMIX, 10);
        RS(P_RECIPE, 3); RS(P_VARY, 10); break;
    case 10:                                    /* UI ERR: low, buzzing, falling a little */
        RS(P_WAVE, 3); RS(P_TUNE, -7); RS(P_ATT, 0); RS(P_DEC, 33); RS(P_SUS, 0); RS(P_REL, 27);
        RS(P_MTGT, tgt_of(P_TUNE)); RS(P_MATK, 0); RS(P_MDEC, 33); RS(P_MAMT, 12);
        RS(P_GROWL, 35); RS(P_GRATE, 40); RS(P_RFMODE, 1); RS(P_RFFREQ, 36); RS(P_RFRES, 10); RS(P_RMIX, 8);
        RS(P_RECIPE, 3); RS(P_VARY, 10); break;
    case 11:                                    /* UI TICK: a short high sine */
        RS(P_WAVE, 0); RS(P_TUNE, 19); RS(P_ATT, 0); RS(P_DEC, 6); RS(P_SUS, 0); RS(P_REL, 6); RS(P_RMIX, 4);
        RS(P_RECIPE, 3); RS(P_VARY, 15); break;
    case 12:                                    /* PICKUP: the coin: a 25 % pulse jumping up a fourth */
        RS(P_WAVE, W_CPLS); RS(P_CDUTY, 1); RS(P_TUNE, 19); RS(P_CARP, 8); RS(P_CARPS, 4); RS(P_CSTEP, 1);
        RS(P_ATT, 0); RS(P_DEC, 32); RS(P_SUS, 0); RS(P_REL, 33); RS(P_RMIX, 6);
        RS(P_RECIPE, 3); RS(P_VARY, 10); break;
    case 13:                                    /* POWERUP: a rising major arpeggio on the console pulse */
        RS(P_WAVE, W_CPLS); RS(P_CDUTY, 2); RS(P_CSWEEP, 18); RS(P_CARP, 4); RS(P_CARPS, 2); RS(P_CSTEP, 1);
        RS(P_ATT, 0); RS(P_DEC, 40); RS(P_SUS, 0); RS(P_REL, 38); RS(P_DMIX, 12); RS(P_DTIME, 30); RS(P_DFB, 25); RS(P_RMIX, 8);
        RS(P_RECIPE, 3); RS(P_VARY, 10); break;
    case 14:                                    /* MAGIC: metallic FM in three voices, glass ringing, long echoes */
        RS(P_WAVE, 9); RS(P_COLOR, 75); RS(P_TUNE, 7);
        RS(P_VOICES, 2); RS(P_LPIT, 12); RS(P_LDET, 15); RS(P_LLVL, 60);
        RS(P_ATT, 18); RS(P_DEC, 48); RS(P_SUS, 35); RS(P_REL, 48);
        RS(P_ZMIX, 45); RS(P_ZMAT, 1); RS(P_ZDEC, 42); RS(P_ZTRK, 1);
        RS(P_L1SHP, 0); RS(P_L1RATE, 50); RS(P_L1MIX, 8); RS(P_L1TGT, tgt_of(P_PITCH));
        RS(P_DMIX, 32); RS(P_DFB, 55); RS(P_DTIME, 45); RS(P_RMIX, 38); RS(P_RSIZE, 90); RS(P_RDEC, 45);
        RS(P_RECIPE, 4); RS(P_VARY, 20); break;
    case 15:                                    /* MONSTER: a low formant voice, its vowel wandering, growling */
        RS(P_WAVE, 8); RS(P_COLOR, 60); RS(P_TUNE, -17); RS(P_ATT, 21); RS(P_DEC, 50); RS(P_SUS, 60); RS(P_REL, 42);
        RS(P_VMIX, 80); RS(P_VOWEL, 25); RS(P_VSIZE, -8); RS(P_VRES, 60);
        RS(P_L2SHP, 6); RS(P_L2RATE, 40); RS(P_L2MIX, 40); RS(P_L2TGT, tgt_of(P_VOWEL));
        RS(P_GROWL, 50); RS(P_GRATE, 30); RS(P_XDRV, 20); RS(P_OSTYLE, 1); RS(P_ODEPTH, 40); RS(P_RMIX, 15);
        RS(P_RECIPE, 5); RS(P_VARY, 25); break;
    case 16:                                    /* ALARM: a two-tone square, for as long as the key is held */
        RS(P_WAVE, 3); RS(P_TUNE, 7); RS(P_ATT, 3); RS(P_DEC, 63); RS(P_SUS, 100); RS(P_REL, 18);
        RS(P_L1SHP, 4); RS(P_L1RATE, 44); RS(P_L1MIX, 25); RS(P_L1TGT, tgt_of(P_PITCH));
        RS(P_RFMODE, 1); RS(P_RFFREQ, 45); RS(P_RFRES, 20); RS(P_RMIX, 10);
        RS(P_RECIPE, 3); RS(P_VARY, 0); break;
    case 18:                                    /* SYNTH: a plain saw, open, a little vibrato, dry: the best way to hear
                                                   what a SOUND CHIP (ASSETS B) does to a sound */
        RS(P_WAVE, 2); RS(P_COLOR, 50); RS(P_ATT, 2); RS(P_DEC, 40); RS(P_SUS, 75); RS(P_REL, 30); RS(P_GLIDE, 8);
        RS(P_L1SHP, 0); RS(P_L1RATE, 30); RS(P_L1MIX, 4); RS(P_L1TGT, tgt_of(P_PITCH));
        RS(P_RECIPE, 0); RS(P_VARY, 0); break;
    default:                                    /* ENGINE: a sub-saw firing (GROWL), drifting; RTPC revs it (pitch, filter) */
        RS(P_WAVE, 7); RS(P_COLOR, 40); RS(P_TUNE, -24); RS(P_ATT, 9); RS(P_DEC, 63); RS(P_SUS, 100); RS(P_REL, 27);
        RS(P_L2SHP, 6); RS(P_L2RATE, 30); RS(P_L2MIX, 10); RS(P_L2TGT, tgt_of(P_PITCH));
        RS(P_GROWL, 55); RS(P_GRATE, 35); RS(P_XDRV, 24); RS(P_RFMODE, 1); RS(P_RFFREQ, 33); RS(P_RFRES, 30); RS(P_RMIX, 6);
        RS(P_Q1T, tgt_of(P_PITCH)); RS(P_Q1A, 60); RS(P_Q2T, tgt_of(P_RFFREQ)); RS(P_Q2A, 40); RS(P_Q3A, 0);
        RS(P_RECIPE, 0); RS(P_VARY, 10);
    }
    RS(P_VOL, ASSET_VOL[k]);
}
static const uint8_t HA_ORDER[NPAGE] = {PG_SRC, PG_CHIP, PG_RES, PG_GRN, PG_LAB, PG_ENV, PG_TRN, PG_DST, PG_VOX, PG_FLT, PG_OTT, PG_DLY,
                                         PG_REV, PG_SPC, PG_ASSET, PG_VAR, PG_BAT, PG_PHY, PG_LFO, PG_RTPC, PG_PAD};
static void happy_accident(void)
{
    uint32_t m, i;
    int16_t was[NP];
    push_undo();
    for (i = 0; i < NP; i++) was[i] = P[i];
    morph_on = 0;
    if (P[P_RECIPE]) recipe((uint32_t)P[P_RECIPE]);
    else for (i = 0; i < NPAGE; i++) {
        m = HA_ORDER[i];                        /* (a fixed order: the same accident from the same state, release after release) */
        if (m == PG_PAD || m == PG_LAB || m == PG_PHY || m == PG_CHIP || m == PG_VAR || m == PG_BAT || m == PG_RTPC || m == PG_SPC || m == PG_ASSET ||
            (m == PG_LFO && rnd_range(0, 99) < 50)) continue;
        if (m == PG_RES && rnd_range(0, 99) < 65) { set_unlocked(P_ZMIX, 0); continue; }   /* the extra modules: sometimes */
        if (m == PG_DST && rnd_range(0, 99) < 60) { set_unlocked(P_XMIX, 0); continue; }
        if (m == PG_VOX && rnd_range(0, 99) < 70) { set_unlocked(P_VMIX, 0); set_unlocked(P_SMIX, 0); set_unlocked(P_GROWL, 0); continue; }
        if (m == PG_TRN && rnd_range(0, 99) < 50) { set_unlocked(P_TATK, 0); set_unlocked(P_TSUS, 0); continue; }
        rnd_module(m);
    }
    if (!P[P_RECIPE]) {
        set_unlocked(P_MODE, rnd_range(0, 2));
        set_unlocked(P_GMODE, rnd_range(0, 99) > 70); set_unlocked(P_GFRZ, 0);
        set_unlocked(P_PITCH, rnd_range(70, 130)); set_unlocked(P_RNDP, rnd_range(0, 99) > 70);
    }
    if (P[P_HAAMT] < 100)                       /* HA AMT: only part of the way (a list: kept with that chance) */
        for (i = 0; i < NP; i++) {
            if (P[i] == was[i]) continue;
            if (CONT[i]) P[i] = (int16_t)(was[i] + (P[i] - was[i]) * P[P_HAAMT] / 100);
            else if (rnd_range(0, 99) >= P[P_HAAMT]) P[i] = was[i];
        }
    mutant = 1; mutant_t = fm1_ms;
    touched();
    { char t[32] = "ACCIDENT: "; if (P[P_RECIPE]) { cat(t, E_RECIPE[P[P_RECIPE] & 7]); say(t); } else say("HAPPY ACCIDENT"); }
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
static uint32_t var_store_b(void) { return !(has_ab & 1u) ? 0u : !(has_ab & 2u) ? 1u : P[P_MORPH] > 50; }
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
/* the levels of the last 16 hits (BATCH, the FIRE TEST): each hit's peak, to see how far the variations spread */
static uint16_t lvl_h[16], lvl_v[16];           /* peak (tm_peak), the variation */
static uint32_t lvl_n, lvl_cur, lvl_live;
static void lvl_hit(uint32_t var)               /* a new hit: the last one's peak goes into the list */
{
    if (lvl_live) { lvl_h[lvl_n & 15u] = (uint16_t)(lvl_cur > 65535u ? 65535u : lvl_cur); lvl_n++; }
    lvl_v[lvl_n & 15u] = (uint16_t)var; lvl_cur = 0; lvl_live = 1;
}
static void lvl_poll(void) { if (lvl_live && tm_peak > lvl_cur) lvl_cur = tm_peak; }
static void lvl_clear(void) { lvl_n = 0; lvl_cur = 0; lvl_live = 0; }
static uint32_t lvl_match;
static int32_t lvl_mean_q8(void)                /* the hits' mean peak, log2 Q8 (a geometric mean: dB averaged) */
{
    uint32_t i, k = lvl_n < 16u ? lvl_n : 16u, c = 0;
    int32_t sum = 0;
    for (i = 0; i < k; i++) if (lvl_h[i] > 33u) { sum += (int32_t)sn_lg2(lvl_h[i]); c++; }
    return c ? sum / (int32_t)c : -1;
}
static void lvl_do_match(void)                  /* MATCH: OUT so the hits' mean peak lands on TARGET */
{
    int32_t m, d, v;
    lvl_hit(0); lvl_live = 0;                   /* (the last hit in) */
    m = lvl_mean_q8();
    if (m < 0 || lock_get(P_VOL)) { say(m < 0 ? "MATCH: TOO QUIET" : "MATCH: OUT IS LOCKED"); return; }
    d = (int32_t)sn_lg2(32768u) + P[P_LTGT] * 128000 / 6021 - m;   /* (TARGET in half dB) */
    v = (int32_t)(((uint32_t)P[P_VOL] * exp2_q16(clampi(d, -2048, 1024)) + 32768u) >> 16);
    push_undo();
    P[P_VOL] = (int16_t)clampi(v, 1, 100);
    touched();
    say(v > 100 ? "MATCHED: OUT AT 100 %" : "LEVEL MATCHED");
}
static const uint8_t BCNT[4] = {4, 8, 12, 16};
static void batch_start(void)
{
    if (batch_on) { batch_on = 0; vary_mute = 0; all_off(); say("BATCH STOPPED"); ui_dirty |= 2; return; }
    if (P[P_BMODE] && has_ab != 3u) { say("MORPH: STORE A AND B"); return; }
    if (!P[P_BMODE] && !P[P_VARY]) say("BATCH: VARY 0, ALL SAME");
    else say("BATCH: RECORD IN A DAW");
    batch_n = BCNT[P[P_BCNT] & 3]; batch_i = 0; batch_t0 = fm1_ms + 300u; batch_on = 1; batch_ph = 0;
    lvl_clear();
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
        if (batch_i >= batch_n) {
            lvl_hit(0); lvl_live = 0;
            batch_on = 0; vary_mute = 0; ui_dirty |= 2;
            if (P[P_VLOCK]) { P[P_VSEED] = (int16_t)rnd_range(1, 999); ui_dirty |= 4; }
            say(P[P_VLOCK] ? "BATCH DONE: NEW SEED" : "BATCH DONE");
            if (P[P_LMATCH]) lvl_match = fm1_ms + 400u;   /* (then OUT to TARGET) */
            return;
        }
        if (P[P_BMODE]) morph_set((int32_t)(batch_n > 1u ? batch_i * 100u / (batch_n - 1u) : 0u));
        lvl_hit(batch_i + 1u);
        note_on_var((int32_t)last_note, 32767, P[P_BMODE] == 1 ? 0u : batch_i + 1u);   /* variation n of the seed: the same
                                                                                    batch again, sound for sound */
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
/* the FIRE TEST: the sound fired on its own at RATE, each shot one of POOL variations, picked as a game's random
 * container picks: RANDOM, NO REPEAT (never the same twice in a row), SHUFFLE (all of them before any again) */
static uint32_t ft_on, ft_t, ft_off, ft_prev, ft_bag, ft_hits;
static uint32_t fire_pick(void)
{
    uint32_t n = (uint32_t)P[P_TPOOL], v, all = (1u << n) - 1u, tries = 0;
    switch (P[P_TORDER]) {
    case 0: return 1u + (uint32_t)rnd_range(0, (int32_t)n - 1);
    case 1: do v = 1u + (uint32_t)rnd_range(0, (int32_t)n - 1); while (v == ft_prev && ++tries < 32u); return v;
    default:                                    /* (a new round never opens with the last one played) */
        if ((ft_bag & all) == all) ft_bag = 0;
        do v = 1u + (uint32_t)rnd_range(0, (int32_t)n - 1);
        while (((ft_bag >> (v - 1u) & 1u) || (!ft_bag && v == ft_prev)) && ++tries < 64u);
        if (ft_bag >> (v - 1u) & 1u) for (v = 1u; v < n && (ft_bag >> (v - 1u) & 1u); v++) {}
        ft_bag |= 1u << (v - 1u);
        return v;
    }
}
static void fire_start(void)
{
    if (ft_on) { ft_on = 0; note_off((int32_t)last_note); lvl_hit(0); lvl_live = 0; say("FIRE TEST STOPPED"); ui_dirty |= 2; return; }
    if (batch_on) batch_start();
    if (!P[P_VARY]) say("FIRE: VARY 0, ALL SAME"); else say("FIRE TEST");
    ft_on = 1; ft_t = fm1_ms + 200u; ft_off = 0; ft_prev = 0; ft_bag = 0; ft_hits = 0; lvl_clear();
    ui_dirty |= 2;
}
static void fire_poll(void)
{
    uint32_t iv;
    if (!ft_on) return;
    iv = 60000u / (uint32_t)P[P_TRATE];
    if (ft_off && (int32_t)(fm1_ms - ft_off) >= 0) { note_off((int32_t)last_note); ft_off = 0; }
    if ((int32_t)(fm1_ms - ft_t) < 0) return;
    ft_prev = fire_pick();
    lvl_hit(ft_prev);
    note_on_var((int32_t)last_note, 32767, ft_prev);
    ft_off = fm1_ms + (iv / 2u < 120u ? iv / 2u : 120u);   /* held half a shot, 120 ms at most */
    ft_t += iv;
    if ((int32_t)(fm1_ms - ft_t) > 0) ft_t = fm1_ms + iv;   /* (late: no burst to catch up) */
    ft_hits++;
    ui_dirty |= 2;
}
/* 13 VARIATION: EVOLVE nudges a few parameters at every note played, so a sound drifts as you play it */
static int32_t zc_near(const int16_t *b, int32_t len, int32_t at)   /* the rising zero crossing nearest to at */
{
    int32_t r;
    for (r = 0; r < 600; r++) {
        int32_t i = at + r, j = at - r;
        if (i > 0 && i < len && b[i - 1] < 0 && b[i] >= 0) return i;
        if (j > 0 && j < len && b[j - 1] < 0 && b[j] >= 0) return j;
    }
    return at;
}
static const int16_t *lp_pb;                    /* (0: the sample changed under the same pointer: find the points again) */
static void loop_points(void)                   /* main loop: where the loop sits, when the sample or L POS / LEN / XFADE move */
{
    static int32_t pl, pp, pn, px;
    const int16_t *b = smp_ptr;
    int32_t len = (int32_t)smp_len, s, e, xf;
    if (b == lp_pb && len == pl && P[P_LPOS] == pp && P[P_LLEN] == pn && P[P_LXF] == px) return;
    lp_pb = b; pl = len; pp = P[P_LPOS]; pn = P[P_LLEN]; px = P[P_LXF];
    if (len < 1024) { lp_s = 0; lp_e = len; lp_xf = 0; return; }
    s = len * pp / 100; e = s + len * pn / 100;
    if (e > len - 64) e = len - 64;
    if (s > e - 256) s = e - 256;
    if (s < 0) s = 0;
    s = zc_near(b, len, s); e = zc_near(b, len - 64, e);
    if (e - s < 256) e = s + 256;
    xf = (e - s) * px / 100;
    if (xf > s) xf = s;                         /* (the crossfade reads before the loop start) */
    lp_xf = 0; lp_s = s; lp_e = e; lp_xf = xf;  /* (xf 0 while the two points move) */
    ui_dirty |= 2;
}
static uint32_t rt_t0, rt_dur, sp_off;
static void rtpc_sweep(void)                    /* SEL on 16 RTPC: 0 -> 100 % over SWEEP, the last note held */
{
    if (rt_on) { rt_on = 0; note_off((int32_t)last_note); say("SWEEP STOPPED"); ui_dirty |= 2; return; }
    rt_dur = (uint32_t)P[P_QSWP] * 100u; rt_t0 = fm1_ms; rt_cur = 0; rt_on = 1;
    note_on((int32_t)last_note, 32767);
    say("RTPC SWEEP");
}
static void motion_poll(void)                   /* the sweep's value; the end of a SPACE audition */
{
    if (rt_on) {
        uint32_t t = fm1_ms - rt_t0;
        if (t >= rt_dur) { rt_on = 0; note_off((int32_t)last_note); ui_dirty |= 2; }
        else rt_cur = t * 10000u / rt_dur;
    }
    if (sp_off && (int32_t)(fm1_ms - sp_off) >= 0) { sp_off = 0; note_off((int32_t)last_note); }
}
/* 16 PHYSICS TRIGGER: a key plays a pattern of hits, not one. BOUNCE: closer and closer, falling (a ball, a coin);
 * DEBRIS: thick at first, then stragglers, each its own level; ROLL: even and restless; STUTTER: a machine gun;
 * SCATTER: for as long as the key is held, at random (drips, sparks, an ambience). The pattern comes from the SEED:
 * the same seed, the same pattern. Each hit is a variation of the seed (VARY), FALL quieter and PITCH away by the end. */
static uint16_t ph_t[32];                       /* each hit: when (ms), */
static uint8_t ph_l[32];                        /* how loud (0..255), */
static int8_t ph_p[32];                         /* semitones away */
static uint32_t ph_n, ph_len, ph_on, ph_i, ph_t0, ph_cyc, ph_off, ph_vel, ph_key;
static int32_t ph_note, ph_cur;
static void phys_build(uint32_t cyc)            /* the pattern as the knobs say (also for the screen) */
{
    uint32_t mode = (uint32_t)P[P_PMODE], n = (uint32_t)clampi(P[P_PHITS], 2, 32), T = (uint32_t)P[P_PTIME] * 100u, i, sum = 0, w = 256;
    uint32_t rs = var_hash((uint32_t)P[P_VSEED], 7000u + cyc), jit = (uint32_t)P[P_PJIT], cum[33];
    int32_t fall = P[P_PFALL], pit = P[P_PPIT];
    if (mode == 2u || mode == 5u) jit = 50u + jit / 2u;   /* (these two are random by nature) */
    for (i = 0; i < n; i++) {
        uint32_t r = 100u - jit + xrand(&rs) % (2u * jit + 1u), ww, lv;
        cum[i] = sum;
        ww = mode == 1u ? w : mode == 2u ? 256u + 768u * i / n : 256u;
        sum += ww * (r ? r : 1u) / 100u + 1u;
        w = w * 200u >> 8;                      /* BOUNCE: each gap 0.78 of the last */
        if (w < 8u) w = 8u;
        r = xrand(&rs) % 256u;
        lv = mode == 1u || mode == 4u ? 255u - (uint32_t)fall * 255u / 100u * i / (n - 1u)
           : mode == 2u ? (255u - (uint32_t)fall * 200u / 100u * i / (n - 1u)) * (128u + r / 2u) >> 8
           : 255u - (uint32_t)fall * r / 100u;
        ph_l[i] = (uint8_t)(lv < 12u ? 12u : lv);
        ph_p[i] = (int8_t)(mode == 5u || mode == 3u ? pit * ((int32_t)(xrand(&rs) % 201u) - 100) / 100 : pit * (int32_t)i / (int32_t)(n - 1u));
    }
    cum[n] = sum;
    if (mode != 5u) sum = cum[n - 1u] ? cum[n - 1u] : 1u;   /* (the last hit at TIME; SCATTER: the round is TIME long) */
    for (i = 0; i < n; i++) ph_t[i] = (uint16_t)(cum[i] * T / sum);
    ph_n = n; ph_len = T;
}
static void phys_stop(void) { if (ph_on) { ph_on = 0; if (ph_off) note_off(ph_cur); ph_off = 0; ui_dirty |= 2; } }
static void phys_start(int32_t note, int32_t vel15, uint32_t key)
{
    phys_stop();
    ph_cyc = 0; phys_build(0);
    ph_note = note; ph_vel = (uint32_t)vel15; ph_key = key; ph_i = 0; ph_t0 = fm1_ms; ph_on = 1;
    lvl_clear();
}
static void phys_poll(void)
{
    uint32_t t, gap;
    if (!ph_on) return;
    t = fm1_ms - ph_t0;
    if (ph_off && (int32_t)(fm1_ms - ph_off) >= 0) { note_off(ph_cur); ph_off = 0; }
    if (ph_i >= ph_n) {
        if (P[P_PMODE] != 5) { if (!ph_off) { ph_on = 0; lvl_hit(0); lvl_live = 0; ui_dirty |= 2; } return; }
        if (t < ph_len) return;
        ph_t0 += ph_len; ph_i = 0; phys_build(++ph_cyc);   /* SCATTER: round after round, each a new pattern */
        return;
    }
    if (t < ph_t[ph_i]) return;
    if (ph_off) { note_off(ph_cur); ph_off = 0; }
    ph_cur = clampi(ph_note + ph_p[ph_i], 0, 127);
    lvl_hit(ph_i + 1u);
    note_on_var(ph_cur, (int32_t)(ph_vel * ph_l[ph_i] / 255u), P[P_VARY] ? ph_cyc * 32u + ph_i + 1u : 0u);
    gap = (ph_i + 1u < ph_n ? ph_t[ph_i + 1u] : ph_len + (P[P_PMODE] == 5 ? 0u : 300u)) - ph_t[ph_i];
    gap = gap * 6u / 10u;
    ph_off = fm1_ms + (gap < 15u ? 15u : gap > 150u ? 150u : gap); if (!ph_off) ph_off = 1;
    ph_i++;
    ui_dirty |= 2;
}
/* 05 SAMPLE LAB: the active slot worked on in place (no CPU while playing): REVERSE, CROP to 01's loop, FADE IN / OUT
 * over AMOUNT, HALF SPD (an octave down, twice as long), DOUBLE, NORMALIZE; REVERT: the slot as it is in flash */
static void slot_select(int32_t i);
static void slot_factory(void);
static void lab_apply(void)
{
    uint32_t tool = (uint32_t)P[P_STOOL], n = smp_len, i, k;
    char t[32] = "LAB: ";
    if (rec_state != REC_IDLE) { say("LAB: BUSY RECORDING"); return; }
    if (tool == 8u) { slot_factory(); return; }  /* FACTORY: the slot's factory sound back (it was SEL + PLAY) */
    if (tool == 7u) {                           /* REVERT */
        SL[cur_slot].dirty = 0;
        slot_select(cur_slot);
        lp_pb = 0;
        say("LAB: SLOT AS SAVED"); ui_dirty |= 2 | 4;
        return;
    }
    if (n > SMP_MAX) n = SMP_MAX;
    smp_busy = 1;
    switch (tool) {
    case 0: for (i = 0; i < n / 2u; i++) { int16_t a = smp16[i]; smp16[i] = smp16[n - 1u - i]; smp16[n - 1u - i] = a; } break;
    case 1: {                                   /* CROP: what 01's loop marks */
        uint32_t a = (uint32_t)clampi(lp_s, 0, (int32_t)n), b = (uint32_t)clampi(lp_e, 0, (int32_t)n);
        if (b >= a + 256u) { for (i = 0; i < b - a; i++) smp16[i] = smp16[a + i]; n = b - a; }
        break;
    }
    case 2: case 3:                             /* FADE IN / OUT over AMOUNT of it */
        k = n * (uint32_t)P[P_SAMT] / 100u; if (k < 2u) k = 2u;
        for (i = 0; i < k; i++) { uint32_t j = tool == 2u ? i : n - 1u - i; smp16[j] = (int16_t)((int32_t)smp16[j] * (int32_t)i / (int32_t)k); }
        break;
    case 4:                                     /* HALF SPD (from the end: in place) */
        k = n * 2u > SMP_MAX ? SMP_MAX : n * 2u;
        if (k < n * 2u) cat(t, "CUT AT 1.48s, ");
        for (i = k; i-- > 0u;) { uint32_t a = i >> 1, b = a + 1u < n ? a + 1u : a; smp16[i] = (int16_t)(i & 1u ? (smp16[a] + smp16[b]) / 2 : smp16[a]); }
        n = k;
        break;
    case 5:                                     /* DOUBLE */
        if (n >= 512u) { for (i = 0; i < n / 2u; i++) smp16[i] = (int16_t)((smp16[2u * i] + smp16[2u * i + 1u]) / 2); n /= 2u; }
        break;
    default: {                                  /* NORMALIZE */
        uint32_t pk = 1;
        for (i = 0; i < n; i++) { uint32_t v = (uint32_t)absi(smp16[i]); if (v > pk) pk = v; }
        for (i = 0; i < n; i++) smp16[i] = sat16((int32_t)smp16[i] * 31000 / (int32_t)pk);
    }
    }
    if (tool == 1u || tool == 4u || tool == 5u) {   /* (new ends: no click) */
        for (i = 0; i < 32u && i < n; i++) { smp16[i] = (int16_t)(smp16[i] * (int32_t)i / 32); smp16[n - 1u - i] = (int16_t)(smp16[n - 1u - i] * (int32_t)i / 32); }
    }
    SL[cur_slot].user = 1; SL[cur_slot].dirty = 1; SL[cur_slot].len = (uint16_t)n;
    smp_ptr = smp16; smp_len = n;
    smp_busy = 0;
    lp_pb = 0;
    smp_overview();
    cat(t, E_STOOL[tool % 9u]); cat(t, " DONE");
    say(t);
    ui_dirty |= 2 | 4 | 8;
}
/* 19 GESTURE REC (SEL on the pad's page): armed, the next touch of the pad is recorded until the finger lifts */
static uint32_t g_rec, g_rt;
static void gest_poll(void)
{
    if (g_rec == 1u && pad_on) { g_rec = 2; g_n = 0; g_rt = fm1_ms; }
    if (g_rec != 2u) return;
    if (!pad_on || g_n >= GEST_MAX) {
        g_rec = 0; session_dirty = 1;
        if (g_n < 5u) { g_n = 0; say("GESTURE: TOO SHORT"); }
        else { if (!P[P_GEST]) P[P_GEST] = 1; say("GESTURE RECORDED"); }
        ui_dirty |= 2 | 4 | 8;
        return;
    }
    while ((int32_t)(fm1_ms - g_rt) >= 0 && g_n < GEST_MAX) { g_x[g_n] = pad_x; g_y[g_n] = pad_y; g_n = g_n + 1u; g_rt += 20u; }
}
static void evolve_step(void);
static void play_key(uint32_t k, int32_t note, int32_t vel15)   /* a key: its note, or (KEYS VARIANT) variation k + 1 */
{                                                                 /* of the seed at the last note played */
    if (P[P_VKEYS]) {
        char t[16] = "VARIATION ", n[8];
        note_on_var((int32_t)last_note, vel15, k + 1u);
        fmt_num(n, (int32_t)k + 1, 0); cat(t, n); say(t);
        lvl_hit(k + 1u);
        ui_dirty |= 2;
        return;
    }
    last_note = (uint32_t)note;
    if (P[P_PMODE]) phys_start(note, vel15, k); else note_on(note, vel15);
    evolve_step();
}
static void evolve_step(void)
{
    uint32_t k, tries;
    if (!P[P_EVOLVE]) return;
    push_undo();
    for (k = 0; k < 3u; k++)
        for (tries = 0; tries < 16u; tries++) {
            uint32_t p = (uint32_t)rnd_range(0, NP_V13 - 1), m = PMOD[p];
            int32_t r, d;
            if (!CONT[p] || lock_get(p) || p == P_SLOT || p == P_VOL || m >= PG_SPC || m == PG_LFO || m == PG_LAB) continue;
            r = PD[p].hi - PD[p].lo;
            d = r * P[P_EVOLVE] / 800;          /* 100 %: up to 1/8 of the range a step */
            P[p] = (int16_t)clampi(P[p] + rnd_range(-d - 1, d + 1), PD[p].lo, PD[p].hi);
            break;
        }
    ui_dirty |= 4;
}
static void asset_take(uint32_t k)              /* an asset loaded (with an undo step) and played */
{
    char t[24] = "ASSET: ";
    k %= NASSET;
    push_undo();
    morph_on = 0;
    phys_stop();
    asset_load(k);
    P[P_ASSET] = (int16_t)k;
    touched();
    asset_cur = (uint8_t)k; asset_sum = patch_sum();
    cat(t, E_ASSET[k]); say(t);
    if (P[P_PMODE]) phys_start(60, 32767, 999u); else audition_ms(asset_hold_ms(k));
}
static void stop_all(void)                      /* SEL + PLAY: everything that plays on its own stops, every note */
{
    if (batch_on) batch_start();
    if (ft_on) fire_start();
    phys_stop();
    rt_on = 0;
    sp_off = 0;
    aud_t = 0;
    all_off();
    say("ALL STOPPED");
    ui_dirty |= 2 | 8;
}
static void module_random(void)                 /* SEL + OCT+: this page's module at random (ASSETS: an asset at random) */
{
    if (page == PG_ASSET && !sub) { asset_take((uint32_t)rnd_range(0, NASSET - 1)); return; }
    if (page == PG_LAB || page == PG_BAT) { say("NOTHING TO RANDOMIZE HERE"); return; }
    push_undo();
    rnd_module(page);
    touched();
    say(page == PG_LFO ? "RND LFOs" : page == PG_PAD ? "RND PAD TARGETS" : "RANDOMIZED MODULE");
    audition();
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
    if (page == PG_BAT) { if (sub == 1u) fire_start(); else batch_start(); return; }
    if (page == PG_RTPC) { rtpc_sweep(); return; }
    if (page == PG_LAB) { lab_apply(); return; }
    if (page == PG_PHY) {                       /* the pattern on the last note */
        if (ph_on) { phys_stop(); return; }
        if (!P[P_PMODE]) { say("TRIGGER IS OFF"); return; }
        phys_start((int32_t)last_note, 32767, 999u);
        return;
    }
    if (page == PG_PAD) {
        if (g_rec) { g_rec = 0; say("GESTURE REC CANCELLED"); }
        else { g_rec = 1; say("ARMED: MOVE THE PAD"); }
        ui_dirty |= 2 | 8;
        return;
    }
    if (page == PG_SPC) {                       /* a note held through one whole PASS-BY (or a second) */
        note_on((int32_t)last_note, 32767);
        sp_off = fm1_ms + (P[P_SPASS] ? (uint32_t)P[P_SPASS] * 100u : 1000u);
        return;
    }
    if (page == PG_ASSET && !sub) { asset_take((uint32_t)P[P_ASSET] % NASSET); return; }   /* load the asset, play it */
    if (page == PG_VAR && sub == 1u) { morph_store(var_store_b()); return; }   /* B: STORE A, then B (both kept: the side MORPH is nearer) */
    if (page == PG_VAR) { P[P_VSEED] = (int16_t)rnd_range(1, 999); say("NEW SEED"); ui_dirty |= 2 | 4; audition(); return; }
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
    char t[32] = "SLOT x RECORDED";
    while (n > 256u && absi(T_ULAW[rec.d[n - 1u]]) < 200) n--;
    if (n < 512u) { say("REC: NOTHING HEARD"); rec_state = REC_IDLE; return; }
    for (i = 0; i < n; i++) { uint32_t v = (uint32_t)absi(T_ULAW[rec.d[i]]); if (v > pk) pk = v; }
    g = 31000u * 4096u / pk;
    if (P[P_RECM] == 1 && smp_len >= 256u) {    /* OVERDUB: the new take over the slot, both at the same level */
        const int16_t *o = smp_ptr;
        uint32_t on = smp_len, m = n > on ? n : on, opk = 1, mpk = 1, go;
        if (m > SMP_MAX) m = SMP_MAX;
        for (i = 0; i < on; i++) { uint32_t v = (uint32_t)absi(o[i]); if (v > opk) opk = v; }
        go = 31000u * 4096u / opk;
        for (i = 0; i < m; i++) {               /* (twice: the sum's peak first, then the sum, normalized) */
            int32_t v = (i < on ? (int32_t)(((int64_t)o[i] * go) >> 13) : 0) + (i < n ? (int32_t)(((int64_t)T_ULAW[rec.d[i]] * g) >> 13) : 0);
            if ((uint32_t)absi(v) > mpk) mpk = (uint32_t)absi(v);
        }
        g = 31000u * 4096u / mpk;
        smp_busy = 1;
        for (i = 0; i < m; i++) {
            int32_t v = (i < on ? (int32_t)(((int64_t)o[i] * go) >> 13) : 0) + (i < n ? (int32_t)(((int64_t)T_ULAW[rec.d[i]] * (int32_t)(31000u * 4096u / pk)) >> 13) : 0);
            smp16[i] = sat16((int32_t)(((int64_t)v * g) >> 12));
        }
        n = m;
    } else {
        smp_busy = 1;
        for (i = 0; i < n; i++) smp16[i] = sat16((int32_t)(((int64_t)T_ULAW[rec.d[i]] * g) >> 12));
    }
    for (i = n - 64u; i < n; i++) smp16[i] = (int16_t)(smp16[i] * (int32_t)(n - i) / 64);
    SL[cur_slot].user = 1; SL[cur_slot].dirty = 1; SL[cur_slot].len = (uint16_t)n;
    smp_ptr = smp16; smp_len = n;
    smp_busy = 0;
    lp_pb = 0;
    smp_overview();
    rec_state = REC_IDLE;
    t[5] = (char)('1' + cur_slot);
    say(P[P_RECM] == 1 ? "OVERDUBBED (SAVE KEEPS)" : t);
    if (P[P_RECM] == 2) {                       /* BOUNCE: the sound is the sample now; the chain back to dry */
        static const uint8_t DRY[] = {P_ZMIX, P_TATK, P_TSUS, P_TOUT, P_XMIX, P_ODEPTH, P_DMIX, P_FMIX, P_RMIX, P_L1MIX, P_L2MIX,
                                      P_SDIST, P_SOCCL, P_SPASS, P_QINT, P_GMODE, P_GFRZ, P_TUNE, P_SLOOP, P_ATT, P_VOICES, P_VARY};
        push_undo();
        for (i = 0; i < sizeof DRY; i++) P[DRY[i]] = 0;
        P[P_MODE] = 1; P[P_SLOT] = (int16_t)cur_slot; P[P_STR] = 100; P[P_PITCH] = 100; P[P_SUS] = 100; P[P_REL] = 10;
        if (!lock_get(P_VOL)) P[P_VOL] = (int16_t)clampi((int32_t)(pk * 100u / 31000u), 1, 100);   /* as loud as it was recorded */
        P[P_RFMODE] = 0;
        touched();
        say("BOUNCED: THE CHAIN IS DRY");
    }
    ui_dirty |= 2 | 4;
}
static void slot_factory(void)                  /* LAB TOOL FACTORY: the factory sound back */
{
    if (!SL[cur_slot].user) { say("ALREADY FACTORY"); return; }
    smp_busy = 1;
    fsmp_unpack((uint32_t)cur_slot);
    smp_ptr = smp16; smp_len = FSMP_LEN[cur_slot];
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
#define NTHEME 5u
static const theme_t THEMES[NTHEME] = {
    {RGB(189, 112, 255), RGB(82, 250, 133), RGB(158, 255, 51), RGB(255, 138, 41), RGB(17, 17, 20), RGB(26, 26, 31), RGB(50, 48, 60)},
    {RGB(250, 71, 66), RGB(255, 199, 51), RGB(250, 97, 209), RGB(255, 214, 61), RGB(20, 16, 17), RGB(30, 24, 26), RGB(60, 43, 46)},
    {RGB(255, 173, 41), RGB(77, 209, 255), RGB(140, 250, 255), RGB(255, 122, 46), RGB(19, 18, 18), RGB(28, 28, 27), RGB(54, 51, 48)},
    {RGB(230, 125, 201), RGB(89, 224, 245), RGB(140, 255, 56), RGB(255, 194, 26), RGB(18, 18, 21), RGB(27, 27, 32), RGB(47, 47, 57)},
    {RGB(158, 255, 51), RGB(82, 250, 133), RGB(189, 112, 255), RGB(255, 138, 41), RGB(11, 10, 16), RGB(20, 19, 27), RGB(46, 44, 58)},
};
#define TH (THEMES[(uint32_t)P[P_THEME] % NTHEME])
/* EVA-01 (Acid Eva-01): the 01 TEST colours as acid graphics. Purple is the structure (the page, the rules, the
 * messages), lime what you touch, green the signal, orange a lock or a warning; flat blocks, crop marks, no frames */
#include "eva_font.h"
#define EVA (P[P_THEME] == 4)
#define EV_BG RGB(11, 10, 16)
#define EV_LN RGB(46, 44, 58)
#define EV_HATCH RGB(28, 26, 36)
#define EV_PUR RGB(189, 112, 255)
#define EV_GRN RGB(82, 250, 133)
#define EV_LIM RGB(158, 255, 51)
#define EV_ORG RGB(255, 138, 41)
#define EV_TXT RGB(236, 234, 242)
#define EV_MUT RGB(140, 145, 165)
#define EV_INK RGB(0, 0, 0)
#define TER_BASE 12                             /* Terminus 8x16: the baseline row */
static const uint16_t EV_KC[4] = {EV_LIM, EV_GRN, EV_PUR, EV_TXT};   /* (the knobs' colours, EVA-01) */
static uint32_t hot_k = 4u, hot_t;              /* the knob turned last, when (EVA: its cell lights up a moment) */
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
    case F_CENT: fmt_num(s, v, 0); cat(s, "c"); break;
    case F_VOWEL: {                             /* U O A E I, or between two */
        static const char L[] = "UOAEI";
        int32_t q = v * 4, i = q / 100, f = q % 100;
        s[0] = 0;
        if (f < 15 || i >= 4) { s[0] = L[i >= 4 ? 4 : i]; s[1] = 0; }
        else if (f > 85) { s[0] = L[i + 1]; s[1] = 0; }
        else { s[0] = L[i]; s[1] = '>'; s[2] = L[i + 1]; s[3] = 0; }
        break;
    }
    case F_SHZ: {                               /* Hz: 0.2 .. 2000, +- */
        int32_t c = v * v * 20;                 /* 0.01 Hz */
        char t[12];
        s[0] = v < 0 ? '-' : '+'; s[1] = 0;
        if (!v) { s[0] = 0; cat(s, "0Hz"); break; }
        if (c >= 100000) { fmt_dec(t, c / 10, 10000u, c >= 1000000 ? 1u : 2u, 0); cat(s, t); cat(s, "k"); break; }   /* 1.01k (as fmt_hz) */
        if (c < 1000) fmt_dec(t, c, 100u, 2u, 0); else if (c < 10000) fmt_dec(t, c / 10, 10u, 1u, 0); else fmt_num(t, c / 100, 0);
        cat(s, t); cat(s, "Hz");
        break;
    }
    case F_SEC10: if (!v) cat(s, "OFF"); else { fmt_dec(s, v, 10u, 1u, 0); cat(s, "s"); } break;
    case F_RPM: fmt_num(s, v, 0); cat(s, "rpm"); break;
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
    case F_ERATE: if (!P[P_EMU]) cat(s, "--"); else fmt_hz(s, emu_rate(P[P_EMU], v * 256)); break;
    case F_ETIME: fmt_num(s, v * 16, 0); cat(s, "ms"); break;
    case F_ETONE: fmt_hz(s, (1500u * exp2_q16(v * 957 / 100)) >> 16); break;
    default: fmt_num(s, v, 0); break;
    }
}
/* FONT_S at twice its size (the seed): the 32 px font without its 24 KB table */
static void cv_text_x2(int32_t x, int32_t y, const char *s, uint16_t c)
{
    const felucca_font_t *f = &FONT_S;
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s), gx, gy, w = f->bw[gi], bpr = (w + 1u) / 2u;
        const uint8_t *gd = f->data + f->off[gi];
        for (gy = 0; gy < f->h; gy++)
            for (gx = 0; gx < w; gx++) {
                uint32_t v = gd[gy * bpr + gx / 2u];
                if ((gx & 1u) ? (v & 15u) : (v >> 4))
                    cv_rect(x + 2 * ((int32_t)gx - f->pad), y + 2 * (int32_t)gy, 2, 2, c);
            }
        x += 2 * f->adv[gi];
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

/* the screen, top to bottom: the chain map (every module a tick: lit = at work, underlined = the page), the page and
 * its knob sets; the module's display; the four knobs; SEL's action here, the locks, UNDO, the octave, the level */
static int page_on(uint32_t m)                  /* is the module doing something to the sound? */
{
    switch (m) {
    case PG_SRC: case PG_ENV: return 1;
    case PG_CHIP: return K.c_on || (P[P_WAVE] >= W_CPLS && P[P_WAVE] <= W_CNSE && P[P_MODE] != 1);
    case PG_RES: return P[P_ZMIX] != 0;
    case PG_GRN: return (P[P_GMODE] || P[P_GFRZ]) && P[P_MODE] != 0;
    case PG_TRN: return P[P_TATK] || P[P_TSUS];
    case PG_DST: return K.x_on;
    case PG_VOX: return K.vx_on;
    case PG_FLT: return P[P_RFMODE] != 0;
    case PG_OTT: return K.ott;
    case PG_DLY: return P[P_DMIX] || P[P_FMIX];
    case PG_REV: return P[P_RMIX] != 0;
    case PG_SPC: return P[P_SDIST] || P[P_SOCCL] || P[P_SPASS];
    case PG_VAR: return P[P_VARY] || P[P_VKEYS] || morph_on;
    case PG_BAT: return batch_on || ft_on;
    case PG_PHY: return P[P_PMODE] != 0;
    case PG_LAB: return SL[cur_slot].dirty != 0;
    case PG_ASSET: return asset_cur != 255u || (P[P_EMU] && P[P_EMIX]);
    case PG_LFO: return P[P_L1MIX] || P[P_L2MIX];
    case PG_RTPC: return (rt_on || P[P_QINT]) && (P[P_Q1A] || P[P_Q2A] || P[P_Q3A]);
    default: return pad_on || pad_held || g_act || (P[P_ATAMT] && (at_keys || at_midi));
    }
}
static uint32_t map_bits(void) { uint32_t m, b = 0; for (m = 0; m < NPAGE; m++) b |= (uint32_t)page_on(m) << m; return b; }
static uint32_t map_drawn;
/* EVA-01's header: the page number on a purple block, the name big; under it the chain (a tick a module, lime: here)
 * and the knob sets (A B C D) */
static void eva_header(void)
{
    char s[4];
    uint32_t m, k, n = NSUB[page];
    int32_t x = 42, w;
    cv_begin(240, 40, EV_BG);
    cv_rect(0, 0, 36, 38, mutant ? EV_LIM : EV_PUR);
    s[0] = (char)('0' + (page + 1u) / 10u); s[1] = (char)('0' + (page + 1u) % 10u); s[2] = 0;
    w = text_w(&FONT_D, s);
    cv_text(18 - w / 2, 12 - FONT_D_CAP, &FONT_D, s, EV_INK);
    if (text_w(&FONT_D, PAGE_NAME[page]) <= 194) cv_text(43, 5 - FONT_D_CAP, &FONT_D, PAGE_NAME[page], EV_TXT);
    else cv_text(43, 18 - FONT_M_BASE, &FONT_M, PAGE_NAME[page], EV_TXT);
    map_drawn = map_bits();
    for (m = 0; m < NPAGE; m++) {               /* 20 ticks, a gap between the buttons' groups */
        int on = (int)(map_drawn >> m & 1u);
        uint16_t c = m == page ? (on ? EV_LIM : EV_TXT) : m == PG_VAR && morph_on ? EV_PUR : on ? EV_GRN : EV_LN;
        if (m && PAGE_GRP[m] != PAGE_GRP[m - 1u]) x += 3;   /* (21 ticks end at 168: clear of A B C D at 173) */
        cv_rect(x, on || m == page ? 27 : 32, 4, on || m == page ? 8 : 3, c);
        if (m == page) cv_rect(x - 1, 36, 6, 1, EV_LIM);
        x += 5;
    }
    for (k = 0; k < n && n > 1u; k++) {         /* the knob sets */
        int32_t tx = 237 - (int32_t)(n - k) * 16;
        char l[2];
        l[0] = (char)('A' + k); l[1] = 0;
        if (sub == k) cv_rect(tx, 25, 14, 12, EV_TXT);
        cv_text(tx + 7 - text_w(&FONT_A, l) / 2, 27 - FONT_A_CAP, &FONT_A, l, sub == k ? EV_INK : EV_MUT);
    }
    cv_rect(0, 38, 240, 2, EV_PUR);
    cv_blit(0, 0);
}
static void draw_header(void)
{
    const theme_t *th = &TH;
    char s[8];
    uint32_t m, k;
    int32_t x = 66;
    if (EVA) { eva_header(); return; }
    cv_begin(240, 40, th->panel);
    cv_rect(4, 3, 7, 14, RGB(8, 8, 10)); cv_rect(5, 5, 2, 10, th->acc); cv_rect(8, 8, 2, 7, th->cyan);
    cv_text(15, 1, &FONT_S, "ND-07", mutant ? th->mut : C_TXT);
    map_drawn = map_bits();
    for (m = 0; m < NPAGE; m++) {               /* the chain: a tick a module, a gap between the buttons' groups */
        int on = (int)(map_drawn >> m & 1u);
        if (m && PAGE_GRP[m] != PAGE_GRP[m - 1u]) x += 4;
        cv_rect(x, on ? 4 : 8, 4, on ? 10 : 6, m == PG_VAR && morph_on ? th->mut : on ? th->cyan : C_DK);
        if (m == page) cv_rect(x - 1, 16, 6, 2, th->acc);
        x += 6;                                 /* (21 ticks: 66 .. 220) */
    }
    cv_rect(0, 19, 240, 1, th->border);
    s[0] = (char)('0' + (page + 1u) / 10u); s[1] = (char)('0' + (page + 1u) % 10u); s[2] = 0;
    cv_rect(3, 23, 20, 14, mutant ? th->mut : th->acc);
    cv_text(5, 21, &FONT_S, s, RGB(10, 10, 12));
    cv_text_fit(28, 21, 236 - 18 * (int32_t)NSUB[page] - 30, PAGE_NAME[page], C_TXT, 0);
    for (k = 0; k < NSUB[page] && NSUB[page] > 1u; k++) {   /* A B (C D): which knob set */
        int32_t tx = 240 - (int32_t)(NSUB[page] - k) * 18;
        char l[2];
        l[0] = (char)('A' + k); l[1] = 0;
        if (sub == k) cv_rect(tx, 23, 16, 14, C_TXT); else cv_frame(tx, 23, 16, 14, C_DK);
        cv_text(tx + 4, 21, &FONT_S, l, sub == k ? RGB(10, 10, 12) : C_MUTE);
    }
    cv_rect(0, 39, 240, 1, th->border);
    cv_blit(0, 0);
}
static void vis_crt(const theme_t *th)
{
    int32_t y;
    if (EVA) {                                  /* flat black, crop marks at the corners */
        cv_begin(240, 112, EV_BG);
        cv_rect(0, 0, 6, 1, EV_MUT); cv_rect(0, 0, 1, 6, EV_MUT); cv_rect(234, 0, 6, 1, EV_MUT); cv_rect(239, 0, 1, 6, EV_MUT);
        cv_rect(0, 111, 6, 1, EV_MUT); cv_rect(0, 106, 1, 6, EV_MUT); cv_rect(234, 111, 6, 1, EV_MUT); cv_rect(239, 106, 1, 6, EV_MUT);
        return;
    }
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
    case W_SHEP: return (isin(ph) + (isin(ph << 1) >> 1) + (isin(ph << 2) >> 2)) * 3 >> 3;
    default: return isin(ph) >> 1;
    }
}
/* ======================================================================== the motion layer
 * Each page has an object drawn in thin lines (grains, a struck body, a ball through a shaper, a room as a tunnel...)
 * that moves with the sound, the knobs and the notes, and keeps breathing when it is quiet. Each frame reads the output
 * (stage_out keeps its last 1024 samples): a 256-point FFT into 24 log bands (60 Hz .. 11 kHz), the level, the onsets,
 * and the last second of both. The object is drawn first and the page's words over it. It all runs in the main loop;
 * the audio only writes its samples. It has its own random numbers, so the patch's never change. */
static uint8_t fx_band[24], fx_lv;              /* the spectrum (0..255 a band), the level (0..255) */
static int16_t fx_s[256];                       /* the last 256 samples (every 2nd: 22 kHz), for the figures */
static int32_t fx_flux;
static uint32_t fx_ang, fx_onset, fx_rng = 0x51F15EEDu, fx_pg = 255u, fx_n, fx_was_on;
static uint8_t fx_hist[6][24], fx_hlv[16];       /* the spectrum's last 6 snapshots (~8 a second), the level's last 16 */
typedef struct { int16_t x, y, vx, vy; uint8_t life, a; } fx_part_t;
static fx_part_t fx_pt[24];                     /* grains, wave fronts: whatever the page throws */
static int16_t fx_tx[32], fx_ty[32];            /* a trail: the last 32 places (the finger, the LFOs' pen) */
static uint32_t fx_tn;
static uint16_t fx_glast[16];                   /* (the grains seen: a new one each time a slot changes) */
static const uint8_t FX_EDGE[25] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 17, 21, 25, 31, 38, 47, 57, 70, 85, 105, 128};
static uint32_t fx_rand(void) { uint32_t x = fx_rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return fx_rng = x; }
static uint16_t mix565(uint16_t a, uint16_t b, int32_t t)   /* a .. b, t 0..256 */
{
    int32_t ra = a >> 11, ga = a >> 5 & 63, ba = a & 31, rb = b >> 11, gb = b >> 5 & 63, bb = b & 31;
    return (uint16_t)((ra + ((rb - ra) * t >> 8)) << 11 | (ga + ((gb - ga) * t >> 8)) << 5 | (ba + ((bb - ba) * t >> 8)));
}
static void fx_fft(int32_t *re, int32_t *im)    /* 256 points, in place, halved each stage (no overflow) */
{
    uint32_t i, j, len, k;
    for (i = 0; i < 256u; i++) {                /* bit reversed order */
        j = (i >> 7 & 1u) | (i >> 5 & 2u) | (i >> 3 & 4u) | (i >> 1 & 8u) | (i << 1 & 16u) | (i << 3 & 32u) | (i << 5 & 64u) | (i << 7 & 128u);
        if (j > i) { int32_t t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (len = 2; len <= 256u; len <<= 1) {
        uint32_t half = len >> 1, step = 256u / len;
        for (i = 0; i < 256u; i += len)
            for (k = 0; k < half; k++) {
                uint32_t ph = k * step << 24;
                int32_t wr = isin(ph + 0x40000000u), wi = -isin(ph);
                int32_t *ar = &re[i + k], *ai = &im[i + k], *br = &re[i + k + half], *bi = &im[i + k + half];
                int32_t tr = (*br * wr - *bi * wi) >> 15, ti = (*br * wi + *bi * wr) >> 15;
                *br = (*ar - tr) >> 1; *bi = (*ai - ti) >> 1;
                *ar = (*ar + tr) >> 1; *ai = (*ai + ti) >> 1;
            }
    }
}
static void fx_listen(void)                     /* the spectrum, the level, an onset */
{
    static int32_t re[256], im[256];
    uint32_t w = tm_sc_w, i, k, live = tm_peak > 16u;
    int32_t flux = 0;
    for (i = 0; i < 256u; i++) {
        int32_t v = live ? tm_scope[(w - 512u + 2u * i) & 1023u] : 0;
        fx_s[i] = (int16_t)v;
        re[i] = v * (int32_t)T_HANN[i] >> 16; im[i] = 0;
    }
    fx_fft(re, im);
    for (k = 0; k < 24u; k++) {
        int32_t m = 0, v, o = fx_band[k];
        for (i = FX_EDGE[k]; i < FX_EDGE[k + 1u]; i++) {
            int32_t a = absi(re[i]), b = absi(im[i]), mm = a > b ? a + (b >> 1) : b + (a >> 1);
            if (mm > m) m = mm;
        }
        v = m ? clampi((ilog2_q8((uint32_t)m * 64u) - 5 * 256) * 255 / (11 * 256), 0, 255) : 0;
        if (v > o) flux += v - o;
        fx_band[k] = (uint8_t)(v > o ? o + ((v - o) * 3 >> 2) : o - ((o - v + 5) / 6));
    }
    fx_lv = (uint8_t)clampi(meter_px(tm_peak, 255), 0, 255);
    if (!(fx_n % 3u)) {                         /* (a snapshot every 3rd frame: the layers of the contours, the echoes' levels) */
        uint32_t h;                             /* (moved back one by hand: the firmware has no memmove) */
        for (h = 5; h > 0; h--) memcpy(fx_hist[h], fx_hist[h - 1], 24);
        memcpy(fx_hist[0], fx_band, 24);
        for (h = 15; h > 0; h--) fx_hlv[h] = fx_hlv[h - 1];
        fx_hlv[0] = fx_lv;
    }
    fx_flux = flux;
    if (flux > 420 || (tm_awake && !fx_was_on)) fx_onset++;
    fx_was_on = tm_awake;
}
/* ---- the kit: thin lines, a level each (0 the background .. 128 the line's colour .. 255 its accent), a small 3D
 * view (turned about the vertical, tilted toward the eye: the far edges dimmer) */
static uint16_t vk_bg, vk_lo, vk_hi;
static uint16_t vk_c(uint16_t c, int32_t lv) { return mix565(vk_bg, c, clampi(lv, 0, 256)); }
static uint16_t vk_lv(int32_t lv) { lv = clampi(lv, 0, 255); return lv < 128 ? mix565(vk_bg, vk_lo, lv * 2) : mix565(vk_lo, vk_hi, (lv - 128) * 2); }
static void fx_vline(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t lv) { if (lv > 0) cv_line(x0, y0, x1, y1, vk_lv(lv)); }
static int32_t vk_x0, vk_y0, vk_yc = 32767, vk_ys, vk_tc = 32767, vk_ts;
static void vk_view(int32_t cx, int32_t cy, uint32_t yaw, uint32_t tilt)
{
    vk_x0 = cx; vk_y0 = cy; vk_yc = isin(yaw + 0x40000000u); vk_ys = isin(yaw); vk_tc = isin(tilt + 0x40000000u); vk_ts = isin(tilt);
}
static int32_t vk_pt(int32_t x, int32_t y, int32_t z, int32_t *sx, int32_t *sy)   /* to the screen; returns the depth (+: near) */
{
    int32_t X = (x * vk_yc - z * vk_ys) >> 15, Z = (x * vk_ys + z * vk_yc) >> 15;
    *sx = vk_x0 + X; *sy = vk_y0 - ((y * vk_tc - Z * vk_ts) >> 15);
    return (Z * vk_tc + y * vk_ts) >> 15;
}
static void vk_l3(int32_t ax, int32_t ay, int32_t az, int32_t bx, int32_t by, int32_t bz, uint16_t c, uint16_t cb)
{
    int32_t x0, y0, x1, y1, d = vk_pt(ax, ay, az, &x0, &y0) + vk_pt(bx, by, bz, &x1, &y1);
    cv_line(x0, y0, x1, y1, d >= 0 ? c : cb);
}
static void vk_ring3(int32_t y, int32_t r, int32_t n, uint32_t rot, int32_t wob, uint16_t c, uint16_t cb)
{   /* a ring lying flat at height y, n sides (2: a bar, 4: a square, 24: round), turned by rot; wob: its edge shaken */
    int32_t k, px = 0, pz = 0;
    uint32_t st = 0xFFFFFFFFu / (uint32_t)n;
    for (k = 0; k <= n; k++) {
        uint32_t a = rot + st * (uint32_t)(k % n);
        int32_t rr = r + (wob ? isin(a * 3u + fx_ang * 9u) * wob >> 15 : 0), x = rr * isin(a + 0x40000000u) >> 15, z = rr * isin(a) >> 15;
        if (k) vk_l3(px, y, pz, x, y, z, c, cb);
        px = x; pz = z;
    }
}
static void vk_ell(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t a0, uint32_t span, int32_t n, uint16_t c)
{   /* an ellipse (span 0) or an arc of one, from a0, in n pieces */
    int32_t k, px = 0, py = 0;
    uint32_t st = (span ? span : 0xFFFFFFFFu) / (uint32_t)n;
    for (k = 0; k <= n; k++) {
        uint32_t a = a0 + st * (uint32_t)(span ? k : k % n);
        int32_t x = cx + (rx * isin(a + 0x40000000u) >> 15), y = cy + (ry * isin(a) >> 15);
        if (k) cv_line(px, py, x, y, c);
        px = x; py = y;
    }
}
static void vk_circ(int32_t x, int32_t y, int32_t r, uint16_t c) { if (r < 1) cv_pset(x, y, c); else vk_ell(x, y, r, r, 0, 0, r < 5 ? 8 : r < 16 ? 16 : 28, c); }
static void vk_dash(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* a dotted line: a pixel in three */
{
    int32_t n = absi(x1 - x0) > absi(y1 - y0) ? absi(x1 - x0) : absi(y1 - y0), i;
    for (i = 0; i <= n; i += 3) cv_pset(x0 + (n ? (x1 - x0) * i / n : 0), y0 + (n ? (y1 - y0) * i / n : 0), c);
}
static void vk_crop(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* a frame by its corners only */
{
    cv_rect(x, y, 5, 1, c); cv_rect(x, y, 1, 5, c); cv_rect(x + w - 5, y, 5, 1, c); cv_rect(x + w - 1, y, 1, 5, c);
    cv_rect(x, y + h - 1, 5, 1, c); cv_rect(x, y + h - 5, 1, 5, c); cv_rect(x + w - 5, y + h - 1, 5, 1, c); cv_rect(x + w - 1, y + h - 5, 1, 5, c);
}
static void vk_smp(uint16_t c, int32_t x0, int32_t w, int32_t y0, int32_t h)   /* the sample as its outline (top and bottom), a
                                                   stroke every 4th column inside */
{
    int32_t i, mid = y0 + h / 2, pa = 0;
    uint16_t d = mix565(vk_bg, c, 70);
    for (i = 0; i < w; i += 2) {
        int32_t a = smp_view[i * 228 / w] * h / 2 / 128;
        if (!(i & 3) && a > 1) cv_rect(x0 + i, mid - a + 1, 1, 2 * a - 1, d);
        if (i) { cv_line(x0 + i - 2, mid - pa, x0 + i, mid - a, c); cv_line(x0 + i - 2, mid + pa, x0 + i, mid + a, c); }
        pa = a;
    }
}
static void vk_stems(uint16_t c, int32_t x0, int32_t w, int32_t y0, int32_t h)   /* the sample as stems every 3 px:
                                                   a faint line up and down, a bright point at each end */
{
    int32_t i, mid = y0 + h / 2;
    uint16_t d = mix565(vk_bg, c, 80);
    for (i = 0; i < w; i += 3) {
        int32_t a = smp_view[i * 228 / w] * h / 2 / 128;
        if (a > 1) { cv_rect(x0 + i, mid - a, 1, 2 * a, d); cv_pset(x0 + i, mid - a, c); cv_pset(x0 + i, mid + a, c); }
        else cv_pset(x0 + i, mid, d);
    }
}
static void fx_contours(int32_t cx, int32_t cy, int32_t r0, int32_t amp, uint32_t ang, int32_t layers)
{   /* the spectrum as closed contours, one a snapshot: the newest outside and bright, the older ones inside, dimmer
       (a topography of the last second); the x radius 15/8 of the y */
    int32_t h, k;
    for (h = layers - 1; h >= 0; h--) {
        int32_t px = 0, py = 0, r = r0 - h * 4, lv = 255 - h * 200 / layers;
        for (k = 0; k <= 48; k++) {
            int32_t kk = k % 48, b = fx_hist[h][kk < 24 ? kk : 47 - kk], rr = r + b * (amp - h * amp / (layers + 2)) / 255, x, y;
            uint32_t a = ang + (uint32_t)kk * (0xFFFFFFFFu / 48u) - (uint32_t)h * (1u << 25);
            x = cx + (rr * 15 * isin(a + 0x40000000u) >> 18); y = cy + (rr * isin(a) >> 15);
            if (k) fx_vline(px, py, x, y, lv);
            px = x; py = y;
        }
    }
}
static void fx_slabs(int32_t vx, int32_t vy, int32_t n, uint32_t turn, int32_t fall)   /* the echoes as slabs going back to a
                                                   vanishing point: the nearest the sound now, each further one smaller,
                                                   turned a bit more, as loud as the level was that many snapshots ago */
{
    int32_t k;
    for (k = n - 1; k >= 0; k--) {
        int32_t sc = 256 - k * 200 / (n + 1), cx = vx + (40 * sc >> 8), cy = vy + (18 * sc >> 8), hw = 13 * sc >> 8, hh = 26 * sc >> 8;
        int32_t lv = fx_hlv[k < 16 ? k : 15] + 40, i, c = isin(turn * (uint32_t)k + 0x40000000u), si = isin(turn * (uint32_t)k), px[4], py[4];
        static const int8_t SX[4] = {-1, 1, 1, -1}, SY[4] = {-1, -1, 1, 1};
        for (i = 0; i < k; i++) lv = lv * fall >> 8;
        for (i = 0; i < 4; i++) { int32_t u = SX[i] * hw, v = SY[i] * hh; px[i] = cx + ((u * c - v * si) >> 15); py[i] = cy + ((u * si + v * c) >> 15); }
        for (i = 0; i < 4; i++) fx_vline(px[i], py[i], px[(i + 1) & 3], py[(i + 1) & 3], lv);
    }
    vk_dash(vx, vy, vx + 52, vy + 24, vk_lv(70));   /* (the axis they sit on) */
}
static void fx_spawn(int32_t x, int32_t y, int32_t vx, int32_t vy, uint32_t life)
{
    uint32_t i, best = 0;
    for (i = 0; i < 24u; i++) if (fx_pt[i].life < fx_pt[best].life) best = i;
    fx_pt[best].x = (int16_t)x; fx_pt[best].y = (int16_t)y; fx_pt[best].vx = (int16_t)vx; fx_pt[best].vy = (int16_t)vy;
    fx_pt[best].life = (uint8_t)life; fx_pt[best].a = (uint8_t)life;
}
static void fx_trail(int32_t x, int32_t y) { fx_tx[fx_tn & 31u] = (int16_t)x; fx_ty[fx_tn & 31u] = (int16_t)y; fx_tn++; }
static void fx_trail_draw(uint16_t c)           /* the trail, the oldest faintest */
{
    uint32_t i, n = fx_tn < 32u ? fx_tn : 32u;
    for (i = 1; i < n; i++) {
        uint32_t a = (fx_tn - n + i - 1u) & 31u, b = (fx_tn - n + i) & 31u;
        cv_line(fx_tx[a], fx_ty[a], fx_tx[b], fx_ty[b], vk_c(c, 40 + (int32_t)i * 216 / (int32_t)n));
    }
}
static const int8_t VK_CUBE[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
static const int8_t VK_OCTA[8][3] = {{0, -1, 0}, {1, 0, 0}, {0, 1, 0}, {-1, 0, 0}, {0, 0, -1}, {1, 0, 0}, {0, 0, 1}, {-1, 0, 0}};
static const uint8_t VK_EDGE[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
static void vk_morph(int32_t s, int32_t m, uint16_t c, uint16_t cb)   /* a cube (m 0) .. an octahedron (m 256), size s */
{
    int32_t e, v[8][3], i, j;
    for (i = 0; i < 8; i++) for (j = 0; j < 3; j++) v[i][j] = s * (VK_CUBE[i][j] * (256 - m) + VK_OCTA[i][j] * m * 3 / 2) >> 8;
    for (e = 0; e < 12; e++) { const int32_t *a = v[VK_EDGE[e][0]], *b = v[VK_EDGE[e][1]]; vk_l3(a[0], a[1], a[2], b[0], b[1], b[2], c, cb); }
}
static void vk_tool(int32_t cx, int32_t cy, int32_t tool, uint16_t c)   /* SAMPLE LAB's tool, as a sign */
{
    int32_t k;
    switch (tool) {
    case 0: for (k = 0; k < 2; k++) { cv_line(cx + 2 + k * 8, cy - 7, cx - 5 + k * 8, cy, c); cv_line(cx - 5 + k * 8, cy, cx + 2 + k * 8, cy + 7, c); } break;   /* << */
    case 1: cv_line(cx - 8, cy - 8, cx - 8, cy + 8, c); cv_line(cx - 8, cy - 8, cx - 4, cy - 8, c); cv_line(cx - 8, cy + 8, cx - 4, cy + 8, c);   /* [ ] */
            cv_line(cx + 8, cy - 8, cx + 8, cy + 8, c); cv_line(cx + 8, cy - 8, cx + 4, cy - 8, c); cv_line(cx + 8, cy + 8, cx + 4, cy + 8, c); break;
    case 2: cv_line(cx - 10, cy + 6, cx + 10, cy - 6, c); cv_line(cx - 10, cy + 6, cx + 10, cy + 6, c); cv_line(cx + 10, cy - 6, cx + 10, cy + 6, c); break;
    case 3: cv_line(cx - 10, cy - 6, cx + 10, cy + 6, c); cv_line(cx - 10, cy + 6, cx + 10, cy + 6, c); cv_line(cx - 10, cy - 6, cx - 10, cy + 6, c); break;
    case 4: case 5: {                           /* a spring: stretched (half speed) or squeezed */
        int32_t n = tool == 4 ? 3 : 7, w = 22, px = cx - w / 2, py = cy;
        for (k = 1; k <= n * 2; k++) { int32_t x = cx - w / 2 + k * w / (n * 2), y = cy + (k & 1 ? -6 : 6); if (k == n * 2) y = cy; cv_line(px, py, x, y, c); px = x; py = y; }
        break;
    }
    case 6: cv_line(cx - 10, cy - 8, cx + 10, cy - 8, c); for (k = -6; k <= 6; k += 6) { cv_line(cx + k, cy + 8, cx + k, cy - 5, c); cv_line(cx + k - 2, cy - 3, cx + k, cy - 5, c); cv_line(cx + k + 2, cy - 3, cx + k, cy - 5, c); } break;
    case 7: vk_ell(cx, cy, 8, 8, 0x40000000u, 0xC0000000u, 18, c); cv_line(cx + 8, cy, cx + 5, cy - 4, c); cv_line(cx + 8, cy, cx + 11, cy - 4, c); break;   /* back */
    default: cv_line(cx - 4, cy - 10, cx + 4, cy - 10, c); cv_line(cx - 4, cy - 10, cx - 4, cy + 10, c); cv_line(cx + 4, cy - 10, cx + 4, cy + 10, c);   /* the slab */
             cv_line(cx - 4, cy + 10, cx + 4, cy + 10, c); cv_line(cx + 4, cy - 10, cx + 7, cy - 13, c); cv_line(cx - 4, cy - 10, cx - 1, cy - 13, c); cv_line(cx - 1, cy - 13, cx + 7, cy - 13, c); cv_line(cx + 7, cy - 13, cx + 7, cy + 7, c); break;
    }
}
static uint8_t fx_rip[4];                       /* ripples: their age (0: none) */
static void fx_ripples(int32_t x, int32_t y, int32_t hit, uint16_t c)
{
    uint32_t i;
    if (hit) for (i = 0; i < 4u; i++) if (!fx_rip[i]) { fx_rip[i] = 1; break; }
    for (i = 0; i < 4u; i++) if (fx_rip[i]) {
        int32_t a = fx_rip[i];
        vk_circ(x, y, a, vk_c(c, 256 - a * 6));
        fx_rip[i] = (uint8_t)(a + 2 > 40 ? 0 : a + 2);
    }
}
static int32_t vk_shape(int32_t x, int32_t rc, int32_t kind)   /* DESTROY's shaper on a coordinate: rc the edge */
{
    switch (kind) {
    case 0: return x * 2 * rc / (rc + absi(x) + 1);
    case 1: return clampi(x, -rc, rc);
    case 2: while (x > rc || x < -rc) x = x > 0 ? 2 * rc - x : -2 * rc - x; return x;
    default: return absi(x) - rc / 2;           /* (RECTIFY: on the height only, the rest as SOFT) */
    }
}
static void fx_frame(const theme_t *th)         /* the page's object: one frame, under the page's words */
{
    uint32_t t = fm1_ms, br = (uint32_t)(isin(t * 0x00030000u) + 32768) >> 8;   /* (a slow breath, 0..255: ~2.4 s) */
    int32_t lv = fx_lv, live = tm_peak > 16u, k, sp = mutant ? 4 : 1;   /* (HAPPY ACCIDENT: everything spins faster) */
    uint16_t c1 = EVA ? EV_GRN : th->cyan, c2 = th->acc, c3 = th->mut;
    uint32_t ons = fx_onset;
    static uint32_t on_seen;
    int32_t hit = ons != on_seen;
    on_seen = ons;
    vk_bg = EVA ? EV_BG : RGB(5, 5, 7); vk_lo = c1; vk_hi = c2;
    if (page != fx_pg) { memset(fx_pt, 0, sizeof fx_pt); memset(fx_rip, 0, sizeof fx_rip); fx_tn = 0; fx_pg = page; }
    fx_n++;
    fx_ang += (uint32_t)sp * (0x00400000u + ((uint32_t)lv << 16));   /* (louder: faster) */
    switch (page) {
    case PG_ASSET:                              /* a small spectrometer at the right of the status line (A) */
        if (sub) break;
        for (k = 0; k < 18; k++) {
            int32_t h = live ? fx_band[k + 3] * 12 / 255 : (int32_t)((uint32_t)(isin(t * (1u << 18) + (uint32_t)k * 0x0A000000u) + 32768) >> 13), j;
            for (j = 0; j <= h; j += 2) cv_rect(182 + k * 3, 13 - j, 2, 1, vk_c(c3, 110 + j * 12));
        }
        break;
    case PG_CHIP:                               /* the sound as 8-bit blocks, on the right; at rest a square wave walks */
    {
        int32_t m = 2600;
        for (k = 0; k < 256; k++) if (absi(fx_s[k]) > m) m = absi(fx_s[k]);
        for (k = 0; k < 28; k++) {
            int32_t v = live ? fx_s[k * 8] * 12 / m : ((((int32_t)(t >> 6) + k) & 7) < 4 ? 8 : -8);
            v = clampi(v, -12, 12) & ~3;
            cv_rect(118 + k * 4, 76 - v, 3, 3, vk_c(c3, 150 + absi(v) * 8));
        }
        break;
    }
    case PG_GRN: {                              /* the grains: a ring each, born where it reads, rising and shrinking */
        int32_t r = 2 + P[P_GSIZE] / 24;
        for (k = 0; k < 16; k++) {
            uint16_t gp = tm_gpos[k];
            if (gp && gp != fx_glast[k] && P[P_GMODE] && P[P_MODE]) fx_spawn(6 + (int32_t)gp * 228 / 32768, 30 + (int32_t)(fx_rand() % 70u), r, tm_grev[k], 36);
            fx_glast[k] = gp;
        }
        for (k = 0; k < 24; k++) {
            fx_part_t *p = &fx_pt[k];
            if (!p->life) continue;
            vk_circ(p->x, p->y, 1 + p->vx * p->life / 36, vk_c(p->vy ? C_TXT : c1, 60 + p->life * 6));
            if (!(fx_n & 1u)) p->y--;
            p->life--;
        }
        if (!live || !P[P_GMODE]) {             /* at rest: three rings breathing at POS */
            int32_t x = 6 + P[P_GPOS] * 228 / 100;
            for (k = 0; k < 3; k++) vk_circ(x, 60 + (isin(fx_ang * (uint32_t)(k + 1) + (uint32_t)k * 0x55555555u) * 24 >> 15), 3 + k * 2 + (int32_t)(br >> 6), vk_c(c1, 90 + k * 30));
        }
        break;
    }
    case PG_LAB: {                              /* the slot wound in a ring, turning; the tool as a sign in the middle */
        int32_t cx = 120, cy = 53, r = 28, len = (int32_t)smp_len > 0 ? (int32_t)smp_len : 1, tool = P[P_STOOL] % 9;
        uint32_t ang = t << 19;            /* (a turn in 8 s) */
        for (k = 0; k < 76; k++) {
            int32_t a = smp_view[k * 3] * 10 / 128 + 1, cs = isin(ang + (uint32_t)k * (0xFFFFFFFFu / 76u) + 0x40000000u), sn = isin(ang + (uint32_t)k * (0xFFFFFFFFu / 76u));
            int32_t in = 0;
            if (tool == 1) in = k * len / 76 >= lp_s && k * len / 76 <= lp_e;
            else if (tool == 2) in = k * 100 / 76 <= P[P_SAMT];
            else if (tool == 3) in = k * 100 / 76 >= 100 - P[P_SAMT];
            cv_line(cx + ((r - a) * cs >> 15), cy + ((r - a) * sn >> 15), cx + ((r + a) * cs >> 15), cy + ((r + a) * sn >> 15), in ? c2 : vk_c(c1, 120 + a * 12));
        }
        vk_circ(cx, cy, r - 14, vk_c(c3, 90));
        vk_tool(cx, cy, tool, SL[cur_slot].dirty ? th->haz : c2);
        if (tm_awake) {                         /* where it plays */
            uint32_t q = ang + tm_play * 131072u;
            vk_circ(cx + ((r + 14) * isin(q + 0x40000000u) >> 15), cy + ((r + 14) * isin(q) >> 15), 2, C_TXT);
        }
        break;
    }
    case PG_TRN: fx_ripples(12, 26, hit, c1); break;   /* (each hit a ripple from the attack) */
    case PG_RES: {                              /* the body struck, seen from above at an angle: a ring a mode, stacked as the
                                                   material is built (a bell, a glass, planks, strings, a drum, a tube);
                                                   each ring shakes as its mode rings */
        static const int8_t SIDES[7] = {6, 24, 4, 2, 24, 24, 24}, R0[7] = {44, 16, 40, 50, 44, 46, 18}, DR[7] = {-6, 5, -4, 0, -7, 0, 0}, SP[7] = {10, 11, 9, 11, 11, 3, 13};
        int32_t mat = P[P_ZMAT] % 7, on = P[P_ZMIX] != 0, gap = SP[mat] * (128 + P[P_ZSPRD] * 2) >> 8;
        vk_view(120, 84, fx_ang / 3u, 0x12000000u);
        for (k = 0; k < 6; k++) {
            int32_t z = clampi(tm_z[k] * 8 / 6000, 0, 8), a = on ? 110 + (int32_t)ZAMP[mat][k] * 100 / 32768 + z * 6 : 60;
            vk_ring3(k * gap, R0[mat] + DR[mat] * k, SIDES[mat], (uint32_t)k * 0x0A000000u, on ? z : 0, vk_c(z > 1 ? c2 : c1, a), vk_c(c1, a / 3));
        }
        break;
    }
    case PG_DST: {                              /* a ball through the shaper: HARD squares it, FOLD folds it, BITS snap its
                                                   points to a grid, RATE holds them; it swells with the sound */
        int32_t drv = (int32_t)T_DB2LIN[clampi(P[P_XDRV] * 2 + 288, 0, 480)] >> 4, rc = 30, rr = clampi(rc * drv / 256 * (256 + lv / 2) / 256, 8, 46);
        int32_t step = 1 << clampi((16 - P[P_XBITS]) / 3, 0, 4), hold = 1 + P[P_XSR] / 12, shp = P[P_XSHP], on = P[P_XMIX] != 0, la, lo2, px = 0, py = 0;
        uint16_t cf = vk_c(on ? c2 : C_MUTE, 230), cb = vk_c(on ? c1 : C_MUTE, 90);
        vk_view(178, 58, fx_ang, 0x10000000u);
        for (la = -2; la <= 2; la++)            /* rings of latitude, then the meridians */
            for (lo2 = 0; lo2 <= 16; lo2++) {
                uint32_t a = (uint32_t)(lo2 / hold * hold % 16) * 0x10000000u, b = (uint32_t)la * 0x13000000u;
                int32_t cr = isin(b + 0x40000000u), x = rr * cr >> 15, y = rr * isin(b) >> 15, z, sx, sy, d;
                z = x * isin(a) >> 15; x = x * isin(a + 0x40000000u) >> 15;
                x = vk_shape(x, rc, shp == 3 ? 0 : shp); y = vk_shape(y, rc, shp); z = vk_shape(z, rc, shp == 3 ? 0 : shp);
                d = vk_pt(x, y, z, &sx, &sy);
                sx = sx / step * step; sy = sy / step * step;
                if (lo2) cv_line(px, py, sx, sy, d >= 0 ? cf : cb);
                px = sx; py = sy;
            }
        for (lo2 = 0; lo2 < 8; lo2++)
            for (la = -4; la <= 4; la++) {
                uint32_t a = (uint32_t)lo2 * 0x20000000u, b = (uint32_t)(la / hold * hold) * 0x0F000000u;
                int32_t x = rr * isin(b + 0x40000000u) >> 15, y = rr * isin(b) >> 15, z, sx, sy, d;
                z = x * isin(a) >> 15; x = x * isin(a + 0x40000000u) >> 15;
                x = vk_shape(x, rc, shp == 3 ? 0 : shp); y = vk_shape(y, rc, shp); z = vk_shape(z, rc, shp == 3 ? 0 : shp);
                d = vk_pt(x, y, z, &sx, &sy);
                sx = sx / step * step; sy = sy / step * step;
                if (la > -4) cv_line(px, py, sx, sy, d >= 0 ? cf : cb);
                px = sx; py = sy;
            }
        break;
    }
    case PG_VOX:                                /* A, C: a mouth saying the vowel: U small and round .. A open .. I wide and
                                                   thin; it opens with the sound, GROWL shakes it; the voice going out */
        if (sub != 1u) {
            static const int8_t MW[5] = {12, 17, 26, 34, 38}, MH[5] = {11, 15, 21, 12, 6};
            int32_t pos = clampi(E[P_VOWEL], 0, 100) * 4, i0 = pos / 100 > 3 ? 3 : pos / 100, f = pos - i0 * 100, cx = 120, cy = 46;
            int32_t w = MW[i0] + (MW[i0 + 1] - MW[i0]) * f / 100, hg = MH[i0] + (MH[i0 + 1] - MH[i0]) * f / 100;
            int32_t op = 70 + lv * 186 / 255 + (int32_t)(br >> 4), pxi = 0, pyu = 0, pyl = 0, pou = 0, pol = 0, pxo = 0;
            if (P[P_GROWL]) op = op * (256 + (isin(t * (uint32_t)P[P_GRATE] * 4294967u) >> 9) * P[P_GROWL] / 100) >> 8;
            hg = 2 + hg * op / 256;
            for (k = 0; k <= 24; k++) {
                int32_t u = k * 256 / 12 - 256, sq = (65536 - u * u) >> 8, xi = cx + w * u / 256, xo = cx + w * 13 * u / 2560;
                int32_t yu = cy - hg * sq / 256, yl = cy + hg * sq * 115 / 25600;
                int32_t ou = cy - (hg + 6) * sq / 256 + (absi(u) < 52 ? 3 * (2704 - u * u) / 2704 : 0), ol = cy + (hg * 115 / 100 + 8) * sq / 256;
                if (k) {
                    cv_line(pxi, pyu, xi, yu, c1); cv_line(pxi, pyl, xi, yl, c1);
                    cv_line(pxo, pou, xo, ou, vk_c(c2, 200)); cv_line(pxo, pol, xo, ol, vk_c(c2, 200));
                }
                pxi = xi; pyu = yu; pyl = yl; pxo = xo; pou = ou; pol = ol;
            }
            if (hg > 8) cv_line(cx - w * 5 / 10, cy - hg * 6 / 10, cx + w * 5 / 10, cy - hg * 6 / 10, vk_c(C_TXT, 120));   /* (teeth) */
            for (k = 0; k < 3; k++) vk_ell(cx + w * 13 / 10 + 6, cy, 6 + k * 6, 8 + k * 7, 0xD5555555u, 0x55555555u, 6, vk_c(c3, live ? 60 + lv / 2 - k * 30 : 40));
        }
        break;
    case PG_FLT: fx_contours(120, 34, 8 + (int32_t)(br >> 6), 22, fx_ang, 5); break;   /* the spectrum's topography, above
                                                   the response */
    case PG_DLY: fx_slabs(118, 14, 3 + P[P_DFB] / 12, 0x01800000u + (uint32_t)P[P_DTIME] * 0x00060000u, 150 + P[P_DFB]); break;
    case PG_REV: {                              /* the room as a tunnel: SIZE spaces its frames, DECAY how far they stay lit;
                                                   the top edges HIGH's decay, the floor LOW's; the sound goes in frame by frame */
        int32_t n = 9, s = 170 + P[P_RSIZE] * 60 / 100, w = 92 + (int32_t)(br >> 5), dk = 150 + P[P_RDEC] * 100 / 63, a = 255, pw = 0, ph = 0;
        for (k = 0; k < n; k++) {
            int32_t h = w * 38 / 100, l = (a * (90 + fx_hlv[k]) >> 8) + 20, lt = l * clampi(P[P_RHIGH], 20, 200) / 100, lb = l * clampi(P[P_RLOW], 20, 200) / 100;
            uint16_t cs = vk_c(c1, l), ct = vk_c(c2, lt), cb = vk_c(c1, lb);
            cv_line(120 - w, 54 - h, 120 + w, 54 - h, ct); cv_line(120 - w, 54 + h, 120 + w, 54 + h, cb);
            cv_line(120 - w, 54 - h, 120 - w, 54 + h, cs); cv_line(120 + w, 54 - h, 120 + w, 54 + h, cs);
            if (k) { uint16_t cd = vk_c(c1, l / 3); cv_line(120 - pw, 54 - ph, 120 - w, 54 - h, cd); cv_line(120 + pw, 54 - ph, 120 + w, 54 - h, cd);
                     cv_line(120 - pw, 54 + ph, 120 - w, 54 + h, cd); cv_line(120 + pw, 54 + ph, 120 + w, 54 + h, cd); }
            pw = w; ph = h;
            w = w * s >> 8; a = a * dk >> 8;
        }
        break;
    }
    case PG_SPC: {                              /* wave fronts from the source: ahead of it they bunch (the Doppler) */
        int32_t lx = 60, ly = 100, r = 22 + E[P_SDIST] * 62 / 100, sx = P[P_SPASS] ? lx + clampi(tm_px, -1024, 1024) * 54 / 1024 : lx;
        if ((live || P[P_SPASS]) && !(fx_n % 6u)) fx_spawn(sx, ly - r, 0, 0, 30);
        for (k = 0; k < 24; k++) {
            fx_part_t *p = &fx_pt[k];
            int32_t rr = 31 - p->life;   /* (30 px at most: the words on the right stay clear) */
            if (!p->life) continue;
            vk_ell(p->x, p->y, rr, rr, 0, 0, 20, vk_c(c1, p->life * 7));
            p->life--;
        }
        break;
    }
    case PG_LFO: {                              /* a harmonograph: LFO 1 left-right, LFO 2 up-down, the pen's last places */
        int32_t a1 = P[P_L1MIX] ? tm_lfo[0] : isin(t * 0x00110000u), a2 = P[P_L2MIX] ? tm_lfo[1] : isin(t * 0x000B0000u);
        fx_trail(56 + a1 * 44 / 32768, 56 - a2 * 44 / 32768);
        cv_rect(56, 10, 1, 92, vk_c(c3, 60)); cv_rect(10, 56, 92, 1, vk_c(c3, 60));
        vk_crop(8, 8, 97, 97, vk_c(c3, 120));
        fx_trail_draw(P[P_L1MIX] || P[P_L2MIX] ? c2 : C_MUTE);
        vk_circ(fx_tx[(fx_tn - 1u) & 31u], fx_ty[(fx_tn - 1u) & 31u], 3, C_TXT);
        break;
    }
    case PG_PAD:                                /* the finger's last places */
        if (pad_on || pad_held) fx_trail(10 + pad_x * 100 / 127, 104 - pad_y * 100 / 127);
        else if (fx_tn) fx_trail(fx_tx[(fx_tn - 1u) & 31u], fx_ty[(fx_tn - 1u) & 31u]);
        fx_trail_draw(c2);
        break;
    case PG_VAR:                                /* A: the seed as a constellation; B: A (a cube) turning into B */
        if (sub) {
            int32_t m = clampi(E[P_MORPH], 0, 100);
            vk_view(28, 37, fx_ang, 0x12000000u); vk_morph(11, 0, has_ab & 1u ? c2 : C_DK, vk_c(c2, 60));
            vk_view(212, 37, fx_ang, 0x12000000u); vk_morph(11, 256, has_ab & 2u ? c1 : C_DK, vk_c(c1, 60));
            vk_view(54 + m * 132 / 100, 37, fx_ang * 2u, 0x12000000u);
            vk_morph(9 + lv / 40, m * 256 / 100, morph_on ? c3 : C_MUTE, vk_c(c3, 60));
        } else {
            uint32_t sd = (uint32_t)P[P_VSEED] * 2654435761u + 1u;
            int32_t px = 0, py = 0;
            for (k = 0; k < 12; k++) {
                int32_t x, y;
                sd ^= sd << 13; sd ^= sd >> 17; sd ^= sd << 5;
                x = 62 + (int32_t)(sd % 96u) + (isin(fx_ang + (uint32_t)k * 0x15000000u) * 2 >> 15); y = 20 + (int32_t)((sd >> 9) % 26u);
                if (k) vk_dash(px, py, x, y, vk_c(c3, 140));
                vk_circ(x, y, hit && (k & 3) == (int32_t)(ons & 3u) ? 3 : 1, vk_c(c2, 120 + (int32_t)((br * (uint32_t)(k + 3)) & 127u)));
                px = x; py = y;
            }
        }
        break;
    case PG_BAT: {                              /* each new hit a ripple from its bar */
        static uint32_t seen;
        uint32_t nh = lvl_n < 16u ? lvl_n : 16u;
        fx_ripples(14 + (int32_t)(nh ? nh - 1u : 0u) * 14, 40, lvl_n != seen, ft_on ? c1 : c2);
        seen = lvl_n;
        break;
    }
    case PG_PHY: break;                         /* (its ball is drawn with the hits, page code) */
    default: break;
    }
}
/* ======================================================================== the solids (OTT, REVERB) */
static void solid(int32_t kind, int32_t cx, int32_t base, int32_t h, uint16_t c, uint16_t back)   /* wireframe: 0 cylinder,
                                                   1 prism, 2 pyramid; h tall, standing on base, the hidden edges dim */
{
    int32_t rx = 16, ry = 5, k, top = base - h;
    if (h < 2) h = 2;
    if (kind == 0) {
        int32_t px = 0, py = 0, qx = 0, qy = 0;
        for (k = 0; k <= 24; k++) {
            uint32_t a = (uint32_t)k * (0xFFFFFFFFu / 24u);
            int32_t x = cx + (rx * isin(a + 0x40000000u) >> 15), y = (ry * isin(a) >> 15);
            if (k) { cv_line(px, top + py, x, top + y, c); cv_line(qx, base + qy, x, base + y, y < 0 || py < 0 ? back : c); }
            px = x; py = y; qx = x; qy = y;
        }
        cv_line(cx - rx, top, cx - rx, base, c); cv_line(cx + rx, top, cx + rx, base, c);
    } else if (kind == 1) {
        int32_t dx = 7, dy = 5, w = 12;
        cv_frame(cx - w - dx / 2, top + dy, 2 * w, h - dy + 1, c);
        cv_line(cx - w - dx / 2, top + dy, cx - w + dx / 2, top, c); cv_line(cx + w - dx / 2, top + dy, cx + w + dx / 2, top, c);
        cv_line(cx - w + dx / 2, top, cx + w + dx / 2, top, c);
        cv_line(cx + w + dx / 2, top, cx + w + dx / 2, base - dy, c); cv_line(cx + w - dx / 2, base, cx + w + dx / 2, base - dy, c);
        cv_line(cx - w + dx / 2, top, cx - w + dx / 2, base - dy, back); cv_line(cx - w + dx / 2, base - dy, cx + w + dx / 2, base - dy, back);
        cv_line(cx - w - dx / 2, base, cx - w + dx / 2, base - dy, back);
    } else {
        int32_t w = 15, dy = 5;
        cv_line(cx - w, base, cx + w - 6, base, c); cv_line(cx + w - 6, base, cx + w, base - dy, c);
        cv_line(cx - w, base, cx - w + 6, base - dy, back); cv_line(cx - w + 6, base - dy, cx + w, base - dy, back);
        cv_line(cx, top, cx - w, base, c); cv_line(cx, top, cx + w - 6, base, c); cv_line(cx, top, cx + w, base - dy, c);
        cv_line(cx, top, cx - w + 6, base - dy, back);
    }
}
static void vis_chips(const theme_t *th)       /* ASSETS B, C: the SOUND CHIP as a part on a board, its pins busy with
                                                   the sound; the sound as it comes out of its DAC (steps, the DAC's
                                                   levels as a ruler); the five as cartridge labels */
{
    char s[40], t[16];
    int32_t i, k;
    static const char *const DEP[6] = {"", "LOG DAC", "9-BIT DAC", "BRR 4-BIT", "LINEAR PCM", "4-BIT WAVE"};
    int32_t m = P[P_EMU], on = m && P[P_EMIX], qfp = m >= 3, np = qfp ? 8 : m == 1 ? 10 : 8, mx = 2600, px = 0, py = 0;
    uint16_t cb = on ? th->acc : C_MUTE, cl = on ? th->cyan : C_DK;
    uint32_t q;
    cv_text(8, 0, &FONT_S, E_EMU[m], m ? th->acc : C_MUTE);
    cv_text(14 + text_w(&FONT_S, E_EMU[m]), 5 - FONT_A_CAP, &FONT_A, EMU_HOST[m], C_MUTE);
    if (m) { fmt_hz(t, emu_rate(m, P[P_ERATE] * 256)); cv_text_fit(160, 0, 74, t, th->cyan, 2); }
    if (!m) {                               /* an empty socket */
        for (k = 0; k < 8; k++) { cv_rect(26, 24 + k * 7, 5, 2, C_DK); cv_rect(65, 24 + k * 7, 5, 2, C_DK); }
        vk_dash(32, 20, 64, 20, C_MUTE); vk_dash(32, 76, 64, 76, C_MUTE); vk_dash(32, 20, 32, 76, C_MUTE); vk_dash(64, 20, 64, 76, C_MUTE);
    } else if (qfp) {                       /* a square part, pins on four sides, pin 1 marked */
        cv_frame(24, 22, 48, 48, cb);
        for (k = 0; k < np; k++) {
            int32_t o = 26 + k * 6, lit = on && fx_lv > 20 && ((k + (int32_t)(fm1_ms >> 6)) % 5) == 0;
            uint16_t c = lit ? C_TXT : cl;
            cv_rect(o, 18, 2, 4, c); cv_rect(o, 70, 2, 4, c); cv_rect(20, o - 2, 4, 2, lit ? cl : c); cv_rect(72, o - 2, 4, 2, c);
        }
        vk_circ(30, 28, 2, cb);
        cv_text(48 - text_w(&FONT_A, E_EMU[m]) / 2, 42 - FONT_A_CAP, &FONT_A, E_EMU[m], cb);
    } else {                                /* a DIP: two rows of pins, the notch */
        cv_frame(22, 18, 52, 62, cb);
        vk_ell(48, 18, 5, 5, 0, 0x80000000u, 8, cb);
        for (k = 0; k < np; k++) {
            int32_t y = 22 + k * 54 / (np - 1), lit = on && fx_lv > 20 && ((k + (int32_t)(fm1_ms >> 6)) % 4) == 0;
            cv_rect(16, y, 6, 2, lit ? C_TXT : cl); cv_rect(74, y, 6, 2, lit ? C_TXT : cl);
        }
        cv_text(48 - text_w(&FONT_A, E_EMU[m]) / 2, 45 - FONT_A_CAP, &FONT_A, E_EMU[m], cb);
    }
    s[0] = 0; cat(s, DEP[m]);
    if (m == 2 || m == 4) { fmt_num(t, K.em_bits, 0); s[0] = 0; cat(s, t); cat(s, m == 2 ? "-BIT LADDER" : "-BIT PCM"); }
    if (m == 1) { fmt_num(t, K.em_n, 0); s[0] = 0; cat(s, t); cat(s, " LOG STEPS"); }
    if (m == 5) { fmt_num(t, 16 >> K.em_sh, 0); s[0] = 0; cat(s, t); cat(s, " LEVELS"); }
    if (m) cv_text(48 - text_w(&FONT_A, s) / 2, 86 - FONT_A_CAP, &FONT_A, s, on ? C_TXT : C_MUTE);
    vk_crop(104, 18, 130, 72, C_MUTE);      /* the output, as stepped as the chip leaves it */
    for (i = 0; i < 256; i++) if (absi(fx_s[i]) > mx) mx = absi(fx_s[i]);
    if (m == 1) for (q = 0; q < K.em_n; q++) cv_rect(106, 86 - K.em_lv[q] * 66 / 65535, q & 3u ? 2 : 4, 1, th->mut);   /* (its log steps: a ruler) */
    if (m == 5) for (q = 0; q < 16u; q += 1u << K.em_sh) cv_rect(106, 86 - (int32_t)q * 66 / 15, 3, 1, th->mut);
    if (m == 3) { int32_t bl = (int32_t)(16u * 22050u / (emu_rate(3, P[P_ERATE] * 256) + 1u)); for (k = 110; k < 232 && bl > 3; k += bl) vk_dash(k, 22, k, 86, C_DK); }   /* (BRR blocks) */
    for (i = 0; i < 120; i++) {
        int32_t x = 112 + i, y = 54 - fx_s[136 + i] * 30 / mx;
        if (m == 5) { if (!(i & 1)) cv_rect(x, y & ~1, 2, 2, cl); }   /* (the Game Boy: chunky LCD pixels) */
        else if (i && (m == 1 || m == 2)) { cv_line(px, py, x, py, cl); cv_line(x, py, x, y, cl); }
        else if (i) cv_line(px, py, x, y, cl);
        px = x; py = y;
    }
    if (K.em_ev && on) { fmt_num(t, P[P_ETIME] * 16, 0); s[0] = 0; cat(s, "ECHO "); cat(s, t); cat(s, "ms"); cv_text_fit(108, 72, 122, s, th->mut, 2); }
    for (k = 1; k <= 5; k++) {              /* the five, as cartridge labels */
        int32_t x = 4 + (k - 1) * 47, w = text_w(&FONT_A, E_EMU[k]);
        if (k == m) { cv_rect(x, 96, 45, 13, th->acc); cv_text(x + 23 - w / 2, 99 - FONT_A_CAP, &FONT_A, E_EMU[k], RGB(0, 0, 0)); }
        else { cv_frame(x, 96, 45, 13, C_DK); cv_text(x + 23 - w / 2, 99 - FONT_A_CAP, &FONT_A, E_EMU[k], C_MUTE); }
    }
    }
static void draw_vis(void)
{
    const theme_t *th = &TH;
    char s[40], t[16];
    int32_t i, k;
    vis_crt(th);
    fx_listen(); fx_frame(th);                  /* the page's object, under its words */
    switch (page) {
    case PG_SRC: {                              /* the oscillator | the sample, the blend underneath */
        int32_t md = P[P_MODE], px = 0, py = 0, on_o = md != 1, on_s = md != 0;
        uint16_t co = on_o ? th->acc : C_DK, cs = on_s ? th->cyan : C_DK;
        if (sub == 2u) {                        /* C: the loop on the whole sample (START > LOOP > STOP) */
            int32_t len = (int32_t)smp_len > 0 ? (int32_t)smp_len : 1, xs = 6 + lp_s * 228 / len, xe = 6 + lp_e * 228 / len;
            int32_t xx = 6 + (lp_s - lp_xf) * 228 / len, lm = P[P_SLOOP];
            vk_stems(lm ? th->cyan : C_MUTE, 6, 228, 22, 60);
            if (lm) {
                cv_rect(xs, 18, 1, 68, th->acc); cv_rect(xe, 18, 1, 68, th->acc);
                cv_rect(xs, 18, xe - xs, 1, th->acc); cv_rect(xs, 85, xe - xs, 1, th->acc);
                if (lp_xf) cv_line(xx, 83, xs, 18, th->mut);
                if (lm == 2) {
                    cv_text_fit(6, 88, xs - 6 > 30 ? xs - 6 : 30, "START", C_MUTE, 1);
                    cv_text_fit(xs, 88, xe - xs, "LOOP", th->acc, 1);
                    cv_text_fit(xe, 88, 234 - xe > 30 ? 234 - xe : 30, "STOP", C_MUTE, 1);
                }
            }
            if (on_s && tm_awake && !P[P_GMODE]) cv_rect(6 + (int32_t)tm_play * 228 / 32768, 18, 1, 68, C_TXT);
            s[0] = 0; cat(s, E_SLOOP[lm % 3]);
            if (lm) { cat(s, "  "); fmt_dec(t, (lp_e - lp_s) * 100 / 22050, 100u, 2u, 0); cat(s, t); cat(s, "s"); }
            cv_text_fit(8, 0, 226, s, lm ? th->acc : C_MUTE, 0);
            if (lm != 2) cv_text_fit(8, 92, 226, lm ? "XFADE ON ZERO CROSSINGS" : "PLAYS ONCE", C_MUTE, 1);
            break;
        }
        if (sub == 3u) {                        /* D: the voice and its layers, how loud each */
            int32_t nv = P[P_VOICES], dt = P[P_LDET];
            cv_text_fit(8, 0, 226, E_VOICES[nv % 3], nv ? th->acc : C_MUTE, 0);
            for (k = 0; k < 3; k++) {
                int32_t y = 24 + k * 24, on = k <= nv, w = k ? P[P_LLVL] * 124 / 100 : 124;
                s[0] = 0;
                if (!k) cat(s, "MAIN");
                else if (k == 1) { fmt_num(t, P[P_LPIT], 1); cat(s, t); cat(s, "st"); if (dt) { cat(s, " +"); fmt_num(t, dt, 0); cat(s, t); cat(s, "c"); } }
                else { cat(s, "-"); fmt_num(t, dt, 0); cat(s, t); cat(s, "c"); }
                cv_text_fit(8, y, 94, s, on ? C_TXT : C_DK, 0);
                {                               /* a ring a voice, in dashes turning (the layers: faster as they detune) */
                    int32_t rr = 12 + k * 15;
                    uint32_t q;
                    uint16_t rc = on ? vk_c(k ? th->cyan : th->acc, 80 + w) : C_DK;
                    for (q = 0; q < 12u; q++) vk_ell(170, 56, rr, rr * 3 / 4, fx_ang * (uint32_t)(1 + k * (dt / 8 + 1)) / 2u + q * 0x15555555u, 0x0E000000u, 3, rc);
                }
            }
            if (nv && on_s && (P[P_GMODE] || P[P_GFRZ] || P[P_STR] < 98 || P[P_STR] > 102))
                cv_text_fit(8, 96, 226, "SAMPLE: LAYERS AT 1.00x ONLY", th->haz, 1);
            break;
        }
        {   /* the oscillator as a wavetable seen from the side: the cycle in front, and behind it the same wave with less
               and less COLOR (what turning COLOR does), each further back fainter. The front is the same 109 points as
               ever (the noise waves take their dice from the patch's); behind a noise wave, those points again */
            int32_t wv[109], nz = P[P_WAVE] == 6 || P[P_WAVE] == W_CNSE, h;
            for (i = 0; i <= 108; i++) wv[i] = wave_shape((uint32_t)P[P_WAVE], (uint32_t)i * 39768216u, P[P_COLOR] * 327);
            for (h = 5; h >= 1; h--) {
                int32_t ch = clampi(P[P_COLOR] - h * 16, 0, 100), x0 = 8 + h * 3, w = 100 - h * 3, y0 = 66 - h * 8, amp = 24 - h * 2;
                uint16_t c = vk_c(co, 190 - h * 28);
                for (i = 0; i <= 108; i += 3) {
                    int32_t v = nz ? wv[(i + h * 17) % 109] : wave_shape((uint32_t)P[P_WAVE], (uint32_t)i * 39768216u, ch * 327);
                    int32_t x = x0 + i * w / 108, y = y0 - clampi(v, -36000, 36000) * amp / 32768;
                    if (i) cv_line(px, py, x, y, c);
                    px = x; py = y;
                }
            }
            vk_dash(8, 66, 23, 26, vk_c(co, 90)); vk_dash(108, 66, 120, 26, vk_c(co, 90));   /* (the table's sides) */
            for (i = 0; i <= 108; i++) {
                int32_t x = 8 + i * 100 / 108, y = 66 - clampi(wv[i], -36000, 36000) * 28 / 32768;
                if (i) cv_line(px, py, x, y, co);
                px = x; py = y;
            }
        }
        vk_stems(cs, 124, 110, 24, 68);
        if (on_s && tm_awake && !P[P_GMODE]) cv_rect(124 + (int32_t)tm_play * 110 / 32768, 18, 1, 80, C_TXT);
        cv_text_fit(8, 0, 108, E_WAVE[P[P_WAVE]], co, 0);
        slot_name(t, (uint32_t)P[P_SLOT]);
        s[0] = (char)('1' + P[P_SLOT]); s[1] = ' '; s[2] = 0; cat(s, t);
        cv_text_fit(124, 0, 110, s, cs, 0);
        cv_rect(6, 105, 228, 1, C_DK);          /* the blend: a line, where it splits */
        if (md == 2) { k = 234 - P[P_BLEND] * 228 / 100; cv_rect(6, 104, k - 6, 2, th->acc); cv_rect(k, 104, 234 - k, 2, th->cyan); vk_circ(k, 105, 3, C_TXT); }
        else cv_rect(md ? 120 : 6, 104, 114, 2, md ? th->cyan : th->acc);
        if (rec_state) {
            s[0] = 0; cat(s, rec_state == REC_ARMED ? "ARMED" : rec_state == REC_USB ? "USB " : "REC ");
            if (rec_state != REC_ARMED) { fmt_dec(t, (int32_t)(rec_n * 100u / 22050u), 100u, 1u, 0); cat(s, t); cat(s, "s"); }
            cv_rect(150, 84, 84, 16, RGB(120, 16, 30));
            cv_text_fit(152, 84, 80, s, C_TXT, 1);
        }
        break;
    }
    case PG_ENV: if (sub == 2u) {               /* C: the MOD ENV, up or down from the middle, its target */
        uint32_t am = T_ATT_MS[P[P_MATK]], dm = T_DEC_MS[P[P_MDEC]], tot = (am + dm) * 44u + 2u, tg = TGT[clampi(P[P_MTGT], 0, NTGT - 1)];
        int32_t amt = P[P_MAMT], px = 0, py = 0, mid = amt > 0 ? 96 : amt < 0 ? 28 : 62;
        cv_rect(8, mid, 224, 1, C_DK);
        for (i = 0; i <= 112; i++) {
            int32_t y = mid - menv_at((uint32_t)i * (tot / 112u), am, dm) * (amt ? (amt > 0 ? 68 : -68) : 30) / 32768 * (amt ? absi(amt) + 20 : 100) / 120;
            if (i) cv_line(8 + px, py, 8 + i * 2, y, amt ? th->mut : C_DK);
            px = i * 2; py = y;
        }
        if (amt && tm_awake && ns_t < tot) cv_rect(8 + (int32_t)(ns_t / (tot / 224u + 1u)), 20, 1, 84, C_TXT);
        fmt_num(t, PMOD[tg] + 1, 0); s[0] = 0; cat(s, "MOD ENV > "); cat(s, t); cat(s, " "); cat(s, PD[tg].name);
        cv_text_fit(8, 0, 160, s, amt ? th->mut : C_MUTE, 0);
        fmt_param(tg, E[tg], t); cv_text_fit(170, 0, 64, t, amt ? C_TXT : C_MUTE, 2);
        break;
    } else {                                    /* ADSR and where the voice is on it */
        int32_t a = 6 + P[P_ATT] * 50 / 63, d = a + 6 + P[P_DEC] * 50 / 63, r0 = 170, r1 = r0 + 6 + P[P_REL] * 56 / 63;
        int32_t top = 22, bot = 104, sy = bot - P[P_SUS] * (bot - top) / 100, ey;
        {                                       /* the stage the voice is in: bright; the corners: small rings */
            int32_t st = !tm_awake ? -1 : tm_stage == ST_A ? 0 : tm_stage == ST_R ? 3 : tm_env <= P[P_SUS] * 328 + 400 ? 2 : 1;
            uint16_t cd = vk_c(th->acc, 150);
            cv_line(6, bot, a, top, st == 0 ? C_TXT : cd); cv_line(a, top, d, sy, st == 1 ? C_TXT : cd);
            cv_line(d, sy, r0, sy, st == 2 ? C_TXT : cd); cv_line(r0, sy, r1, bot, st == 3 ? C_TXT : cd);
            vk_circ(a, top, 2, th->acc); vk_circ(d, sy, 2, th->acc); vk_circ(r0, sy, 2, th->acc);
            vk_dash(6, bot + 2, 232, bot + 2, C_DK);
        }
        cv_text(8, 0, &FONT_S, "ADSR", th->acc);
        fmt_param(P_GLIDE, P[P_GLIDE], t); s[0] = 0; cat(s, "GLIDE "); cat(s, t); cv_text_fit(110, 0, 124, s, C_MUTE, 2);
        if (tm_awake) {
            ey = bot - tm_env * (bot - top) / 32768;
            k = tm_stage == ST_A ? a : tm_stage == ST_D ? d + (r0 - d) / 2 : r0 + (r1 - r0) / 2;
            vk_circ(k, ey, 3 + tm_env * 6 / 32768, th->cyan); cv_pset(k, ey, C_TXT);
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
            cv_rect(6 + x, y, 4, 106 - y, mode ? vk_c(th->cyan, 64) : RGB(26, 26, 30));   /* (filled, as a filter box draws it) */
            if (i) cv_line(6 + px, py, 6 + x, y, mode ? th->cyan : C_MUTE);
            px = x; py = y;
        }
        if (mode) cv_rect(6 + cut, 16, 1, 92, th->acc);
        cv_text(8, 0, &FONT_S, P[P_RFPOLE] ? "ZDF 24dB SVF" : "ZDF 12dB SVF", th->acc);
        fmt_hz(s, T_HZ[clampi(li, 0, 1023)]); cv_text_fit(150, 0, 84, s, th->cyan, 2);
        break;
    }
    case PG_GRN:                                /* the grains (rings: the motion layer) over the sample's outline, POS,
                                                   the spray's edges */
        vk_smp(vk_c(th->mut, 120), 6, 228, 18, 88);
        k = 6 + P[P_GPOS] * 228 / 100; i = P[P_GSPRAY] * 228 / 100;
        if (i) { int32_t a = k - i / 2 < 6 ? 6 : k - i / 2, b = k + i / 2 > 234 ? 234 : k + i / 2; vk_dash(a, 18, a, 106, th->mut); vk_dash(b, 18, b, 106, th->mut); }
        cv_rect(k, 16, 1, 92, th->acc);
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
        for (k = 0; k < 3; k++) {               /* a solid a band (LOW a cylinder, MID a prism, HIGH a pyramid): as tall
                                                   as the band is loud; the gain the compressor gives under it */
            static const char *const BN[3] = {"LOW", "MID", "HIGH"};
            int32_t cx = 46 + k * 74, gd = on ? tm_gd[k] : 0, lv = clampi((tm_benv[k] + 60 * 256) * 56 / (60 * 256), 2, 56);
            int32_t muted = P[P_OSOLO] && P[P_OSOLO] != k + 1;
            uint16_t c = muted ? C_DK : k == 0 ? th->cyan : k == 1 ? th->acc : C_TXT;
            solid(k, cx, 78, lv + 4, c, mix565(RGB(0, 0, 0), c, 90));
            if (absi(gd) >= 256) {              /* an arrow: up the gain it gives, down the gain it takes */
                int32_t al = clampi(absi(gd) / 128, 6, 22), ay = 74 - lv / 2, d = gd > 0 ? -1 : 1;
                cv_line(cx + 24, ay, cx + 24, ay + d * al, c);
                cv_line(cx + 21, ay + d * (al - 3), cx + 24, ay + d * al, c); cv_line(cx + 27, ay + d * (al - 3), cx + 24, ay + d * al, c);
            }
            cv_text_fit(cx - 34, 82, 68, BN[k], c, 1);
            if (absi(gd) >= 10 * 256) fmt_num(s, gd / 256, 1); else fmt_dec(s, gd * 10 / 256, 10u, 1u, 1);
            cat(s, "dB");
            cv_text_fit(cx - 34, 96, 68, muted ? "SOLO" : s, muted ? C_DK : C_MUTE, 1);
        }
        (void)B;
        break;
    }
    case PG_DLY: {                              /* the repeats over 2 s, the flanger's comb */
        int32_t tms = T_DTIME_MS[P[P_DTIME]], h = 60 * P[P_DMIX] / 100 + 6, x, comb;
        cv_text_fit(8, 0, 140, E_ROUTE[P[P_DROUTE]], th->acc, 0);
        fmt_ms(t, (uint32_t)tms); cv_text_fit(150, 0, 84, t, th->cyan, 2);
        for (x = tms, k = 0; x <= 2000 && k < 40 && h > 1; x += tms, k++) {
            int32_t px = 8 + x * 224 / 2000;
            cv_rect(px, 70 - h, 1, h, th->cyan); vk_circ(px, 70 - h, 2, th->cyan);
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
        uint32_t mid = T_RDEC_MS[P[P_RDEC]], tt[3];
        tt[0] = mid * (uint32_t)P[P_RLOW] / 100u; tt[1] = mid; tt[2] = mid * (uint32_t)P[P_RHIGH] / 100u;
        cv_text(8, 0, &FONT_S, "FDN x4 // 3-BAND", th->acc);
        for (k = 0; k < 3; k++) {               /* (the room: the motion layer) each band's decay */
            static const char *const BS[3] = {"LO ", "MID ", "HI "};
            s[0] = 0; cat(s, BS[k]); fmt_ms(t, tt[k]); cat(s, t);
            cv_text_fit(4 + k * 78, 92, 76, s, k == 1 ? C_TXT : k ? th->acc : th->cyan, 1);
        }
        k = meter_px(tm_rv, 220);
        cv_rect(8, 110, 224, 1, C_DK); cv_rect(8, 110, k, 1, th->cyan);
        break;
    }
    case PG_LFO: {                              /* the two LFOs, their targets */
        uint32_t l;
        for (l = 0; l < 2u; l++) {
            int32_t y0 = 6 + (int32_t)l * 52, shp = P[l ? P_L2SHP : P_L1SHP], mix = P[l ? P_L2MIX : P_L1MIX], px = 0, py = 0;
            uint32_t tg = TGT[P[l ? P_L2TGT : P_L1TGT]];
            uint16_t c = mix ? (l ? th->cyan : th->acc) : C_DK;
            for (i = 0; i <= 24; i++) {         /* its shape, small, beside its target (the pen: the motion layer) */
                uint32_t ph = (uint32_t)i * 178956970u;
                int32_t t2 = (int32_t)(ph >> 16), v;
                v = shp == 0 ? isin(ph) : shp == 1 ? (t2 < 32768 ? t2 * 2 : (65535 - t2) * 2) - 32768 : shp == 2 ? t2 - 32768
                  : shp == 3 ? 32767 - t2 : shp == 4 ? (t2 < 32768 ? 30000 : -30000) : ((i / 3) * 7919 % 60000) - 30000;
                v = y0 + 16 - v * 6 / 32768;
                if (i) cv_line(112 + px, py, 112 + i, v, c);
                px = i; py = v;
            }
            cv_rect(112 + 24 * (int32_t)(tm_lfo[l] + 32768) / 65536, y0 + 8, 1, 17, mix ? C_TXT : C_DK);
            s[0] = 'L'; s[1] = (char)('1' + l); s[2] = ' '; s[3] = 0; cat(s, PD[tg].name);
            cv_text_fit(140, y0 + 4, 94, s, c, 0);
            fmt_param(tg, E[tg], t);
            s[0] = 0; cat(s, "= "); cat(s, t);
            cv_text_fit(140, y0 + 24, 94, s, mix ? C_TXT : C_DK, 0);
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
            h = (int32_t)ZAMP[mat][i] * 6 / 32768 + 1;   /* (the body: the motion layer) each mode a tick on a log axis */
            z = clampi(tm_z[i] * 8 / 6000, 0, 8);
            cv_rect(x, 110 - h, 1, h, on ? th->acc : C_DK);
            if (on && z) cv_rect(x - 1, 108 - h - z, 3, 2, th->cyan);
        }
        cv_rect(6, 110, 228, 1, C_DK);
        break;
    }
    case PG_TRN: {                              /* a hit's envelope, and the same shaped; the gain now */
        int32_t px = 0, py = 0, qy = 0, ga = P[P_TATK], gs = P[P_TSUS], gl;
        cv_text(8, 0, &FONT_S, "DRY", C_MUTE); cv_text(44, 0, &FONT_S, "SHAPED", ga || gs ? th->cyan : C_DK);
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
        vk_ell(208, 70, 22, 22, 0x80000000u, 0x80000000u, 16, C_DK);   /* a gauge: the gain now, its needle */
        { uint32_t q = 0xC0000000u + (uint32_t)(gl * 0x01000000); cv_line(208, 70, 208 + (20 * isin(q + 0x40000000u) >> 15), 70 + (20 * isin(q) >> 15), gl >= 0 ? th->acc : th->cyan); }
        vk_circ(208, 70, 2, C_TXT);
        fmt_dec(s, tm_tg * 10 / 256, 10u, 1u, 1); cv_text_fit(166, 0, 70, s, C_MUTE, 2);
        break;
    }
    case PG_DST: {                              /* the shaper's curve | a cycle through bits and rate */
        int32_t px = 0, py = 0, drv = (int32_t)T_DB2LIN[clampi(P[P_XDRV] * 2 + 288, 0, 480)] >> 4, on = P[P_XMIX] != 0;
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
        vk_crop(6, 18, 105, 85, C_MUTE);        /* (the ball through it: the motion layer) */
        s[0] = 0; fmt_param(P_XBITS, P[P_XBITS], t); cat(s, t); cat(s, " "); fmt_param(P_XSR, P[P_XSR], t); cat(s, t);
        cv_text_fit(122, 0, 112, s, th->cyan, 2);
        if (P[P_XRMIX]) { fmt_param(P_XRING, P[P_XRING], t); s[0] = 0; cat(s, "RING "); cat(s, t); cv_text_fit(122, 96, 112, s, th->mut, 2); }
        break;
    }
    case PG_VOX: {
        if (sub == 1u) {                        /* B: the last note's partials (grey) and where SHIFT puts them; the echoes */
            int32_t f0 = (int32_t)((440u * exp2_q16(((int32_t)last_note - 69) * 256 / 12)) >> 16), sh = P[P_SHIFT] * P[P_SHIFT] / 5, h, j, fb = P[P_SFB];
            int32_t on = P[P_SMIX] != 0;
            if (P[P_SHIFT] < 0) sh = -sh;
            cv_rect(8, 54, 224, 1, C_DK);
            for (h = 1; h <= 10; h++) {         /* 0 .. 4 kHz */
                int32_t fi = f0 * h, fo = absi(fi + sh);
                if (fi < 4000) cv_rect(8 + fi * 224 / 4000, 42, 2, 12, C_MUTE);
                if (fo < 4000) cv_rect(8 + fo * 224 / 4000, 24, 2, 30, on ? th->cyan : C_DK);
            }
            s[0] = 0; cat(s, "IN "); fmt_hz(t, (uint32_t)f0); cat(s, t); cat(s, "  OUT "); fmt_hz(t, (uint32_t)absi(f0 + sh)); cat(s, t);
            cv_text_fit(8, 0, 226, s, on ? C_TXT : C_MUTE, 0);
            cv_text_fit(8, 56, 226, "0", C_MUTE, 0); cv_text_fit(8, 56, 226, "4kHz", C_MUTE, 2);
            for (j = 0; j < 16; j++) {          /* the feedback: an echo every S TIME, each FDBK of the last */
                int32_t x = 8 + j * clampi(P[P_STIME] * 2, 6, 60), hh = 28;
                int32_t q;
                if (x > 230) break;
                for (q = 0; q < j; q++) hh = hh * fb / 100;
                if (hh < 1 && j) break;
                cv_rect(x, 106 - hh, 3, hh ? hh : 1, j ? (on && fb ? th->mut : C_DK) : (on ? th->acc : C_DK));
            }
            break;
        }
        if (sub == 2u) {                        /* C: the growl, three of its cycles */
            int32_t dep = P[P_GROWL] * 327, px = 0, py = 0;
            for (i = 0; i <= 224; i++) {
                int32_t m = 16384 + (isin((uint32_t)i * 57521883u) >> 1), y;
                m = m * m >> 15;
                y = 32768 - (((32768 - m) * dep) >> 15);
                y = 104 - y * 34 / 32768;
                if (i) cv_line(8 + px, py, 8 + i, y, dep ? th->mut : C_DK);
                px = i; py = y;
            }
            fmt_num(t, P[P_GRATE], 0); s[0] = 0; cat(s, "GROWL "); cat(s, t); cat(s, "Hz, UNSTEADY");
            cv_text_fit(8, 0, 226, s, dep ? C_TXT : C_MUTE, 0);
            break;
        }
        {                                       /* A: the vowels, where VOWEL is; the three formants' response */
            static const char LT[5][2] = {"U", "O", "A", "E", "I"};
            int32_t pos = clampi(E[P_VOWEL], 0, 100) * 4, on = P[P_VMIX] != 0, q = 10 * 65536 / (K.vk ? K.vk : 1), px = 0, py = 0;
            for (k = 0; k < 5; k++) {
                int32_t near = absi(pos - k * 100) < 50;
                cv_text(18 + k * 48, 0, &FONT_S, LT[k], near ? (on ? th->acc : C_TXT) : C_MUTE);
            }
            cv_rect(8, 18, 224, 1, C_DK);
            cv_rect(20 + pos * 192 / 400, 16, 5, 5, on ? th->acc : C_MUTE);
            for (i = 0; i <= 224; i++) {        /* 100 Hz .. 5 kHz */
                static const int32_t VG2[3] = {58, 38, 24};
                int32_t li = 240 + i * 579 / 224, m = 0, f, y;
                for (f = 0; f < 3; f++) {
                    int32_t x = clampi(absi(li - K.vli[f]) * q * 3457 / 10000, 0, 60000);   /* Q (f/f0 - f0/f), Q8 */
                    m += VG2[f] * 65536 / isqrt32((uint32_t)(65536 + x * x)) ;
                }
                y = 104 - clampi(m * 36 / 16000, 0, 38);   /* (low: the mouth above it) */
                if (i) cv_line(8 + px, py, 8 + i, y, on ? th->cyan : C_DK);
                px = i; py = y;
            }
            if (!on) cv_text_fit(8, 92, 226, "V MIX 0: DRY", C_MUTE, 2);
        }
        break;
    }
    case PG_LAB: {                              /* the slot, what the TOOL will do to it */
        static const char *const D[9] = {"PLAYS BACKWARDS", "KEEPS THE LOOP REGION", "FADES IN OVER AMOUNT", "FADES OUT OVER AMOUNT",
                                         "TWICE AS LONG, -12st", "HALF AS LONG, +12st", "PEAK TO FULL LEVEL", "BACK TO THE SAVED SLOT",
                                         "THE FACTORY SOUND BACK"};
        int32_t tool = P[P_STOOL] % 9, len = (int32_t)smp_len > 0 ? (int32_t)smp_len : 1;   /* (the ring, the sign: the motion layer) */
        slot_name(t, (uint32_t)P[P_SLOT]); s[0] = (char)('1' + P[P_SLOT]); s[1] = ' '; s[2] = 0; cat(s, t);
        cv_text_fit(8, 0, 120, s, SL[cur_slot].dirty ? th->haz : th->acc, 0);
        fmt_dec(t, len * 100 / 22050, 100u, 2u, 0); cat(t, "s"); cv_text_fit(150, 0, 84, t, C_MUTE, 2);
        cv_text_fit(8, 94, 226, D[tool], C_TXT, 1);
        break;
    }
    case PG_PHY: {                              /* the pattern: each hit when, how loud; where it is */
        uint32_t q;
        int32_t on = P[P_PMODE] != 0;
        if (!ph_on) phys_build(0);
        s[0] = 0; cat(s, E_PMODE[P[P_PMODE] % 6]);
        cv_text_fit(8, 0, 110, s, on ? th->acc : C_MUTE, 0);
        fmt_num(t, P[P_PHITS], 0); s[0] = 0; cat(s, t); cat(s, P[P_PMODE] == 5 ? " EVERY " : " IN "); fmt_param(P_PTIME, P[P_PTIME], t); cat(s, t);
        cv_text_fit(120, 0, 114, s, on ? th->cyan : C_MUTE, 2);
        cv_rect(8, 97, 224, 1, C_DK);
        for (q = 0; q < ph_n; q++) {            /* each hit a stem and a head, as loud as it is; played: the other colour */
            int32_t x = 10 + (int32_t)ph_t[q] * 220 / (int32_t)(ph_len ? ph_len : 1u), h = 6 + ph_l[q] * 54 / 255;
            uint16_t c = !on ? C_DK : ph_on && q < ph_i ? th->cyan : th->acc;
            vk_dash(x, 96, x, 97 - h, c); vk_circ(x, 97 - h, 2 + ph_l[q] / 100, c);
            if (on && ph_p[q]) cv_rect(x - 2, clampi(97 - h - 7 - ph_p[q], 17, 95), 5, 1, th->mut);   /* (its pitch, up or down) */
        }
        if (ph_on) {                            /* the ball, hopping from hit to hit; where the pattern is */
            uint32_t e = fm1_ms - ph_t0, L = ph_len ? ph_len : 1u;
            if (e < ph_len) {
                cv_rect(10 + (int32_t)(e * 220u / L), 18, 1, 78, C_DK);
                if (ph_i > 0u && ph_i < ph_n) {
                    int32_t ta = ph_t[ph_i - 1u], tb = ph_t[ph_i], f = clampi(((int32_t)e - ta) * 256 / (tb - ta > 0 ? tb - ta : 1), 0, 256);
                    int32_t xa = 10 + ta * 220 / (int32_t)L, xb = 10 + tb * 220 / (int32_t)L, hg = 12 + ph_l[ph_i] * 50 / 255;
                    vk_circ(xa + (xb - xa) * f / 256, 91 - hg * 4 * f / 256 * (256 - f) / 256, 4, C_TXT);
                }
            }
        }
        if (!on) cv_text_fit(8, 18, 226, "A KEY PLAYS ONE HIT", C_MUTE, 1);
        break;
    }
    case PG_ASSET: {                            /* A: the assets, four to a row; the one chosen, the one loaded. B, C: the chip */
        uint32_t q, sel = (uint32_t)P[P_ASSET] % NASSET, ed = asset_cur != 255u && patch_sum() != asset_sum;
        s[0] = 0;
        if (asset_cur == 255u) cat(s, "SEL: LOAD AN ASSET");
        else { cat(s, E_ASSET[asset_cur]); cat(s, ed ? "  EDITED" : "  LOADED"); }
        if (sub) { vis_chips(th); break; }
        cv_text_fit(8, 0, 226, s, asset_cur == 255u ? C_MUTE : ed ? th->haz : th->cyan, 0);
        for (q = 0; q < NASSET; q++) {
            int32_t x = 4 + (int32_t)(q % 4u) * 58, y = 18 + (int32_t)(q / 4u) * 18, w = 56, tw;
            int here = q == sel, live = q == asset_cur;
            if (here) cv_rect(x, y, w, 14, th->acc);
            else if (live) cv_frame(x, y, w, 14, th->cyan);
            else cv_rect(x, y, w, 14, EVA ? EV_BG : th->box);
            tw = text_w(&FONT_A, E_ASSET[q]);
            cv_text(x + (w - tw) / 2, y + 3 - FONT_A_CAP, &FONT_A, E_ASSET[q], here ? RGB(0, 0, 0) : live ? th->cyan : C_TXT);
        }
        break;
    }
    case PG_VAR: {                              /* A: the seed, the 27 keys as its variations; B: A and B, the morph */
        uint32_t last = lvl_v[lvl_n & 15u], q;
        if (sub == 1u) {
            uint32_t nd = 0;
            int32_t x = 54 + clampi(E[P_MORPH], 0, 100) * 132 / 100;
            for (q = 0; q < NP; q++) if (!NOMORPH[q] && snapA[q] != snapB[q]) nd++;
            if (has_ab == 3u) { fmt_num(t, (int32_t)nd, 0); s[0] = 0; cat(s, t); cat(s, " PARAMS DIFFER"); }
            else { s[0] = 0; cat(s, has_ab ? "ONE MORE TO STORE" : "STORE A AND B TO MORPH"); }
            cv_text_fit(8, 0, 226, s, has_ab == 3u ? C_TXT : C_MUTE, 1);
            cv_text(2, 46, &FONT_S, "A", has_ab & 1u ? th->acc : C_MUTE);   /* (A a cube, B an octahedron, the mix
                                                   between: the motion layer) */
            cv_text(230, 46, &FONT_S, "B", has_ab & 2u ? th->cyan : C_MUTE);
            vk_dash(54, 37, 186, 37, C_DK);
            cv_rect(54, 37, x - 54, 1, morph_on ? th->mut : C_MUTE);
            cv_text_fit(8, 60, 226, has_ab == 3u ? "SEL: STORE THE NEARER ONE" : has_ab ? "SEL: STORE B" : "SEL: STORE A, THEN B",
                        has_ab == 3u ? C_MUTE : has_ab ? th->cyan : th->acc, 1);
            cv_text_fit(8, 76, 226, "TURN MORPH: A TO B", has_ab == 3u ? th->mut : C_MUTE, 1);
            if (morph_on) cv_text_fit(8, 94, 226, "MORPH ON: KNOBS SHOW THE MIX", th->mut, 1);
            break;
        }
        cv_text_fit(8, 2, 100, "SEED", C_MUTE, 0);
        fmt_num(t, P[P_VSEED], 0); if (EVA) cv_text(8, 20, &FONT_D, t, th->acc); else cv_text_x2(8, 16, t, th->acc);
        cv_text_fit(110, 2, 124, P[P_VLOCK] ? "NEW AFTER BATCH" : "SEED KEPT", P[P_VLOCK] ? th->haz : th->cyan, 2);
        s[0] = 0; cat(s, "VARY "); fmt_num(t, P[P_VARY], 0); cat(s, t); cat(s, "%");
        cv_text_fit(110, 22, 124, s, P[P_VARY] ? C_TXT : C_MUTE, 2);
        for (q = 0; q < 27u; q++) {             /* the keys: their variations (the one last played lit) */
            int32_t x = 8 + (int32_t)(q % 14u) * 16, y = 54 + (int32_t)(q / 14u) * 18;
            if ((q + 1u) == last && P[P_VKEYS]) cv_rect(x, y, 14, 14, th->mut);
            else cv_frame(x, y, 14, 14, P[P_VKEYS] ? vk_c(th->mut, 110) : C_DK);
        }
        cv_text_fit(8, 94, 226, P[P_VKEYS] ? "EACH KEY: A VARIATION" : "KEYS PLAY NOTES", P[P_VKEYS] ? th->mut : C_MUTE, 1);
        break;
    }
    case PG_BAT: {                              /* the hits so far (BATCH) or the shots (FIRE TEST), each one's level */
        uint32_t q, nh = lvl_n < 16u ? lvl_n : 16u, mx = 1, mn = 0xFFFFFFFFu;
        int32_t fs = (int32_t)sn_lg2(32768u), tq = fs + P[P_LTGT] * 128000 / 6021;
        if (P[P_LMATCH] || sub == 2u) {         /* MATCH's TARGET across the bars */
            int32_t ty = 58 - clampi((tq - fs + 1531) * 38 / 1531, 1, 38);
            for (q = 0; q < 224u; q += 4u) cv_rect(8 + (int32_t)q, ty, 2, 1, th->haz);
        }
        for (q = 0; q < nh; q++) { uint32_t h = lvl_h[(lvl_n - 1u - q) & 15u]; if (h > mx) mx = h; if (h < mn) mn = h; }
        for (q = 0; q < nh; q++) {              /* the oldest left; each bar its peak, the variation under it */
            uint32_t ix = (lvl_n - nh + q) & 15u, h = lvl_h[ix];
            int32_t bh = clampi(((int32_t)sn_lg2(h > 1u ? h : 1u) - fs + 1531) * 38 / 1531, 1, 38);   /* -36 .. 0 dBFS */
            cv_rect(14 + (int32_t)q * 14, 58 - bh, 1, bh, ft_on ? th->cyan : th->acc);   /* (a stem, a cap) */
            cv_rect(10 + (int32_t)q * 14, 58 - bh, 9, 1, ft_on ? th->cyan : th->acc);
            if (lvl_v[ix]) {                    /* (10 and up: the small font, two digits in the bar's 13 px) */
                fmt_num(t, lvl_v[ix], 0);
                if (lvl_v[ix] < 10u) cv_text_fit(8 + (int32_t)q * 14, 60, 13, t, C_MUTE, 0);
                else cv_text(8 + (int32_t)q * 14 + (13 - text_w(&FONT_A, t)) / 2, 60 + TER_BASE - FONT_A_BASE, &FONT_A, t, C_MUTE);
            }
        }
        if (nh >= 2u && mn > 0u) {              /* the spread: loudest over quietest, in dB */
            int32_t db10 = (int32_t)((sn_lg2(mx) - sn_lg2(mn)) * 60206u / 256u / 1000u);
            fmt_dec(t, db10, 10u, 1u, 0); s[0] = 0; cat(s, "SPREAD "); cat(s, t); cat(s, "dB");
            cv_text_fit(8, 0, 226, s, db10 > 30 ? th->haz : C_TXT, 1);
        } else cv_text_fit(8, 0, 226, "PEAK OF EACH HIT", C_MUTE, 1);
        for (q = nh; q < 16u; q++) cv_rect(8 + (int32_t)q * 14, 57, 12, 1, C_DK);   /* (the hits to come) */
        if (ft_on) {
            fmt_num(t, (int32_t)ft_hits, 0); s[0] = 0; cat(s, "FIRING "); cat(s, t); cat(s, "  ");
            fmt_param(P_TRATE, P[P_TRATE], t); cat(s, t);
            cv_text_fit(8, 78, 226, s, th->cyan, 1);
        } else if (batch_on) {
            fmt_num(t, (int32_t)batch_i, 0); s[0] = 0; cat(s, "BATCH "); cat(s, t); cat(s, "/"); fmt_num(t, (int32_t)batch_n, 0); cat(s, t);
            cv_text_fit(8, 78, 226, s, th->mut, 1);
        } else if (sub == 2u) {                 /* the hits' mean peak against TARGET */
            int32_t m = lvl_mean_q8();
            s[0] = 0; cat(s, "MEAN ");
            if (m < 0) cat(s, "--"); else { fmt_dec(t, (m - fs) * 602 / 256 / 10, 10u, 1u, 1); cat(s, t); cat(s, "dB"); }
            cat(s, P[P_LMATCH] ? "  MATCH ON" : "  MATCH OFF");
            cv_text_fit(8, 78, 226, s, P[P_LMATCH] ? th->haz : C_MUTE, 1);
        } else if (sub == 1u) {
            s[0] = 0; cat(s, "FIRE x"); fmt_num(t, P[P_TPOOL], 0); cat(s, t); cat(s, " "); cat(s, E_TORDER[P[P_TORDER] % 3]);
            cv_text_fit(8, 78, 226, s, C_TXT, 1);
        } else {
            s[0] = 0; cat(s, "BATCH x"); cat(s, E_BCNT[P[P_BCNT] & 3]); cat(s, " "); cat(s, E_BMODE[P[P_BMODE]]);
            cv_text_fit(8, 78, 226, s, C_TXT, 1);
        }
        fmt_num(t, P[P_VSEED], 0); s[0] = 0; cat(s, "SEED "); cat(s, t); cat(s, "  VARY "); fmt_num(t, P[P_VARY], 0); cat(s, t); cat(s, "%");
        cv_text_fit(8, 96, 226, s, C_MUTE, 1);
        break;
    }
    case PG_RTPC: {                             /* the curve, where the value is on it, the three targets */
        int32_t c = (int32_t)tm_rtpc, px = 0, py = 0, x0 = 8, y0 = 6, w = 100, h = 96;
        cv_rect(x0, y0 + h, w, 1, C_DK); cv_rect(x0, y0, 1, h, C_DK);
        for (i = 0; i <= w; i++) {              /* the curve, as rtpc_block bends it */
            int32_t r = i * 100, cc, y;
            switch (P[P_QCRV]) {
            case 1: cc = r * r / 10000; break;
            case 2: cc = 10000 - (10000 - r) * (10000 - r) / 10000; break;
            case 3: cc = r * r / 10000 * (30000 - 2 * r) / 10000; break;
            default: cc = r;
            }
            y = y0 + h - cc * h / 10000;
            if (i) cv_line(x0 + px, py, x0 + i, y, th->acc);
            px = i; py = y;
        }
        {
            int32_t r = rt_on ? (int32_t)rt_cur : E[P_QINT] * 100, cx = x0 + r * w / 10000, cy = y0 + h - c * h / 10000;
            cv_rect(cx, y0, 1, h, rt_on ? th->mut : C_DK);
            cv_rect(cx - 3, cy - 3, 7, 7, rt_on || E[P_QINT] ? C_TXT : C_MUTE);
        }
        for (k = 0; k < 3; k++) {               /* the targets and how far each one goes */
            uint32_t tp = (uint32_t)(P_Q1T + k * 2), ap = tp + 1u;
            uint32_t tg = TGT[clampi(P[tp], 0, NTGT - 1)];
            int32_t a = P[ap], y = 4 + k * 30;
            fmt_num(t, PMOD[tg] + 1, 0); s[0] = 0; cat(s, t); cat(s, " "); cat(s, PD[tg].name);
            cv_text_fit(116, y, 118, s, a ? C_TXT : C_MUTE, 0);
            cv_rect(116, y + 18, 118, 4, C_DK);
            if (a) { int32_t bw = absi(a) * 59 / 100; cv_rect(a > 0 ? 175 : 175 - bw, y + 18, bw, 4, a > 0 ? th->cyan : th->haz); }
        }
        s[0] = 0; cat(s, rt_on ? "SWEEP " : "VALUE "); fmt_num(t, c / 100, 0); cat(s, t); cat(s, "%");
        cv_text_fit(116, 94, 118, s, rt_on ? th->mut : c ? C_TXT : C_MUTE, 0);
        break;
    }
    case PG_SPC: {                              /* from above: the listener, rings of distance, the source, the wall */
        int32_t lx = 60, ly = 100, d = E[P_SDIST], o = E[P_SOCCL], r = 22 + d * 62 / 100, sx = lx, sy = ly - r, db10;
        for (k = 1; k <= 3; k++) {              /* near, mid, far */
            uint32_t a;
            for (a = 0; a <= 180u; a += 6u) {
                uint32_t ph = a * 11930465u;
                int32_t x = lx - (isin(ph + 0x40000000u) * (k * 28) >> 15), y = ly - (isin(ph) * (k * 28) >> 15);
                if (x >= 4 && x <= 116 && y >= 4) cv_rect(x, y, 1, 1, C_DK);
            }
        }
        if (P[P_SPASS]) {                       /* the path across, the source on it */
            cv_rect(6, sy, 110, 1, C_MUTE);
            sx = lx + clampi(tm_px, -1024, 1024) * 54 / 1024;
        }
        if (o) cv_rect(lx - 34, ly - r / 2 - 1, 69, 2 + o / 25, th->haz);   /* (the wall between) */
        vk_circ(lx, ly, 4, C_TXT); cv_line(lx, ly - 4, lx, ly - 7, C_TXT);   /* (the listener, looking up; the source and its
                                                   wave fronts: the motion layer) */
        vk_circ(sx, sy, 4, K.sp_on ? th->cyan : C_MUTE); cv_pset(sx, sy, K.sp_on ? th->cyan : C_MUTE);
        db10 = tm_sg > 0 ? -(int32_t)((sn_lg2(32768u) - sn_lg2((uint32_t)tm_sg)) * 60206u / 256u / 1000u) : -999;
        fmt_dec(t, db10, 10u, 1u, 1); s[0] = 0; cat(s, t); cat(s, "dB");
        cv_text_fit(124, 2, 110, "LEVEL", C_MUTE, 0); cv_text_fit(124, 2, 110, s, K.sp_on ? C_TXT : C_MUTE, 2);
        fmt_hz(t, (uint32_t)tm_sfc);
        cv_text_fit(124, 22, 110, "LOW-PASS", C_MUTE, 0); cv_text_fit(124, 22, 110, t, K.sp_on ? C_TXT : C_MUTE, 2);
        fmt_num(t, clampi(d * 90 / 256 + o * 38 / 256, 0, 100), 1); cat(t, "%");
        cv_text_fit(124, 42, 110, "REVERB", C_MUTE, 0); cv_text_fit(124, 42, 110, t, d || o ? C_TXT : C_MUTE, 2);
        if (P[P_SPASS]) {
            fmt_param(P_SDOPP, P[P_SDOPP], t);
            cv_text_fit(124, 62, 110, "DOPPLER", th->acc, 0); cv_text_fit(124, 62, 110, t, th->acc, 2);
            cv_rect(124, 90, 110, 1, C_DK);     /* (the pass, where it is) */
            vk_circ(124 + (clampi(tm_px, -1024, 1024) + 1024) * 110 / 2048, 90, 3, th->cyan);
        }
        break;
    }
    case PG_CHIP: {                             /* the pitch over the first 48 frames (0.8 s): SWEEP and ARP, as played */
        int32_t sw = 0, csw = P[P_CSWEEP] * (P[P_CSWEEP] < 0 ? -P[P_CSWEEP] : P[P_CSWEEP]) * 16 / 150, ai = 0, n = 0;
        int32_t ar = P[P_CARP] % NARP, prev = -1, w = P[P_WAVE] >= W_CPLS && P[P_WAVE] <= W_CNSE;
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
        cv_text_fit(116, 28, 118, w ? E_WAVE[P[P_WAVE]] : "ANY WAVE", w ? th->cyan : C_MUTE, 2);
        s[0] = 0; cat(s, "VOL "); cat(s, E_CSTEP[P[P_CSTEP] & 3]);
        cv_text_fit(116, 46, 118, s, P[P_CSTEP] ? C_TXT : C_MUTE, 2);
        cv_text_fit(116, 92, 118, "PITCH, 0.8s", C_MUTE, 2);
        break;
    }
    case PG_PAD: {                              /* the XY field, where the finger is; the routings; the keys */
        int32_t on = pad_on || pad_held, px = 10 + pad_x * 100 / 127, py = 104 - pad_y * 100 / 127;
        vk_crop(8, 2, 104, 104, C_MUTE);        /* the field by its corners, a dot each quarter; the finger's last
                                                   places (the motion layer), its cross; SPRING: the pull back */
        for (i = 1; i < 4; i++) for (k = 1; k < 4; k++) cv_rect(9 + i * 25, 3 + k * 25, 2, 2, C_DK);
        cv_line(px, 4, px, 104, on ? vk_c(th->acc, 110) : C_DK);
        cv_line(10, py, 110, py, on ? vk_c(th->cyan, 110) : C_DK);
        if (on && P[P_PADM] == 0) vk_dash(60, 54, px, py, th->mut);
        {                                       /* the GESTURE's path, where its replay is */
            uint32_t q, gn = g_n;
            for (q = 0; q < gn; q++) cv_pset(10 + g_x[q] * 100 / 127, 103 - g_y[q] * 99 / 127, P[P_GEST] ? th->mut : C_MUTE);
            if (g_act && g_pos < gn) vk_circ(10 + g_x[g_pos] * 100 / 127, 103 - g_y[g_pos] * 99 / 127, 4, th->mut);
        }
        vk_circ(px, py, on ? 5 : 3, on ? C_TXT : C_MUTE);
        for (k = 0; k < 2; k++) {
            uint32_t a = k ? P_YAMT : P_XAMT;
            s[0] = 0; cat(s, k ? "Y " : "X "); fmt_param(a - 1u, P[a - 1u], t); cat(s, t);
            cv_text_fit(120, 1 + k * 16, 114, s, !P[a] ? C_MUTE : k ? th->cyan : th->acc, 0);
        }
        s[0] = 0; cat(s, "PAD "); cat(s, E_PADM[P[P_PADM] % 3]);
        cv_text_fit(120, 34, 114, s, on ? C_TXT : C_MUTE, 0);
        s[0] = 0;
        if (g_rec) cat(s, g_rec == 1u ? "REC ARMED" : "RECORDING");
        else if (!g_n) cat(s, "NO GESTURE");
        else { cat(s, "GEST "); fmt_dec(t, (int32_t)g_n * 2, 100u, 1u, 0); cat(s, t); cat(s, "s "); cat(s, E_GEST[P[P_GEST] % 3]); }
        cv_text_fit(120, 50, 114, s, g_rec ? th->haz : g_n && P[P_GEST] ? th->mut : C_MUTE, 0);
        s[0] = 0; cat(s, "KEY VEL "); cat(s, E_KVEL[P[P_KVEL] % 3]);
        cv_text_fit(120, 66, 114, s, P[P_KVEL] ? th->mut : C_MUTE, 0);
        k = at_keys > at_midi ? at_keys : at_midi;
        cv_text_fit(120, 84, 24, "AT", P[P_ATAMT] ? C_TXT : C_MUTE, 0);
        cv_rect(146, 89, 88, 6, C_DK);
        cv_rect(146, 89, k * 88 / 127, 6, P[P_ATAMT] ? th->haz : C_MUTE);
        break;
    }
    }
    cv_blit(0, 40);
}
static uint32_t knob_ph(int32_t norm) { return ((uint32_t)norm + 225u) * 11930465u; }   /* degrees -> phase */
static int mod_src(uint32_t p)                  /* who moves the parameter now: 0 no one, 1 an LFO, 2 the RTPC, 3 pad / aftertouch */
{
    if ((P[P_L1MIX] && TGT[P[P_L1TGT]] == p) || (P[P_L2MIX] && TGT[P[P_L2TGT]] == p)) return 1;
    if ((rt_on || P[P_QINT]) && ((P[P_Q1A] && TGT[P[P_Q1T]] == p) || (P[P_Q2A] && TGT[P[P_Q2T]] == p) || (P[P_Q3A] && TGT[P[P_Q3T]] == p))) return 2;
    if ((pad_on || pad_held) && ((P[P_XAMT] && TGT[P[P_XTGT]] == p) || (P[P_YAMT] && TGT[P[P_YTGT]] == p))) return 3;
    if (P[P_ATAMT] && (at_keys || at_midi) && TGT[P[P_ATTGT]] == p) return 3;
    if (g_act && ((P[P_XAMT] && TGT[P[P_XTGT]] == p) || (P[P_YAMT] && TGT[P[P_YTGT]] == p))) return 3;
    if (P[P_MAMT] && tm_menv && TGT[P[P_MTGT]] == p) return 2;
    return 0;
}
static int mod_any(void) { return g_act || tm_menv || P[P_L1MIX] || P[P_L2MIX] || rt_on || P[P_QINT] || pad_on || pad_held || at_keys || at_midi; }
static void glyph_lock(int32_t x, int32_t y, uint16_t c) { cv_frame(x + 1, y, 5, 5, c); cv_rect(x, y + 3, 7, 5, c); }
static void glyph_undo(int32_t x, int32_t y, uint16_t c)   /* an arrow turning back */
{
    cv_rect(x + 1, y + 2, 6, 1, c); cv_rect(x + 7, y + 3, 1, 4, c); cv_rect(x + 3, y + 7, 4, 1, c);
    cv_rect(x + 1, y + 1, 1, 3, c); cv_rect(x + 2, y, 1, 5, c);
}
/* EVA-01's knob row: four cells, each its knob's colour (K1 lime, K2 green, K3 purple, K4 bone). The knob turned last
 * fills its cell for a moment, black on colour. Top left the knob (an arc of dots; a list: its positions), top right
 * K1..K4 and a bar (orange: locked, else who modulates it), the name, the value big (numbers) or medium (words) */
static int eva_numlike(uint32_t p, const char *s)   /* a number with a unit: digits big, the unit small */
{
    uint32_t n = 0;
    int lc = 0;
    if (PD[p].fmt == F_TGT || PD[p].fmt == F_SLOT || PD[p].fmt == F_VOWEL || PD[p].fmt == F_BLEND) return 0;
    if (*s == '+' || *s == '-') s++;
    if (*s < '0' || *s > '9') return 0;
    if (PD[p].fmt != F_ENUM) return 1;
    while ((*s >= '0' && *s <= '9') || *s == '.') s++;   /* a list: only "24dB", "60 Hz" (a unit after the number) */
    for (; s[n]; n++) if (s[n] >= 'a' && s[n] <= 'z') lc = 1;
    return lc && n <= 3u;
}
/* how a value fits a cell w px wide: 0 the number in D and the unit small (A, capitals: MS, HZ, DB), 1 the same with the number in M,
 * 2 a word in M (7: short enough for D), 3 a target: its page in Terminus and its name in M, 4 the name alone in M, 5 Terminus, 6 Terminus cut.
 * nb gets the number (or the target's page), *u the unit (or the target's name) */
static int eva_fits_d(uint32_t p, const char *s, int32_t w)   /* this value, all in D (a number and its unit, or a word) */
{
    uint32_t n = 0;
    char nb[16];
    if (eva_numlike(p, s)) {
        while (s[n] && n < 15u && ((s[n] >= '0' && s[n] <= '9') || s[n] == '.' || s[n] == '+' || s[n] == '-')) { nb[n] = s[n]; n++; }
        nb[n] = 0;
        return text_w(&FONT_D, nb) + 2 + text_w(&FONT_A, s + n) <= w;
    }
    return PD[p].fmt != F_TGT && text_w(&FONT_D, s) <= w;
}
/* a knob keeps one size: D only if every one of its values fits in D (no jumping between sizes while it turns).
 * Worked out the first time the knob is drawn (a few hundred values at most) */
static uint8_t eva_big[NP];                     /* 0 not yet, 1 D, 2 M */
static int eva_big_ok(uint32_t p, int32_t w)
{
    if (!eva_big[p]) {
        int32_t v;
        char t[40];
        eva_big[p] = 1;
        for (v = PD[p].lo; v <= PD[p].hi && eva_big[p] == 1; v++) { fmt_param(p, v, t); if (!eva_fits_d(p, t, w)) eva_big[p] = 2; }
    }
    return eva_big[p] == 1;
}
static int eva_plan(uint32_t p, const char *s, int32_t w, char *nb, const char **u)
{
    uint32_t n = 0;
    int big = eva_big_ok(p, w);
    *u = s; nb[0] = 0;
    if (PD[p].fmt == F_TGT) {
        while (s[n] && s[n] != ' ' && n < 7u) { nb[n] = s[n]; n++; }
        nb[n] = 0;
        *u = s[n] ? s + n + 1 : s + n;
        if (text_w(&FONT_S, nb) + 3 + text_w(&FONT_M, *u) <= w) return 3;
        if (text_w(&FONT_M, *u) <= w) return 4;
    } else if (eva_numlike(p, s)) {
        while (s[n] && n < 15u && ((s[n] >= '0' && s[n] <= '9') || s[n] == '.' || s[n] == '+' || s[n] == '-')) { nb[n] = s[n]; n++; }
        nb[n] = 0;
        *u = s + n;
        if (big && text_w(&FONT_D, nb) + 2 + text_w(&FONT_A, *u) <= w) return 0;
        if (text_w(&FONT_M, nb) + 2 + text_w(&FONT_A, *u) <= w) return 1;
    } else if (big && text_w(&FONT_D, s) <= w) return 7;   /* a short word: big, as the numbers */
    else if (text_w(&FONT_M, s) <= w) return 2;
    *u = s;
    return text_w(&FONT_S, s) <= w ? 5 : 6;
}
static void eva_value(int32_t x, int32_t yb, int32_t w, uint32_t p, const char *s, uint16_t fg, uint16_t fu)
{
    char nb[16];
    const char *u;
    int32_t e;
    switch (eva_plan(p, s, w, nb, &u)) {
    case 0: e = cv_text(x, yb - FONT_D_BASE, &FONT_D, nb, fg); cv_text(e + 2, yb - FONT_A_BASE, &FONT_A, u, fu); break;
    case 1: e = cv_text(x, yb - FONT_M_BASE, &FONT_M, nb, fg); cv_text(e + 2, yb - FONT_A_BASE, &FONT_A, u, fu); break;
    case 2: cv_text(x, yb - FONT_M_BASE, &FONT_M, s, fg); break;
    case 7: cv_text(x, yb - FONT_D_BASE, &FONT_D, s, fg); break;
    case 3: e = cv_text(x, yb - TER_BASE, &FONT_S, nb, fu); cv_text(e + 3, yb - FONT_M_BASE, &FONT_M, u, fg); break;
    case 4: cv_text(x, yb - FONT_M_BASE, &FONT_M, u, fg); break;
    default: cv_text_fit(x, yb - TER_BASE, w, s, fg, 0); break;
    }
}
/* the other themes' knob text, centred: Terminus; what is wider, in M on Terminus' baseline (no cut "TARGET") */
static void fit_text(int32_t x, int32_t y, int32_t w, const char *s, uint16_t c)
{
    int32_t tw = text_w(&FONT_S, s);
    if (tw > w && text_w(&FONT_M, s) > w && *s >= '0' && *s <= '9') {   /* a target too long: its name without the page */
        const char *q = s;
        while (*q && *q != ' ') q++;
        if (*q) s = q + 1;
        tw = text_w(&FONT_S, s);
    }
    if (tw > w && text_w(&FONT_M, s) <= w) { tw = text_w(&FONT_M, s); cv_text(x + (w - tw) / 2, y + TER_BASE - FONT_M_BASE, &FONT_M, s, c); }
    else cv_text_fit(x, y, w, s, c, 1);
}
static void eva_knobs(void)
{
    uint32_t k;
    char s[24];
    cv_begin(240, 64, EV_BG);
    cv_rect(0, 0, 240, 1, EV_MUT);
    for (k = 0; k < 4u; k++) {
        uint32_t p = PAGE_P[page][sub][k], a;
        int32_t x = (int32_t)k * 60, cx = x + 12, cy = 14, lo, rng, norm, en, nopt, xx, yy;
        int src, locked, list, act, off = 0;
        uint16_t kc, fg, sb, cell;
        char kn[3];
        if (k) cv_rect(x, 1, 1, 63, EV_LN);
        if (p == NO) {                          /* nothing here: hatched */
            for (yy = 2; yy < 64; yy++)
                for (xx = x + 2; xx < x + 60; xx++)
                    if (((xx + yy) % 7) == 0) cv_pset(xx, yy, EV_HATCH);
            continue;
        }
        lo = PD[p].lo; rng = PD[p].hi - lo ? PD[p].hi - lo : 1;
        norm = ((P[p] - lo) * 270) / rng; en = ((clampi(E[p], lo, PD[p].hi) - lo) * 270) / rng;
        src = mod_src(p); locked = lock_get(p);
        nopt = rng + 1;
        list = (PD[p].fmt == F_ENUM || PD[p].fmt == F_SLOT) && nopt <= 8;
        act = k == hot_k && fm1_ms - hot_t < 1200u;
        cell = locked ? EV_ORG : EV_KC[k];
        if (act) cv_rect(x + 1, 1, 59, 63, cell);
        fg = act ? EV_INK : locked ? EV_ORG : src ? EV_GRN : EV_TXT;
        sb = act ? EV_INK : EV_MUT;
        kc = act ? EV_INK : mutant ? EV_PUR : cell;
        if (list) {                             /* its positions, the one it is on (OFF / BYPASS: grey) */
            const char *e0 = PD[p].en ? PD[p].en[0] : "";
            int32_t q, x0 = x + 5, pitch = nopt * 6 <= 37 ? 6 : 37 / nopt;   /* (clear of K1..K4: 37 px at most) */
            off = P[p] == lo && !locked && ((e0[0] == 'O' && e0[1] == 'F') || (e0[0] == 'B' && e0[1] == 'Y'));
            for (q = 0; q < nopt; q++) {
                int32_t qx = x0 + q * pitch;
                if (q == P[p] - lo) cv_rect(qx, cy - 3, pitch - 1, 6, off && !act ? EV_MUT : kc);
                else { cv_rect(qx, cy + 2, pitch - 1, 1, act ? EV_INK : EV_LN); }
            }
        } else {                                /* an arc of dots from 7 o'clock (+-: from 12), the pointer */
            int32_t z = lo < 0 && PD[p].hi > 0 ? (-lo * 270) / rng : 0, from = norm < z ? norm : z, to = norm < z ? z : norm;
            for (a = 0; a <= 270u; a += 15u) {
                uint32_t ph = knob_ph((int32_t)a);
                int lit = (int32_t)a + 7 >= from && (int32_t)a - 7 <= to && (z || (int32_t)a <= norm);
                cv_rect(cx + (isin(ph) * 9 >> 15) - 1, cy - (isin(ph + 0x40000000u) * 9 >> 15) - 1, 2, 2, lit ? kc : act ? EV_INK : EV_LN);
            }
            { uint32_t ph = knob_ph(norm); cv_line(cx, cy, cx + (isin(ph) * 5 >> 15), cy - (isin(ph + 0x40000000u) * 5 >> 15), kc); }
            if (src && en != norm) { uint32_t ph = knob_ph(en); cv_rect(cx + (isin(ph) * 9 >> 15) - 2, cy - (isin(ph + 0x40000000u) * 9 >> 15) - 2, 4, 4, act ? EV_INK : EV_GRN); }
        }
        kn[0] = 'K'; kn[1] = (char)('1' + k); kn[2] = 0;
        cv_text(x + 57 - text_w(&FONT_A, kn), 6 - FONT_A_CAP, &FONT_A, kn, sb);
        if (locked || src) cv_rect(x + 34, 18, 23, 2, act ? EV_INK : locked ? EV_ORG : src == 1 ? EV_GRN : src == 2 ? EV_PUR : EV_TXT);
        if (text_w(&FONT_A, PD[p].name) <= 55) cv_text(x + 3, 30 - FONT_A_CAP, &FONT_A, PD[p].name, locked && !act ? EV_ORG : sb);
        else cv_text_fit(x + 3, 38 - TER_BASE, 55, PD[p].name, locked && !act ? EV_ORG : sb, 0);
        fmt_param(p, src && !list ? clampi(E[p], lo, PD[p].hi) : P[p], s);   /* (modulated: where it is now) */
        eva_value(x + 3, 57, 56, p, s, off && !act ? EV_MUT : fg, sb);
    }
    cv_blit(0, 152);
}
/* a knob by what it is: a list as its positions in a row (two: a switch), a +- one as an arc from 12 o'clock, the rest as
 * an arc from the left. Orange with a padlock: locked. A dot and a ring marker: something modulates it (where it is now) */
static void draw_knobs(void)
{
    const theme_t *th = &TH;
    uint32_t k;
    char s[24];
    if (EVA) { eva_knobs(); return; }
    cv_begin(240, 64, th->panel);
    for (k = 0; k < 4u; k++) {
        uint32_t p = PAGE_P[page][sub][k], a;
        int32_t x = 2 + (int32_t)k * 60, cx = x + 28, cy = 16, lo, rng, norm, en, nopt;
        int src, locked, list;
        uint16_t c;
        if (p == NO) { for (a = 0; a < 56u; a += 4u) cv_rect(x + (int32_t)a, 31, 2, 1, C_DK); continue; }
        lo = PD[p].lo; rng = PD[p].hi - lo ? PD[p].hi - lo : 1;
        norm = ((P[p] - lo) * 270) / rng; en = ((clampi(E[p], lo, PD[p].hi) - lo) * 270) / rng;
        src = mod_src(p); locked = lock_get(p);
        nopt = rng + 1;
        list = (PD[p].fmt == F_ENUM || PD[p].fmt == F_SLOT) && nopt <= 8;
        c = locked ? th->haz : mutant ? th->mut : th->acc;
        cv_rect(x, 0, 56, 63, th->box);
        if (list && nopt == 2) {                /* a switch */
            int on = P[p] != lo;
            cv_rect(cx - 13, cy - 6, 26, 12, on ? c : C_DK);
            cv_rect(on ? cx + 2 : cx - 11, cy - 4, 9, 8, on ? RGB(10, 10, 12) : C_MUTE);
        } else if (list) {                      /* its positions, the one it is on */
            int32_t w = (48 - (nopt - 1) * 2) / nopt, x0 = cx - (nopt * w + (nopt - 1) * 2) / 2, q;
            const char *e0 = PD[p].en ? PD[p].en[0] : "";
            int off = P[p] == lo && !locked && ((e0[0] == 'O' && e0[1] == 'F') || (e0[0] == 'B' && e0[1] == 'Y'));   /* on OFF / BYPASS */
            for (q = 0; q < nopt; q++) {
                int here = q == P[p] - lo;
                cv_rect(x0 + q * (w + 2), here ? cy - 6 : cy - 2, w, here ? 12 : 4, here ? (off ? C_MUTE : c) : C_DK);
            }
        } else {
            int32_t z = lo < 0 && PD[p].hi > 0 ? (-lo * 270) / rng : 0, from = norm < z ? norm : z, to = norm < z ? z : norm;
            for (a = 0; a <= 270u; a += 10u) {
                uint32_t ph = knob_ph((int32_t)a);
                int lit = (int32_t)a + 5 >= from && (int32_t)a - 5 <= to && (z || (int32_t)a <= norm);
                cv_rect(cx + (isin(ph) * 13 >> 15) - 1, cy - (isin(ph + 0x40000000u) * 13 >> 15) - 1, 2, 2, lit ? c : C_DK);
            }
            { uint32_t ph = knob_ph(norm); cv_line(cx, cy, cx + (isin(ph) * 9 >> 15), cy - (isin(ph + 0x40000000u) * 9 >> 15), locked ? th->haz : C_TXT); }
            if (src && en != norm) { uint32_t ph = knob_ph(en); cv_rect(cx + (isin(ph) * 13 >> 15) - 2, cy - (isin(ph + 0x40000000u) * 13 >> 15) - 2, 4, 4, th->cyan); }
        }
        if (src) cv_rect(x + 3, 3, 5, 5, src == 1 ? th->cyan : src == 2 ? th->mut : C_TXT);
        if (locked) glyph_lock(x + 46, 3, th->haz);
        fit_text(x - 1, 30, 58, PD[p].name, locked ? th->haz : C_MUTE);   /* 7 characters (8: narrower) */
        fmt_param(p, src && !list ? clampi(E[p], lo, PD[p].hi) : P[p], s);       /* (modulated: where it is now) */
        fit_text(x - 1, 45, 58, s, locked ? th->haz : src ? th->cyan : C_TXT);
    }
    cv_blit(0, 152);
}
static const char *sel_hint(void)               /* what a tap on SEL does on this page */
{
    switch (page) {
    case PG_SRC: return rec_state == REC_IDLE ? "REC ARM" : rec_state == REC_ARMED ? "CANCEL" : "REC STOP";
    case PG_CHIP: return "NEW SFX";
    case PG_SPC: return P[P_SPASS] ? "PLAY PASS" : "PLAY";
    case PG_VAR: return sub == 1u ? (var_store_b() ? "STORE B" : "STORE A") : "NEW SEED";
    case PG_BAT: return batch_on || ft_on ? "STOP" : sub == 1u ? "FIRE" : "BATCH";
    case PG_RTPC: return rt_on ? "STOP" : "SWEEP";
    case PG_LAB: return "APPLY";
    case PG_PHY: return ph_on ? "STOP" : "PLAY";
    case PG_PAD: return g_rec ? "CANCEL" : "REC GEST";
    case PG_ASSET: return sub ? "RANDOMIZE" : "LOAD";
    default: return "RANDOMIZE";
    }
}
static uint32_t ft_pk, ft_px = 0xFFFFu, ft_hold, ft_hold_t;   /* the footer's meter: the peak, drawn at, held */
static void eva_footer(void)                    /* SEL's job here on lime; messages on a purple bar; the meter */
{
    char t[12];
    uint32_t nl = 0, p;
    int32_t x = 118, px, hx;
    cv_begin(240, 24, EV_BG);
    cv_rect(0, 0, 240, 1, EV_LN);
    if (toast[0]) { cv_rect(0, 1, 194, 23, EV_PUR); cv_text_fit(5, 18 - TER_BASE, 184, toast, EV_INK, 0); }
    else {
        cv_rect(3, 5, 29, 15, EV_LIM);
        cv_text(6, 17 - TER_BASE, &FONT_S, "SEL", EV_INK);
        cv_text_fit(37, 17 - TER_BASE, 78, sel_hint(), EV_TXT, 0);
        for (p = 0; p < NP; p++) nl += (uint32_t)lock_get(p);
#ifndef DINGHY_HOST
        fmt_num(t, (int32_t)tm_cpu, 0); cat(t, "%");
        cv_text(x, 17 - TER_BASE, &FONT_S, t, tm_cpu > 85u ? EV_ORG : EV_MUT); x += text_w(&FONT_S, t) + 5;
#endif
        if (nl) { fmt_num(t, (int32_t)nl, 0); glyph_lock(x, 8, EV_ORG); cv_text(x + 9, 17 - TER_BASE, &FONT_S, t, EV_ORG); x += 9 + text_w(&FONT_S, t) + 5; }
        if (oct_shift) { fmt_num(t, oct_shift, 1); cv_text(x, 17 - TER_BASE, &FONT_S, t, EV_GRN); x += text_w(&FONT_S, t) + 5; }
        if (undo_n && x < 172) { fmt_num(t, (int32_t)undo_n, 0); glyph_undo(x, 8, EV_MUT); cv_text(x + 10, 17 - TER_BASE, &FONT_S, t, EV_MUT); x += 10 + text_w(&FONT_S, t) + 5; }
        if (SL[cur_slot].dirty && x < 188) cv_rect(x, 9, 6, 6, EV_ORG);
    }
    px = meter_px(ft_pk, 36); hx = meter_px(ft_hold, 36);
    cv_rect(200, 9, 36, 7, EV_LN);
    cv_rect(200, 9, px, 7, EV_LIM);
    if (px > 30) cv_rect(230, 9, px - 30, 7, EV_ORG);
    if (hx > 1) cv_rect(199 + hx, 6, 1, 13, hx > 30 ? EV_ORG : EV_TXT);
    ft_px = (uint32_t)px;
    cv_blit(0, 216);
}
static void draw_footer(void)
{
    const theme_t *th = &TH;
    char t[12];
    uint32_t nl = 0, p;
    int32_t x = 118, px, hx;
    if (EVA) { eva_footer(); return; }
    cv_begin(240, 24, RGB(10, 10, 12));
    cv_rect(0, 0, 240, 1, th->border);
    if (toast[0]) cv_text_fit(4, 4, 190, toast, th->mut, 0);
    else {
        cv_rect(4, 6, 27, 13, C_DK);
        cv_text(6, 4, &FONT_S, "SEL", th->acc);
        cv_text_fit(35, 4, 80, sel_hint(), C_MUTE, 0);
        for (p = 0; p < NP; p++) nl += (uint32_t)lock_get(p);
#ifndef DINGHY_HOST
        fmt_num(t, (int32_t)tm_cpu, 0); cat(t, "%");
        cv_text(x, 4, &FONT_S, t, tm_cpu > 85u ? th->haz : C_MUTE); x += text_w(&FONT_S, t) + 5;
#endif
        if (nl) { fmt_num(t, (int32_t)nl, 0); glyph_lock(x, 8, th->haz); cv_text(x + 9, 4, &FONT_S, t, th->haz); x += 9 + text_w(&FONT_S, t) + 5; }
        if (oct_shift) { fmt_num(t, oct_shift, 1); cv_text(x, 4, &FONT_S, t, th->cyan); x += text_w(&FONT_S, t) + 5; }
        if (undo_n && x < 172) { fmt_num(t, (int32_t)undo_n, 0); glyph_undo(x, 8, C_MUTE); cv_text(x + 10, 4, &FONT_S, t, C_MUTE); x += 10 + text_w(&FONT_S, t) + 5; }
        if (SL[cur_slot].dirty && x < 188) cv_rect(x, 9, 6, 6, th->haz);   /* (a recording not in flash yet: SAVE) */
    }
    px = meter_px(ft_pk, 36); hx = meter_px(ft_hold, 36);   /* -48 .. 0 dBFS; the last 3 dB hot */
    cv_rect(200, 8, 36, 8, RGB(22, 22, 27));
    cv_rect(200, 8, px, 8, th->cyan);
    if (px > 30) cv_rect(230, 8, px - 30, 8, th->haz);
    if (hx > 1) cv_rect(199 + hx, 6, 1, 12, hx > 30 ? th->haz : C_TXT);
    ft_px = (uint32_t)px;
    cv_blit(0, 216);
}

/* ======================================================================== the session in flash */
typedef struct { uint32_t ver; int16_t p[NP]; uint32_t lk[NLOCKW]; int16_t a[NP], b[NP]; uint8_t ab, mon, gn, gx[GEST_MAX], gy[GEST_MAX]; } session_t;
_Static_assert(NP <= NLOCKW * 32, "the locks: a bit a parameter");
_Static_assert(sizeof(session_t) <= SECTOR_MAX, "the session must fit the settings sector");
_Static_assert(NPAGE <= 24, "a page's change flag (1 << PMOD) must stay below KG_NOTE");
#define SESSION_VER (0x0D071000u + NP)
_Static_assert(sizeof(session_t) <= SECTOR_MAX, "the session: one settings sector");
_Static_assert(P_ETONE == NP - 1, "a new parameter goes last, and session_load learns the old layout");
static void session_save(void)
{
    session_t ss;
    uint32_t i;
    ss.ver = SESSION_VER;
    for (i = 0; i < NP; i++) { ss.p[i] = P[i]; ss.a[i] = snapA[i]; ss.b[i] = snapB[i]; }
    for (i = 0; i < NLOCKW; i++) ss.lk[i] = locks[i];
    ss.ab = has_ab; ss.mon = morph_on;
    ss.gn = (uint8_t)g_n; for (i = 0; i < GEST_MAX; i++) { ss.gx[i] = g_x[i]; ss.gy[i] = g_y[i]; }
    all_off();
    slot_flush(cur_slot);
    say(settings_save(&ss, sizeof ss) ? "SAVE FAILED" : "SESSION SAVED");
    session_dirty = 0;
}
/* the v1.0 / v1.1 session: the same parameters without P_ASSET (added last, so every index holds) */
#define NP_V11 P_ASSET
typedef struct { uint32_t ver; int16_t p[NP_V11]; uint32_t lk[NLOCKW]; int16_t a[NP_V11], b[NP_V11]; uint8_t ab, mon, gn, gx[GEST_MAX], gy[GEST_MAX]; } session_v11_t;
/* the v1.2 / v1.3 session: without SOUND CHIPS (added last) */
typedef struct { uint32_t ver; int16_t p[NP_V13]; uint32_t lk[NLOCKW]; int16_t a[NP_V13], b[NP_V13]; uint8_t ab, mon, gn, gx[GEST_MAX], gy[GEST_MAX]; } session_v13_t;
static void session_load(void)
{
    static union { session_t n; session_v11_t o; session_v13_t m; } u;   /* (static: 2.5 KB off the stack) */
    uint32_t i, np = NP;
    const int16_t *pp, *pa, *pb;
    const uint32_t *lk;
    const uint8_t *tail;
    for (i = 0; i < NP; i++) P[i] = PD[i].def;
    if (!settings_load(&u.n, sizeof u.n) && u.n.ver == SESSION_VER) { pp = u.n.p; pa = u.n.a; pb = u.n.b; lk = u.n.lk; tail = &u.n.ab; }
    else if (!settings_load(&u.m, sizeof u.m) && u.m.ver == 0x0D071000u + NP_V13) { np = NP_V13; pp = u.m.p; pa = u.m.a; pb = u.m.b; lk = u.m.lk; tail = &u.m.ab; }
    else if (!settings_load(&u.o, sizeof u.o) && u.o.ver == 0x0D071000u + NP_V11) { np = NP_V11; pp = u.o.p; pa = u.o.a; pb = u.o.b; lk = u.o.lk; tail = &u.o.ab; }
    else return;
    for (i = 0; i < np; i++) {
        P[i] = (int16_t)clampi(pp[i], PD[i].lo, PD[i].hi);
        snapA[i] = (int16_t)clampi(pa[i], PD[i].lo, PD[i].hi); snapB[i] = (int16_t)clampi(pb[i], PD[i].lo, PD[i].hi);
    }
    for (; i < NP; i++) snapA[i] = snapB[i] = PD[i].def;
    for (i = 0; i < NLOCKW; i++) locks[i] = lk[i];
    has_ab = tail[0] & 3u; morph_on = has_ab == 3u && tail[1];   /* ab, mon, gn, gx[], gy[]: the same in both */
    for (i = 0; i < GEST_MAX; i++) { g_x[i] = tail[3 + i] & 127u; g_y[i] = tail[3 + GEST_MAX + i] & 127u; }
    g_n = tail[2] <= GEST_MAX ? tail[2] : 0u;
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
 * F0 7D 'N' '7' (0x40 | cmd) status a0 a1 a2 F7 (status 0 ok; a = the samples so far / the most a slot holds).
 * The other way (the page's "download"): LEN answers the active slot's length (what is heard: a recording, a LAB edit,
 * a factory sound) and its name after a2; READ off (3 x 7 bit) answers 128 of its samples from off, 16-bit little
 * endian packed 7 bits per byte after a2. */
enum { SX_HELLO = 1, SX_BEGIN = 2, SX_DATA = 3, SX_END = 4, SX_ABORT = 5, SX_LEN = 6, SX_READ = 7 };
static uint8_t sx_out[10 + 300];
static void sx_reply_data(uint32_t cmd, uint32_t st, uint32_t a, const uint8_t *d, uint32_t n, int pack)
{
    uint32_t o = 9, i, acc = 0, nb = 0;
    sx_out[0] = 0xF0; sx_out[1] = 0x7D; sx_out[2] = 'N'; sx_out[3] = '7'; sx_out[4] = (uint8_t)(0x40u | cmd); sx_out[5] = (uint8_t)st;
    sx_out[6] = (uint8_t)(a & 127u); sx_out[7] = (uint8_t)(a >> 7 & 127u); sx_out[8] = (uint8_t)(a >> 14 & 127u);
    for (i = 0; i < n && o < sizeof sx_out - 2u; i++) {
        if (!pack) { sx_out[o++] = d[i] & 127u; continue; }
        acc |= (uint32_t)d[i] << nb; nb += 8u;
        while (nb >= 7u) { sx_out[o++] = (uint8_t)(acc & 127u); acc >>= 7; nb -= 7u; }
    }
    if (pack && nb) sx_out[o++] = (uint8_t)(acc & 127u);
    sx_out[o++] = 0xF7;
    ota_wire_send(sx_out, o);
}
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
    case SX_LEN: {
        char nm[16];
        uint32_t k;
        slot_name(nm, (uint32_t)cur_slot);
        if (smp_busy || rec_state == REC_USB) { st = 1; break; }
        for (k = 0; k < 11u && nm[k]; k++) {}
        sx_reply_data(cmd, 0, smp_len, (const uint8_t *)nm, k, 0);
        return 1;
    }
    case SX_READ: {
        static uint8_t d[256];
        uint32_t off = n > 6u ? (uint32_t)p[4] | (uint32_t)p[5] << 7 | (uint32_t)p[6] << 14 : 0u, k, m;
        const int16_t *b = smp_ptr;
        if (smp_busy || rec_state == REC_USB) { st = 1; break; }
        if (off >= smp_len) { st = 2; break; }
        m = smp_len - off < 128u ? smp_len - off : 128u;
        for (k = 0; k < m; k++) { uint16_t v = (uint16_t)b[off + k]; d[2u * k] = (uint8_t)v; d[2u * k + 1u] = (uint8_t)(v >> 8); }
        sx_reply_data(cmd, 0, off, d, 2u * m, 1);
        return 1;
    }
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
        if (ty == 0x90u && d2) { last_note = d1; if (P[P_PMODE]) phys_start(d1, d2 * 258, 100u + d1); else note_on(d1, d2 * 258); evolve_step(); }
        else if (ty == 0x80u || ty == 0x90u) { if (ph_on && ph_key == 100u + d1 && P[P_PMODE] == 5) phys_stop(); note_off(d1); }
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
        else if (ty == 0xB0u && d1 == 19u) {   /* GESTURE REC armed from the phone (web/index.html) */
            g_rec = d2 >= 64u; say(g_rec ? "ARMED: MOVE THE PAD" : "GESTURE REC CANCELLED"); ui_dirty |= 2 | 8;
        }
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
    if (p == P_THEME) { P[p] = (int16_t)((P[p] + (e > 0 ? 1u : NTHEME - 1u)) % NTHEME); ui_dirty = 0xFF; return; }
    if (p == P_MORPH) {
        morph_set(P[p] + e * step);
        if (has_ab != 3u) say(has_ab ? "STORE B FIRST: SEL" : "STORE A AND B: SEL");
        touched();
        return;
    }
    hot_k = knob; hot_t = fm1_ms;
    if (!NOMORPH[p]) morph_on = 0;              /* an edit: the panel is yours again (it holds the mix) */
    P[p] = (int16_t)clampi(P[p] + e * step, PD[p].lo, PD[p].hi);
    touched();
}

static void app_init(void)                      /* the session, the tables, the screen, the engine (app_main, the host tests) */
{
    uint32_t i;
    session_load();
    sense_setup();
    for (i = 0; i < NP; i++) {                  /* the page of each parameter, the LFO targets */
        uint32_t m, s, k;
        for (m = NPAGE; m-- > 0;) if (m != PG_ASSET) for (s = 0; s < 4u; s++) for (k = 0; k < 4u; k++) if (PAGE_P[m][s][k] == i) PMOD[i] = (uint8_t)m;   /* (its module: ASSETS only repeats some) */
        {                                       /* not a mod target: what no modulation could move (lists of actions, the
                                                   seed, the batch, the meters) */
            static const uint8_t NT[] = {P_SLOT, P_THEME, P_RECM, P_VSEED, P_VKEYS, P_VLOCK, P_HAAMT, P_EVOLVE, P_BMODE, P_BCNT, P_BSPC,
                                         P_BGATE, P_TRATE, P_TPOOL, P_TORDER, P_LMATCH, P_LTGT, P_QSWP, P_QCRV, P_SPASS,
                                         P_MATK, P_MDEC, P_PMODE, P_PHITS, P_PTIME, P_PFALL, P_PPIT, P_PJIT, P_STOOL, P_SAMT, P_RECIPE, P_ASSET};
            int no = PD[i].fmt == F_TGT || PMOD[i] == PG_PAD || i >= P_EMU;
            for (s = 0; s < sizeof NT; s++) if (NT[s] == i) no = 1;
            if (!no) TGT[NTGT++] = (uint8_t)i;
        }
        NOMORPH[i] = i == P_SLOT || i == P_THEME || i == P_ASSET || PD[i].fmt == F_TGT || PMOD[i] == PG_PAD ||
                     PMOD[i] == PG_VAR || PMOD[i] == PG_BAT || PMOD[i] == PG_LAB || i >= P_EMU;
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
    for (i = 0; i < 8u; i++)                    /* which slots hold a recording */
        if (slot_load(i, &rec, sizeof rec) == 0 && rec.len >= 256u && rec.len <= SMP_MAX) { SL[i].user = 1; SL[i].len = (uint16_t)rec.len; }
    slot_select(P[P_SLOT]);
    oct_shift = 0;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    k_update(KG_ALL);
    fx_clear();
}
static void app_main(void)
{
    uint32_t prev_keys = 0, t_vis = 0, t_knob = 0, t_meter = 0, sel_used = 0, sel_down = 0, kpend = 0, i;
    uint16_t kpend_t[27] = {0};
    app_init();
    for (;;) {
        uint32_t keys, down, up, b, rel, k;
        int32_t e;
        int sel = (int)(fm1_in.buttons >> BTN_SEL & 1u);
        platform_poll();
        if (P[P_SLOT] != cur_slot) { slot_select(P[P_SLOT]); ui_dirty |= 2 | 4; }
        if (rec_state == REC_DONE) rec_commit();
        /* buttons */
        b = fm1_input_edges(&rel);
        if (b & (1u << BTN_SEL)) { sel_down = 1; sel_used = 0; sel_t = fm1_ms; help_n = 0; }
        for (k = 0; k < NGRP; k++)
            if (b & (1u << GRP_BTN[k])) {           /* its modules in turn, each one's knob sets in turn */
                if (PAGE_GRP[page] != k) { uint32_t q = 0; while (PAGE_GRP[q] != k) q++; page = (uint8_t)q; sub = 0; }
                else if (sub + 1u < NSUB[page]) sub++;
                else { uint32_t q = page + 1u; if (q >= NPAGE || PAGE_GRP[q] != k) while (q > 0u && PAGE_GRP[q - 1u] == k) q--; page = (uint8_t)q; sub = 0; }
                ui_dirty = 0xFF;
            }
        /* one rule on every page: a button alone does its job; with SEL held, its second job (the same everywhere):
           PLAY note / STOP ALL, REC HAPPY ACCIDENT / UNDO, SAVE session / unlock all, OCT- octave / RESET the module,
           OCT+ octave / RANDOMIZE the module; a tap on SEL alone is the page's own action (the footer says which) */
        if (b & (1u << BTN_PLAY)) { if (sel) { stop_all(); sel_used = 1; } else note_on(60, 32767); }
        if (rel & (1u << BTN_PLAY)) note_off(60);
        if (b & (1u << BTN_REC)) { if (sel) { undo(); sel_used = 1; } else happy_accident(); }
        if (b & (1u << BTN_SAVE)) { if (sel) { for (i = 0; i < NLOCKW; i++) locks[i] = 0; say("ALL LOCKS CLEARED"); ui_dirty |= 4; sel_used = 1; } else session_save(); }
        if (b & (1u << BTN_OCT_DOWN)) {
            if (sel) { reset_module(page); sel_used = 1; }
            else if (oct_shift > -3) { oct_shift--; ui_dirty |= 8; }
        }
        if (b & (1u << BTN_OCT_UP)) {
            if (sel) { module_random(); sel_used = 1; }
            else if (oct_shift < 3) { oct_shift++; ui_dirty |= 8; }
        }
        if (rel & (1u << BTN_SEL)) {
            if (sel_down && !sel_used && fm1_ms - sel_t < 700u) page_action();   /* (a tap; held longer it was a look at the help) */
            if (help_n) { toast[0] = 0; ui_dirty |= 8; }
            sel_down = 0; help_n = 0;
        }
        if (sel && sel_down && !sel_used && fm1_ms - sel_t >= 700u + help_n * 900u) {   /* SEL held: its second jobs, in turn */
            static const char *const HELP[9] = {"SEL+K1..K4: LOCK", "SEL+REC: UNDO", "SEL+PLAY: STOP ALL", "SEL+OCT-: RESET MODULE",
                                                "SEL+OCT+: RANDOMIZE", "SEL+SAVE: UNLOCK ALL", "SEL+SELECT: KNOB SET",
                                                "SEL+ALGORITHM: THEME", "SEL+PRESETS: ASSET"};
            say(HELP[help_n % 9u]); help_n++;
        }
        /* encoders: alone, as printed (SELECT the page, ALGORITHM the wave, PRESETS the sample); with SEL held, a step
           further: SELECT the knob set (A B C D), ALGORITHM the look (theme), PRESETS the asset (loaded and played) */
        if ((e = fm1_enc_take(ENC_SELECT)) != 0) {
            if (sel) { sub = (uint8_t)((sub + (e > 0 ? 1u : NSUB[page] - 1u)) % NSUB[page]); sel_used = 1; }
            else { page = (uint8_t)((page + (e > 0 ? 1u : NPAGE - 1u)) % NPAGE); sub = 0; }
            ui_dirty = 0xFF;
        }
        if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) {
            if (sel) { P[P_THEME] = (int16_t)((P[P_THEME] + (e > 0 ? 1u : NTHEME - 1u)) % NTHEME); ui_dirty = 0xFF; sel_used = 1; say(E_THEME[P[P_THEME]]); }
            else { P[P_WAVE] = (int16_t)clampi(P[P_WAVE] + e, 0, NWAVE - 1); touched(); }
        }
        if ((e = fm1_enc_take(ENC_PRESETS)) != 0) {
            if (sel) { asset_take((uint32_t)(P[P_ASSET] + (e > 0 ? 1 : NASSET - 1))); sel_used = 1; }
            else { P[P_SLOT] = (int16_t)clampi(P[P_SLOT] + e, 0, 7); touched(); }
        }
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
            if (up >> k & 1u) { if (kpend >> k & 1u) kpend &= ~(1u << k); else if (!P[P_VKEYS]) { if (ph_on && ph_key == k && P[P_PMODE] == 5) phys_stop(); note_off(note); } else if (!keys) note_off((int32_t)last_note); }   /* (variations: the last key up) */
            if (down >> k & 1u) { if (key_waits(k)) { kpend |= 1u << k; kpend_t[k] = (uint16_t)fm1_ms; } else play_key(k, note, key_vel15(k)); }
            if ((kpend >> k & 1u) && (!key_waits(k) || (uint16_t)((uint16_t)fm1_ms - kpend_t[k]) >= 12u)) {
                kpend &= ~(1u << k);
                play_key(k, note, key_vel15(k));
            }
        }
        batch_poll();
        fire_poll();
        loop_points();
        motion_poll();
        phys_poll();
        gest_poll();
        if (lvl_match && (int32_t)(fm1_ms - lvl_match) >= 0) { lvl_match = 0; lvl_do_match(); }
        lvl_poll();
        idle_ok = 1;
        midi_in();
        if (mutant && fm1_ms - mutant_t > 700u) { mutant = 0; ui_dirty |= 1 | 4; }
        if (hot_k < 4u && fm1_ms - hot_t >= 1200u) { hot_k = 4u; if (EVA) ui_dirty |= 4; }
        /* LEDs */
        for (k = 0; k < NGRP; k++) fm1_led_key(GRP_BTN[k], PAGE_GRP[page] == k);
        fm1_led_key(BTN_PLAY, (int)tm_awake);
        fm1_led_key(BTN_REC, mutant || rec_state == REC_ON || (rec_state == REC_ARMED && (fm1_ms & 256u)));
        fm1_led_key(BTN_SEL, sel || ((batch_on || ft_on) && (fm1_ms & 128u)));
        fm1_led_key(BTN_SAVE, session_dirty || SL[cur_slot].dirty);
        fm1_led_key(BTN_OCT_DOWN, oct_shift < 0);
        fm1_led_key(BTN_OCT_UP, oct_shift > 0);
        for (k = 0; k < 27u; k++) fm1_led_key(KEY_LED(k), (int)(keys >> k & 1u));
        /* the screen */
        if (map_bits() != map_drawn) ui_dirty |= 1;
        if (ui_dirty & 1) { draw_header(); ui_dirty &= (uint8_t)~1u; }
        if ((ui_dirty & 2) || fm1_ms - t_vis > ((tm_awake || tm_peak || rec_state || (page == PG_LFO && (P[P_L1MIX] || P[P_L2MIX])) ||
                                                  (page == PG_PAD && (pad_on || pad_held || at_keys || at_midi))) ? 40u : 66u)) {
            draw_vis(); ui_dirty &= (uint8_t)~2u; t_vis = fm1_ms;   /* (25 frames a second while something sounds or moves, 15 at rest) */
        }
        if ((ui_dirty & 4) || (mod_any() && fm1_ms - t_knob > 80u)) { draw_knobs(); ui_dirty &= (uint8_t)~4u; t_knob = fm1_ms; }
        {                                       /* the meter: the peak since the last frame, a hold of 1 s */
            uint32_t pk = tm_peak;
            if (pk > ft_pk) ft_pk = pk;
            if (pk >= ft_hold) { ft_hold = pk; ft_hold_t = fm1_ms; } else if (fm1_ms - ft_hold_t > 1000u) ft_hold = ft_hold * 7u >> 3;
            if (fm1_ms - t_meter > 60u) {
                if ((uint32_t)meter_px(ft_pk, 36) != ft_px || ft_hold) ui_dirty |= 8;
                t_meter = fm1_ms;
            }
        }
        if (toast[0] && fm1_ms - t_toast >= 1800u) { toast[0] = 0; ui_dirty |= 8; }
        if (ui_dirty & 8) { draw_footer(); ui_dirty &= (uint8_t)~8u; ft_pk = 0; }
        idle_ok = 0;
    }
}
