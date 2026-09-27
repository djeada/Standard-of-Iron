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
    gain_of,
    highpass,
    lowpass,
    mix,
    place,
    seconds,
    silence,
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


def ui_notification():
    flourish = inst.paper(0.26, 116, density=110)
    quill = gain_of(inst.wood(880.0, 0.05, 117), 0.3)
    lift = gain_of(inst.bronze(1320.0, 0.16, 118), 0.22)
    out = place(_pad(gain_of(flourish, 0.7), 0.32), quill, 0.02)
    return place(out, lift, 0.1)


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
    "ui.tab_switch": Recipe("sfx/ui/tab_slide.ogg", -18.0, ui_tab_switch, 1, takes=2),
    "ui.panel_open": Recipe("sfx/ui/panel_open.ogg", -16.0, ui_panel_open, 2),
    "ui.panel_close": Recipe("sfx/ui/panel_close.ogg", -16.0, ui_panel_close, 2),
    "ui.notification": Recipe("sfx/ui/notification.ogg", -18.0, ui_notification, 2),
    "ui.select_group": Recipe(
        "sfx/ui/select_group.ogg", -14.0, ui_select_group, 2, takes=3
    ),
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
