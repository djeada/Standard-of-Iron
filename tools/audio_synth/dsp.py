"""A very small synthesis toolkit, standard library only.

The game's cue sounds are short, dry and abstract, so they do not need a
sampler or a recording booth -- they need noise, a few resonators and
envelopes. Keeping this dependency free means `make audio-assets` works on any
checkout that already has Python and ffmpeg, and the sounds stay regenerable
rather than being binary blobs nobody can edit.

Buffers are plain Python lists of floats in roughly [-1, 1], mono, at SR.
"""

from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path

SR = 32000

TWO_PI = 2.0 * math.pi


_VARIANT = 0


_VARIANT_DETUNE = (0.0, 0.029, -0.034, 0.016, -0.021)

_SEED_STRIDE = 10007


def set_variant(index: int) -> None:
    """Select which take of a recipe the primitives should render."""
    global _VARIANT
    _VARIANT = max(0, int(index))


def variant() -> int:
    return _VARIANT


def salt(seed: int) -> int:
    """Seed for the current variant."""
    return seed + _VARIANT * _SEED_STRIDE


def detune(freq: float) -> float:
    """Pitch for the current variant."""
    if _VARIANT == 0:
        return freq
    return freq * (1.0 + _VARIANT_DETUNE[_VARIANT % len(_VARIANT_DETUNE)])


def seconds(count: float) -> int:
    """Sample count for a duration."""
    return max(1, int(round(count * SR)))


def silence(n: int) -> list[float]:
    return [0.0] * n


def noise(n: int, seed: int) -> list[float]:
    rng = random.Random(salt(seed))
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def pink(n: int, seed: int) -> list[float]:
    """Voss-McCartney style pink-ish noise. Warmer than white for cloth and air."""
    rng = random.Random(salt(seed))
    rows = 5
    values = [rng.uniform(-1.0, 1.0) for _ in range(rows)]
    out = []
    for i in range(n):
        for row in range(rows):
            if i % (1 << row) == 0:
                values[row] = rng.uniform(-1.0, 1.0)
        out.append(sum(values) / rows)
    return out


def sine(freq: float, n: int, phase: float = 0.0) -> list[float]:
    step = TWO_PI * detune(freq) / SR
    return [math.sin(phase + step * i) for i in range(n)]


def sweep(f0: float, f1: float, n: int, curve: float = 1.0) -> list[float]:
    """Sine with a frequency glide. curve > 1 spends longer near f0."""
    f0, f1 = detune(f0), detune(f1)
    out = []
    phase = 0.0
    for i in range(n):
        t = (i / max(1, n - 1)) ** curve
        freq = f0 + (f1 - f0) * t
        phase += TWO_PI * freq / SR
        out.append(math.sin(phase))
    return out


def modal(
    n: int,
    partials: list[tuple[float, float, float]],
    seed: int = 0,
) -> list[float]:
    """A struck body: exponentially decaying sinusoids.

    partials is a list of (frequency_hz, decay_seconds, amplitude). This is the
    workhorse for shield knocks, wood blocks, bells and bronze latches -- the
    partial ratios are what make it read as wood or metal.
    """
    rng = random.Random(salt(seed))
    out = silence(n)
    for freq, decay, amp in partials:
        freq = detune(freq)
        if freq <= 0.0 or freq >= SR * 0.5:
            continue
        phase = rng.uniform(0.0, TWO_PI)
        step = TWO_PI * freq / SR
        tau = max(1e-4, decay)
        for i in range(n):
            out[i] += amp * math.exp(-i / (tau * SR)) * math.sin(phase + step * i)
    return out


def grains(
    n: int,
    seed: int,
    count: int,
    spread: float,
    grain_ms: tuple[float, float],
    freq: tuple[float, float],
    decay_curve: float = 2.0,
) -> list[float]:
    """A scatter of tiny filtered clicks: gravel, rubble, mail rustle."""
    rng = random.Random(salt(seed))
    out = silence(n)
    for _ in range(count):

        at = int(n * spread * (rng.random() ** decay_curve))
        if at >= n:
            continue
        length = seconds(rng.uniform(*grain_ms) / 1000.0)
        centre = rng.uniform(*freq)
        grain = bandpass(noise(length, rng.randrange(1 << 30)), centre, 3.0)
        grain = apply(grain, env_perc(length, attack=0.001, curve=3.0))
        level = rng.uniform(0.3, 1.0)
        for i, sample in enumerate(grain):
            if at + i >= n:
                break
            out[at + i] += sample * level
    return out


