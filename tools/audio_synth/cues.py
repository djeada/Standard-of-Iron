"""One recipe per cue.

Each recipe answers the `wanted:` line in assets/audio/audio_cues.json. Read
them side by side -- the description is the spec and the function is the
implementation. Seeds are fixed so a regeneration is byte-identical.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable

import dsp
import instruments as inst
from dsp import (
    apply,
    at_db,
    env_perc,
    env_swell,
    fade_out,
    gain_of,
    highpass,
    lowpass,
    mix,
    place,
    seconds,
    silence,
    sweep,
    tail,
)


@dataclass(frozen=True)
class Recipe:
    """Where a generated sound lands, how loud it may be, how many takes."""

    path: str
    peak_dbfs: float
    render: Callable[[], list]
    quality: int = 2

    takes: int = 1

    def take_path(self, index: int) -> str:
        if index == 0:
            return self.path
        stem, _, suffix = self.path.rpartition(".")
        return f"{stem}_v{index + 1}.{suffix}"


def _pad(buf, duration: float):
    """Make sure a buffer is at least `duration` long so tails are not clipped."""
    need = seconds(duration)
    return buf + silence(need - len(buf)) if len(buf) < need else buf


def ui_hover():
    return gain_of(inst.cloth(0.055, 101, centre=2700.0, curve=3.2), 0.8)


def ui_click():
    body = inst.wood(430.0, 0.055, 102)
    low = gain_of(inst.thud(180.0, 0.05, 103), 0.35)
    leather = gain_of(inst.cloth(0.03, 104, centre=1500.0, curve=3.0), 0.4)
    return lowpass(mix(body, low, leather), 7000.0)


def ui_back():
    first = gain_of(inst.wood(370.0, 0.05, 105), 0.8)
    second = gain_of(inst.wood(255.0, 0.07, 106), 0.7)
    out = place(_pad(first, 0.16), second, 0.042)
    return lowpass(out, 3400.0)


def ui_tab_switch():
    return gain_of(inst.whoosh(0.12, 107, 1700.0, 3800.0, q=1.4), 1.0)


def ui_panel_open():
    body = at_db(inst.creak(0.26, 108, 380.0, 1900.0, rate=23.0), -3.0)
    leather = at_db(inst.whoosh(0.24, 109, 700.0, 2000.0, q=1.0), -12.0)
    return lowpass(mix(body, leather), 6000.0)


def ui_panel_close():
    body = at_db(inst.creak(0.24, 110, 1900.0, 380.0, rate=21.0), -3.0)
    leather = at_db(inst.whoosh(0.2, 111, 1800.0, 600.0, q=1.0), -12.0)
    settle = at_db(inst.wood(220.0, 0.05, 112), -8.0)
    return lowpass(place(_pad(mix(body, leather), 0.28), settle, 0.21), 5200.0)


def ui_toggle():
    return gain_of(inst.bronze(1180.0, 0.045, 113), 0.9)


def ui_error():
    first = inst.thud(148.0, 0.09, 114, drop=0.7)
    second = gain_of(inst.thud(132.0, 0.1, 115, drop=0.7), 0.85)
    out = place(_pad(first, 0.24), second, 0.085)
    return lowpass(out, 900.0)


def ui_notification():
    flourish = inst.paper(0.26, 116, density=110)
    quill = gain_of(inst.wood(880.0, 0.05, 117), 0.3)
    lift = gain_of(inst.bronze(1320.0, 0.16, 118), 0.22)
    out = place(_pad(gain_of(flourish, 0.7), 0.32), quill, 0.02)
    return place(out, lift, 0.1)


def ui_select_unit():
    knock = inst.shield(255.0, 0.075, 119)
    strap = gain_of(inst.cloth(0.05, 120, centre=1800.0, curve=3.0), 0.35)
    return mix(knock, strap)


def ui_select_group():
    out = silence(seconds(0.34))
    for index, (at, freq, seed) in enumerate(
        (
            (0.0, 250.0, 121),
            (0.035, 288.0, 122),
            (0.078, 224.0, 123),
            (0.125, 268.0, 124),
        )
    ):
        out = place(
            out, gain_of(inst.shield(freq, 0.08, seed), 0.85 - index * 0.12), at
        )
    out = mix(out, gain_of(inst.mail(0.3, 125, density=70), 0.5))
    return lowpass(out, 6500.0)


def ui_deselect():
    body = inst.cloth(0.09, 126, centre=620.0, curve=2.2)
    return lowpass(lowpass(body, 1100.0), 900.0)


def order_move():
    out = silence(seconds(0.38))
    out = mix(out, gain_of(inst.mail(0.34, 201, density=85), 0.75))
    out = mix(out, gain_of(inst.cloth(0.22, 202, centre=1400.0, curve=1.8), 0.5))
    scuff = mix(
        gain_of(inst.gravel(0.13, 203, density=45), 0.8),
        gain_of(inst.thud(150.0, 0.07, 204), 0.4),
    )
    return place(out, scuff, 0.17)


def order_attack():
    return gain_of(inst.horn(233.0, 0.32, 205, brightness=1.25), 1.0)


def order_patrol():
    first = inst.horn(196.0, 0.34, 206, brightness=0.8)
    second = inst.horn(262.0, 0.42, 207, brightness=0.85)
    return place(_pad(gain_of(first, 0.9), 0.78), gain_of(second, 0.85), 0.3)


def order_stop():
    body = inst.drum(96.0, 0.16, 208, damped=True)
    return fade_out(body[: seconds(0.17)], 0.03)


def order_hold():
    plant = inst.thud(112.0, 0.14, 209)
    board = gain_of(inst.shield(190.0, 0.09, 210), 0.7)
    dirt = gain_of(inst.gravel(0.18, 211, density=55), 0.55)
    return lowpass(mix(plant, board, dirt), 5000.0)


def order_guard():
    def tap(seed):
        return mix(
            gain_of(inst.wood(330.0, 0.07, seed), 0.9),
            gain_of(inst.gravel(0.08, seed + 1, density=22), 0.4),
        )

    out = place(_pad(tap(212), 0.32), gain_of(tap(214), 0.85), 0.135)
    return lowpass(out, 6000.0)


def order_run():
    rattle = gain_of(inst.mail(0.26, 216, density=120), 0.85)
    puff = gain_of(inst.breath(0.3, 217, pitch=1.15), 0.5)
    return place(_pad(rattle, 0.36), puff, 0.06)


def order_formation():
    pole = at_db(inst.creak(0.17, 218, 300.0, 700.0, rate=15.0), -4.0)
    snap = at_db(inst.cloth(0.09, 219, centre=2400.0, curve=3.4), -2.0)
    return place(_pad(pole, 0.3), snap, 0.1)


def order_formation_placed():
    strike = inst.thud(94.0, 0.18, 220)
    pole = gain_of(inst.wood(280.0, 0.09, 221), 0.55)
    dirt = gain_of(inst.gravel(0.22, 222, density=60), 0.5)
    cloth = gain_of(inst.cloth(0.16, 223, centre=1900.0, curve=2.0), 0.3)
    return lowpass(mix(strike, pole, dirt, cloth), 6000.0)


def order_gate_mode():
    slide = at_db(inst.scrape(0.17, 224, 1250.0, 2150.0), -5.0)
    clack = at_db(inst.bronze(760.0, 0.06, 225), -1.0)
    return place(_pad(slide, 0.28), clack, 0.15)


def order_rally_set():
    flap = at_db(
        mix(
            inst.whoosh(0.13, 226, 900.0, 2200.0, q=0.9),
            gain_of(inst.cloth(0.15, 227, centre=1700.0, curve=2.0), 0.8),
        ),
        -4.0,
    )
    peg = at_db(
        mix(
            inst.wood(400.0, 0.06, 228), gain_of(inst.gravel(0.1, 229, density=30), 0.6)
        ),
        -1.0,
    )
    return place(_pad(flap, 0.36), peg, 0.18)


def build_placement_begin():
    cord = at_db(inst.cloth(0.08, 301, centre=3100.0, curve=3.0), -3.0)
    stake = at_db(
        mix(
            inst.wood(360.0, 0.07, 302), gain_of(inst.gravel(0.1, 303, density=26), 0.5)
        ),
        -1.0,
    )
    return place(_pad(cord, 0.4), stake, 0.18)


def build_placement_confirmed():
    hammer = gain_of(inst.wood(300.0, 0.1, 304, bright=1.4), 1.0)
    drive = gain_of(inst.thud(120.0, 0.11, 305), 0.7)
    dirt = gain_of(inst.gravel(0.14, 306, density=34), 0.4)
    return lowpass(mix(hammer, drive, dirt), 7000.0)


def build_placement_rejected():
    return lowpass(gain_of(inst.thud(140.0, 0.1, 307, drop=0.75), 1.0), 800.0)


def build_construction_started():
    out = silence(seconds(1.05))
    for at, seed in ((0.0, 308), (0.22, 310), (0.47, 312)):
        out = place(out, gain_of(inst.wood(330.0, 0.08, seed, bright=1.3), 0.75), at)
        out = place(out, gain_of(inst.thud(130.0, 0.08, seed + 1), 0.4), at)
    saw = gain_of(inst.scrape(0.45, 314, 1400.0, 2400.0), 0.45)
    out = place(out, saw, 0.55)
    out = mix(out, gain_of(inst.gravel(1.0, 315, density=60, decay_curve=1.0), 0.3))
    return lowpass(out, 7000.0)


def build_construction_complete():
    settle = mix(
        gain_of(inst.wood(180.0, 0.22, 316), 0.8),
        gain_of(inst.thud(95.0, 0.2, 317), 0.6),
        gain_of(inst.creak(0.3, 318, 380.0, 260.0, rate=12.0), 0.3),
    )
    out = _pad(settle, 2.0)
    for at, freq in ((0.34, 196.0), (0.62, 262.0), (0.92, 294.0)):
        out = place(out, gain_of(inst.horn(freq, 0.85, 319, brightness=0.6), 0.5), at)
    return tail(out, 0.5, 0.16)


def build_unit_queued():
    return highpass(gain_of(inst.paper(0.09, 320, density=42), 1.0), 1300.0)


def build_unit_ready():
    return gain_of(inst.bronze(330.0, 0.95, 321), 1.0)


def build_building_destroyed():
    out = silence(seconds(2.5))
    out = place(out, gain_of(inst.thud(70.0, 0.5, 322), 1.0), 0.0)
    for at, freq, seed in ((0.06, 240.0, 323), (0.21, 300.0, 324), (0.44, 190.0, 325)):
        out = place(out, gain_of(inst.wood(freq, 0.16, seed, bright=1.5), 0.7), at)
    out = mix(out, at_db(inst.rubble(2.4, 326, density=340, curve=0.9), -5.0))
    out = place(out, at_db(inst.drum(60.0, 0.6, 327), -7.0), 0.02)
    return tail(out, 0.7, 0.2)


def build_gate_open():
    hinge = at_db(inst.creak(1.15, 328, 320.0, 880.0, rate=17.0), -2.0)
    rumble = at_db(
        apply(
            lowpass(dsp.noise(seconds(1.2), 329), 190.0),
            env_swell(seconds(1.2), 0.4, 1.3),
        ),
        -9.0,
    )
    out = mix(_pad(hinge, 1.5), _pad(rumble, 1.5))
    out = place(out, at_db(inst.thud(105.0, 0.2, 330), -3.0), 1.2)
    return lowpass(out, 6000.0)


def build_gate_close():
    hinge = at_db(inst.creak(0.85, 331, 880.0, 330.0, rate=16.0), -5.0)
    out = _pad(hinge, 1.5)
    slam = at_db(
        mix(inst.thud(88.0, 0.26, 332), gain_of(inst.wood(210.0, 0.14, 333), 0.6)),
        -0.5,
    )
    out = place(out, slam, 0.86)
    bar = at_db(
        mix(inst.bronze(520.0, 0.14, 334), inst.wood(260.0, 0.1, 335)),
        -6.0,
    )
    out = place(out, bar, 1.16)
    return lowpass(out, 6500.0)


def alert_objective_complete():
    out = silence(seconds(1.5))
    for at, freq in ((0.0, 196.0), (0.18, 247.0), (0.38, 294.0)):
        length = 1.0 if at > 0.3 else 0.34
        out = place(out, gain_of(inst.horn(freq, length, 336, brightness=1.1), 0.8), at)
    return tail(out, 0.45, 0.14)


def alert_objective_failed():
    out = silence(seconds(1.6))
    for at, freq in ((0.0, 262.0), (0.2, 220.0), (0.42, 165.0)):
        length = 1.05 if at > 0.35 else 0.36
        out = place(
            out, gain_of(inst.horn(freq, length, 337, brightness=0.55), 0.8), at
        )
    return lowpass(tail(out, 0.6, 0.18), 3200.0)


def alert_unit_lost():
    bell = gain_of(inst.bronze(196.0, 0.5, 338), 0.55)
    tap = gain_of(inst.drum(78.0, 0.3, 339, damped=True), 0.8)
    return lowpass(mix(_pad(tap, 0.8), _pad(bell, 0.8)), 2600.0)


def combat_arrow_launch():
    string = gain_of(inst.wood(620.0, 0.045, 401, bright=1.6), 0.7)
    snap = gain_of(inst.cloth(0.035, 402, centre=3200.0, curve=3.8), 0.8)
    air = gain_of(inst.whoosh(0.12, 403, 2400.0, 900.0, q=1.6), 0.55)
    limb = gain_of(inst.wood(240.0, 0.06, 404), 0.3)
    return place(_pad(mix(string, snap, limb), 0.2), air, 0.018)


def combat_charge():
    out = silence(seconds(2.4))
    out = mix(out, gain_of(inst.shout(2.1, 405, freq=168.0), 0.85))
    out = mix(out, gain_of(inst.shout(2.0, 406, freq=214.0), 0.45))
    feet = gain_of(
        dsp.grains(seconds(2.3), 407, 260, 1.0, (16.0, 40.0), (90.0, 420.0), 1.0), 0.8
    )
    out = mix(out, feet)
    out = mix(out, gain_of(inst.mail(2.2, 408, density=200), 0.3))
    return tail(out, 0.5, 0.15)


def combat_siege_launch():
    rope = gain_of(inst.creak(0.42, 409, 260.0, 700.0, rate=26.0), 0.7)
    out = _pad(rope, 1.0)
    slam = mix(
        gain_of(inst.wood(150.0, 0.22, 410, bright=1.5), 1.0),
        gain_of(inst.thud(72.0, 0.28, 411), 0.9),
    )
    out = place(out, slam, 0.44)
    out = mix(out, gain_of(inst.whoosh(0.3, 412, 700.0, 2100.0, q=0.8), 0.3))
    return tail(out, 0.35, 0.12)


def combat_siege_impact():
    out = silence(seconds(1.5))
    out = place(out, gain_of(inst.thud(64.0, 0.35, 413), 1.0), 0.0)
    out = place(out, gain_of(inst.wood(210.0, 0.18, 414, bright=1.6), 0.6), 0.01)
    out = mix(out, gain_of(inst.rubble(1.4, 415, density=190), 0.7))
    return tail(out, 0.45, 0.16)


def combat_heal():
    out = _pad(gain_of(inst.water(0.7, 416), 0.75), 1.15)
    out = place(
        out, gain_of(inst.cloth(0.35, 417, centre=1500.0, curve=1.4), 0.35), 0.4
    )
    out = place(out, gain_of(inst.breath(0.5, 418, pitch=0.9), 0.3), 0.55)
    return lowpass(out, 6000.0)


def combat_guard_raise():
    strap = gain_of(inst.creak(0.13, 419, 520.0, 780.0, rate=28.0), 0.55)
    swing = gain_of(inst.whoosh(0.12, 420, 1300.0, 600.0, q=1.2), 0.5)
    brace = gain_of(inst.shield(200.0, 0.08, 421), 0.75)
    return lowpass(place(_pad(mix(strap, swing), 0.24), brace, 0.09), 5500.0)


def combat_block():
    edge = gain_of(inst.transient(14.0, 422, 2600.0, 0.9), 0.7)
    boss = gain_of(inst.shield(230.0, 0.085, 423), 1.0)
    body = gain_of(inst.thud(120.0, 0.1, 424), 0.6)
    return lowpass(mix(edge, boss, body), 4600.0)


def combat_perfect_guard():
    ting = gain_of(inst.bronze(2350.0, 0.3, 425), 1.0)
    edge = gain_of(inst.transient(8.0, 426, 5200.0, 0.8), 0.6)
    body = gain_of(inst.shield(260.0, 0.05, 427), 0.35)
    return highpass(mix(ting, edge, body), 320.0)


def combat_guard_break():
    crack = gain_of(inst.wood(190.0, 0.16, 428, bright=1.8), 1.0)
    out = _pad(crack, 0.85)
    out = place(out, gain_of(inst.gravel(0.34, 429, density=70), 0.6), 0.1)
    out = place(out, gain_of(inst.scrape(0.3, 430, 700.0, 380.0), 0.45), 0.14)
    out = place(out, gain_of(inst.breath(0.4, 431, pitch=1.25), 0.5), 0.34)
    return lowpass(out, 5200.0)


def combat_dodge():
    swish = gain_of(inst.whoosh(0.22, 432, 700.0, 2600.0, q=0.9), 0.8)
    out = _pad(swish, 0.52)
    out = mix(out, gain_of(inst.gravel(0.42, 433, density=95, decay_curve=1.2), 0.6))
    out = place(out, gain_of(inst.thud(130.0, 0.1, 434), 0.45), 0.12)
    out = place(out, gain_of(inst.breath(0.26, 435, pitch=1.3), 0.4), 0.05)
    return lowpass(out, 6000.0)


def combat_jump():
    grunt = gain_of(inst.shout(0.2, 436, freq=150.0), 0.55)
    push = gain_of(inst.thud(140.0, 0.08, 437), 0.5)
    rattle = gain_of(inst.mail(0.24, 438, density=80), 0.6)
    return lowpass(mix(_pad(grunt, 0.36), _pad(push, 0.36), _pad(rattle, 0.36)), 6500.0)


def combat_land():
    impact = gain_of(inst.thud(82.0, 0.16, 439), 1.0)
    rattle = gain_of(inst.mail(0.2, 440, density=110), 0.5)
    grit = gain_of(inst.gravel(0.3, 441, density=80), 0.55)
    return lowpass(
        mix(_pad(impact, 0.42), _pad(rattle, 0.42), _pad(grit, 0.42)), 5500.0
    )


def combat_shield_bash():
    boss = gain_of(inst.thud(105.0, 0.16, 442), 1.0)
    slap = gain_of(inst.cloth(0.09, 443, centre=900.0, curve=3.0), 0.6)
    damped = gain_of(inst.shield(175.0, 0.05, 444), 0.5)
    return lowpass(mix(boss, slap, damped), 2400.0)


def combat_vanguard_rush():
    push = gain_of(inst.thud(120.0, 0.12, 445), 0.7)
    out = _pad(push, 1.0)
    out = mix(out, gain_of(inst.whoosh(0.5, 446, 600.0, 2400.0, q=0.8), 0.6))
    out = place(out, gain_of(inst.shout(0.65, 447, freq=182.0), 0.8), 0.1)
    out = mix(out, gain_of(inst.mail(0.8, 448, density=150), 0.35))
    return lowpass(out, 7000.0)


def combat_second_wind():
    inhale = gain_of(inst.breath(0.55, 449, inhale=True, pitch=1.05), 0.75)
    out = _pad(inhale, 1.25)
    out = place(out, gain_of(inst.breath(0.55, 450, pitch=0.85), 0.55), 0.55)
    swell_n = seconds(1.2)
    low = mix(
        gain_of(dsp.sine(82.0, swell_n), 0.6),
        gain_of(dsp.sine(123.0, swell_n), 0.3),
    )
    out = mix(out, gain_of(apply(low, env_swell(swell_n, 0.45, 1.8)), 0.5))
    return lowpass(out, 4200.0)


def combat_ability_refused():
    return lowpass(gain_of(inst.thud(160.0, 0.045, 451, drop=0.8), 1.0), 700.0)


def combat_lock_on():
    n = seconds(0.06)
    click = apply(sweep(900.0, 1080.0, n, 0.5), env_perc(n, 0.0006, 3.0))
    body = gain_of(inst.wood(940.0, 0.04, 452), 0.7)
    return highpass(mix(gain_of(click, 0.7), body), 400.0)


def state_pause():
    n = seconds(0.26)
    fall = apply(sweep(140.0, 78.0, n, 0.6), env_perc(n, 0.002, 2.0))
    skin = gain_of(inst.drum(110.0, 0.16, 501, damped=True), 0.6)
    return lowpass(mix(gain_of(fall, 0.9), _pad(skin, 0.26)), 1800.0)


def state_resume():
    n = seconds(0.24)
    rise = apply(sweep(82.0, 148.0, n, 0.7), env_perc(n, 0.004, 1.8))
    skin = gain_of(inst.drum(130.0, 0.14, 502, damped=True), 0.55)
    return lowpass(mix(gain_of(rise, 0.9), _pad(skin, 0.24)), 2400.0)


def state_speed_change():
    first = gain_of(inst.wood(1150.0, 0.028, 503), 0.9)
    second = gain_of(inst.wood(1420.0, 0.024, 504), 0.7)
    return highpass(place(_pad(first, 0.075), second, 0.026), 600.0)


def state_save_complete():
    press = gain_of(
        apply(
            lowpass(dsp.noise(seconds(0.14), 505), 700.0),
            env_perc(seconds(0.14), 0.01, 2.2),
        ),
        0.7,
    )
    settle = gain_of(inst.paper(0.22, 506, density=70), 0.55)
    return place(_pad(press, 0.4), settle, 0.11)


def state_load_complete():
    unroll = gain_of(inst.creak(0.42, 507, 500.0, 1300.0, rate=24.0), 0.55)
    grain = gain_of(inst.paper(0.5, 508, density=150), 0.6)
    out = mix(_pad(unroll, 0.72), _pad(grain, 0.72))
    out = place(out, gain_of(inst.wood(210.0, 0.07, 509), 0.4), 0.52)
    return lowpass(out, 7000.0)


def state_commander_enter():
    slide = gain_of(inst.scrape(0.22, 510, 900.0, 1700.0), 0.5)
    out = _pad(slide, 0.9)
    out = place(out, gain_of(inst.bronze(430.0, 0.12, 511), 0.75), 0.23)

    close = at_db(lowpass(inst.breath(0.52, 512, inhale=True, pitch=0.9), 1500.0), -4.0)
    out = place(out, close, 0.34)
    return lowpass(out, 4000.0)


def state_commander_exit():
    lift = gain_of(inst.scrape(0.22, 513, 1700.0, 900.0), 0.5)
    out = _pad(lift, 0.9)
    out = place(out, gain_of(inst.bronze(380.0, 0.1, 514), 0.6), 0.05)
    away = at_db(highpass(inst.breath(0.45, 515, pitch=1.05), 900.0), -9.0)
    out = place(out, away, 0.22)
    return lowpass(out, 6000.0)


RECIPES: dict[str, Recipe] = {
    "ui.hover": Recipe("sfx/ui/hover_brush.ogg", -26.0, ui_hover, 1, takes=3),
    "ui.click": Recipe("sfx/ui/click_confirm.ogg", -14.0, ui_click, 1, takes=3),
    "ui.back": Recipe("sfx/ui/back_cancel.ogg", -17.0, ui_back, 1, takes=2),
    "ui.tab_switch": Recipe("sfx/ui/tab_slide.ogg", -18.0, ui_tab_switch, 1, takes=2),
    "ui.panel_open": Recipe("sfx/ui/panel_open.ogg", -16.0, ui_panel_open, 2),
    "ui.panel_close": Recipe("sfx/ui/panel_close.ogg", -16.0, ui_panel_close, 2),
    "ui.toggle": Recipe("sfx/ui/toggle_latch.ogg", -16.0, ui_toggle, 1, takes=2),
    "ui.error": Recipe("sfx/ui/error_thud.ogg", -13.0, ui_error, 1, takes=2),
    "ui.notification": Recipe("sfx/ui/notification.ogg", -18.0, ui_notification, 2),
    "ui.select_unit": Recipe(
        "sfx/ui/select_unit.ogg", -15.0, ui_select_unit, 1, takes=3
    ),
    "ui.select_group": Recipe(
        "sfx/ui/select_group.ogg", -14.0, ui_select_group, 2, takes=3
    ),
    "ui.deselect": Recipe("sfx/ui/deselect.ogg", -24.0, ui_deselect, 1, takes=2),
    "order.move": Recipe(
        "sfx/orders/move_kit_shuffle.ogg", -13.0, order_move, 2, takes=3
    ),
    "order.attack": Recipe("sfx/orders/attack_horn_stab.ogg", -8.0, order_attack, 2),
    "order.patrol": Recipe(
        "sfx/orders/patrol_horn_two_note.ogg", -10.0, order_patrol, 2
    ),
    "order.stop": Recipe("sfx/orders/stop_drum.ogg", -10.0, order_stop, 1, takes=2),
    "order.hold": Recipe(
        "sfx/orders/hold_shields_plant.ogg", -10.0, order_hold, 2, takes=2
    ),
    "order.guard": Recipe(
        "sfx/orders/guard_spear_taps.ogg", -12.0, order_guard, 2, takes=2
    ),
    "order.run": Recipe("sfx/orders/run_kit_rattle.ogg", -13.0, order_run, 2, takes=2),
    "order.formation": Recipe(
        "sfx/orders/formation_pole_shift.ogg", -14.0, order_formation, 2, takes=2
    ),
    "order.formation_placed": Recipe(
        "sfx/orders/formation_standard_planted.ogg", -9.0, order_formation_placed, 2
    ),
    "order.gate_mode": Recipe(
        "sfx/orders/gate_bolt_slide.ogg", -13.0, order_gate_mode, 2
    ),
    "order.rally_set": Recipe(
        "sfx/orders/rally_banner_peg.ogg", -12.0, order_rally_set, 2
    ),
    "build.placement_begin": Recipe(
        "sfx/build/placement_begin.ogg", -14.0, build_placement_begin, 2
    ),
    "build.placement_confirmed": Recipe(
        "sfx/build/placement_confirmed.ogg",
        -10.0,
        build_placement_confirmed,
        2,
        takes=2,
    ),
    "build.placement_rejected": Recipe(
        "sfx/build/placement_rejected.ogg", -12.0, build_placement_rejected, 1, takes=2
    ),
    "build.construction_started": Recipe(
        "sfx/build/construction_started.ogg", -12.0, build_construction_started, 3
    ),
    "build.construction_complete": Recipe(
        "sfx/build/construction_complete.ogg", -9.0, build_construction_complete, 3
    ),
    "build.unit_queued": Recipe(
        "sfx/build/unit_queued.ogg", -20.0, build_unit_queued, 1, takes=2
    ),
    "build.unit_ready": Recipe(
        "sfx/build/unit_ready_bell.ogg", -13.0, build_unit_ready, 2
    ),
    "build.building_destroyed": Recipe(
        "sfx/build/building_destroyed.ogg", -6.0, build_building_destroyed, 3
    ),
    "build.gate_open": Recipe("sfx/build/gate_open.ogg", -11.0, build_gate_open, 3),
    "build.gate_close": Recipe("sfx/build/gate_close.ogg", -10.0, build_gate_close, 3),
    "alert.objective_complete": Recipe(
        "sfx/alerts/objective_complete.ogg", -7.0, alert_objective_complete, 3
    ),
    "alert.objective_failed": Recipe(
        "sfx/alerts/objective_failed.ogg", -7.0, alert_objective_failed, 3
    ),
    "alert.unit_lost": Recipe("sfx/alerts/unit_lost.ogg", -16.0, alert_unit_lost, 2),
    "combat.arrow_launch": Recipe(
        "sfx/combat/bow_release_single.ogg", -14.0, combat_arrow_launch, 2, takes=3
    ),
    "combat.charge": Recipe("sfx/combat/charge_roar.ogg", -7.0, combat_charge, 3),
    "combat.siege_launch": Recipe(
        "sfx/combat/siege_launch.ogg", -8.0, combat_siege_launch, 3
    ),
    "combat.siege_impact": Recipe(
        "sfx/combat/siege_impact.ogg", -7.0, combat_siege_impact, 3
    ),
    "combat.heal": Recipe("sfx/combat/heal_bind_wound.ogg", -18.0, combat_heal, 2),
    "combat.guard_raise": Recipe(
        "sfx/combat/guard_raise.ogg", -15.0, combat_guard_raise, 2, takes=2
    ),
    "combat.block": Recipe(
        "sfx/combat/shield_block.ogg", -9.0, combat_block, 2, takes=3
    ),
    "combat.perfect_guard": Recipe(
        "sfx/combat/perfect_guard.ogg", -8.0, combat_perfect_guard, 2, takes=2
    ),
    "combat.guard_break": Recipe(
        "sfx/combat/guard_break.ogg", -8.0, combat_guard_break, 3
    ),
    "combat.dodge": Recipe(
        "sfx/combat/dodge_roll.ogg", -13.0, combat_dodge, 2, takes=3
    ),
    "combat.jump": Recipe("sfx/combat/jump_effort.ogg", -14.0, combat_jump, 2, takes=2),
    "combat.land": Recipe("sfx/combat/land_thud.ogg", -11.0, combat_land, 2, takes=3),
    "combat.shield_bash": Recipe(
        "sfx/combat/shield_bash.ogg", -8.0, combat_shield_bash, 2, takes=2
    ),
    "combat.vanguard_rush": Recipe(
        "sfx/combat/vanguard_rush.ogg", -8.0, combat_vanguard_rush, 3
    ),
    "combat.second_wind": Recipe(
        "sfx/combat/second_wind.ogg", -12.0, combat_second_wind, 3
    ),
    "combat.ability_refused": Recipe(
        "sfx/combat/ability_refused.ogg", -22.0, combat_ability_refused, 1, takes=2
    ),
    "combat.lock_on": Recipe(
        "sfx/combat/lock_on_tick.ogg", -18.0, combat_lock_on, 1, takes=2
    ),
    "state.pause": Recipe("sfx/state/pause.ogg", -12.0, state_pause, 1),
    "state.resume": Recipe("sfx/state/resume.ogg", -12.0, state_resume, 1),
    "state.speed_change": Recipe(
        "sfx/state/speed_notch.ogg", -18.0, state_speed_change, 1
    ),
    "state.save_complete": Recipe(
        "sfx/state/save_complete.ogg", -16.0, state_save_complete, 2
    ),
    "state.load_complete": Recipe(
        "sfx/state/load_complete.ogg", -15.0, state_load_complete, 2
    ),
    "state.commander_enter": Recipe(
        "sfx/state/commander_enter.ogg", -11.0, state_commander_enter, 3
    ),
    "state.commander_exit": Recipe(
        "sfx/state/commander_exit.ogg", -11.0, state_commander_exit, 3
    ),
}
