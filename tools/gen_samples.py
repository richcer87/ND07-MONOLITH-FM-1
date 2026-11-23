#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# ND-07 // MONOLITH (FM-1): the 8 factory samples of the SAMPLE bank -> firmware/src/nd07_samples.h
#
#   python3 tools/gen_samples.py            # samples/01_*.wav .. samples/08_*.wav where present, a synthesized SFX otherwise
#   python3 tools/gen_samples.py --write-wavs   # also writes the synthesized ones to samples/ (to hear or replace them)
#
# Your own: drop 01_NAME.wav .. 08_NAME.wav into samples/ (replacing the one with that number; NAME: up to 8 letters) (any rate, mono or stereo, 16/24-bit PCM), run this, push. Each is
# mixed to mono, resampled to 22.05 kHz, normalized and cut at 1.48 s (a user slot's length), then stored as 16-bit
# PCM with TPDF dither (the user slots keep 8-bit mu-law in flash; they are decoded to 16-bit in RAM). Pitched sounds should sit on C4 (261.63 Hz): the keyboard plays them from there.
# In flash each one is packed without loss (about half the size): per block of 128 samples a predictor (none, the last
# sample, or the last two) and a Rice code for what it misses; fsmp_unpack() in nd07.c turns it back into the same
# 16-bit samples when the slot is chosen.
import math, pathlib, struct, sys, wave
import numpy as np

SR = 22050
MAXN = 32700                                     # samples: as long as a 32 KiB mu-law user slot
ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "firmware" / "src" / "nd07_samples.h"
NAMES = ["BOOM", "METAL", "ZAP", "GLITCH", "WHOOSH", "VOX AH", "BELL", "DRONE"]
C4 = 261.6256
rng = np.random.default_rng(707)


def t_of(sec): return np.arange(int(sec * SR)) / SR


def env_ad(t, a, d): return np.minimum(1.0, t / max(a, 1e-4)) * np.exp(-np.maximum(0, t - a) / d)


def onepole_lp(x, fc):
    a = 1 - math.exp(-2 * math.pi * fc / SR); y = np.empty_like(x); s = 0.0
    for i, v in enumerate(x): s += a * (v - s); y[i] = s
    return y


def boom():
    t = t_of(0.9); f = 38 + 140 * np.exp(-t * 28)
    body = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 4.0)
    click = rng.standard_normal(len(t)) * np.exp(-t * 90) * 0.5
    return np.tanh(1.8 * (body + click))


def metal():
    t = t_of(1.3); x = np.zeros_like(t)
    for k, (r, d) in enumerate([(1.0, 1.1), (2.76, 0.7), (5.40, 0.45), (8.93, 0.3), (13.34, 0.2), (3.17, 0.9)]):
        x += np.sin(2 * np.pi * C4 * r * t + k) * np.exp(-t / d) / (1 + k * 0.4)
    x += rng.standard_normal(len(t)) * np.exp(-t * 60) * 0.4
    return x


def zap():
    t = t_of(0.6); f = 90 + 2600 * np.exp(-t * 11)
    ph = np.cumsum(f) / SR
    saw = 2 * (ph % 1.0) - 1
    fm = np.sin(2 * np.pi * ph * 3.0 + 2.0 * np.sin(2 * np.pi * ph * 0.5))
    return (0.6 * saw + 0.6 * fm) * env_ad(t, 0.002, 0.25)