def env_perc(n: int, attack: float = 0.002, curve: float = 2.0) -> list[float]:
    """Fast attack, exponential-ish decay across the whole buffer."""
    rise = max(1, seconds(attack))
    out = []
    for i in range(n):
        if i < rise:
            a = i / rise
        else:
            a = 1.0
        t = (i - rise) / max(1, n - rise)
        out.append(a * max(0.0, (1.0 - t)) ** curve)
    return out


def env_ad(n: int, attack: float, decay: float, curve: float = 2.0) -> list[float]:
    rise = max(1, seconds(attack))
    fall = max(1, seconds(decay))
    out = []
    for i in range(n):
        if i < rise:
            out.append(i / rise)
        elif i < rise + fall:
            t = (i - rise) / fall
            out.append(max(0.0, 1.0 - t) ** curve)
        else:
            out.append(0.0)
    return out


def env_swell(n: int, peak: float = 0.4, curve: float = 1.6) -> list[float]:
    """Slow in, slow out. For breaths and warm musical beds."""
    top = max(1, int(n * peak))
    out = []
    for i in range(n):
        if i < top:
            out.append((i / top) ** curve)
        else:
            t = (i - top) / max(1, n - top)
            out.append(max(0.0, 1.0 - t) ** curve)
    return out


def _biquad(buf: list[float], b0, b1, b2, a0, a1, a2) -> list[float]:
    b0, b1, b2 = b0 / a0, b1 / a0, b2 / a0
    a1, a2 = a1 / a0, a2 / a0
    x1 = x2 = y1 = y2 = 0.0
    out = []
    for x0 in buf:
        y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        out.append(y0)
        x2, x1 = x1, x0
        y2, y1 = y1, y0
    return out


def _coeffs(kind: str, freq: float, q: float):
    freq = min(max(freq, 20.0), SR * 0.45)
    w0 = TWO_PI * freq / SR
    cos_w0 = math.cos(w0)
    alpha = math.sin(w0) / (2.0 * max(0.05, q))
    if kind == "lp":
        b0 = (1.0 - cos_w0) / 2.0
        b1 = 1.0 - cos_w0
        b2 = b0
    elif kind == "hp":
        b0 = (1.0 + cos_w0) / 2.0
        b1 = -(1.0 + cos_w0)
        b2 = b0
    elif kind == "bp":
        b0 = alpha
        b1 = 0.0
        b2 = -alpha
    else:
        raise ValueError(kind)
    return b0, b1, b2, 1.0 + alpha, -2.0 * cos_w0, 1.0 - alpha


def lowpass(buf, freq, q=0.707):
    return _biquad(buf, *_coeffs("lp", freq, q))


def highpass(buf, freq, q=0.707):
    return _biquad(buf, *_coeffs("hp", freq, q))


def bandpass(buf, freq, q=2.0):
    return _biquad(buf, *_coeffs("bp", freq, q))


def resonate(buf, freq, q, gain=1.0):
    """Add a resonant band back on top of the source. Cheap formant / body."""
    return mix(buf, gain_of(bandpass(buf, freq, q), gain))


def moving_bandpass(buf, f0: float, f1: float, q: float) -> list[float]:
    """Bandpass whose centre glides. Creaks, scrapes and whooshes live here."""
    n = len(buf)
    out = []
    x1 = x2 = y1 = y2 = 0.0
    for i, x0 in enumerate(buf):
        t = i / max(1, n - 1)
        b0, b1, b2, a0, a1, a2 = _coeffs("bp", f0 + (f1 - f0) * t, q)
        b0, b1, b2 = b0 / a0, b1 / a0, b2 / a0
        na1, na2 = a1 / a0, a2 / a0
        y0 = b0 * x0 + b1 * x1 + b2 * x2 - na1 * y1 - na2 * y2
        out.append(y0)
        x2, x1 = x1, x0
        y2, y1 = y1, y0
    return out


def softclip(buf: list[float], drive: float = 2.0) -> list[float]:
    return [math.tanh(x * drive) / math.tanh(drive) for x in buf]


