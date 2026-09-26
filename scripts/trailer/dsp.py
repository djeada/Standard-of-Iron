"""Signal-processing toolkit for the trailer mix.

Everything works on float32 arrays shaped ``(samples, 2)`` at ``RATE``. The
functions are deliberately small and composable: the score, the effects layer
and the master bus in ``mix.py`` are all built from them, so a change of taste
is a change of numbers in ``cut.json`` rather than of code.
"""

from __future__ import annotations

import functools
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy import signal

RATE = 48_000


def seconds(n: int) -> float:
    return n / RATE


def samples(t: float) -> int:
    return int(round(t * RATE))


def silence(duration: float) -> np.ndarray:
    return np.zeros((samples(duration), 2), dtype=np.float32)


@functools.lru_cache(maxsize=256)
def _load_cached(path: str) -> np.ndarray:
    data, rate = sf.read(path, dtype="float32", always_2d=True)
    if data.shape[1] == 1:
        data = np.repeat(data, 2, axis=1)
    elif data.shape[1] > 2:
        data = data[:, :2]
    if rate != RATE:
        g = np.gcd(rate, RATE)
        data = signal.resample_poly(data, RATE // g, rate // g, axis=0).astype(
            np.float32
        )
    return data


def load(path: str | Path) -> np.ndarray:
    return _load_cached(str(Path(path).resolve())).copy()


def save(path: str | Path, audio: np.ndarray, subtype: str = "PCM_24") -> None:
    sf.write(str(path), audio, RATE, subtype=subtype)


def db(value: float) -> float:
    return float(10.0 ** (value / 20.0))


def place(bus: np.ndarray, clip: np.ndarray, at: float) -> None:
    """Add ``clip`` into ``bus`` starting at ``at`` seconds (clipped to the bus)."""
    start = samples(at)
    if start < 0:
        clip = clip[-start:]
        start = 0
    end = min(bus.shape[0], start + clip.shape[0])
    if end > start:
        bus[start:end] += clip[: end - start]


def trim(audio: np.ndarray, start: float, duration: float | None = None) -> np.ndarray:
    a = samples(start)
    b = audio.shape[0] if duration is None else a + samples(duration)
    out = audio[a:b]
    if out.shape[0] < b - a:
        out = np.concatenate([out, np.zeros((b - a - out.shape[0], 2), np.float32)])
    return out


def fade(
    audio: np.ndarray, fade_in: float = 0.0, fade_out: float = 0.0, shape: str = "equal"
) -> np.ndarray:
    out = audio.copy()
    n = out.shape[0]
    if fade_in > 0:
        k = min(n, samples(fade_in))
        ramp = np.linspace(0.0, 1.0, k, dtype=np.float32)
        ramp = np.sin(ramp * np.pi / 2) if shape == "equal" else ramp
        out[:k] *= ramp[:, None]
    if fade_out > 0:
        k = min(n, samples(fade_out))
        ramp = np.linspace(1.0, 0.0, k, dtype=np.float32)
        ramp = np.sin(ramp * np.pi / 2) if shape == "equal" else ramp
        out[n - k :] *= ramp[:, None]
    return out


def envelope(n: int, points: list[tuple[float, float]]) -> np.ndarray:
    """Piecewise-linear gain curve from ``(seconds, dB)`` points."""
    if not points:
        return np.ones(n, np.float32)
    times = np.array([samples(t) for t, _ in points], dtype=np.float64)
    gains = np.array([db(g) for _, g in points], dtype=np.float64)
    return np.interp(np.arange(n), times, gains).astype(np.float32)


def automate(audio: np.ndarray, points: list[tuple[float, float]]) -> np.ndarray:
    return audio * envelope(audio.shape[0], points)[:, None]


def mono(audio: np.ndarray) -> np.ndarray:
    return audio.mean(axis=1)


def pan(audio: np.ndarray, position: float | list[tuple[float, float]]) -> np.ndarray:
    """Equal-power pan. ``position`` is -1 (left) .. 1 (right), or a list of
    ``(seconds, position)`` points for a moving source."""
    source = mono(audio)
    n = source.shape[0]
    if isinstance(position, (int, float)):
        p = np.full(n, float(position), np.float32)
    else:
        times = np.array([samples(t) for t, _ in position], np.float64)
        values = np.array([v for _, v in position], np.float64)
        p = np.interp(np.arange(n), times, values).astype(np.float32)
    angle = (np.clip(p, -1, 1) + 1.0) * np.pi / 4
    return np.stack([source * np.cos(angle), source * np.sin(angle)], axis=1) * np.sqrt(
        2
    )


