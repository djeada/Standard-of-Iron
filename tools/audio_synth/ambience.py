"""Recipes for the looping ambience beds.

The beds that shipped before this were 16 kHz mono ten-second clips whose energy
sat in the 2-6 kHz band the ear is most sensitive to, so the field sounded like
hiss rather than like weather. These are generated at the mixer's own rate, run
twice as long, are built to loop without a seam, and are deliberately shaped so
the loudest thing in them is the low-mid body.

Every layer here is noise, a filter and a slow envelope. Nothing is sampled.
"""

from __future__ import annotations

import math
import random

import dsp

RATE = 48000

LENGTH_SECONDS = 20.0


PEAK_DBFS = -6.0


AIR_ROLLOFF_HZ = 3200.0


def _n() -> int:
    return dsp.seconds(LENGTH_SECONDS)


def _slow_shape(n: int, seed: int, rate_hz: float, depth: float) -> list[float]:
    """A drifting 0..1 contour: gusts, swells, the tide of a crowd."""
    rng = random.Random(dsp.salt(seed))
    phases = [rng.uniform(0.0, dsp.TWO_PI) for _ in range(4)]
    rates = [rate_hz * factor for factor in (1.0, 0.61, 0.37, 0.23)]
    out = []
    for i in range(n):
        total = 0.0
        for phase, hz in zip(phases, rates, strict=False):
            total += math.sin(phase + dsp.TWO_PI * hz * i / dsp.SR)
        contour = 0.5 + 0.5 * (total / len(rates))
        out.append(1.0 - depth + depth * contour)
    return out


def wind(
    seed: int, body_hz: float, gust_hz: float, depth: float, brightness: float = 1.0
) -> list[float]:
    """Air moving over ground: pink noise with a gusting low-pass."""
    n = _n()
    air = dsp.pink(n, seed)
    air = dsp.highpass(air, body_hz * 1.2, 0.7)
    air = dsp.lowpass(air, AIR_ROLLOFF_HZ * brightness, 0.6)
    air = dsp.apply(air, _slow_shape(n, seed + 1, gust_hz, depth))
    rumble = dsp.lowpass(dsp.pink(n, seed + 2), body_hz, 0.9)
    rumble = dsp.apply(rumble, _slow_shape(n, seed + 3, gust_hz * 0.6, depth * 0.8))
    return dsp.mix(dsp.gain_of(air, 1.0), dsp.gain_of(rumble, 0.7))


def rain(
    seed: int, density: int, drop_hz: tuple[float, float], sheet_hz: float
) -> list[float]:
    """A sheet of water plus individual drops, both kept out of the hiss band."""
    n = _n()
    sheet = dsp.lowpass(dsp.noise(n, seed), sheet_hz, 0.7)
    sheet = dsp.highpass(sheet, 180.0, 0.7)
    sheet = dsp.apply(sheet, _slow_shape(n, seed + 1, 0.06, 0.35))
    drops = dsp.grains(
        n,
        seed + 2,
        count=density,
        spread=1.0,
        grain_ms=(3.0, 11.0),
        freq=drop_hz,
        decay_curve=1.0,
    )
    drops = dsp.lowpass(drops, 2600.0, 0.7)
    return dsp.mix(dsp.gain_of(sheet, 0.85), dsp.gain_of(drops, 0.5))


def water(seed: int, centre: float) -> list[float]:
    """Moving water: a burbling band that wanders rather than sits still."""
    n = _n()
    flow = dsp.lowpass(dsp.noise(n, seed), centre * 2.0, 0.6)
    flow = dsp.highpass(flow, 140.0, 0.7)
    burble = dsp.moving_bandpass(
        dsp.noise(n, seed + 1), centre * 0.7, centre * 1.5, 1.4
    )
    burble = dsp.apply(burble, _slow_shape(n, seed + 2, 0.5, 0.6))
    return dsp.mix(dsp.gain_of(flow, 0.7), dsp.gain_of(burble, 0.35))


