#!/usr/bin/env python3
"""Reproduce the bundled, original drum one-shots. No third-party sample content."""
import math
from pathlib import Path
import random
import struct
import wave

RATE = 48000
ROOT = Path(__file__).resolve().parents[1] / "assets" / "samples"


def generate(name, duration, synth):
    rng = random.Random(2026)
    samples = []
    phase = 0.0
    previous_noise = 0.0
    for i in range(int(duration * RATE)):
        t = i / RATE
        noise = rng.uniform(-1.0, 1.0)
        high = (noise - previous_noise) * 0.5
        previous_noise = noise
        phase += 2 * math.pi * (47 + 115 * math.exp(-t * 38)) / RATE
        value = synth(t, phase, noise, high)
        value *= min(1.0, i / 32) * min(1.0, (duration - t) * 500)
        samples.append(max(-32767, min(32767, round(value * 30000))))
    with wave.open(str(ROOT / f"{name}.wav"), "wb") as wav:
        wav.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
        wav.writeframes(struct.pack(f"<{len(samples)}h", *samples))


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    generate("kick", 0.55, lambda t, p, n, h:
             math.sin(p) * math.exp(-t * 10) * 0.94 + h * math.exp(-t * 180) * 0.16)
    generate("snare", 0.32, lambda t, p, n, h:
             n * math.exp(-t * 20) * 0.63 + math.sin(2 * math.pi * 185 * t) * math.exp(-t * 30) * 0.32)
    generate("hat", 0.13, lambda t, p, n, h: h * math.exp(-t * 48) * 0.70)
    generate("clap", 0.32, lambda t, p, n, h:
             h * 0.85 * sum(math.exp(-(t - onset) * decay) if t >= onset else 0
                            for onset, decay in ((0, 220), (0.012, 220), (0.024, 30))))


if __name__ == "__main__":
    main()
