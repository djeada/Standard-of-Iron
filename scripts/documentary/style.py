"""The series look in one place: colours, faces, type scale, strokes, margins, timing.

Every graphic in ``graphics.py``, the reel captions, the thumbnail and the
world-registered tactical overlays (#1532) draw from these tokens, so one edit
here restyles the whole series and no two tools drift apart. Nothing in this
module needs Pillow until a drawing helper is called, so a renderer written in
another language can read the same tokens from ``python3 scripts/documentary
style --json``.

Sizes are authored at a 1080-pixel reference on the frame's *short* side and
scaled with :func:`unit`, so a 3840x2160 master and a 1080x1920 reel letter
identically. Colours are RGB tuples; :func:`hex_rgb` and :func:`qml_argb` give
the web and QML spellings (QML eight-digit colours are ``#AARRGGBB``).

The palette is the game's own: ink, gold and iron come from ``ui/theme.h``
(``textMain``, ``accent``, ``bgShade``), so cards read as the same product as
the menus. Side colours follow what the footage shows -- Roman red, Carthaginian
blue -- with contingent shades for the tactical overlays.
"""

from __future__ import annotations

import json
from functools import lru_cache
from pathlib import Path

from . import REPO

SERIES_TITLE = "THE BARCID ROAD"
SERIES_SUBTITLE = "HANNIBAL'S WAR"
SERIES_SHORT = "THE BARCID ROAD"

FONTS = {
    "display": REPO / "assets" / "fonts" / "StandardIronDisplay-Bold.ttf",
    "text": REPO / "assets" / "fonts" / "EBGaramond12-Bold.ttf",
}

COLORS: dict[str, tuple[int, int, int]] = {
    "ink": (244, 231, 200),
    "ink_dim": (212, 181, 124),
    "ink_faint": (141, 113, 70),
    "gold": (212, 161, 90),
    "gold_bright": (232, 201, 139),
    "rule": (167, 129, 74),
    "iron": (18, 13, 9),
    "panel": (29, 22, 16),
    "shadow": (0, 0, 0),
    "ember": (255, 140, 52),
    "ember_core": (255, 214, 150),
    "blood": (163, 41, 37),
}

SIDES: dict[str, dict] = {
    "rome": {
        "name": "ROME",
        "color": (196, 64, 52),
        "deep": (120, 34, 28),
        "contingents": {
            "legions": (196, 64, 52),
            "allies": (214, 106, 80),
            "cavalry": (232, 150, 112),
            "velites": (170, 92, 74),
        },
    },
    "carthage": {
        "name": "CARTHAGE",
        "color": (72, 122, 186),
        "deep": (36, 64, 112),
        "contingents": {
            "libyans": (72, 122, 186),
            "gauls": (95, 138, 60),
            "iberians": (176, 112, 52),
            "numidians": (201, 162, 39),
            "heavy_cavalry": (120, 160, 214),
            "balearics": (150, 176, 206),
            "elephants": (130, 130, 140),
        },
    },
}

TYPE_SCALE = {
    "title": 112,
    "h1": 76,
    "h2": 50,
    "h3": 38,
    "label": 30,
    "small": 24,
    "number": 96,
    "caption": 40,
    "reel_caption": 58,
    "reel_hook": 72,
}

TRACKING = {"title": 0.30, "heading": 0.18, "label": 0.16, "number": 0.06, "caption": 0.04}

STROKE = {
    "rule": 2.0,
    "rule_heavy": 4.0,
    "side_bar": 6.0,
    "arrow": 7.0,
    "arrow_outline": 2.5,
    "front_line": 5.0,
    "frame": 1.5,
}

OPACITY = {
    "panel": 0.78,
    "scrim": 0.55,
    "shadow": 0.72,
    "arrow_fill": 0.88,
    "zone_fill": 0.22,
}

