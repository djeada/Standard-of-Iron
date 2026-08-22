"""Outline primitives for the Standard Iron display face.

The face is drawn from three recurring forms and nothing else:

    wedge  -- the flared Roman serif that ends a stem or an arm
    cut    -- a 35-degree chisel bite taken out of a joint
    point  -- a blade terminal, used where a stroke wants to look sharpened

Every glyph in `glyph_shapes.py` is assembled out of the helpers below, which
is the whole reason the alphabet reads as one alphabet. Adding a fourth form
here is a decision about the game's identity, not a drawing convenience.

A glyph is material added and material taken away; the builder resolves the two
with boolean operations, so a letter can be written as a stem plus an arm plus
a bowl plus a bite without anyone reasoning about where the pieces overlap. It
is also why every contour here is wound the same way regardless of its job, and
why `cw()` normalises direction rather than trusting the order points happen
to be written in.

The constants below, and what each one controls:

    STEM/THIN
        Stem weights. The face is heavy because captions sit over moving
        footage behind a dark border, and anything lighter gets eaten by the
        border.
    EXT/WEDGE_H
        The wedge. EXT is how far the serif flares past the stem on each side,
        and WEDGE_H how far up the stem the flare runs. The ratio between them
        sets how carved the letter looks: taller and narrower reads as cut
        stone, shorter and wider reads as a slab.
    CUT_DEGREES
        The cut. One angle for the entire font -- this is the dark-fantasy
        half of the brief, and it only works as an identity if it never
        varies.
    CAP
        The design grid. Cap height rather than em size is the number that
        matters: this is an all-caps face, so every vertical proportion is a
        fraction of CAP, and changing CAP alone rescales the alphabet
        coherently.
"""

from __future__ import annotations

import math

UPM = 1000
CAP = 700
BASELINE = 0
ASCENDER = 780
DESCENDER = -220

STEM = 148
THIN = 96

EXT = 42
WEDGE_H = 64

CUT_DEGREES = 35.0

SIDEBEARING = 46


CURVE_OVERSHOOT = 12

Point = tuple[float, float]
Contour = list[Point]


def signed_area(points: list[Point]) -> float:
    total = 0.0
    for index, (x0, y0) in enumerate(points):
        x1, y1 = points[(index + 1) % len(points)]
        total += (x0 * y1) - (x1 * y0)
    return total / 2.0


def cw(points: list[Point]) -> Contour:
    """Normalise a contour's direction.

    Every contour in this font is wound the same way, whether it is material
    being added or material being taken away, because the builder resolves the
    two with boolean operations rather than by winding. Normalising anyway
    keeps the outlines tidy and the point order predictable.
    """
    return list(points) if signed_area(points) < 0 else list(reversed(points))


def stem(
    x_left: float,
    x_right: float,
    y_bottom: float = BASELINE,
    y_top: float = CAP,
    foot: bool = True,
    head: bool = True,
    ext: float = EXT,
) -> Contour:
    """A vertical stem, wedge-served at whichever ends are asked for.

    `foot`/`head` are separate because the joined stems (B, D, P, R) keep their
    outer serif and lose the inner one, and letters that grow a bowl out of the
    stem top (P, R) want no head flare on that side at all.
    """
    head_ext = ext if head else 0.0
    foot_ext = ext if foot else 0.0
    return cw(
        [
            (x_left - foot_ext, y_bottom),
            (x_left, y_bottom + (WEDGE_H if foot else 0.0)),
            (x_left, y_top - (WEDGE_H if head else 0.0)),
            (x_left - head_ext, y_top),
            (x_right + head_ext, y_top),
            (x_right, y_top - (WEDGE_H if head else 0.0)),
            (x_right, y_bottom + (WEDGE_H if foot else 0.0)),
            (x_right + foot_ext, y_bottom),
        ]
    )