def width(audio: np.ndarray, amount: float) -> np.ndarray:
    mid = (audio[:, 0] + audio[:, 1]) * 0.5
    side = (audio[:, 0] - audio[:, 1]) * 0.5 * amount
    return np.stack([mid + side, mid - side], axis=1).astype(np.float32)


def _sos(kind: str, freq, order: int = 2):
    return signal.butter(order, freq, btype=kind, fs=RATE, output="sos")


def lowpass(audio: np.ndarray, freq: float, order: int = 2) -> np.ndarray:
    return signal.sosfilt(
        _sos("lowpass", min(freq, RATE * 0.45), order), audio, axis=0
    ).astype(np.float32)


def highpass(audio: np.ndarray, freq: float, order: int = 2) -> np.ndarray:
    return signal.sosfilt(_sos("highpass", freq, order), audio, axis=0).astype(
        np.float32
    )


def bandpass(audio: np.ndarray, low: float, high: float, order: int = 2) -> np.ndarray:
    return signal.sosfilt(
        _sos("bandpass", [low, min(high, RATE * 0.45)], order), audio, axis=0
    ).astype(np.float32)


def shelf(
    audio: np.ndarray, freq: float, gain_db: float, kind: str = "low"
) -> np.ndarray:
    """RBJ shelving biquad."""
    a = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * freq / RATE
    alpha = np.sin(w0) / 2 * np.sqrt(2)
    cw = np.cos(w0)
    if kind == "low":
        b0 = a * ((a + 1) - (a - 1) * cw + 2 * np.sqrt(a) * alpha)
        b1 = 2 * a * ((a - 1) - (a + 1) * cw)
        b2 = a * ((a + 1) - (a - 1) * cw - 2 * np.sqrt(a) * alpha)
        a0 = (a + 1) + (a - 1) * cw + 2 * np.sqrt(a) * alpha
        a1 = -2 * ((a - 1) + (a + 1) * cw)
        a2 = (a + 1) + (a - 1) * cw - 2 * np.sqrt(a) * alpha
    else:
        b0 = a * ((a + 1) + (a - 1) * cw + 2 * np.sqrt(a) * alpha)
        b1 = -2 * a * ((a - 1) + (a + 1) * cw)
        b2 = a * ((a + 1) + (a - 1) * cw - 2 * np.sqrt(a) * alpha)
        a0 = (a + 1) - (a - 1) * cw + 2 * np.sqrt(a) * alpha
        a1 = 2 * ((a - 1) - (a + 1) * cw)
        a2 = (a + 1) - (a - 1) * cw - 2 * np.sqrt(a) * alpha
    sos = np.array([[b0 / a0, b1 / a0, b2 / a0, 1.0, a1 / a0, a2 / a0]])
    return signal.sosfilt(sos, audio, axis=0).astype(np.float32)


def peak_eq(
    audio: np.ndarray, freq: float, gain_db: float, q: float = 1.0
) -> np.ndarray:
    a = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * freq / RATE
    alpha = np.sin(w0) / (2 * q)
    cw = np.cos(w0)
    b = np.array([1 + alpha * a, -2 * cw, 1 - alpha * a])
    den = np.array([1 + alpha / a, -2 * cw, 1 - alpha / a])
    sos = np.concatenate([b / den[0], den / den[0]])[None, :]
    return signal.sosfilt(sos, audio, axis=0).astype(np.float32)


def sweep_lowpass(
    audio: np.ndarray, points: list[tuple[float, float]], block: int = 1024
) -> np.ndarray:
    """Time-varying one-pole-pair lowpass: ``points`` are ``(seconds, hz)``."""
    n = audio.shape[0]
    times = np.array([samples(t) for t, _ in points], np.float64)
    freqs = np.array([np.log(f) for _, f in points], np.float64)
    out = np.empty_like(audio)
    zi = None
    for start in range(0, n, block):
        end = min(n, start + block)
        f = float(np.exp(np.interp((start + end) / 2, times, freqs)))
        sos = _sos("lowpass", min(max(f, 20.0), RATE * 0.45), 2)
        if zi is None:
            zi = np.zeros((sos.shape[0], 2, 2))
        chunk, zi = signal.sosfilt(sos, audio[start:end], axis=0, zi=zi)
        out[start:end] = chunk
    return out.astype(np.float32)