def leaves(seed: int, rate_hz: float) -> list[float]:
    """Foliage rustle: a mid band that breathes with the gusts."""
    n = _n()
    rustle = dsp.bandpass(dsp.noise(n, seed), 1500.0, 0.8)
    rustle = dsp.lowpass(rustle, 2800.0, 0.7)
    return dsp.apply(rustle, _slow_shape(n, seed + 1, rate_hz, 0.75))


def murmur(seed: int, centre: float, depth: float = 0.5) -> list[float]:
    """Distant human activity, well below the words being audible."""
    n = _n()
    voices = dsp.bandpass(dsp.pink(n, seed), centre, 0.7)
    voices = dsp.lowpass(voices, 1800.0, 0.7)
    voices = dsp.apply(voices, _slow_shape(n, seed + 1, 0.35, depth))
    return voices


def knocks(
    seed: int, count: int, freq: tuple[float, float], level: float = 0.35
) -> list[float]:
    """Sparse work sounds: tent pegs, cart boards, a shield set down."""
    n = _n()
    hits = dsp.grains(
        n,
        seed,
        count=count,
        spread=1.0,
        grain_ms=(18.0, 55.0),
        freq=freq,
        decay_curve=1.0,
    )
    return dsp.gain_of(dsp.lowpass(hits, 1800.0, 0.8), level)


def march(seed: int, bpm: float, level: float = 0.5) -> list[float]:
    """A column somewhere out of sight: a soft periodic tread, not a drum."""
    n = _n()
    out = dsp.silence(n)
    rng = random.Random(dsp.salt(seed))
    step = dsp.SR * 60.0 / bpm
    index = 0
    while True:
        at = int(index * step + rng.uniform(-0.02, 0.02) * step)
        if at >= n:
            break
        length = dsp.seconds(0.09)
        hit = dsp.lowpass(dsp.noise(length, rng.randrange(1 << 30)), 220.0, 0.9)
        hit = dsp.apply(hit, dsp.env_perc(length, attack=0.006, curve=2.2))
        amount = rng.uniform(0.6, 1.0)
        for offset, sample in enumerate(hit):
            if at + offset >= n:
                break
            out[at + offset] += sample * amount
        index += 1
    return dsp.gain_of(out, level)


def fire(seed: int, seethe: float = 0.6) -> list[float]:
    """Burning: a low seethe with crackles, kept dark."""
    n = _n()
    body = dsp.lowpass(dsp.pink(n, seed), 900.0, 0.7)
    body = dsp.apply(body, _slow_shape(n, seed + 1, 0.4, seethe))
    crackle = dsp.grains(
        n,
        seed + 2,
        count=90,
        spread=1.0,
        grain_ms=(4.0, 16.0),
        freq=(700.0, 2100.0),
        decay_curve=1.0,
    )
    return dsp.mix(
        dsp.gain_of(body, 0.8), dsp.gain_of(dsp.lowpass(crackle, 2400.0, 0.7), 0.3)
    )


def gulls(seed: int, count: int) -> list[float]:
    """Very occasional seabirds, so a harbour is not just wind."""
    n = _n()
    out = dsp.silence(n)
    rng = random.Random(dsp.salt(seed))
    for _ in range(count):
        at = rng.randrange(max(1, n - dsp.seconds(0.5)))
        length = dsp.seconds(rng.uniform(0.16, 0.3))
        call = dsp.sweep(
            rng.uniform(900.0, 1200.0), rng.uniform(1500.0, 1900.0), length
        )
        call = dsp.apply(call, dsp.env_ad(length, 0.05, 0.6, 2.0))
        call = dsp.lowpass(call, 2600.0, 0.8)
        for offset, sample in enumerate(call):
            if at + offset >= n:
                break
            out[at + offset] += sample * 0.16
    return out


