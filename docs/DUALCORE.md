# ND-07 on two cores: the design (not built yet)

The AC791N has two RISC-V cores; ND-07 uses one. This note is the plan for using the second when the audio engine
needs it, and why it is not in v0.5.

## Why not now

- **The CPU fits.** Measured on the host (instructions per sample): default patch ~1015, everything on at once ~1790,
  a one-shot with its reverb tail ~565. Firmwares that run well on the FM-1 sit around 1000–1500, and ND-07's heavy
  stages already run from RAM. The footer's CPU % on a real unit is the number that decides.
- **It cannot be tested here.** Starting core 1 means changing `crt0.S` (its entry point), `app.ld` (a `.c1_text`
  section, a stack, a `.noinit` mailbox) and `fm1_irq.h` (stopping core 1 and catching its faults, so a crash screen
  still appears). The browser emulator runs both cores, but some units read RAM wrongly from core 1 (seen by the
  JIANT and Melodee authors), which only the hardware shows. A firmware that boots on one FM-1 and hangs on another
  is worse than one that is a little slower.

## The plan, when it is needed

Follow JIANT's `fm1_multicore.h` (GPL-3.0, after Melodee and fm1-x0x):

1. **What moves:** `stage_fx` (delay + flanger + reverb, ~330 of the worst case) runs on core 1, one block behind.
   It only reads the block `stage_ott` leaves and writes a block `stage_out` mixes in, so the two cores never share
   state inside a block. The cost is 64 samples (1.5 ms) of extra latency on the effects only; the dry sound is not
   delayed.
2. **The mailbox:** a job word with a sequence number and a check word (`~seq`) in `.noinit` RAM. Core 0 writes the
   input block, then the job; core 1 polls, runs `stage_fx`, writes the output and `done = seq`. Core 0 checks `done`
   at the next block; if core 1 is late or the check word is wrong, core 0 runs `stage_fx` itself for that block and
   counts a miss. Three misses in a row and core 1 is stopped for the session: the sound never depends on it.
3. **Opt-in:** a GLOBAL parameter `2ND CORE` (OFF by default) and a compile flag, so a build without it is exactly
   v0.5.
4. **Tests:** host build: the mailbox run by two threads, bit-exact against one core. Emulator: boots, plays, falls
   back when core 1 is held. Hardware: a night of BATCH runs with the miss counter on the screen.
