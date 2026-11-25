"""Placeholder sounds for Ikigai: assets/audio/sfx/*.wav and assets/audio/music/*.wav.

Simple synthesis (noise, sines, envelopes) with a fixed seed: running it again writes identical bytes.
Python 3 standard library only. Usage (from the repo root): py tools/gen_placeholder_sfx.py
"""

import math
import random
import struct
import wave
from pathlib import Path

RATE = 22050
ROOT = Path(__file__).resolve().parent.parent / "assets" / "audio"


def silence(seconds):
    return [0.0] * int(seconds * RATE)


def env(i, n, attack=0.005, curve=4.0):
    """Fast attack, exponential-ish decay to 0 at the end."""
    t = i / RATE
    a = min(1.0, t / attack) if attack > 0 else 1.0
    return a * (1.0 - i / n) ** curve


def tone(seconds, f0, f1=None, shape="sine", attack=0.005, curve=4.0, gain=1.0):
    """A tone sweeping f0 -> f1 (exponentially), shaped by env."""
    f1 = f0 if f1 is None else f1
    n = int(seconds * RATE)
    out, phase = [], 0.0
    for i in range(n):
        f = f0 * (f1 / f0) ** (i / n)
        phase += 2 * math.pi * f / RATE
        s = math.sin(phase)
        if shape == "square":
            s = 1.0 if s >= 0 else -1.0
        elif shape == "tri":
            s = 2 / math.pi * math.asin(s)
        out.append(gain * s * env(i, n, attack, curve))
    return out


def noise(rng, seconds, cutoff0, cutoff1=None, attack=0.003, curve=3.0, gain=1.0):
    """White noise through a one-pole low-pass sweeping cutoff0 -> cutoff1 Hz."""
    cutoff1 = cutoff0 if cutoff1 is None else cutoff1
    n = int(seconds * RATE)
    out, y = [], 0.0
    for i in range(n):
        c = cutoff0 + (cutoff1 - cutoff0) * i / n
        k = 1.0 - math.exp(-2 * math.pi * c / RATE)
        y += k * (rng.uniform(-1.0, 1.0) - y)
        out.append(gain * y * env(i, n, attack, curve))
    return out


def mix(*parts, offsets=None):
    offsets = offsets or [0.0] * len(parts)
    n = max(int(o * RATE) + len(p) for p, o in zip(parts, offsets))
    out = [0.0] * n
    for p, o in zip(parts, offsets):
        start = int(o * RATE)
        for i, s in enumerate(p):
            out[start + i] += s
    return out


def normalize(samples, peak=0.8):
    top = max(abs(s) for s in samples) or 1.0
    return [s * peak / top for s in samples]


def write(path, samples):
    path.parent.mkdir(parents=True, exist_ok=True)
    data = b"".join(struct.pack("<h", max(-32767, min(32767, int(round(s * 32767))))) for s in samples)
    with wave.open(str(path), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(data)


def sfx(rng):
    return {
        "slash_1": mix(noise(rng, 0.12, 6000, 1500, curve=2.0), tone(0.12, 900, 400, gain=0.2)),
        "slash_2": mix(noise(rng, 0.14, 7000, 1800, curve=2.0), tone(0.14, 1100, 450, gain=0.2)),
        "slash_3": mix(noise(rng, 0.22, 8000, 1000, curve=1.6), tone(0.22, 700, 220, gain=0.35)),
        "hit_ice": mix(noise(rng, 0.08, 9000, 3000), tone(0.3, 2100, gain=0.5), tone(0.25, 3150, gain=0.3),
                       offsets=[0.0, 0.01, 0.02]),
        "hit_block": mix(tone(0.15, 140, 70, curve=3.0), noise(rng, 0.06, 2500), tone(0.12, 900, gain=0.2)),
        "parry": mix(tone(0.45, 1320, gain=0.6, curve=2.5), tone(0.45, 1980, gain=0.4, curve=3.0), noise(rng, 0.03, 9000)),
        "shard_throw": mix(tone(0.18, 600, 1600, shape="tri", curve=1.5), noise(rng, 0.18, 3000, 6000, gain=0.3)),
        "shard_break": mix(noise(rng, 0.3, 9000, 2000, curve=3.0), tone(0.15, 2600, gain=0.3), tone(0.1, 3400, gain=0.25),
                           offsets=[0.0, 0.03, 0.07]),
        "guard_up": mix(tone(0.15, 300, 600, shape="tri", curve=2.0), tone(0.15, 600, 1200, gain=0.25)),
        "guard_break": mix(noise(rng, 0.4, 5000, 400, curve=2.0), tone(0.4, 500, 120, shape="square", gain=0.25)),
        "no_energy": mix(tone(0.09, 220, shape="square", curve=1.0), tone(0.09, 180, shape="square", curve=1.0),
                         offsets=[0.0, 0.13]),
        "step": noise(rng, 0.05, 1800, 600, curve=3.0),
        "land": mix(tone(0.12, 120, 60, curve=3.0), noise(rng, 0.08, 1500, 400)),
        "jump": mix(tone(0.1, 280, 560, curve=2.0), noise(rng, 0.06, 2500, gain=0.3)),
    }


def loop(seconds, notes):
    """notes: (start s, length s, Hz, gain). Tails past the end wrap to the start: a seamless loop."""
    n = int(seconds * RATE)
    out = [0.0] * n
    for start, length, freq, gain in notes:
        s = int(start * RATE)
        for i, v in enumerate(tone(length, freq, attack=0.02, curve=2.0, gain=gain)):
            out[(s + i) % n] += v
    return out


def music():
    # Forest: bright pentatonic arpeggio (C major pentatonic) over two pad chords, 8 s.
    penta = [261.63, 293.66, 329.63, 392.0, 440.0, 523.25]
    rng = random.Random(7)
    forest = [(i * 0.25, 0.6, rng.choice(penta) * (2 if rng.random() < 0.3 else 1), 0.25) for i in range(32)]
    forest += [(0.0, 4.0, 130.81, 0.3), (0.0, 4.0, 196.0, 0.2), (4.0, 4.0, 110.0, 0.3), (4.0, 4.0, 164.81, 0.2)]
    # Cave: slow low minor notes with echoes over a drone, 8 s.
    minor = [110.0, 130.81, 146.83, 164.81, 196.0]
    cave = [(0.0, 8.0, 55.0, 0.35), (0.0, 8.0, 82.41, 0.2)]
    for i in range(8):
        f = rng.choice(minor)
        for echo in range(3):
            cave.append((i * 1.0 + echo * 0.33, 1.2, f, 0.3 * 0.5 ** echo))
    return {"forest_theme": loop(8.0, forest), "cave_theme": loop(8.0, cave)}


def main():
    rng = random.Random(2026)  # fixed seed: same bytes every run
    for name, samples in sfx(rng).items():
        write(ROOT / "sfx" / f"{name}.wav", normalize(samples))
    for name, samples in music().items():
        write(ROOT / "music" / f"{name}.wav", normalize(samples, 0.6))
    print(f"wrote {ROOT}")


if __name__ == "__main__":
    main()