def tremolo(buf: list[float], rate: float, depth: float, seed: int = 0) -> list[float]:
    """Irregular amplitude wobble. Stick-slip creak, breath texture."""
    rng = random.Random(salt(seed))
    jitter = rng.uniform(0.8, 1.2)
    out = []
    for i, x in enumerate(buf):
        lfo = 0.5 + 0.5 * math.sin(TWO_PI * rate * jitter * i / SR)
        out.append(x * (1.0 - depth + depth * lfo))
    return out


def tail(buf: list[float], decay: float, mix_level: float = 0.25) -> list[float]:
    """A couple of short comb delays: a room, not a cathedral."""
    out = list(buf)
    for delay_ms, level in ((23.0, 1.0), (37.0, 0.8), (51.0, 0.6)):
        d = seconds(delay_ms / 1000.0)
        feedback = math.exp(-delay_ms / 1000.0 / max(1e-3, decay))
        buffered = [0.0] * len(out)
        for i in range(len(out)):
            prev = buffered[i - d] if i >= d else 0.0
            buffered[i] = out[i] + prev * feedback
        for i in range(len(out)):
            out[i] += buffered[i] * mix_level * level * 0.3
    return out


def apply(buf: list[float], envelope: list[float]) -> list[float]:
    return [x * e for x, e in zip(buf, envelope, strict=False)]


def gain_of(buf: list[float], amount: float) -> list[float]:
    return [x * amount for x in buf]


def mix(*buffers: list[float]) -> list[float]:
    length = max((len(b) for b in buffers), default=0)
    out = silence(length)
    for buf in buffers:
        for i, x in enumerate(buf):
            out[i] += x
    return out


def place(target: list[float], buf: list[float], at_seconds: float) -> list[float]:
    """Mix buf into target starting at a time offset."""
    at = seconds(at_seconds)
    out = list(target)
    if at + len(buf) > len(out):
        out.extend(silence(at + len(buf) - len(out)))
    for i, x in enumerate(buf):
        out[at + i] += x
    return out


def fade_out(buf: list[float], duration: float = 0.01) -> list[float]:
    n = min(len(buf), seconds(duration))
    out = list(buf)
    for i in range(n):
        out[len(out) - n + i] *= 1.0 - (i / max(1, n - 1))
    return out


def fade_in(buf: list[float], duration: float = 0.002) -> list[float]:
    n = min(len(buf), seconds(duration))
    out = list(buf)
    for i in range(n):
        out[i] *= i / max(1, n - 1)
    return out


def normalize(buf: list[float], peak_dbfs: float = -1.0) -> list[float]:
    peak = max((abs(x) for x in buf), default=0.0)
    if peak <= 1e-9:
        return buf
    target = 10.0 ** (peak_dbfs / 20.0)
    return gain_of(buf, target / peak)


def at_db(buf: list[float], peak_dbfs: float) -> list[float]:
    """Normalise one layer to a peak before mixing.

    Layered recipes are otherwise at the mercy of whichever element happens to
    be loudest: a resonant creak peaks far lower than a struck thud, so mixing
    raw and normalising the sum leaves the creak inaudible. Stating each
    layer's level makes the balance the thing you read in the recipe.
    """
    return normalize(buf, peak_dbfs)


def trim_silence(buf: list[float], floor_dbfs: float = -60.0) -> list[float]:
    """Drop leading and trailing near-silence so short cues stay short."""
    floor = 10.0 ** (floor_dbfs / 20.0)
    start = 0
    while start < len(buf) and abs(buf[start]) < floor:
        start += 1
    end = len(buf)
    while end > start and abs(buf[end - 1]) < floor:
        end -= 1
    if end <= start:
        return buf
    return buf[start:end]


def finish(buf: list[float], peak_dbfs: float = -1.5) -> list[float]:
    """Every cue leaves through here: trimmed, de-clicked and levelled."""
    out = trim_silence(buf)
    out = fade_in(out, 0.0015)
    out = fade_out(out, 0.008)
    return normalize(out, peak_dbfs)


def write_wav(path: Path, buf: list[float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    frames = bytearray()
    for x in buf:
        clamped = max(-1.0, min(1.0, x))
        frames += struct.pack("<h", int(clamped * 32767.0))
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(SR)
        handle.writeframes(bytes(frames))