def foot_serif(
    centre: float, width: float, y: float = BASELINE, ext: float = EXT
) -> Contour:
    """The wedge that finishes a stroke at the baseline.

    `stem()` grows its own serifs, but a diagonal cannot: its terminal is cut
    horizontally at whatever x the slope happens to reach, so the serif has to
    be placed there separately. Without these the diagonal letters read as a
    different, sans-serif alphabet sitting inside the serifed one -- which is
    exactly how X, V, W, Y and K looked before they had them.

    Symmetric rather than flared outward only: at this weight an asymmetric
    diagonal serif reads as a mistake rather than as a stroke direction.
    """
    half = width / 2.0
    return cw(
        [
            (centre - half - ext, y),
            (centre - half, y + WEDGE_H),
            (centre + half, y + WEDGE_H),
            (centre + half + ext, y),
        ]
    )


def head_serif(
    centre: float, width: float, y: float = CAP, ext: float = EXT
) -> Contour:
    """The same wedge, finishing a stroke at the cap line."""
    half = width / 2.0
    return cw(
        [
            (centre - half - ext, y),
            (centre - half, y - WEDGE_H),
            (centre + half, y - WEDGE_H),
            (centre + half + ext, y),
        ]
    )


def bar(
    x_left: float,
    x_right: float,
    y_bottom: float,
    y_top: float,
    flare_left: bool = False,
    flare_right: bool = False,
    ext: float = EXT,
) -> Contour:
    """A horizontal arm. Free ends flare vertically; joined ends stay square."""
    points: list[Point] = []
    if flare_left:
        points += [
            (x_left, y_bottom - ext),
            (x_left, y_top + ext),
            (x_left + WEDGE_H, y_top),
        ]
    else:
        points += [(x_left, y_bottom), (x_left, y_top)]

    if flare_right:
        points += [
            (x_right - WEDGE_H, y_top),
            (x_right, y_top + ext),
            (x_right, y_bottom - ext),
            (x_right - WEDGE_H, y_bottom),
        ]
    else:
        points += [(x_right, y_top), (x_right, y_bottom)]

    if flare_left:
        points += [(x_left + WEDGE_H, y_bottom)]
    return cw(points)


def diagonal(
    x_top: float,
    y_top: float,
    x_bottom: float,
    y_bottom: float,
    width_top: float,
    width_bottom: float | None = None,
) -> Contour:
    """A diagonal stroke cut horizontally at both ends.

    Horizontal cuts rather than perpendicular ones: the ends have to sit flush
    against the cap line and the baseline, the way a chisel working along a
    ruled line leaves them, and a perpendicular cut leaves a visible nick.
    """
    if width_bottom is None:
        width_bottom = width_top
    return cw(
        [
            (x_top - width_top / 2.0, y_top),
            (x_top + width_top / 2.0, y_top),
            (x_bottom + width_bottom / 2.0, y_bottom),
            (x_bottom - width_bottom / 2.0, y_bottom),
        ]
    )


def cut(
    x: float,
    y: float,
    size: float,
    facing: str = "left",
    degrees: float = CUT_DEGREES,
) -> Contour:
    """The chisel bite. A hole, so it removes material from whatever it meets.

    `(x, y)` is the point of the notch -- the deepest point of the bite -- and
    `facing` says which way the open mouth points, i.e. which edge of the
    stroke the chisel came in from.
    """
    angle = math.radians(degrees)
    run = size * math.sin(angle)
    rise = size * math.cos(angle)
    sign = -1.0 if facing == "left" else 1.0
    return cw(
        [
            (x, y),
            (x + sign * run, y + rise),
            (x + sign * run, y - rise),
        ]
    )


def ellipse(cx: float, cy: float, rx: float, ry: float) -> list:
    """A bowl, as four cubic arcs. Returned as pen instructions, not points."""
    k = 0.5522847498
    ox, oy = rx * k, ry * k
    return [
        ("move", (cx, cy + ry)),
        ("curve", ((cx + ox, cy + ry), (cx + rx, cy + oy), (cx + rx, cy))),
        ("curve", ((cx + rx, cy - oy), (cx + ox, cy - ry), (cx, cy - ry))),
        ("curve", ((cx - ox, cy - ry), (cx - rx, cy - oy), (cx - rx, cy))),
        ("curve", ((cx - rx, cy + oy), (cx - ox, cy + ry), (cx, cy + ry))),
        ("close", ()),
    ]