SAFE = {
    "16:9": {"left": 0.06, "right": 0.06, "top": 0.07, "bottom": 0.08},
    "9:16": {"left": 0.07, "right": 0.07, "top": 0.12, "bottom": 0.22},
}

TIMING = {
    "fade_in": 0.5,
    "fade_out": 0.45,
    "rule_draw": 0.6,
    "count": 1.6,
    "stagger": 0.18,
    "min_hold": 2.5,
}

SEPARATOR = "·"


def aspect(width: int, height: int) -> str:
    return "9:16" if height > width else "16:9"


def unit(width: int, height: int) -> float:
    """Pixels per reference pixel: the short side over 1080."""
    return min(width, height) / 1080.0


def px(value: float, width: int, height: int) -> int:
    return max(1, int(round(value * unit(width, height))))


def safe_box(width: int, height: int) -> tuple[int, int, int, int]:
    """Title-safe rectangle (left, top, right, bottom) for the frame's aspect."""
    margins = SAFE[aspect(width, height)]
    return (
        int(width * margins["left"]),
        int(height * margins["top"]),
        int(width * (1 - margins["right"])),
        int(height * (1 - margins["bottom"])),
    )


def side_color(side: str) -> tuple[int, int, int]:
    return SIDES.get(side, {}).get("color", COLORS["ink_dim"])


def hex_rgb(color: tuple[int, int, int]) -> str:
    return "#{:02x}{:02x}{:02x}".format(*color[:3])


def qml_argb(color: tuple[int, int, int], alpha: float = 1.0) -> str:
    """QML spelling: ``#AARRGGBB`` (alpha first; ``#RRGGBBAA`` renders wrong)."""
    a = int(round(max(0.0, min(1.0, alpha)) * 255))
    return "#{:02x}{:02x}{:02x}{:02x}".format(a, *color[:3])


def as_dict() -> dict:
    """Every token, JSON-ready, for renderers outside Python."""
    return {
        "series": {"title": SERIES_TITLE, "subtitle": SERIES_SUBTITLE},
        "reference_short_side_px": 1080,
        "fonts": {k: str(v.relative_to(REPO)) for k, v in FONTS.items()},
        "fonts_note": "display face has capitals, digits and punctuation only; "
        "fall back to 'text' per glyph",
        "colors": {k: hex_rgb(v) for k, v in COLORS.items()},
        "sides": {
            k: {
                "name": v["name"],
                "color": hex_rgb(v["color"]),
                "deep": hex_rgb(v["deep"]),
                "contingents": {c: hex_rgb(rgb) for c, rgb in v["contingents"].items()},
            }
            for k, v in SIDES.items()
        },
        "type_scale_px": TYPE_SCALE,
        "tracking_em": TRACKING,
        "stroke_px": STROKE,
        "opacity": OPACITY,
        "safe_margins": SAFE,
        "timing_s": TIMING,
    }


def to_json() -> str:
    return json.dumps(as_dict(), indent=2)


@lru_cache(maxsize=256)
def font(face: str, size: int):
    from PIL import ImageFont

    return ImageFont.truetype(str(FONTS[face]), max(4, int(size)))


@lru_cache(maxsize=4096)
def has_glyph(face: str, char: str) -> bool:
    if char.isspace():
        return True
    return font(face, 64).getmask(char).getbbox() is not None


def _face_for(face: str, char: str) -> str:
    if face == "display" and not has_glyph("display", char):
        return "text"
    return face


def runs(text: str, face: str) -> list[tuple[str, str]]:
    """Split text into same-face runs, falling back per glyph to EB Garamond."""
    out: list[tuple[str, str]] = []
    for char in text:
        f = _face_for(face, char)
        if out and out[-1][1] == f:
            out[-1] = (out[-1][0] + char, f)
        else:
            out.append((char, f))
    return out


def text_width(text: str, size: int, face: str = "display", tracking: float = 0.0) -> float:
    gap = tracking * size
    width = 0.0
    for char in text:
        width += font(_face_for(face, char), size).getlength(char)
    return width + gap * max(0, len(text) - 1)