@functools.lru_cache(maxsize=16)
def _impulse(decay: float, predelay: float, damping: float, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed)
    n = samples(decay * 1.4)
    t = np.arange(n) / RATE
    noise = rng.standard_normal((n, 2)).astype(np.float32)
    env = np.exp(-6.9 * t / decay)[:, None]
    ir = noise * env
    ir = lowpass(ir, damping)
    early = np.zeros_like(ir)
    for k in range(10):
        pos = samples(0.004 + rng.random() * 0.045)
        early[pos] += (0.7**k) * (rng.random(2) - 0.5)
    ir = ir + early
    ir = np.concatenate([np.zeros((samples(predelay), 2), np.float32), ir])
    return (ir / np.sqrt(np.sum(ir**2) / 2)).astype(np.float32)


def reverb(
    audio: np.ndarray,
    mix: float,
    decay: float = 2.2,
    predelay: float = 0.02,
    damping: float = 6000.0,
    seed: int = 3,
) -> np.ndarray:
    """Synthetic stereo hall. Returns dry + wet, longer by the tail."""
    if mix <= 0:
        return audio
    ir = _impulse(round(decay, 2), round(predelay, 3), round(damping), seed)
    tail = ir.shape[0]
    padded = np.concatenate([audio, np.zeros((tail, 2), np.float32)])
    wet = np.stack(
        [
            signal.fftconvolve(padded[:, 0], ir[:, 0])[: padded.shape[0]],
            signal.fftconvolve(padded[:, 1], ir[:, 1])[: padded.shape[0]],
        ],
        axis=1,
    ).astype(np.float32)
    return padded * (1 - mix * 0.5) + wet * mix * 0.35


def distance(audio: np.ndarray, metres: float) -> np.ndarray:
    """Air absorption and level for a source ``metres`` away (reference 5 m)."""
    metres = max(1.0, metres)
    cutoff = float(np.clip(18000 * (8.0 / metres) ** 0.55, 900, 18000))
    out = lowpass(audio, cutoff, 2)
    gain = min(1.4, 5.0 / metres) ** 0.8
    return out * gain


def varispeed(audio: np.ndarray, ratio: float) -> np.ndarray:
    """Pitch and time together (tape-style); ratio < 1 is slower and lower."""
    n = int(audio.shape[0] / ratio)
    return signal.resample(audio, n, axis=0).astype(np.float32)


def normalize_peak(audio: np.ndarray, peak_db: float = -1.0) -> np.ndarray:
    peak = float(np.max(np.abs(audio))) or 1.0
    return audio * (db(peak_db) / peak)


def rms_db(audio: np.ndarray) -> float:
    return float(20 * np.log10(np.sqrt(np.mean(audio**2)) + 1e-9))


# --- synthesis -------------------------------------------------------------


def _t(duration: float) -> np.ndarray:
    return np.arange(samples(duration)) / RATE


def sub_drop(
    duration: float = 3.5,
    start_hz: float = 62.0,
    end_hz: float = 27.0,
    attack: float = 0.004,
) -> np.ndarray:
    t = _t(duration)
    freq = end_hz + (start_hz - end_hz) * np.exp(-t * 2.4)
    phase = 2 * np.pi * np.cumsum(freq) / RATE
    env = np.minimum(1.0, t / attack) * np.exp(-t * 1.25)
    tone = np.sin(phase) + 0.18 * np.sin(2 * phase)
    sig = (tone * env).astype(np.float32)
    return np.stack([sig, sig], axis=1)


def noise(duration: float, seed: int = 1, stereo: bool = True) -> np.ndarray:
    rng = np.random.default_rng(seed)
    n = samples(duration)
    if stereo:
        return rng.standard_normal((n, 2)).astype(np.float32)
    m = rng.standard_normal(n).astype(np.float32)
    return np.stack([m, m], axis=1)