def glitch():
    t = t_of(0.55); x = np.zeros_like(t)
    seg = int(0.045 * SR); freqs = [1760, 880, 2640, 1320, 3520, 660, 1980, 990, 2200, 1100, 2960, 1480]
    for i in range(0, len(t), seg):
        f = freqs[(i // seg) % len(freqs)]; n = min(seg, len(t) - i); tt = np.arange(n) / SR
        sq = np.sign(np.sin(2 * np.pi * f * tt)) * (0.9 if (i // seg) % 3 else 0.0)
        x[i:i + n] = sq * np.minimum(1, (n - np.arange(n)) / 60)
    x = np.round(x * 6) / 6                                  # crushed
    return x * np.exp(-t * 2.5)


def whoosh():
    t = t_of(1.4); n = rng.standard_normal(len(t))
    sweep = 300 + 5000 * np.sin(np.pi * t / t[-1]) ** 2
    y = np.empty_like(n); s1 = s2 = 0.0
    for i, v in enumerate(n):                                # swept 2-pole band-ish
        a = 1 - math.exp(-2 * math.pi * sweep[i] / SR); s1 += a * (v - s1); s2 += a * (s1 - s2); y[i] = s1 - s2 * 0.6
    return y * np.sin(np.pi * t / t[-1]) ** 1.5


def vox():
    t = t_of(1.48); f0 = C4 * (1 + 0.006 * np.sin(2 * np.pi * 5.2 * t))
    ph = np.cumsum(f0) / SR; src = np.zeros_like(t)
    for h in range(1, 30): src += np.sin(2 * np.pi * h * ph) / h
    out = np.zeros_like(t)
    for fc, bw, g in [(730, 90, 1.0), (1090, 110, 0.5), (2440, 160, 0.25)]:   # "ah"
        r = math.exp(-math.pi * bw / SR); c = 2 * r * math.cos(2 * math.pi * fc / SR); y1 = y2 = 0.0; y = np.empty_like(t)
        for i, v in enumerate(src): y0 = v + c * y1 - r * r * y2; y2, y1 = y1, y0; y[i] = y0
        out += g * y / np.max(np.abs(y))
    return out * np.minimum(1, t / 0.04)


def bell():
    t = t_of(1.48); m = np.sin(2 * np.pi * C4 * 3.5 * t) * 4.0 * np.exp(-t * 2.2)
    return np.sin(2 * np.pi * C4 * t + m) * env_ad(t, 0.001, 0.7)


def drone():
    t = t_of(1.48); x = np.zeros_like(t)
    for f, d in [(C4 / 2, 0), (C4 / 2 * 1.006, 0.3), (C4, 0.7), (C4 * 0.995, 1.1), (C4 * 1.5, 2.0)]:
        ph = (f * t + d) % 1.0; x += 2 * ph - 1
    return onepole_lp(x, 1800) * np.minimum(1, t / 0.15)


SYNTH = [boom, metal, zap, glitch, whoosh, vox, bell, drone]


def read_wav(path):
    with wave.open(str(path)) as w:
        ch, sw, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if sw == 2: x = np.frombuffer(raw, "<i2").astype(np.float64) / 32768
    elif sw == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3)
        x = ((b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16)) << 8 >> 8) / 8388608
    elif sw == 1: x = (np.frombuffer(raw, np.uint8).astype(np.float64) - 128) / 128
    else: raise SystemExit(f"{path}: 8/16/24-bit PCM only")
    x = x.reshape(-1, ch).mean(axis=1)
    if sr != SR:                                             # windowed-sinc-free but band-limited enough: FFT resample
        n2 = int(round(len(x) * SR / sr)); x = np.fft.irfft(np.fft.rfft(x), len(x))[:len(x)]
        x = np.interp(np.arange(n2) * sr / SR, np.arange(len(x)), onepole_lp(x, 0.45 * SR) if sr > SR else x)
    return x


def ulaw(x):
    out = bytearray()
    for v in np.clip(np.round(x * 32635), -32635, 32635).astype(int):
        sign = 0x80 if v < 0 else 0; v = min(abs(v), 32635) + 0x84
        exp = 7
        while exp > 0 and not (v & (0x4000 >> (7 - exp))): exp -= 1
        mant = (v >> (exp + 3)) & 15
        out.append(~(sign | (exp << 4) | mant) & 0xFF)
    return bytes(out)


BLK = 128


class Bits:
    def __init__(self): self.out = bytearray(); self.acc = 0; self.n = 0
    def put(self, v, k):
        for i in range(k - 1, -1, -1):
            self.acc = (self.acc << 1) | ((v >> i) & 1); self.n += 1
            if self.n == 8: self.out.append(self.acc); self.acc = 0; self.n = 0
    def done(self):
        if self.n: self.out.append(self.acc << (8 - self.n))
        return bytes(self.out)


def rice_len(u, k): q = u >> k; return (q + 1 + k) if q < 24 else 24 + 18


def pack(b):
    """the block code fsmp_unpack() reads: 2 bits predictor, 5 bits k, then per sample a Rice code of the
    zigzagged miss (24 ones: an escape, then 18 bits)"""
    w = Bits(); a = bb = 0; i = 0
    while i < len(b):
        blk = b[i:i + BLK]; best = None
        for order in range(3):
            pa, pb, us = a, bb, []
            for x in blk:
                e = x - (0 if order == 0 else pa if order == 1 else 2 * pa - pb)
                us.append((e << 1) ^ (e >> 31) if e >= 0 else ((-e) << 1) - 1); pb, pa = pa, x
            for k in range(18):
                n = sum(rice_len(u, k) for u in us)
                if best is None or n < best[0]: best = (n, order, k, us)
        _, order, k, us = best
        w.put(order, 2); w.put(k, 5)
        for u in us:
            q = u >> k
            if q < 24: w.put((1 << (q + 1)) - 2, q + 1); w.put(u & ((1 << k) - 1), k)
            else: w.put((1 << 24) - 1, 24); w.put(u, 18)
        for x in blk: bb, a = a, x
        i += BLK
    return w.done()


def unpack(d, n):                                            # (the C decoder, in Python: checked on every build)
    pos = 0
    def bit():
        nonlocal pos; v = (d[pos >> 3] >> (7 - (pos & 7))) & 1; pos += 1; return v
    def bits(k):
        v = 0
        for _ in range(k): v = (v << 1) | bit()
        return v
    out = []; a = bb = 0
    while len(out) < n:
        order = bits(2); k = bits(5)
        for _ in range(min(BLK, n - len(out))):
            q = 0
            while q < 24 and bit(): q += 1
            u = (q << k) | bits(k) if q < 24 else bits(18)
            e = (u >> 1) ^ -(u & 1)
            x = (0 if order == 0 else a if order == 1 else 2 * a - bb) + e
            out.append(x); bb, a = a, x
    return out


def main():
    write = "--write-wavs" in sys.argv
    (ROOT / "samples").mkdir(exist_ok=True)
    L = ["/* generated by tools/gen_samples.py: do not edit */", "#pragma once",
         "/* the factory SAMPLE bank: 16-bit PCM, 22050 Hz, root C4, packed without loss (tools/gen_samples.py: pack) */"]
    total = 0; lens = []; names = []
    for i, fn in enumerate(SYNTH):
        found = sorted((ROOT / "samples").glob(f"{i + 1:02d}*.wav"))
        path = found[0] if found else ROOT / "samples" / f"{i + 1:02d}_{NAMES[i].replace(' ', '')}.wav"
        if found:
            x = read_wav(path)
            name = (path.stem.split("_", 1)[1] if "_" in path.stem else NAMES[i]).upper().replace("-", " ")[:8]
        else:
            x, name = fn(), NAMES[i]
            if write:
                with wave.open(str(path), "wb") as w:
                    w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
                    w.writeframes((np.clip(x / np.max(np.abs(x)), -1, 1) * 32000).astype("<i2").tobytes())
        x = x[:MAXN] / (np.max(np.abs(x[:MAXN])) + 1e-9) * 0.95
        fade = min(256, len(x) // 8); x[-fade:] *= np.linspace(1, 0, fade)
        tpdf = (rng.random(len(x)) - rng.random(len(x)))          # +-1 LSB triangular dither
        b = np.clip(np.round(x * 32767 + tpdf), -32767, 32767).astype(int)
        d = pack([int(v) for v in b])
        assert unpack(d, len(b)) == [int(v) for v in b], "the packed sample does not unpack to itself"
        total += len(d); lens.append(len(b)); names.append(name)
        L.append(f"static const uint8_t FSMP_{i}[{len(d)}] = {{   /* {len(b)} samples */")
        for k in range(0, len(d), 24): L.append("    " + ",".join(str(v) for v in d[k:k + 24]) + ",")
        L.append("};")
    L.append("static const uint8_t *const FSMP[8] = {" + ", ".join(f"FSMP_{i}" for i in range(8)) + "};")
    L.append("static const uint16_t FSMP_LEN[8] = {" + ", ".join(map(str, lens)) + "};")
    L.append("static const char *const FSMP_NAME[8] = {" + ", ".join(f'"{n}"' for n in names) + "};")
    OUT.write_text("\n".join(L) + "\n")
    print(f"wrote {OUT}: {total} bytes of packed samples", names)


main()
