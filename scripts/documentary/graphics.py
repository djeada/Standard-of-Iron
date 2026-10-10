"""Series graphics (#1534), drawn frame by frame from the episode's data.

Every graphic is a pure function of (data, frame size, time): the same edit
renders the same pixels at 3840x2160 for the episode and 1080x1920 for the
reel, and the layout switches on aspect -- side-by-side columns become stacked
blocks, lower-left stamps move above the platform UI. All sizes, colours and
timings come from ``style.py``.

Kinds:

``title_sequence``  series title, rule and episode line over embers (<= 10 s)
``place_date``      ``CANNAE · 2 AUGUST 216 BC`` with the region beneath
``order_of_battle`` both armies side by side: commanders, historical totals
                    counting up, breakdown rows, strength bars, the source
``name_plate``      a commander's name and office in his side's colour
``army_counter``    side totals ticking from one value to another
``casualty_tally``  losses per side, ranges and the authorities for each
``chapter_card``    numeral, rule and section title
``end_card``        next-episode tease and the Steam CTA (``steam_demo`` preset)

Graphics are rendered to RGBA QuickTime (``qtrle``) for compositing; a still of
each one at its hold point is what ``graphics --stills`` writes for review.
"""

from __future__ import annotations

import importlib.util
import math
import random
import subprocess
import sys
from functools import lru_cache
from pathlib import Path

from . import REPO, EditError, style

STEAM_LINK = "STORE.STEAMPOWERED.COM/APP/5129960/STANDARD_OF_IRON"


@lru_cache(maxsize=1)
def promo_edit():
    """``scripts/promo-edit.py``: the end-card presets and the Steam-link check."""
    spec = importlib.util.spec_from_file_location(
        "promo_edit", REPO / "scripts" / "promo-edit.py"
    )
    module = importlib.util.module_from_spec(spec)
    sys.modules.setdefault("promo_edit", module)
    spec.loader.exec_module(module)
    return module


def end_card_fields(preset: str | dict) -> dict:
    """The CTA fields of a promo-edit end-card preset, refused without a Steam link."""
    module = promo_edit()
    spec = {"end_card": preset} if isinstance(preset, str) else dict(preset)
    spec = module.apply_end_card_preset(spec)
    if not module.end_card_has_steam_link(spec):
        raise EditError(f"end card has no Steam link ({module.STEAM_LINK})")
    return spec


def _img():
    from PIL import Image, ImageDraw, ImageFilter

    return Image, ImageDraw, ImageFilter