def impact(
    duration: float = 4.0, weight: float = 1.0, brightness: float = 0.5, seed: int = 5
) -> np.ndarray:
    """Layered trailer hit: sub drop, low body thump, filtered noise crack, hall."""
    t = _t(duration)
    body_f = 58 * np.exp(-t * 9) + 41
    body = np.sin(2 * np.pi * np.cumsum(body_f) / RATE) * np.exp(-t * 6.5)
    body = np.stack([body, body], axis=1).astype(np.float32)
    crack = noise(0.35, seed) * np.exp(-_t(0.35) * 22)[:, None]
    crack = bandpass(crack, 180, 2500 + 6000 * brightness)
    crack = np.concatenate(
        [crack, np.zeros((samples(duration) - crack.shape[0], 2), np.float32)]
    )
    sub = sub_drop(duration, 55, 26)
    hit = sub * 0.9 * weight + body * 0.8 * weight + crack * 0.55
    hit = reverb(hit, 0.55, decay=3.2, predelay=0.012, damping=4200, seed=seed)
    return (hit[: samples(duration + 2.5)] * 0.6).astype(np.float32)


def boom(duration: float = 5.0, seed: int = 8) -> np.ndarray:
    """Distant, dark cinematic boom (cut-to-black punctuation)."""
    base = impact(duration, weight=1.2, brightness=0.1, seed=seed)
    return lowpass(base, 900) * 1.3


def riser(duration: float = 4.0, seed: int = 11, top_hz: float = 9000.0) -> np.ndarray:
    """Filtered-noise swell with rising tonal shimmer, peaking at the end."""
    t = _t(duration)
    n = noise(duration, seed)
    swell = (t / duration) ** 2.2
    shaped = sweep_lowpass(n, [(0.0, 300.0), (duration, top_hz)]) * swell[:, None]
    tones = np.zeros_like(t)
    for k, f0 in enumerate((110.0, 164.8, 220.0)):
        f = f0 * (1 + 0.9 * (t / duration) ** 2)
        tones += np.sin(2 * np.pi * np.cumsum(f) / RATE + k) * (0.25 / (k + 1))
    tones = tones * swell
    out = shaped * 0.35 + np.stack([tones, np.roll(tones, 480)], axis=1)
    return reverb(out.astype(np.float32), 0.3, decay=1.6)[: samples(duration)]


def reverse_swell(source: np.ndarray, duration: float = 2.0) -> np.ndarray:
    """Reverse of a reverberated sound, ending on the downbeat."""
    wet = reverb(source, 1.0, decay=3.0, damping=5000)
    rev = wet[::-1][-samples(duration) :]
    rev = rev * np.linspace(0.0, 1.0, rev.shape[0])[:, None] ** 1.5
    return fade(rev.astype(np.float32), 0.2, 0.0)


def whoosh(duration: float = 1.2, seed: int = 21, centre: float = 0.55) -> np.ndarray:
    t = _t(duration)
    n = noise(duration, seed)
    peak = centre * duration
    env = np.exp(-((t - peak) ** 2) / (2 * (duration * 0.18) ** 2))
    out = np.zeros_like(n)
    for lo, hi in ((300, 1200), (900, 3500), (2500, 8000)):
        out += bandpass(n, lo, hi)
    out = out * env[:, None] * 0.4
    return pan(out, [(0.0, -0.7), (duration, 0.7)]).astype(np.float32)


def drone(
    duration: float, root_hz: float = 36.7, seed: int = 31, darkness: float = 900.0
) -> np.ndarray:
    """Slowly beating low cluster with filtered air on top."""
    t = _t(duration)
    sig = np.zeros((t.shape[0], 2), np.float32)
    for k, (mult, det) in enumerate(
        ((1.0, 0.0), (1.0, 0.35), (1.5, -0.2), (2.0, 0.12))
    ):
        f = root_hz * mult + det
        lfo = 0.7 + 0.3 * np.sin(2 * np.pi * (0.05 + 0.02 * k) * t + k)
        tone = np.sin(2 * np.pi * f * t + k) * lfo / (k + 1.2)
        sig[:, k % 2] += tone
        sig[:, (k + 1) % 2] += tone * 0.6
    air = lowpass(noise(duration, seed), darkness) * 0.25
    return (sig * 0.5 + air).astype(np.float32)


# --- dynamics ---------------------------------------------------------------


