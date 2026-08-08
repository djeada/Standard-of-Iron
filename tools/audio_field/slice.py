"""Cut individual hits out of a continuous recording.

The CC0 libraries this project sources from record a _performance_: one file
holds forty sword hits, or twenty footsteps, in a row. A cue needs one hit.
This finds the onsets, scores them, and returns the ones worth keeping.

Detection is envelope-based rather than spectral, which is enough here because
every source is a foreground performance against a near-silent floor -- there
is no music or speech to confuse a simple rise detector, and a spectral flux
detector would only add ways to be wrong.
"""

from __future__ import annotations

import math
from dataclasses import dataclass


@dataclass(frozen=True)
class Hit:
    """One detected transient, as sample offsets into the source."""

    start: int
    peak: int
    end: int
    level: float
    """Peak absolute sample value, 0..1."""


def envelope(samples: list[float], window: int) -> list[float]:
    """Block-max envelope: the loudest sample in each `window`."""
    return [
        max((abs(s) for s in samples[i : i + window]), default=0.0)
        for i in range(0, len(samples), window)
    ]


def noise_floor(env: list[float]) -> float:
    """The level the quiet parts sit at, as the 20th percentile of the blocks.

    A mean would be dragged up by the hits themselves, which is the opposite
    of what a floor is for.
    """
    if not env:
        return 0.0
    ordered = sorted(env)
    return ordered[max(0, int(len(ordered) * 0.2) - 1)]


def find_hits(
    samples: list[float],
    rate: int,
    *,
    block_ms: float = 2.0,
    open_db: float = 18.0,
    close_db: float = 6.0,
    min_gap_ms: float = 60.0,
    tail_ms: float = 400.0,
) -> list[Hit]:
    """Every transient in `samples`, loudest first.

    `open_db` above the noise floor starts a hit and `close_db` ends it -- two
    thresholds rather than one, so a decaying tail does not chatter the gate
    back and forth and split one hit into five. `min_gap_ms` then merges hits
    that start too close together to be separate performances.
    """
    window = max(1, int(rate * block_ms / 1000.0))
    env = envelope(samples, window)
    if not env:
        return []

    floor = max(noise_floor(env), 1.0e-5)
    open_level = floor * (10.0 ** (open_db / 20.0))
    close_level = floor * (10.0 ** (close_db / 20.0))
    min_gap = int(min_gap_ms / block_ms)
    tail = int(tail_ms / block_ms)

    hits: list[Hit] = []
    index = 0
    while index < len(env):
        if env[index] < open_level:
            index += 1
            continue

        start = index
        peak = index
        quiet = 0
        while index < len(env) and quiet < tail:
            if env[index] > env[peak]:
                peak = index
            quiet = quiet + 1 if env[index] < close_level else 0
            index += 1

        end = index
        if hits and start - hits[-1].start < min_gap:
            previous = hits[-1]
            merged_peak = peak if env[peak] > env[previous.peak] else previous.peak
            hits[-1] = Hit(previous.start, merged_peak, end, env[merged_peak])
        else:
            hits.append(Hit(start, peak, end, env[peak]))

    return [
        Hit(h.start * window, h.peak * window, h.end * window, h.level) for h in hits
    ]


def cut(
    samples: list[float],
    hit: Hit,
    rate: int,
    *,
    lead_ms: float = 8.0,
    length_ms: float = 0.0,
    fade_out_ms: float = 40.0,
) -> list[float]:
    """One hit as its own buffer, with a lead-in and a faded tail.

    The lead-in matters: cutting exactly on the onset clips the attack, and the
    attack is the entire character of an impact. The fade-out matters for the
    opposite reason -- ending on a non-zero sample is a click, and a click on a
    cue that fires every 90 ms is what the player will actually hear.
    """
    lead = int(rate * lead_ms / 1000.0)
    start = max(0, hit.start - lead)
    if length_ms > 0.0:
        end = min(len(samples), start + int(rate * length_ms / 1000.0))
    else:
        end = min(len(samples), hit.end)

    out = samples[start:end]
    fade = min(int(rate * fade_out_ms / 1000.0), len(out))
    for k in range(fade):
        t = (k / fade) * 0.5 * math.pi
        out[len(out) - fade + k] *= math.cos(t)
    return out


def normalise(samples: list[float], peak: float = 0.89) -> list[float]:
    highest = max((abs(s) for s in samples), default=0.0)
    if highest <= 0.0:
        return samples
    scale = peak / highest
    return [s * scale for s in samples]