class Canvas:
    """One frame: an ink layer (text, rules) with a soft shadow, over panels."""

    def __init__(self, width: int, height: int):
        Image, ImageDraw, _ = _img()
        self.w, self.h = width, height
        self.u = style.unit(width, height)
        self.aspect = style.aspect(width, height)
        self.safe = style.safe_box(width, height)
        self.vertical = self.aspect == "9:16"
        self.panel = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        self.ink = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        self.glow = None
        self.pd = ImageDraw.Draw(self.panel)
        self.d = ImageDraw.Draw(self.ink)

    def px(self, value: float) -> int:
        return max(1, int(round(value * self.u)))

    def text(
        self,
        x,
        y,
        text,
        size,
        color,
        alpha=1.0,
        face="display",
        tracking=0.0,
        anchor="l",
    ) -> float:
        if alpha <= 0.003 or not text:
            return style.text_width(text, self.px(size), face, tracking)
        fill = tuple(color[:3]) + (int(255 * max(0.0, min(1.0, alpha))),)
        return style.draw_text(
            self.d, (x, y), text, self.px(size), fill, face, tracking, anchor
        )

    def width(self, text, size, face="display", tracking=0.0) -> float:
        return style.text_width(text, self.px(size), face, tracking)

    def rule(self, x0, x1, y, color, alpha=1.0, weight=None) -> None:
        if alpha <= 0.003 or x1 <= x0:
            return
        w = max(1, int(round((weight or style.STROKE["rule"]) * self.u)))
        fill = tuple(color[:3]) + (int(255 * alpha),)
        self.d.rectangle([x0, y - w / 2, x1, y + w / 2], fill=fill)

    def lozenge(self, cx, cy, r, color, alpha=1.0) -> None:
        fill = tuple(color[:3]) + (int(255 * alpha),)
        self.d.polygon(
            [(cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy)], fill=fill
        )

    def box(self, rect, color, alpha) -> None:
        if alpha <= 0.003:
            return
        self.pd.rectangle(rect, fill=tuple(color[:3]) + (int(255 * alpha),))

    def scrim(self, rect, alpha: float = 0.5, soft: float = 60.0) -> None:
        """A soft dark pool behind text laid over bright footage."""
        if alpha <= 0.003:
            return
        Image, ImageDraw, ImageFilter = _img()
        k = 8
        small = Image.new("L", (self.w // k + 1, self.h // k + 1), 0)
        x0, y0, x1, y1 = (v / k for v in rect)
        ImageDraw.Draw(small).rectangle([x0, y0, x1, y1], fill=int(255 * alpha))
        small = small.filter(ImageFilter.GaussianBlur(max(1.0, soft * self.u / k)))
        layer = Image.new("RGBA", (self.w, self.h), style.COLORS["iron"] + (0,))
        layer.putalpha(small.resize((self.w, self.h), Image.BILINEAR))
        self.panel.alpha_composite(layer)

    def bar(self, rect, color, alpha=1.0) -> None:
        if alpha <= 0.003 or rect[2] <= rect[0]:
            return
        self.d.rectangle(rect, fill=tuple(color[:3]) + (int(255 * alpha),))

    def compose(self, background=None):
        Image, _, ImageFilter = _img()
        alpha = self.ink.getchannel("A")
        small = alpha.resize((max(1, self.w // 4), max(1, self.h // 4)), Image.BILINEAR)
        blur = small.filter(ImageFilter.GaussianBlur(max(1.0, 7 * self.u / 4)))
        shadow_alpha = blur.resize((self.w, self.h), Image.BILINEAR).point(
            lambda v: min(255, int(v * 2.2 * style.OPACITY["shadow"]))
        )
        if background is None:
            frame = Image.new("RGBA", (self.w, self.h), (0, 0, 0, 0))
        else:
            frame = background.convert("RGBA").copy()
        frame.alpha_composite(self.panel)
        shadow = Image.new("RGBA", (self.w, self.h), (0, 0, 0, 0))
        shadow.putalpha(shadow_alpha)
        frame.alpha_composite(shadow)
        if self.glow is not None:
            frame.alpha_composite(self.glow)
        frame.alpha_composite(self.ink)
        return frame


def _count(
    t: float, start: float, a: float, b: float, length: float | None = None
) -> float:
    length = style.TIMING["count"] if length is None else length
    return a + (b - a) * style.ease_out((t - start) / length)


def _fit(
    cv: Canvas, text: str, size: float, max_width: float, face="display", tracking=0.0
) -> float:
    width = cv.width(text, size, face, tracking)
    return size if width <= max_width else size * max_width / width


@lru_cache(maxsize=8)
def _embers(width: int, height: int, seed: int):
    rng = random.Random(seed)
    count = 110
    u = style.unit(width, height)
    particles = []
    for _ in range(count):
        particles.append(
            {
                "x": rng.uniform(-0.05, 1.05) * width,
                "y": rng.uniform(0.0, 1.0),
                "vy": rng.uniform(0.05, 0.15) * height,
                "sway": rng.uniform(4, 22) * u,
                "freq": rng.uniform(0.2, 0.7),
                "phase": rng.uniform(0, math.tau),
                "size": rng.uniform(1.0, 3.4) * u,
                "flicker": rng.uniform(3.0, 9.0),
                "heat": rng.uniform(0.55, 1.0),
            }
        )
    return particles


def embers(cv: Canvas, t: float, alpha: float = 1.0, seed: int = 216) -> None:
    """Embers rising through the frame; a soft glow plus a hot core."""
    Image, ImageDraw, ImageFilter = _img()
    if alpha <= 0.01:
        return
    scale = 4
    glow = Image.new("RGBA", (cv.w // scale + 1, cv.h // scale + 1), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    span = cv.h * 1.25
    for p in _embers(cv.w, cv.h, seed):
        y = cv.h * 1.1 - ((p["y"] * span + p["vy"] * (t + 6.0)) % span)
        x = p["x"] + p["sway"] * math.sin(p["freq"] * math.tau * t + p["phase"])
        life = max(0.0, min(1.0, y / (cv.h * 0.9)))
        flicker = 0.7 + 0.3 * math.sin(p["flicker"] * t + p["phase"] * 3)
        a = alpha * p["heat"] * flicker * (0.25 + 0.75 * life)
        if a <= 0.02:
            continue
        r = p["size"] * (0.6 + 0.4 * life)
        core = style.COLORS["ember_core"] if p["heat"] > 0.85 else style.COLORS["ember"]
        cv.d.ellipse(
            [x - r, y - r, x + r, y + r], fill=core + (int(255 * min(1.0, a)),)
        )
        gr = r * 3.2 / scale
        gx, gy = x / scale, y / scale
        gd.ellipse(
            [gx - gr, gy - gr, gx + gr, gy + gr],
            fill=style.COLORS["ember"] + (int(150 * a),),
        )
    glow = glow.filter(ImageFilter.GaussianBlur(2.2)).resize(
        (cv.w, cv.h), Image.BILINEAR
    )
    cv.glow = glow


@lru_cache(maxsize=4)
def _vignette(width: int, height: int):
    Image, _, ImageFilter = _img()
    small = Image.radial_gradient("L").resize((max(2, width // 8), max(2, height // 8)))
    inner = style.COLORS["iron"]
    edge = (5, 3, 2)
    rgb = Image.new("RGB", small.size)
    rgb.putdata(
        [
            tuple(
                int(inner[c] * (1 - v / 255) * 1.6 + edge[c] * (v / 255))
                for c in range(3)
            )
            for v in small.getdata()
        ]
    )
    return rgb.filter(ImageFilter.GaussianBlur(2)).resize(
        (width, height), Image.BICUBIC
    )


def title_sequence(cv: Canvas, t: float, dur: float, d: dict) -> None:
    out = style.ease((dur - t) / 1.2)
    embers(cv, t, min(1.0, t / 0.8) * (0.4 + 0.6 * out))
    cx, cy = cv.w / 2, cv.h * (0.46 if not cv.vertical else 0.44)
    usable = cv.safe[2] - cv.safe[0]
    title = d.get("series_title", style.SERIES_TITLE)
    tracking = style.TRACKING["title"] * (0.75 + 0.25 * style.ease_out((t - 1.0) / 2.4))
    size = style.TYPE_SCALE["title"] * (0.82 if cv.vertical else 1.0)
    lines = [title]
    if cv.vertical and cv.width(title, size, tracking=tracking) > usable:
        lines = style.wrap(title, cv.px(size), usable, "display", tracking)
    a_title = style.ease((t - 1.0) / 1.6) * out
    line_h = cv.px(size) * 1.12
    top = cy - cv.px(28) - line_h * (len(lines) - 1)
    for i, line in enumerate(lines):
        fitted = _fit(cv, line, size, usable, tracking=tracking)
        cv.text(
            cx,
            top + i * line_h,
            line,
            fitted,
            style.COLORS["ink"],
            a_title,
            tracking=tracking,
            anchor="c",
        )
    grow = style.ease_out((t - 0.5) / 1.2)
    half = cv.px(260) * grow
    ry = cy + cv.px(10)
    a_rule = min(1.0, grow * 1.5) * out
    cv.rule(cx - half, cx - cv.px(18), ry, style.COLORS["gold"], a_rule)
    cv.rule(cx + cv.px(18), cx + half, ry, style.COLORS["gold"], a_rule)
    cv.lozenge(cx, ry, cv.px(7), style.COLORS["gold_bright"], a_rule)
    sub = d.get("series_subtitle", style.SERIES_SUBTITLE)
    a_sub = style.ease((t - 2.2) / 1.2) * out
    cv.text(
        cx,
        ry + cv.px(70),
        sub,
        style.TYPE_SCALE["h3"],
        style.COLORS["gold_bright"],
        a_sub,
        tracking=style.TRACKING["title"],
        anchor="c",
    )
    episode = d.get("episode_line")
    if episode:
        a_ep = style.ease((t - 3.6) / 1.0) * out
        cv.text(
            cx,
            cv.safe[3] - cv.px(30),
            episode,
            style.TYPE_SCALE["label"],
            style.COLORS["ink_dim"],
            a_ep,
            tracking=style.TRACKING["label"],
            anchor="c",
        )


def place_date(cv: Canvas, t: float, dur: float, d: dict) -> None:
    place, date = style.stamp_text(d["place"], d.get("date"))
    a = style.envelope(t, dur)
    size = style.TYPE_SCALE["h3"]
    tracking = style.TRACKING["label"]
    sep_gap = cv.px(22)
    w_place = cv.width(place, size, tracking=tracking)
    w_date = cv.width(date, size, tracking=tracking) if date else 0
    total = w_place + (2 * sep_gap + w_date if date else 0)
    if cv.vertical:
        x, y = cv.safe[0], cv.safe[1] + cv.px(70)
    else:
        x, y = cv.safe[0], cv.safe[3] - cv.px(48)
    reveal = style.ease_out((t - 0.15) / 0.8)
    cv.scrim(
        [x - cv.px(30), y - cv.px(70), x + total + cv.px(40), y + cv.px(56)], 0.45 * a
    )
    cv.rule(
        x,
        x + cv.px(70) * style.ease_out(t / style.TIMING["rule_draw"]),
        y - cv.px(50),
        style.COLORS["gold"],
        a,
    )
    cv.text(x, y, place, size, style.COLORS["ink"], a, tracking=tracking)
    if date:
        cv.lozenge(
            x + w_place + sep_gap,
            y - cv.px(size) * 0.35,
            cv.px(5),
            style.COLORS["gold"],
            a,
        )
        cv.text(
            x + w_place + 2 * sep_gap,
            y,
            date,
            size,
            style.COLORS["gold_bright"],
            a,
            tracking=tracking,
        )
    if d.get("region"):
        cv.text(
            x,
            y + cv.px(40),
            d["region"].upper(),
            style.TYPE_SCALE["small"],
            style.COLORS["ink_dim"],
            a,
            tracking=0.2,
        )
    if reveal < 1.0:
        cut = int(x + (total + cv.px(40)) * reveal)
        cv.ink.paste(
            (0, 0, 0, 0), (max(0, cut), int(y - cv.px(90)), cv.w, int(y + cv.px(60)))
        )


def _side_column(
    cv: Canvas,
    t: float,
    a: float,
    side: dict,
    x0: float,
    x1: float,
    top: float,
    peak: float,
    delay: float,
    compact: bool,
    rows: int = 3,
) -> float:
    color = side["color"]
    y = top
    cv.bar([x0, y, x0 + cv.px(56), y + cv.px(style.STROKE["side_bar"])], color, a)
    y += cv.px(58 if not compact else 50)
    cv.text(
        x0,
        y,
        side["name"].upper(),
        style.TYPE_SCALE["h2"],
        style.COLORS["ink"],
        a,
        tracking=style.TRACKING["heading"],
    )
    y += cv.px(44 if not compact else 38)
    for commander in side.get("commanders", [])[:rows]:
        cv.text(
            x0,
            y,
            commander.upper(),
            style.TYPE_SCALE["small"],
            style.COLORS["ink_dim"],
            a,
            tracking=0.12,
        )
        y += cv.px(32)
    y += cv.px(32) * max(0, rows - len(side.get("commanders", [])[:rows]))
    y += cv.px(78 if not compact else 64)
    value = _count(t, 0.5 + delay, 0, side["strength"])
    size = style.TYPE_SCALE["number"] * (0.8 if compact else 1.0)
    cv.text(
        x0,
        y,
        style.format_number(value),
        size,
        style.COLORS["ink"],
        a,
        tracking=style.TRACKING["number"],
    )
    cv.text(
        x1,
        y,
        "MEN",
        style.TYPE_SCALE["label"],
        style.COLORS["ink_dim"],
        a,
        tracking=0.2,
        anchor="r",
    )
    y += cv.px(28)
    grow = (value / peak) if peak else 0
    cv.bar([x0, y, x0 + (x1 - x0) * grow, y + cv.px(10)], color, a * 0.95)
    cv.bar(
        [x0 + (x1 - x0) * grow, y, x1, y + cv.px(10)],
        style.COLORS["ink_faint"],
        a * 0.25,
    )
    y += cv.px(58)
    for row in side.get("breakdown", []):
        rv = _count(t, 0.9 + delay, 0, row["value"])
        cv.text(
            x0,
            y,
            row["label"].upper(),
            style.TYPE_SCALE["label"],
            style.COLORS["ink_dim"],
            a,
            tracking=0.16,
        )
        cv.text(
            x1,
            y,
            style.format_number(rv),
            style.TYPE_SCALE["label"],
            style.COLORS["ink"],
            a,
            tracking=0.08,
            anchor="r",
        )
        y += cv.px(44)
    return y


def _column_height(rows: int, breakdown: int, compact: bool) -> float:
    head = (50 + 38) if compact else (58 + 44)
    body = 64 if compact else 78
    return head + 32 * rows + body + 28 + 58 + 44 * breakdown


def order_of_battle(cv: Canvas, t: float, dur: float, d: dict) -> None:
    a = style.envelope(t, dur)
    sides = d["sides"]
    peak = max(s["strength"] for s in sides)
    left, top, r, bottom = cv.safe
    rows = min(3, max(len(s.get("commanders", [])) for s in sides))
    breakdown = max(len(s.get("breakdown", [])) for s in sides)
    foot = d.get("footnote", "").upper()
    foot_size = style.TYPE_SCALE["small"] * 0.9
    foot_lines = (
        style.wrap(foot, cv.px(foot_size), r - left, "display", 0.14) if foot else []
    )
    if cv.vertical and foot:
        foot_lines = [part.strip() for part in foot.split("·") if part.strip()]
    column = cv.px(_column_height(rows, breakdown, cv.vertical))
    foot_h = cv.px(34) * len(foot_lines) + cv.px(30)
    if cv.vertical:
        height = cv.px(90) + 2 * column + cv.px(40) + foot_h
    else:
        height = cv.px(140) + column + cv.px(20) + foot_h
    panel_top = max(top - cv.px(30), cv.h / 2 - height / 2)
    panel_bottom = panel_top + height
    cv.box(
        [0, panel_top, cv.w, panel_bottom],
        style.COLORS["iron"],
        style.OPACITY["panel"] * a,
    )
    cv.rule(0, cv.w, panel_top, style.COLORS["rule"], a * 0.8, style.STROKE["frame"])
    cv.rule(0, cv.w, panel_bottom, style.COLORS["rule"], a * 0.8, style.STROKE["frame"])
    cv.text(
        cv.w / 2,
        panel_top + cv.px(64),
        "ORDER OF BATTLE",
        style.TYPE_SCALE["label"],
        style.COLORS["gold"],
        a,
        tracking=0.3,
        anchor="c",
    )
    if cv.vertical:
        y = panel_top + cv.px(90)
        for i, side in enumerate(sides[:2]):
            _side_column(
                cv,
                t,
                a,
                side,
                left,
                r,
                y,
                peak,
                i * style.TIMING["stagger"],
                True,
                rows,
            )
            y += column + cv.px(20)
    else:
        gap = cv.px(140)
        width = (r - left - gap) / 2
        y = panel_top + cv.px(120)
        cv.d.line(
            [(cv.w / 2, y + cv.px(20)), (cv.w / 2, y + column - cv.px(20))],
            fill=style.COLORS["rule"] + (int(140 * a),),
            width=max(1, cv.px(1.5)),
        )
        for i, side in enumerate(sides[:2]):
            x0 = left + i * (width + gap)
            _side_column(
                cv,
                t,
                a,
                side,
                x0,
                x0 + width,
                y,
                peak,
                i * style.TIMING["stagger"],
                False,
                rows,
            )
    y = panel_bottom - foot_h + cv.px(18)
    for line in foot_lines:
        cv.text(
            cv.w / 2,
            y,
            line,
            foot_size,
            style.COLORS["ink_faint"],
            a,
            tracking=0.14,
            anchor="c",
        )
        y += cv.px(34)


def name_plate(cv: Canvas, t: float, dur: float, d: dict) -> None:
    a = style.envelope(t, dur)
    color = d["color"]
    name, office = d["name"].upper(), d.get("title", "").upper()
    usable = cv.safe[2] - cv.safe[0] - cv.px(40)
    size = _fit(cv, name, 46, usable, tracking=0.1)
    if cv.vertical:
        x, base = cv.safe[0], cv.h * 0.58
    else:
        x, base = cv.safe[0], cv.safe[3] - cv.px(70)
    slide = cv.px(24) * (1 - style.ease_out((t - 0.1) / 0.7))
    grow = style.ease_out(t / 0.4)
    bar_top = base - cv.px(size) * 1.35 - cv.px(26)
    bar_bottom = base + cv.px(42)
    cv.bar(
        [
            x,
            bar_bottom - (bar_bottom - bar_top) * grow,
            x + cv.px(style.STROKE["side_bar"]),
            bar_bottom,
        ],
        color,
        a,
    )
    tx = x + cv.px(26) + slide
    cv.scrim(
        [
            x - cv.px(30),
            bar_top - cv.px(10),
            x + cv.px(60) + cv.width(name, size, tracking=0.1),
            bar_bottom + cv.px(10),
        ],
        0.5 * a,
    )
    a_text = a * style.ease((t - 0.12) / 0.5)
    cv.text(
        tx,
        bar_top + cv.px(20),
        d.get("side", "").upper(),
        style.TYPE_SCALE["small"] * 0.9,
        style.COLORS["gold_bright"],
        a_text,
        tracking=0.24,
    )
    cv.text(tx, base, name, size, style.COLORS["ink"], a_text, tracking=0.1)
    if office:
        cv.text(
            tx,
            base + cv.px(38),
            office,
            style.TYPE_SCALE["small"],
            style.COLORS["ink_dim"],
            a_text,
            tracking=0.16,
        )


def army_counter(cv: Canvas, t: float, dur: float, d: dict) -> None:
    a = style.envelope(t, dur)
    sides = d["sides"]
    top = cv.safe[1] + cv.px(30)
    for i, side in enumerate(sides[:2]):
        right = i == 1
        x = cv.safe[2] if right else cv.safe[0]
        anchor = "r" if right else "l"
        value = _count(t, 0.3, side["from"], side["to"])
        wide = max(
            cv.width(
                style.format_number(side["to"]), style.TYPE_SCALE["h2"], tracking=0.06
            ),
            cv.px(220),
        )
        sx0, sx1 = (x - wide, x) if right else (x, x + wide)
        cv.scrim(
            [sx0 - cv.px(30), top - cv.px(40), sx1 + cv.px(30), top + cv.px(96)],
            0.45 * a,
        )
        cv.text(
            x,
            top,
            side["name"].upper(),
            style.TYPE_SCALE["small"],
            style.COLORS["ink_dim"],
            a,
            tracking=0.24,
            anchor=anchor,
        )
        w = cv.text(
            x,
            top + cv.px(60),
            style.format_number(value),
            style.TYPE_SCALE["h2"],
            style.COLORS["ink"],
            a,
            tracking=style.TRACKING["number"],
            anchor=anchor,
        )
        x0, x1 = (x - w, x) if right else (x, x + w)
        cv.bar([x0, top + cv.px(76), x1, top + cv.px(80)], side["color"], a)


def casualty_tally(cv: Canvas, t: float, dur: float, d: dict) -> None:
    a = style.envelope(t, dur)
    rows = d["rows"]
    width = (cv.safe[2] - cv.safe[0]) * (1.0 if cv.vertical else 0.62)
    x0 = (cv.w - width) / 2
    x1 = x0 + width
    row_h = cv.px(190 if not cv.vertical else 200)
    height = cv.px(110) + row_h * len(rows)
    top = cv.h / 2 - height / 2
    cv.box(
        [x0 - cv.px(50), top - cv.px(20), x1 + cv.px(50), top + height + cv.px(10)],
        style.COLORS["iron"],
        style.OPACITY["panel"] * a,
    )
    cv.text(
        cv.w / 2,
        top + cv.px(50),
        d.get("heading", "LOSSES"),
        style.TYPE_SCALE["label"],
        style.COLORS["gold"],
        a,
        tracking=0.3,
        anchor="c",
    )
    y = top + cv.px(130)
    for i, row in enumerate(rows):
        delay = 0.4 + i * 0.5
        a_row = a * style.ease((t - delay + 0.3) / 0.5)
        cv.bar(
            [x0, y - cv.px(22), x0 + cv.px(style.STROKE["side_bar"]), y + cv.px(70)],
            row["color"],
            a_row,
        )
        cv.text(
            x0 + cv.px(24),
            y,
            row["side"].upper(),
            style.TYPE_SCALE["small"],
            style.COLORS["gold_bright"],
            a_row,
            tracking=0.24,
        )
        cv.text(
            x1,
            y,
            row.get("label", "KILLED").upper(),
            style.TYPE_SCALE["small"],
            style.COLORS["ink_dim"],
            a_row,
            tracking=0.24,
            anchor="r",
        )
        values = row["value"] if isinstance(row["value"], list) else [row["value"]]
        shown = [style.format_number(_count(t, delay, 0, v, 2.0)) for v in values]
        number = " – ".join(shown)
        size = _fit(
            cv, number, style.TYPE_SCALE["h1"], width - cv.px(24), tracking=0.04
        )
        cv.text(
            x0 + cv.px(24),
            y + cv.px(76),
            number,
            size,
            style.COLORS["ink"],
            a_row,
            tracking=0.04,
        )
        sources = " · ".join(
            f"{s['author']} {s.get('ref', '')}: {style.format_number(s['value'])}".upper()
            for s in row.get("sources", [])
        )
        if sources:
            cv.text(
                x0 + cv.px(24),
                y + cv.px(118),
                sources,
                style.TYPE_SCALE["small"] * 0.85,
                style.COLORS["ink_faint"],
                a_row,
                tracking=0.12,
            )
        y += row_h


def chapter_card(cv: Canvas, t: float, dur: float, d: dict) -> None:
    a = style.envelope(t, dur)
    cx, cy = cv.w / 2, cv.h * (0.42 if cv.vertical else 0.47)
    if d.get("scrim", True):
        Image, ImageDraw, ImageFilter = _img()
        small = Image.new("L", (cv.w // 8, cv.h // 8), 0)
        sd = ImageDraw.Draw(small)
        sd.ellipse(
            [cv.w / 8 * 0.18, cv.h / 8 * 0.3, cv.w / 8 * 0.82, cv.h / 8 * 0.66],
            fill=int(255 * 0.42 * a),
        )
        small = small.filter(ImageFilter.GaussianBlur(cv.w / 8 * 0.06)).resize(
            (cv.w, cv.h), Image.BILINEAR
        )
        scrim = Image.new("RGBA", (cv.w, cv.h), style.COLORS["iron"] + (0,))
        scrim.putalpha(small)
        cv.panel.alpha_composite(scrim)
    numeral = d.get("numeral")
    if numeral:
        cv.text(
            cx,
            cy - cv.px(78),
            numeral,
            style.TYPE_SCALE["h3"],
            style.COLORS["gold"],
            a,
            tracking=0.3,
            anchor="c",
        )
    grow = style.ease_out((t - 0.1) / style.TIMING["rule_draw"])
    cv.rule(
        cx - cv.px(70) * grow,
        cx + cv.px(70) * grow,
        cy - cv.px(50),
        style.COLORS["gold"],
        a,
    )
    title = d["title"].upper()
    usable = cv.safe[2] - cv.safe[0]
    size = style.TYPE_SCALE["h1"]
    lines = style.wrap(title, cv.px(size), usable, "display", style.TRACKING["heading"])
    for i, line in enumerate(lines):
        fitted = _fit(cv, line, size, usable, tracking=style.TRACKING["heading"])
        cv.text(
            cx,
            cy + cv.px(40) + i * cv.px(size) * 1.1,
            line,
            fitted,
            style.COLORS["ink"],
            a,
            tracking=style.TRACKING["heading"],
            anchor="c",
        )


def end_card(cv: Canvas, t: float, dur: float, d: dict) -> None:
    out = style.ease((dur - t) / 0.8) if dur - t < 0.8 else 1.0
    cv.box(
        [0, 0, cv.w, cv.h],
        style.COLORS["iron"],
        style.OPACITY["scrim"] * style.ease(t / 0.8),
    )
    cx = cv.w / 2
    usable = cv.safe[2] - cv.safe[0]
    nxt = d.get("next")
    y = cv.h * (0.20 if cv.vertical else 0.22)
    if nxt:
        a_n = style.ease((t - 0.2) / 0.7) * out
        cv.text(
            cx,
            y,
            f"NEXT  ·  EPISODE {style.roman(int(nxt['number']))}",
            style.TYPE_SCALE["label"],
            style.COLORS["gold"],
            a_n,
            tracking=0.28,
            anchor="c",
        )
        title = nxt["title"].upper()
        cv.text(
            cx,
            y + cv.px(78),
            title,
            _fit(cv, title, style.TYPE_SCALE["h2"], usable, tracking=0.16),
            style.COLORS["ink"],
            a_n,
            tracking=0.16,
            anchor="c",
        )
        if nxt.get("years"):
            cv.text(
                cx,
                y + cv.px(128),
                nxt["years"].upper(),
                style.TYPE_SCALE["small"],
                style.COLORS["ink_dim"],
                a_n,
                tracking=0.2,
                anchor="c",
            )
    cy = cv.h * (0.52 if cv.vertical else 0.55)
    a_c = style.ease((t - 1.0) / 0.8) * out
    grow = style.ease_out((t - 0.9) / style.TIMING["rule_draw"])
    cv.rule(
        cx - cv.px(110) * grow,
        cx + cv.px(110) * grow,
        cy - cv.px(96),
        style.COLORS["gold"],
        a_c,
    )
    title = d.get("title", "STANDARD OF IRON").upper()
    cv.text(
        cx,
        cy,
        title,
        _fit(cv, title, style.TYPE_SCALE["h1"], usable, tracking=0.2),
        style.COLORS["ink"],
        a_c,
        tracking=0.2,
        anchor="c",
    )
    sub = d.get("subtitle", "")
    cv.text(
        cx,
        cy + cv.px(62),
        sub,
        _fit(cv, sub, style.TYPE_SCALE["h3"], usable, tracking=0.18),
        style.COLORS["gold_bright"],
        a_c,
        tracking=0.18,
        anchor="c",
    )
    dest = d.get("end_card_destination", "")
    a_d = style.ease((t - 1.4) / 0.8) * out
    cv.text(
        cx,
        cy + cv.px(126),
        dest,
        _fit(cv, dest, style.TYPE_SCALE["label"], usable, tracking=0.06),
        style.COLORS["ink"],
        a_d,
        tracking=0.06,
        anchor="c",
    )
    y = cy + cv.px(186)
    for line in d.get("end_card_lines", []):
        cv.text(
            cx,
            y,
            line,
            _fit(cv, line, style.TYPE_SCALE["small"], usable, tracking=0.12),
            style.COLORS["ink_dim"],
            a_d,
            tracking=0.12,
            anchor="c",
        )
        y += cv.px(38)
    cv.text(
        cx,
        cv.safe[3] - cv.px(10),
        d.get("series", style.SERIES_TITLE),
        style.TYPE_SCALE["small"],
        style.COLORS["ink_faint"],
        a_d,
        tracking=0.3,
        anchor="c",
    )


def reel_hook(cv: Canvas, t: float, dur: float, d: dict) -> None:
    """The reel's first frame: the claim, fully opaque from frame zero."""
    out = style.ease((dur - t) / 0.35)
    usable = cv.safe[2] - cv.safe[0]
    size = style.TYPE_SCALE["reel_hook"]
    lines = style.wrap(d["text"].upper(), cv.px(size), usable, "display", 0.06)
    kicker = d.get("kicker", "").upper()
    k_size = style.TYPE_SCALE["h3"]
    k_lines = (
        style.wrap(kicker, cv.px(k_size), usable, "display", 0.1) if kicker else []
    )
    top = cv.h * 0.24
    height = (
        cv.px(size) * 1.12 * len(lines) + cv.px(k_size) * 1.3 * len(k_lines) + cv.px(60)
    )
    cv.scrim([0, top - cv.px(110), cv.w, top + height], 0.55 * out, 90)
    y = top
    for line in lines:
        cv.text(
            cv.w / 2,
            y,
            line,
            _fit(cv, line, size, usable, tracking=0.06),
            style.COLORS["ink"],
            out,
            tracking=0.06,
            anchor="c",
        )
        y += cv.px(size) * 1.12
    if k_lines:
        y += cv.px(24)
        cv.rule(
            cv.w / 2 - cv.px(60),
            cv.w / 2 + cv.px(60),
            y - cv.px(30),
            style.COLORS["gold"],
            out,
        )
        y += cv.px(20)
        for line in k_lines:
            cv.text(
                cv.w / 2,
                y,
                line,
                k_size,
                style.COLORS["gold_bright"],
                out,
                tracking=0.1,
                anchor="c",
            )
            y += cv.px(k_size) * 1.3


def reel_caption(cv: Canvas, t: float, dur: float, d: dict) -> None:
    """A burned-in caption: big capitals above the platform's own UI."""
    a = style.envelope(t, dur, 0.08, 0.08)
    size = style.TYPE_SCALE["reel_caption"]
    lines = d["lines"]
    y = cv.h * 0.64
    widest = max(cv.width(line, size, tracking=0.04) for line in lines)
    cv.scrim(
        [
            cv.w / 2 - widest / 2 - cv.px(30),
            y - cv.px(size) * 1.1,
            cv.w / 2 + widest / 2 + cv.px(30),
            y + cv.px(size) * 1.15 * (len(lines) - 1) + cv.px(30),
        ],
        0.5 * a,
        40,
    )
    for line in lines:
        cv.text(
            cv.w / 2,
            y,
            line,
            _fit(cv, line, size, cv.safe[2] - cv.safe[0], tracking=0.04),
            style.COLORS["ink"],
            a,
            tracking=0.04,
            anchor="c",
        )
        y += cv.px(size) * 1.15


def reel_tag(cv: Canvas, t: float, dur: float, d: dict) -> None:
    """Series tag: full battle on the channel, the series, and the Steam CTA."""
    a = style.ease(t / 0.4)
    cv.box([0, 0, cv.w, cv.h], style.COLORS["iron"], 0.6 * a)
    usable = cv.safe[2] - cv.safe[0]
    cx = cv.w / 2
    y = cv.h * 0.30
    cv.text(
        cx,
        y,
        "FULL BATTLE",
        style.TYPE_SCALE["h1"],
        style.COLORS["ink"],
        a,
        tracking=0.12,
        anchor="c",
    )
    cv.text(
        cx,
        y + cv.px(90),
        "ON THE CHANNEL",
        style.TYPE_SCALE["h2"],
        style.COLORS["gold_bright"],
        a,
        tracking=0.16,
        anchor="c",
    )
    cv.rule(cx - cv.px(80), cx + cv.px(80), y + cv.px(150), style.COLORS["gold"], a)
    series = d.get("series", style.SERIES_TITLE)
    cv.text(
        cx,
        y + cv.px(230),
        series,
        _fit(cv, series, style.TYPE_SCALE["h2"], usable, tracking=0.2),
        style.COLORS["ink"],
        a,
        tracking=0.2,
        anchor="c",
    )
    cv.text(
        cx,
        y + cv.px(290),
        d.get("episode_line", ""),
        style.TYPE_SCALE["label"],
        style.COLORS["ink_dim"],
        a,
        tracking=0.2,
        anchor="c",
    )
    a2 = style.ease((t - 0.6) / 0.5)
    y2 = cv.h * 0.62
    sub = d.get("subtitle", "")
    cv.text(
        cx,
        y2,
        sub,
        _fit(cv, sub, style.TYPE_SCALE["label"], usable, tracking=0.12),
        style.COLORS["gold_bright"],
        a2,
        tracking=0.12,
        anchor="c",
    )
    dest = d.get("end_card_destination", "")
    cv.text(
        cx,
        y2 + cv.px(56),
        dest,
        _fit(cv, dest, style.TYPE_SCALE["small"], usable, tracking=0.02),
        style.COLORS["ink"],
        a2,
        tracking=0.02,
        anchor="c",
    )


KINDS = {
    "title_sequence": title_sequence,
    "place_date": place_date,
    "order_of_battle": order_of_battle,
    "name_plate": name_plate,
    "army_counter": army_counter,
    "casualty_tally": casualty_tally,
    "chapter_card": chapter_card,
    "end_card": end_card,
    "reel_hook": reel_hook,
    "reel_caption": reel_caption,
    "reel_tag": reel_tag,
}
OPAQUE = {"title_sequence"}
ANIMATED = {"title_sequence"}
HOLD_AT = {
    "title_sequence": 0.62,
    "place_date": 0.5,
    "order_of_battle": 0.7,
    "name_plate": 0.5,
    "army_counter": 0.8,
    "casualty_tally": 0.8,
    "chapter_card": 0.5,
    "end_card": 0.7,
    "reel_hook": 0.1,
    "reel_caption": 0.5,
    "reel_tag": 0.8,
}


def graphic_data(
    episode, graphic: dict, section_numbers: dict[str, int] | None = None
) -> dict:
    """Merge a graphic event with the episode facts it shows."""
    data = episode.data
    kind = graphic["type"]
    if kind == "title_sequence":
        return {
            "series_title": data.get("series_title", style.SERIES_TITLE),
            "series_subtitle": data.get("series_subtitle", style.SERIES_SUBTITLE),
            "episode_line": f"EPISODE {style.roman(int(data['number']))}  ·  {data['title'].upper()}",
            "background": graphic.get("background", "black"),
        }
    if kind == "place_date":
        return {
            "place": graphic.get("place", data.get("place", data["title"])),
            "region": graphic.get("region", data.get("region", "")),
            "date": graphic.get("date", data.get("date")),
        }
    if kind == "order_of_battle":
        sides = []
        for side in data["sides"]:
            sides.append(
                {
                    "name": side["name"],
                    "color": style.side_color(side["id"]),
                    "strength": side["strength"],
                    "breakdown": side.get("breakdown", []),
                    "commanders": [c["name"] for c in side.get("commanders", [])],
                }
            )
        ratio = ""
        rendered = [s.get("rendered") for s in data["sides"]]
        if all(rendered):
            ratio = f"  ·  SHOWN IN GAME AT ABOUT 1:{round(data['sides'][0]['strength'] / rendered[0])}"
        source = data.get("strength_source", "")
        dot = "  ·  "
        footnote = graphic.get(
            "footnote",
            "HISTORICAL STRENGTHS" + (dot + source if source else "") + ratio,
        )
        return {"sides": sides, "footnote": footnote}
    if kind == "name_plate":
        commander, side = episode.commander(graphic["commander"])
        return {
            "name": commander["name"],
            "title": graphic.get("title", commander.get("title", "")),
            "side": side["name"],
            "color": style.side_color(side["id"]),
        }
    if kind == "army_counter":
        sides = []
        for side in data["sides"]:
            a, b = graphic.get("values", {}).get(
                side["id"], [side["strength"], side["strength"]]
            )
            sides.append(
                {
                    "name": side["name"],
                    "color": style.side_color(side["id"]),
                    "from": a,
                    "to": b,
                }
            )
        return {"sides": sides}
    if kind == "casualty_tally":
        rows = []
        for entry in data.get("casualties", []):
            side = episode.side(entry["side"])
            rows.append(
                {
                    "side": side["name"],
                    "color": style.side_color(side["id"]),
                    "label": entry.get("label", "Killed"),
                    "value": entry["value"],
                    "sources": entry.get("sources", []),
                }
            )
        return {"rows": rows, "heading": graphic.get("heading", "LOSSES")}
    if kind == "chapter_card":
        if "section" in graphic:
            section = episode.section(graphic["section"])
            number = (section_numbers or {}).get(graphic["section"])
            return {
                "title": graphic.get(
                    "title",
                    section.get("chapter") or section.get("title", section["id"]),
                ),
                "numeral": graphic.get(
                    "numeral", style.roman(number) if number else ""
                ),
            }
        return {"title": graphic["title"], "numeral": graphic.get("numeral", "")}
    if kind == "end_card":
        fields = end_card_fields(graphic.get("end_card", "steam_demo"))
        nxt = graphic.get("next", data.get("next_episode"))
        return {
            "title": fields.get("title"),
            "subtitle": fields.get("subtitle"),
            "end_card_destination": fields.get("end_card_destination"),
            "end_card_lines": fields.get("end_card_lines", []),
            "next": nxt,
            "series": data.get("series_title", style.SERIES_TITLE),
        }
    raise EditError(f"unknown graphic type {kind}")


def frame(
    kind: str,
    data: dict,
    width: int,
    height: int,
    t: float,
    dur: float,
    background=None,
):
    """One RGBA frame of a graphic ``t`` seconds into its ``dur``."""
    Image, _, _ = _img()
    cv = Canvas(width, height)
    KINDS[kind](cv, t, dur, data)
    if (
        background is None
        and kind in OPAQUE
        and data.get("background", "black") == "black"
    ):
        background = _vignette(width, height)
    return cv.compose(background)


def render_clip(
    kind: str, data: dict, width: int, height: int, fps: float, dur: float, out: Path
) -> Path:
    """Render a graphic to an RGBA ``qtrle`` .mov (cached by its inputs)."""
    from . import media

    out.parent.mkdir(parents=True, exist_ok=True)
    frames = max(1, int(round(dur * fps)))
    proc = subprocess.Popen(
        [
            "ffmpeg",
            "-hide_banner",
            "-v",
            "error",
            "-y",
            "-f",
            "rawvideo",
            "-pix_fmt",
            "rgba",
            "-s",
            f"{width}x{height}",
            "-r",
            f"{fps}",
            "-i",
            "-",
            "-c:v",
            "qtrle",
            "-pix_fmt",
            "argb",
            str(out),
        ],
        stdin=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    hold_start = 1.8
    hold_end = dur - style.TIMING["fade_out"] - 0.05
    held = None
    try:
        for n in range(frames):
            t = n / fps
            static = (
                kind not in ANIMATED
                and kind not in ("army_counter", "casualty_tally", "order_of_battle")
                and hold_start <= t <= hold_end
            )
            if static and held is not None:
                proc.stdin.write(held)
                continue
            raw = frame(kind, data, width, height, t, dur).tobytes()
            if static:
                held = raw
            proc.stdin.write(raw)
        proc.stdin.close()
    except BrokenPipeError:
        pass
    err = proc.stderr.read().decode(errors="replace")
    if proc.wait() != 0:
        raise EditError(f"rendering {kind} failed: {err[-800:]}")
    media.probe(out)
    return out


def still_background(width: int, height: int, source: Path | None = None):
    """A game frame to review stills against (the load-screen art by default)."""
    Image, _, _ = _img()
    path = source or (REPO / "assets" / "visuals" / "load_screen_1.png")
    image = Image.open(path).convert("RGB")
    scale = max(width / image.width, height / image.height)
    image = image.resize(
        (int(image.width * scale + 1), int(image.height * scale + 1)), Image.LANCZOS
    )
    left = (image.width - width) // 2
    top = (image.height - height) // 2
    return image.crop((left, top, left + width, top + height))


def render_still(
    kind: str,
    data: dict,
    width: int,
    height: int,
    out: Path,
    dur: float = 6.0,
    background=None,
    at: float | None = None,
) -> Path:
    t = dur * HOLD_AT[kind] if at is None else at
    bg = (
        background
        if background is not None
        else (None if kind in OPAQUE else still_background(width, height))
    )
    image = frame(kind, data, width, height, t, dur, bg)
    out.parent.mkdir(parents=True, exist_ok=True)
    image.convert("RGB").save(out)
    return out
