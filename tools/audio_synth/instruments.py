"""Reusable sound bodies shared by the cue recipes.

Everything the game needs is built from a handful of physical ideas: something
struck (wood, metal, a drum head), something rubbed (cloth, rope, a hinge),
something scattered (gravel, mail) and something blown (a horn, a breath).
Recipes in cues.py combine these rather than repeating filter maths.
"""

from __future__ import annotations

import random

from dsp import (
    apply,
    bandpass,
    env_perc,
    env_swell,
    gain_of,
    grains,
    highpass,
    lowpass,
    mix,
    modal,
    moving_bandpass,
    noise,
    pink,
    place,
    salt,
    seconds,
    silence,
    sweep,
    tremolo,
)

WOOD_RATIOS = (1.0, 2.57, 4.31, 6.02, 8.19)
BRONZE_RATIOS = (1.0, 2.01, 3.03, 4.21, 5.43, 7.12)
SHIELD_RATIOS = (1.0, 1.94, 3.11, 4.62)


def _partials(ratios, freq, decay, amps=None):
    out = []
    for index, ratio in enumerate(ratios):
        amp = amps[index] if amps else 1.0 / (index + 1.4)
        out.append((freq * ratio, decay / (index * 0.45 + 1.0), amp))
    return out


def transient(length_ms: float, seed: int, centre: float, q: float = 1.0):
    """The scrape of contact that precedes any struck body."""
    n = seconds(length_ms / 1000.0)
    return apply(bandpass(noise(n, seed), centre, q), env_perc(n, 0.0005, 4.0))


def wood(freq: float, decay: float, seed: int, bright: float = 1.0):
    n = seconds(decay * 1.6)
    body = modal(n, _partials(WOOD_RATIOS, freq, decay), seed)
    hit = gain_of(transient(9.0, seed + 1, freq * 5.0, 1.2), 0.5 * bright)
    return apply(mix(body, hit), env_perc(n, 0.0008, 1.4))


def bronze(freq: float, decay: float, seed: int):
    n = seconds(decay * 1.8)
    body = modal(n, _partials(BRONZE_RATIOS, freq, decay), seed)
    hit = gain_of(transient(6.0, seed + 1, freq * 6.0, 0.9), 0.35)
    return apply(mix(body, hit), env_perc(n, 0.0004, 1.1))


def shield(freq: float, decay: float, seed: int):
    """Wood with a leather face and an arm behind it: damped, low mids."""
    n = seconds(decay * 1.5)
    body = modal(n, _partials(SHIELD_RATIOS, freq, decay), seed)
    knock = gain_of(transient(12.0, seed + 2, 1900.0, 0.8), 0.55)
    return lowpass(apply(mix(body, knock), env_perc(n, 0.001, 1.6)), 4200.0)


def whoosh(duration: float, seed: int, f0: float, f1: float, q: float = 1.1):
    n = seconds(duration)
    body = moving_bandpass(pink(n, seed), f0, f1, q)
    return apply(body, env_swell(n, 0.35, 1.4))


def mail(duration: float, seed: int, density: int = 90):
    """Rings of maille shifting: dense, bright, quiet grains."""
    n = seconds(duration)
    return gain_of(
        grains(n, seed, density, 0.85, (3.0, 9.0), (2600.0, 6400.0), 1.6), 0.55
    )


def creak(duration: float, seed: int, f0: float, f1: float, rate: float = 19.0):
    """Stick-slip on rope, leather or an iron hinge."""
    n = seconds(duration)
    body = moving_bandpass(noise(n, seed), f0, f1, 11.0)
    body = tremolo(body, rate, 0.85, seed)
    return apply(body, env_swell(n, 0.45, 1.2))


def breath(duration: float, seed: int, inhale: bool = False, pitch: float = 1.0):
    """Voiceless air with vocal-tract resonances. Effort, exhale, recovery."""
    n = seconds(duration)
    src = pink(n, seed)
    body = bandpass(src, 620.0 * pitch, 1.1)
    body = mix(body, gain_of(bandpass(src, 1180.0 * pitch, 1.6), 0.55))
    body = mix(body, gain_of(bandpass(src, 2450.0 * pitch, 2.2), 0.22))
    envelope = env_swell(n, 0.7 if inhale else 0.22, 1.7)
    return apply(body, envelope)


def water(duration: float, seed: int):
    """Poured water: a bed of hiss plus rising pitch blips."""
    n = seconds(duration)
    rng = random.Random(salt(seed))
    bed = apply(bandpass(pink(n, seed), 2600.0, 0.7), env_swell(n, 0.4, 1.3))
    drops = silence(n)
    for _ in range(26):
        at = rng.random() * duration * 0.9
        length = seconds(rng.uniform(0.008, 0.022))
        f0 = rng.uniform(700.0, 1500.0)
        blip = apply(
            sweep(f0, f0 * rng.uniform(1.6, 2.6), length, 0.7),
            env_perc(length, 0.001, 2.0),
        )
        drops = place(drops, gain_of(blip, rng.uniform(0.08, 0.24)), at)
    return mix(gain_of(bed, 0.5), drops[:n])


def scrape(duration: float, seed: int, f0: float, f1: float):
    n = seconds(duration)
    body = moving_bandpass(noise(n, seed), f0, f1, 5.0)
    body = tremolo(body, 34.0, 0.5, seed + 1)
    return apply(body, env_swell(n, 0.3, 1.3))


def paper(duration: float, seed: int, density: int = 130):
    """Parchment: dry, high, crinkly grains with no low end."""
    n = seconds(duration)
    body = grains(n, seed, density, 0.95, (2.0, 7.0), (1900.0, 6800.0), 1.3)
    return highpass(gain_of(body, 0.8), 900.0)