def compress(
    audio: np.ndarray,
    threshold_db: float = -18.0,
    ratio: float = 2.5,
    attack: float = 0.01,
    release: float = 0.2,
    makeup_db: float = 0.0,
    sidechain: np.ndarray | None = None,
) -> np.ndarray:
    key = np.max(np.abs(sidechain if sidechain is not None else audio), axis=1)
    step = 64
    n = key.shape[0]
    blocks = key[: n - n % step].reshape(-1, step).max(axis=1)
    if n % step:
        blocks = np.append(blocks, key[n - n % step :].max())
    level = 20 * np.log10(blocks + 1e-9)
    a = np.exp(-step / (attack * RATE))
    r = np.exp(-step / (release * RATE))
    gain = np.zeros_like(level)
    g = 0.0
    for i, lv in enumerate(level):
        over = max(0.0, lv - threshold_db)
        target = -over * (1 - 1 / ratio)
        coef = a if target < g else r
        g = coef * g + (1 - coef) * target
        gain[i] = g
    curve = np.repeat(gain, step)[:n]
    return audio * (10 ** ((curve + makeup_db) / 20))[:, None].astype(np.float32)


def duck(
    audio: np.ndarray,
    key: np.ndarray,
    depth_db: float = -6.0,
    threshold_db: float = -30.0,
    attack: float = 0.03,
    release: float = 0.45,
) -> np.ndarray:
    """Pull ``audio`` down by up to ``depth_db`` while ``key`` is loud."""
    lvl = np.max(np.abs(key), axis=1)
    step = 256
    n = lvl.shape[0]
    pad = (-n) % step
    blocks = np.pad(lvl, (0, pad)).reshape(-1, step).max(axis=1)
    level = 20 * np.log10(blocks + 1e-9)
    amount = np.clip((level - threshold_db) / 18.0, 0.0, 1.0) * depth_db
    a = np.exp(-step / (attack * RATE))
    r = np.exp(-step / (release * RATE))
    out = np.zeros_like(amount)
    g = 0.0
    for i, target in enumerate(amount):
        coef = a if target < g else r
        g = coef * g + (1 - coef) * target
        out[i] = g
    curve = np.repeat(out, step)[:n]
    return audio * (10 ** (curve / 20))[:, None].astype(np.float32)


def true_peak_db(audio: np.ndarray) -> float:
    up = signal.resample_poly(audio, 4, 1, axis=0)
    return float(20 * np.log10(np.max(np.abs(up)) + 1e-12))


def limit(
    audio: np.ndarray,
    ceiling_db: float = -1.0,
    lookahead: float = 0.005,
    release: float = 0.08,
) -> np.ndarray:
    """Lookahead brickwall on a 4x-oversampled peak estimate."""
    ceiling = db(ceiling_db)
    up = signal.resample_poly(audio, 4, 1, axis=0)
    peak = np.max(np.abs(up), axis=1).reshape(-1, 4).max(axis=1)[: audio.shape[0]]
    if peak.shape[0] < audio.shape[0]:
        peak = np.pad(peak, (0, audio.shape[0] - peak.shape[0]))
    need = np.minimum(1.0, ceiling / np.maximum(peak, 1e-9))
    la = samples(lookahead)
    win = np.lib.stride_tricks.sliding_window_view(
        np.pad(need, (0, la), constant_values=1.0), la + 1
    ).min(axis=1)
    r = np.exp(-1.0 / (release * RATE))
    gain = np.empty_like(win)
    g = 1.0
    for i, target in enumerate(win):
        g = target if target < g else r * g + (1 - r) * target
        gain[i] = g
    smooth = np.convolve(gain, np.ones(la) / la, mode="same") if la > 1 else gain
    gain = np.minimum(gain, smooth)
    return (audio * gain[:, None]).astype(np.float32)


def integrated_lufs(audio: np.ndarray) -> float:
    import pyloudnorm

    return float(pyloudnorm.Meter(RATE).integrated_loudness(audio.astype(np.float64)))


def short_term_lufs(
    audio: np.ndarray, window: float = 3.0, hop: float = 0.5
) -> list[tuple[float, float]]:
    import pyloudnorm

    meter = pyloudnorm.Meter(RATE, block_size=0.4)
    out = []
    step = samples(hop)
    size = samples(window)
    for start in range(0, max(1, audio.shape[0] - size), step):
        chunk = audio[start : start + size].astype(np.float64)
        try:
            out.append((seconds(start), float(meter.integrated_loudness(chunk))))
        except ValueError:
            out.append((seconds(start), -70.0))
    return out
