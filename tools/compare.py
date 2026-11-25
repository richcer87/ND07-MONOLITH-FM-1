#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Two recordings of the same notes, side by side: what each note is, and where B leaves A.

    python3 tools/compare.py A.wav B.wav [--png out.png] [--names ND07,YM2149,...]

A is the reference (a console, an emulator, the host simulator), B what is being checked (the FM-1, recorded), or
the other way round. The notes are found on their own (each one a burst over the noise floor), so a take played by
hand compares with a script's: they only need the same notes in the same order. Any WAV: 16, 24 or 32 bits, float,
any rate, stereo folded to mono.

For each pair of notes it says:
  pitch      the fundamental of each and how many cents apart (a chip's own divider shows up here)
  level      the note's loudness (dB RMS), each against its own take's loudest note
  bright     the spectral centroid: where the sound's weight sits (a low-pass shows up here)
  harm       harmonics 1..8, dB under the strongest, and the largest gap between A and B
  noise      what is not on the harmonics, dB under the note (aliasing, a noisy DAC, hiss)
  attack     time to 90 % of the peak; decay: from the peak to -20 dB (or the note's end)
A pair that differs more than the margins is flagged (!). Nothing here listens for you: it points where to.
"""
import argparse
import struct
import sys
import wave

import numpy as np

SR = 44100


def load(path):
    """Mono float at 44.1 kHz from a WAV of any common format (the wave module reads PCM; float by hand)."""
    with open(path, "rb") as f:
        raw = f.read()
    if raw[:4] != b"RIFF" or raw[8:12] != b"WAVE":
        sys.exit(f"{path}: not a WAV")
    pos, fmt, data = 12, None, None
    while pos + 8 <= len(raw):
        cid, size = raw[pos:pos + 4], struct.unpack("<I", raw[pos + 4:pos + 8])[0]
        body = raw[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt = struct.unpack("<HHIIHH", body[:16])
        elif cid == b"data":
            data = body
        pos += 8 + size + (size & 1)
    if not fmt or data is None:
        sys.exit(f"{path}: no fmt or data chunk")
    tag, ch, rate, _, _, bits = fmt
    if tag == 0xFFFE:                           # WAVE_FORMAT_EXTENSIBLE: the real tag in the sub-format
        ext = raw.find(b"fmt ")
        tag = struct.unpack("<H", raw[ext + 8 + 24:ext + 8 + 26])[0]
    if tag == 3:
        x = np.frombuffer(data, dtype="<f4" if bits == 32 else "<f8").astype(np.float64)
    elif bits == 16:
        x = np.frombuffer(data[:len(data) // 2 * 2], dtype="<i2") / 32768.0
    elif bits == 24:
        b = np.frombuffer(data[:len(data) // 3 * 3], dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        x = np.where(v & 0x800000, v - 0x1000000, v) / 8388608.0
    elif bits == 32:
        x = np.frombuffer(data[:len(data) // 4 * 4], dtype="<i4") / 2147483648.0
    elif bits == 8:
        x = (np.frombuffer(data, dtype=np.uint8) - 128.0) / 128.0
    else:
        sys.exit(f"{path}: {bits}-bit PCM not read")
    x = x[:len(x) // ch * ch].reshape(-1, ch).mean(axis=1)
    if rate != SR:                              # (through the spectrum: no interpolation's own dulling)
        n2 = int(round(len(x) * SR / rate))
        X = np.fft.rfft(x)
        Y = np.zeros(n2 // 2 + 1, dtype=complex)
        k = min(len(X), len(Y))
        Y[:k] = X[:k]
        x = np.fft.irfft(Y, n2) * (n2 / len(x))
    return x - x.mean()


def notes(x, min_len=0.15):
    """The notes: (start, end) in samples. A note starts where the level jumps (12 dB within 30 ms over the last 100
    ms's low, and over anything in the last 300 ms), so a tail still ringing does not hide the next one; it ends at the next start or
    at the floor."""
    hop = SR // 100
    n = len(x) // hop
    r = np.sqrt(np.mean(x[:n * hop].reshape(n, hop) ** 2, axis=1)) + 1e-9
    db = 20 * np.log10(r)
    floor = np.percentile(db, 10)
    th = max(floor + 12, np.percentile(db, 99) - 45)
    starts, i = [], 1
    while i < n:
        lo = db[max(0, i - 10):i].min()
        up = db[i:i + 4].max()
        if db[i] > th and up - lo > 12 and up > db[max(0, i - 30):i].max() + 3 and (not starts or i - starts[-1] > 30):
            starts.append(i)
            i += 15
        else:
            i += 1
    out = []
    for k, a in enumerate(starts):
        b = starts[k + 1] if k + 1 < len(starts) else n
        e = a
        while e < b and db[e] > th:
            e += 1
        if (e - a) * hop >= min_len * SR:
            out.append((a * hop, e * hop))
    return out


def f0_of(seg):
    """The fundamental: the autocorrelation's first strong peak, refined by the spectrum's nearest peak."""
    seg = seg[: SR // 2] if len(seg) > SR // 2 else seg
    w = seg * np.hanning(len(seg))
    n = 1 << (len(w) * 2 - 1).bit_length()
    S = np.fft.rfft(w, n)
    ac = np.fft.irfft(np.abs(S) ** 2)[: len(w)]
    lo, hi = SR // 4000, SR // 25               # (25 Hz .. 4 kHz)
    if hi >= len(ac) or ac[0] <= 0:
        return 0.0
    k = lo + int(np.argmax(ac[lo:hi]))
    best = ac[k]
    for d in range(2, 6):                       # (prefer the shortest period that holds most of the peak: octave errors)
        kk = int(round(k / d))
        if kk >= lo:
            j = max(lo, kk - 2) + int(np.argmax(ac[max(lo, kk - 2):kk + 3]))
            if ac[j] > 0.85 * best:
                k = j
    if 0 < k < len(ac) - 1:
        a, b, c = ac[k - 1], ac[k], ac[k + 1]
        if b >= a and b >= c and a - 2 * b + c < 0:
            k = k + float(np.clip(0.5 * (a - c) / (a - 2 * b + c), -1, 1))
    f = SR / k
    mag = np.abs(np.fft.rfft(w, 1 << 16))       # (then the spectrum's peak near it, for the cents)
    fr = np.fft.rfftfreq(1 << 16, 1 / SR)
    m = (fr > f * 0.97) & (fr < f * 1.03)
    if m.any():
        i = np.where(m)[0][np.argmax(mag[m])]
        if 0 < i < len(mag) - 1:
            a, b, c = np.log(mag[i - 1:i + 2] + 1e-12)
            d = a - 2 * b + c
            f = (i + (float(np.clip(0.5 * (a - c) / d, -1, 1)) if d < 0 else 0.0)) * SR / (1 << 16)
    return f


def describe(x, s, e):
    seg = x[s:e]
    pk = np.argmax(np.abs(seg))
    body = seg[min(len(seg) - 1, int(0.05 * SR)): max(int(0.05 * SR) + 2048, int(len(seg) * 0.6))]   # (past the attack)
    f0 = f0_of(body)
    w = body * np.hanning(len(body))
    mag = np.abs(np.fft.rfft(w)) ** 2
    fr = np.fft.rfftfreq(len(w), 1 / SR)
    tot = mag.sum() + 1e-20
    harm = []
    onh = np.zeros_like(mag, dtype=bool)
    for h in range(1, 9):
        if f0 <= 0 or h * f0 > SR / 2 - 200:
            harm.append(-99.0)
            continue
        m = np.abs(fr - h * f0) < max(8.0, f0 * 0.03)
        onh |= m
        harm.append(10 * np.log10(mag[m].sum() + 1e-20))
    hmax = max(harm)
    harm = [round(v - hmax, 1) if v > -98 else -99.0 for v in harm]
    if f0 > 0:                                  # (all the harmonics, for the noise figure)
        for h in range(9, int((SR / 2) / f0)):
            onh |= np.abs(fr - h * f0) < max(8.0, f0 * 0.03)
    noise = 10 * np.log10(mag[~onh].sum() / tot + 1e-12)
    env = np.sqrt(np.convolve(seg ** 2, np.ones(441) / 441, mode="same"))
    ep = env.max() + 1e-12
    att = np.argmax(env >= 0.9 * ep) / SR
    after = env[np.argmax(env):]
    below = np.where(after < ep * 0.1)[0]
    dec = (below[0] / SR) if len(below) else len(after) / SR
    return {
        "f0": f0,
        "rms": 20 * np.log10(np.sqrt(np.mean(seg ** 2)) + 1e-12),
        "bright": float((mag * fr).sum() / tot),
        "harm": harm,
        "noise": noise,
        "attack": att,
        "decay": dec,
        "len": (e - s) / SR,
    }


def cents(a, b):
    return 1200 * np.log2(b / a) if a > 0 and b > 0 else float("nan")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--png", help="a picture: each note's spectrum, A over B")
    ap.add_argument("--names", help="labels for the notes, comma separated (e.g. the engines, two notes each)")
    ap.add_argument("--ref", action="store_true", help="the reference take of tests/scripts/ref.txt: its 15 notes named")
    ap.add_argument("--cents", type=float, default=10.0, help="pitch margin (default 10 cents)")
    ap.add_argument("--db", type=float, default=3.0, help="level and harmonic margin (default 3 dB)")
    o = ap.parse_args()
    xa, xb = load(o.a), load(o.b)
    na, nb = notes(xa), notes(xb)
    print(f"A {o.a}: {len(na)} notes   B {o.b}: {len(nb)} notes")
    da = [describe(xa, s, e) for s, e in na]
    db = [describe(xb, s, e) for s, e in nb]
    skip_a = 0
    if len(na) != len(nb):                      # (a note more on one side: the offset whose pitches agree best)
        big, small = (da, db) if len(da) > len(db) else (db, da)
        def miss(k):
            c = [abs(cents(p["f0"], q["f0"])) for p, q in zip(big[k:], small)]
            c = [min(v, 1200) for v in c if v == v]
            return sum(c) / max(len(c), 1)
        k = min(range(len(big) - len(small) + 1), key=miss)
        print(f"  (not the same count: {'A' if big is da else 'B'} skips its first {k}; check the takes)")
        if big is da: da, na, skip_a = da[k:], na[k:], k
        else: db, nb = db[k:], nb[k:]
    ra = max((d["rms"] for d in da), default=0)
    rb = max((d["rms"] for d in db), default=0)
    names = o.names.split(",") if o.names else []
    if o.ref and not names:
        names = ["SYNTH (asset)"] + [f"{e} {n}" for e in ("ND07", "YM2149", "YM2612", "SPC700", "SCSP", "DMG", "2A03") for n in ("C4", "C5")]
    names = names[skip_a:]
    flags = 0
    for i, (p, q) in enumerate(zip(da, db)):
        name = names[i] if i < len(names) else f"note {i + 1}"
        c = cents(p["f0"], q["f0"])
        lv = (q["rms"] - rb) - (p["rms"] - ra)
        hg = max([abs(u - v) for u, v in zip(p["harm"], q["harm"]) if max(u, v) > -60] or [0])   # (under -60 dB: the floor)
        bad = []
        if abs(c) > o.cents and max(p["noise"], q["noise"]) < -6: bad.append("pitch")
        if abs(lv) > o.db: bad.append("level")
        if hg > o.db * 2 and max(p["noise"], q["noise"]) < -6: bad.append("harmonics")   # (a noise has none to compare)
        if abs(q["noise"] - p["noise"]) > 6: bad.append("noise")
        if abs(np.log2((q["bright"] + 1) / (p["bright"] + 1))) > 0.3: bad.append("brightness")
        flags += bool(bad)
        print(f"\n{'!' if bad else ' '} {name}   ({p['len']:.2f} s / {q['len']:.2f} s)" + (f"   differs: {', '.join(bad)}" if bad else ""))
        print(f"    pitch   {p['f0']:8.2f} Hz  {q['f0']:8.2f} Hz   {c:+6.1f} cents")
        print(f"    level   {p['rms'] - ra:+6.1f} dB   {q['rms'] - rb:+6.1f} dB   ({lv:+.1f})")
        print(f"    bright  {p['bright']:7.0f} Hz  {q['bright']:7.0f} Hz")
        print(f"    harm A  {' '.join(f'{v:6.1f}' for v in p['harm'])}")
        print(f"    harm B  {' '.join(f'{v:6.1f}' for v in q['harm'])}   (widest gap {hg:.1f} dB)")
        print(f"    noise   {p['noise']:6.1f} dB   {q['noise']:6.1f} dB")
        print(f"    attack  {p['attack'] * 1000:5.0f} ms  {q['attack'] * 1000:5.0f} ms    decay {p['decay'] * 1000:5.0f} ms  {q['decay'] * 1000:5.0f} ms")
    print(f"\n{flags} of {min(len(da), len(db))} pairs differ past the margins")
    if o.png:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        k = min(len(na), len(nb))
        fig, axs = plt.subplots(max(k, 1), 1, figsize=(10, 2.2 * max(k, 1)), squeeze=False)
        for i in range(k):
            for x, (s, e), col, lab in ((xa, na[i], "#5a3fd6", "A"), (xb, nb[i], "#9ad400", "B")):
                seg = x[s:e][: SR // 2]
                mag = 20 * np.log10(np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) + 1e-9)
                axs[i][0].semilogx(np.fft.rfftfreq(len(seg), 1 / SR)[1:], mag[1:] - mag.max(), color=col, lw=0.8, label=lab)
            axs[i][0].set_ylim(-90, 3)
            axs[i][0].set_xlim(30, SR / 2)
            axs[i][0].set_title(names[i] if i < len(names) else f"note {i + 1}", fontsize=8, loc="left")
            axs[i][0].legend(fontsize=7, loc="upper right")
        fig.tight_layout()
        fig.savefig(o.png, dpi=110)
        print("picture:", o.png)
    return 1 if flags else 0


if __name__ == "__main__":
    sys.exit(main())