def draw_text(
    draw,
    xy: tuple[float, float],
    text: str,
    size: int,
    fill,
    face: str = "display",
    tracking: float = 0.0,
    anchor: str = "l",
) -> float:
    """Draw tracked text with per-glyph fallback; returns the drawn width.

    ``anchor`` is ``l``, ``c`` or ``r`` horizontally; ``y`` is the baseline of
    the display face's capitals so mixed faces sit on one line.
    """
    width = text_width(text, size, face, tracking)
    x = xy[0] - (width / 2 if anchor == "c" else width if anchor == "r" else 0)
    gap = tracking * size
    for char in text:
        f = font(_face_for(face, char), size)
        draw.text((x, xy[1]), char, font=f, fill=fill, anchor="ls")
        x += f.getlength(char) + gap
    return width


def cap_height(size: int) -> float:
    return size * 0.70


def wrap(text: str, size: int, max_width: float, face: str = "display", tracking: float = 0.0) -> list[str]:
    """Greedy wrap, then rebalance the last two lines so neither is a widow."""
    words = text.split()
    lines: list[str] = []
    for word in words:
        trial = f"{lines[-1]} {word}" if lines else word
        if lines and text_width(trial, size, face, tracking) <= max_width:
            lines[-1] = trial
        else:
            lines.append(word)
    if len(lines) >= 2:
        both = (lines[-2] + " " + lines[-1]).split()
        best = None
        for cut in range(1, len(both)):
            a, b = " ".join(both[:cut]), " ".join(both[cut:])
            wa, wb = text_width(a, size, face, tracking), text_width(b, size, face, tracking)
            if wa <= max_width and wb <= max_width:
                score = abs(wa - wb)
                if best is None or score < best[0]:
                    best = (score, a, b)
        if best:
            lines[-2:] = [best[1], best[2]]
    return lines


def roman(number: int) -> str:
    table = [
        (1000, "M"),
        (900, "CM"),
        (500, "D"),
        (400, "CD"),
        (100, "C"),
        (90, "XC"),
        (50, "L"),
        (40, "XL"),
        (10, "X"),
        (9, "IX"),
        (5, "V"),
        (4, "IV"),
        (1, "I"),
    ]
    out = ""
    for value, glyph in table:
        while number >= value:
            out += glyph
            number -= value
    return out


def ease(t: float) -> float:
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def ease_out(t: float) -> float:
    t = max(0.0, min(1.0, t))
    return 1 - (1 - t) ** 3


def envelope(t: float, dur: float, fade_in: float | None = None, fade_out: float | None = None) -> float:
    """Opacity of a graphic ``t`` seconds into its ``dur``: eased in, held, eased out."""
    fi = TIMING["fade_in"] if fade_in is None else fade_in
    fo = TIMING["fade_out"] if fade_out is None else fade_out
    a = ease(t / fi) if fi > 0 else (1.0 if t >= 0 else 0.0)
    b = ease((dur - t) / fo) if fo > 0 else (1.0 if t <= dur else 0.0)
    return max(0.0, min(a, b))


def stamp_text(place: str, date: dict | None) -> tuple[str, str]:
    """``("CANNAE", "2 AUGUST 216 BC")`` -- the two halves of a place/date stamp."""
    if not date:
        return place.upper(), ""
    parts = []
    if date.get("day"):
        parts.append(str(int(date["day"])))
    if date.get("month"):
        parts.append(str(date["month"]).upper())
    if date.get("season") and not date.get("month"):
        parts.append(str(date["season"]).upper())
    year = date.get("year")
    if year is not None:
        parts.append(f"{abs(int(year))} {date.get('era', 'BC').upper()}")
    return place.upper(), " ".join(parts)


def format_number(value: float) -> str:
    return f"{int(round(value)):,}"


def save_tokens(path: Path) -> Path:
    path.write_text(to_json() + "\n")
    return path