def snowfall(seed: int, density: int) -> list[float]:
    """Snow is nearly silent: a soft granular whisper over muffled air."""
    n = _n()
    whisper = dsp.bandpass(dsp.noise(n, seed), 1100.0, 0.5)
    whisper = dsp.lowpass(whisper, 1800.0, 0.7)
    whisper = dsp.apply(whisper, _slow_shape(n, seed + 1, 0.09, 0.55))
    flakes = dsp.grains(
        n,
        seed + 2,
        count=density,
        spread=1.0,
        grain_ms=(9.0, 26.0),
        freq=(500.0, 1300.0),
        decay_curve=1.0,
    )
    flakes = dsp.lowpass(flakes, 1500.0, 0.7)
    return dsp.mix(dsp.gain_of(whisper, 0.6), dsp.gain_of(flakes, 0.22))


def seal_loop(buf: list[float], fade_seconds: float = 1.2) -> list[float]:
    """Fold the tail into the head so the file itself loops without a step."""
    fade = min(dsp.seconds(fade_seconds), len(buf) // 4)
    if fade < 2:
        return buf
    loop = len(buf) - fade
    out = list(buf)
    for k in range(fade):
        t = (k / fade) * 0.5 * math.pi
        out[k] = buf[k] * math.sin(t) + buf[loop + k] * math.cos(t)
    return out[:loop]


def _shape(buf: list[float]) -> list[float]:
    """The house curve for a bed: no rumble, no hiss, body in the middle."""
    buf = dsp.highpass(buf, 45.0, 0.7)
    buf = dsp.lowpass(buf, AIR_ROLLOFF_HZ, 0.6)
    return buf


BEDS: dict[str, callable] = {
    "alpine_mountain_pass": lambda: dsp.mix(
        wind(101, body_hz=190.0, gust_hz=0.05, depth=0.75, brightness=0.85),
        dsp.gain_of(knocks(103, 14, (300.0, 900.0), 0.2), 0.5),
    ),
    "mediterranean_plains": lambda: dsp.mix(
        wind(111, body_hz=150.0, gust_hz=0.07, depth=0.6, brightness=0.8),
        dsp.gain_of(leaves(113, 0.12), 0.35),
    ),
    "forest_ambush": lambda: dsp.mix(
        wind(121, body_hz=130.0, gust_hz=0.06, depth=0.7, brightness=0.7),
        dsp.gain_of(leaves(123, 0.18), 0.7),
        dsp.gain_of(gulls(125, 3), 0.35),
    ),
    "river_crossing": lambda: dsp.mix(
        dsp.gain_of(water(131, 700.0), 0.9),
        dsp.gain_of(
            wind(133, body_hz=140.0, gust_hz=0.06, depth=0.5, brightness=0.7), 0.5
        ),
    ),
    "battlefield_dry_wind_distant_march_01": lambda: dsp.mix(
        wind(151, body_hz=160.0, gust_hz=0.06, depth=0.7, brightness=0.75),
        dsp.gain_of(march(153, 96.0, 0.4), 1.0),
    ),
    "battlefield_dry_wind_distant_march_02": lambda: dsp.mix(
        wind(161, body_hz=175.0, gust_hz=0.045, depth=0.65, brightness=0.7),
        dsp.gain_of(march(163, 108.0, 0.32), 1.0),
    ),
    "desert_army_march": lambda: dsp.mix(
        wind(171, body_hz=185.0, gust_hz=0.05, depth=0.8, brightness=0.9),
        dsp.gain_of(march(173, 88.0, 0.35), 1.0),
    ),
    "mountain_camp_night": lambda: dsp.mix(
        dsp.gain_of(
            wind(181, body_hz=150.0, gust_hz=0.04, depth=0.6, brightness=0.6), 0.8
        ),
        dsp.gain_of(fire(183, 0.5), 0.55),
        dsp.gain_of(knocks(185, 10, (250.0, 700.0), 0.18), 0.6),
    ),
    "roman_army_camp_01": lambda: dsp.mix(
        dsp.gain_of(
            wind(191, body_hz=145.0, gust_hz=0.05, depth=0.45, brightness=0.65), 0.6
        ),
        dsp.gain_of(murmur(193, 520.0), 0.7),
        dsp.gain_of(knocks(195, 26, (280.0, 1100.0), 0.3), 1.0),
    ),
    "roman_army_camp_02": lambda: dsp.mix(
        dsp.gain_of(
            wind(201, body_hz=155.0, gust_hz=0.06, depth=0.5, brightness=0.65), 0.6
        ),
        dsp.gain_of(murmur(203, 460.0, 0.6), 0.75),
        dsp.gain_of(knocks(205, 20, (320.0, 1300.0), 0.28), 1.0),
    ),
    "carthage_war_camp_01": lambda: dsp.mix(
        dsp.gain_of(
            wind(211, body_hz=150.0, gust_hz=0.05, depth=0.45, brightness=0.7), 0.6
        ),
        dsp.gain_of(murmur(213, 600.0), 0.7),
        dsp.gain_of(fire(215, 0.4), 0.35),
    ),
    "carthage_war_camp_02": lambda: dsp.mix(
        dsp.gain_of(
            wind(221, body_hz=160.0, gust_hz=0.055, depth=0.5, brightness=0.7), 0.6
        ),
        dsp.gain_of(murmur(223, 540.0, 0.55), 0.72),
        dsp.gain_of(knocks(225, 18, (300.0, 1000.0), 0.26), 1.0),
    ),
    "siege_camp": lambda: dsp.mix(
        dsp.gain_of(
            wind(231, body_hz=160.0, gust_hz=0.05, depth=0.5, brightness=0.7), 0.55
        ),
        dsp.gain_of(murmur(233, 500.0), 0.6),
        dsp.gain_of(knocks(235, 34, (200.0, 900.0), 0.34), 1.0),
        dsp.gain_of(march(237, 76.0, 0.22), 1.0),
    ),
    "burning_village_aftermath": lambda: dsp.mix(
        dsp.gain_of(fire(241, 0.7), 0.9),
        dsp.gain_of(
            wind(243, body_hz=140.0, gust_hz=0.045, depth=0.6, brightness=0.6), 0.5
        ),
    ),
    "mediterranean_harbor": lambda: dsp.mix(
        dsp.gain_of(water(251, 520.0), 0.7),
        dsp.gain_of(
            wind(253, body_hz=150.0, gust_hz=0.06, depth=0.5, brightness=0.7), 0.5
        ),
        dsp.gain_of(murmur(255, 560.0, 0.45), 0.45),
        dsp.gain_of(gulls(257, 6), 1.0),
    ),
    "mediterranean_city_market": lambda: dsp.mix(
        dsp.gain_of(murmur(261, 620.0, 0.55), 1.0),
        dsp.gain_of(knocks(263, 44, (350.0, 1500.0), 0.3), 1.0),
        dsp.gain_of(
            wind(265, body_hz=140.0, gust_hz=0.07, depth=0.35, brightness=0.6), 0.35
        ),
    ),
    "weather_rain": lambda: dsp.mix(
        rain(301, density=760, drop_hz=(500.0, 2000.0), sheet_hz=2200.0),
        dsp.gain_of(
            wind(303, body_hz=150.0, gust_hz=0.05, depth=0.45, brightness=0.55), 0.3
        ),
    ),
    "weather_snow": lambda: dsp.mix(
        snowfall(311, density=240),
        dsp.gain_of(
            wind(313, body_hz=130.0, gust_hz=0.035, depth=0.7, brightness=0.4), 0.7
        ),
    ),
    "roman_road": lambda: dsp.mix(
        wind(271, body_hz=155.0, gust_hz=0.06, depth=0.6, brightness=0.75),
        dsp.gain_of(march(273, 100.0, 0.3), 1.0),
        dsp.gain_of(leaves(275, 0.1), 0.25),
    ),
}


def render(name: str) -> list[float]:
    dsp.SR = RATE
    dsp.set_variant(0)
    buf = BEDS[name]()
    buf = _shape(buf)
    buf = seal_loop(buf)
    return dsp.at_db(buf, PEAK_DBFS)
