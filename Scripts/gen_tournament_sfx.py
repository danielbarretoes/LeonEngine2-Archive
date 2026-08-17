#!/usr/bin/env python3
"""Generate short procedural SFX WAVs for LeonTournament (no external deps)."""

from __future__ import annotations

import math
import struct
import wave
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / "Projects" / "LeonTournament" / "Content" / "Audio"
RATE = 44100


def write_wav(name: str, samples: list[float]) -> None:
    path = OUT / name
    clipped = [max(-1.0, min(1.0, s)) for s in samples]
    with wave.open(str(path), "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        frames = b"".join(struct.pack("<h", int(s * 32767.0)) for s in clipped)
        w.writeframes(frames)
    print(f"wrote {path.name} ({len(samples) / RATE:.2f}s)")


def tone(freq: float, dur: float, vol: float = 0.35, attack: float = 0.01, release: float = 0.08) -> list[float]:
    n = int(RATE * dur)
    out = []
    for i in range(n):
        t = i / RATE
        env = 1.0
        if t < attack:
            env = t / attack
        rem = dur - t
        if rem < release:
            env *= max(0.0, rem / release)
        out.append(math.sin(2.0 * math.pi * freq * t) * vol * env)
    return out


def chord(freqs: list[float], dur: float, vol: float = 0.28) -> list[float]:
    layers = [tone(f, dur, vol / max(1, len(freqs))) for f in freqs]
    n = max(len(x) for x in layers)
    out = [0.0] * n
    for layer in layers:
        for i, s in enumerate(layer):
            out[i] += s
    return out


def sweep(f0: float, f1: float, dur: float, vol: float = 0.32) -> list[float]:
    n = int(RATE * dur)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / RATE
        f = f0 + (f1 - f0) * (t / dur)
        phase += 2.0 * math.pi * f / RATE
        env = 1.0
        if t < 0.02:
            env = t / 0.02
        if dur - t < 0.1:
            env *= max(0.0, (dur - t) / 0.1)
        out.append(math.sin(phase) * vol * env)
    return out


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    write_wav("SFX_Countdown.wav", tone(660, 0.12, 0.4) + [0.0] * int(RATE * 0.05))
    write_wav("SFX_MatchStart.wav", chord([523.25, 659.25, 783.99], 0.55, 0.4))
    write_wav("SFX_DoubleKill.wav", tone(880, 0.1, 0.4) + tone(1174.7, 0.18, 0.42))
    write_wav("SFX_TripleKill.wav", tone(880, 0.08, 0.38) + tone(1174.7, 0.1, 0.4) + tone(1568, 0.2, 0.45))
    write_wav("SFX_YouDied.wav", sweep(320, 90, 0.55, 0.38))
    write_wav("SFX_MatchEnd.wav", chord([392, 494, 587], 0.8, 0.36))
    write_wav("SFX_Announce.wav", tone(740, 0.16, 0.3))
    write_wav("SFX_PauseOpen.wav", tone(420, 0.08, 0.25) + tone(560, 0.1, 0.28))


if __name__ == "__main__":
    main()
