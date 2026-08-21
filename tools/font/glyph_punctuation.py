"""Figures and punctuation.

The digits get as much attention as the letters and arguably need more of it.
Everything this face exists for -- "240 VS 900", "SURVIVE 60 SECONDS",
"OUTNUMBERED 4:1", a takedown counter ticking over a reel -- is a number read
at a glance on a phone. So the figures are lining (cap height, aligned), they
are drawn wide enough not to jam, and the pairs that collapse into each other
in a heavy face are pulled deliberately apart: 6 against 8, 3 against 8, 1
against 7, 0 against O.
"""

from __future__ import annotations

from glyph_geometry import (
    CAP,
    STEM,
    THIN,
    bar,
    cut,
    cw,
    diagonal,
    stem,
)
from glyph_shapes import BOWL_THIN, Glyph, mask, ring_parts

FIG = CAP  # lining figures: same height as the capitals, no exceptions


def digit_zero() -> Glyph:
    """A shield, not an O. Narrower, with the weight in the sides and the crown
    and base flattened, so 0 and O never trade places in a unit count.
    """
    width = 556
    glyph = Glyph()
    bowl = ring_parts(width / 2.0, FIG / 2.0, width / 2.0, FIG / 2.0, glyph)
    bowl.holes.append(mask(-60, FIG - 8, width + 60, FIG + 60))
    bowl.holes.append(mask(-60, -60, width + 60, 8))
    glyph.contours.append(bar(width * 0.20, width * 0.80, FIG - BOWL_THIN - 6, FIG - 8))
    glyph.contours.append(bar(width * 0.20, width * 0.80, 8, BOWL_THIN + 6))
    return glyph


def digit_one() -> Glyph:
    """A full base bar and a long entry flag. A bare stem with a stub is what
    makes 1 collide with I and with 7 at caption size.
    """
    width = 420
    x0 = (width - STEM) / 2.0
    return Glyph(
        contours=[
            stem(x0, x0 + STEM, foot=False, head=False),
            bar(0, width, 0, THIN, flare_left=True, flare_right=True),
            diagonal(x0 - 120, FIG - 170, x0 + 40, FIG, THIN + 24, THIN),
        ]
    )


def digit_two() -> Glyph:
    width = 556
    glyph = Glyph()
    cy, ry = FIG * 0.715, FIG * 0.285
    bowl = ring_parts(width / 2.0, cy, width / 2.0, ry, glyph)
    bowl.holes.append(mask(-80, -80, width + 80, cy))
    glyph.contours.append(
        diagonal(width * 0.80, cy, width * 0.12, THIN, STEM - 24, STEM)
    )
    glyph.contours.append(bar(0, width, 0, THIN, flare_left=True, flare_right=True))
    return glyph


def digit_three() -> Glyph:
    """The upper bowl loses its lower left and the lower bowl its upper left,
    so the two arcs meet on the right at the waist and the figure opens
    leftward twice. Swapping those two quadrants -- which is easy to do and
    was in fact done first -- produces a mirrored letter with a barb where the
    waist should be, still perfectly closed and still not a 3.

    Each cut also has to reach its own bowl's centre. Stopping short of it
    leaves a hump on the open side, and a 3 with two humps is an 8 with a bite
    taken out of it.
    """
    width = 536
    glyph = Glyph()
    upper_cy, upper_r = FIG * 0.735, FIG * 0.265
    lower_cy, lower_r = FIG * 0.275, FIG * 0.275
    upper_cx, lower_cx = width * 0.47, width * 0.49
    upper = ring_parts(upper_cx, upper_cy, upper_cx, upper_r, glyph)
    lower = ring_parts(lower_cx, lower_cy, lower_cx, lower_r, glyph)
    upper.holes.append(mask(-100, upper_cy - upper_r - 100, upper_cx, upper_cy + 8))
    lower.holes.append(mask(-100, lower_cy - 8, lower_cx, lower_cy + lower_r + 100))
    return glyph


def digit_four() -> Glyph:
    width = 588
    stem_x = width * 0.60
    return Glyph(
        contours=[
            stem(stem_x, stem_x + STEM, foot=True, head=False),
            diagonal(stem_x + 20, FIG, 34, FIG * 0.28, STEM - 24, STEM - 24),
            bar(0, width, FIG * 0.24, FIG * 0.24 + THIN, flare_right=True),
        ]
    )


def digit_five() -> Glyph:
    """The bowl opens at the upper left and the stem stops above where it opens,
    leaving a notch on the left at mid height. Without that notch the stem
    runs straight into the bowl's left arc and the whole figure reads as a
    lowercase b -- which it did, until the cut was taken below the centre.
    """
    width = 540
    glyph = Glyph()
    cy, ry = FIG * 0.295, FIG * 0.295
    bowl_cx = width * 0.49
    bowl = ring_parts(bowl_cx, cy, bowl_cx, ry, glyph)
    bowl.holes.append(mask(-100, cy - ry * 0.28, bowl_cx, FIG + 100))
    glyph.contours.append(
        stem(0, STEM, y_bottom=cy + ry * 0.06, y_top=FIG, foot=False, head=False)
    )
    glyph.contours.append(bar(0, width, FIG - THIN, FIG, flare_right=True))
    return glyph


def digit_six() -> Glyph:
    """The tail leaves the bowl and reaches the cap line. Its length is what
    separates 6 from 8 and from 0 at a glance.
    """
    width = 560
    glyph = Glyph()
    cy, ry = FIG * 0.29, FIG * 0.29
    ring_parts(width / 2.0, cy, width / 2.0, ry, glyph)
    glyph.contours.append(
        diagonal(width * 0.74, FIG, 14, cy + ry * 0.5, STEM - 34, STEM)
    )
    return glyph


def digit_seven() -> Glyph:
    """A blade. The broad bar is the haft and the diagonal the edge, cut thinner
    as it descends.
    """
    width = 552
    return Glyph(
        contours=[
            bar(0, width, FIG - THIN, FIG, flare_left=True, flare_right=True),
            diagonal(width - 104, FIG - THIN, width * 0.22, 0, STEM + 20, STEM - 50),
        ],
        holes=[cut(width * 0.32, 170, 60, facing="left")],
    )


def digit_eight() -> Glyph:
    width = 572
    glyph = Glyph()
    ring_parts(width * 0.50, FIG * 0.735, width * 0.435, FIG * 0.265, glyph)
    ring_parts(width * 0.50, FIG * 0.28, width * 0.50, FIG * 0.28, glyph)
    return glyph


def digit_nine() -> Glyph:
    width = 560
    glyph = Glyph()
    cy, ry = FIG * 0.71, FIG * 0.29
    ring_parts(width / 2.0, cy, width / 2.0, ry, glyph)
    glyph.contours.append(
        diagonal(width - 14, cy - ry * 0.5, width * 0.26, 0, STEM, STEM - 34)
    )
    return glyph


def _dot(cx: float, cy: float, size: float):
    """Punctuation dots are squared, not round: chiselled, and they survive
    the black border the captions are drawn behind."""
    half = size / 2.0
    return cw(
        [
            (cx - half, cy - half),
            (cx - half, cy + half),
            (cx + half, cy + half),
            (cx + half, cy - half),
        ]
    )


DOT = 168.0


def period() -> Glyph:
    return Glyph(contours=[_dot(DOT / 2.0, DOT / 2.0, DOT)])


def ellipsis() -> Glyph:
    # Slightly smaller and closer than three full stops. At title sizes three
    # period glyphs look like separate beats; the ellipsis should read as one
    # quiet mark at the end of the loading label.
    size, gap = 132.0, 58.0
    return Glyph(
        contours=[
            _dot(size / 2.0 + index * (size + gap), size / 2.0, size)
            for index in range(3)
        ]
    )


def comma() -> Glyph:
    return Glyph(
        contours=[
            _dot(DOT / 2.0, DOT / 2.0, DOT),
            cw([(0, 0), (DOT, 0), (DOT * 0.34, -DOT * 0.86)]),
        ]
    )


def colon() -> Glyph:
    return Glyph(
        contours=[_dot(DOT / 2.0, DOT / 2.0, DOT), _dot(DOT / 2.0, CAP * 0.56, DOT)]
    )


def semicolon() -> Glyph:
    return Glyph(
        contours=[
            _dot(DOT / 2.0, DOT / 2.0, DOT),
            cw([(0, 0), (DOT, 0), (DOT * 0.34, -DOT * 0.86)]),
            _dot(DOT / 2.0, CAP * 0.56, DOT),
        ]
    )


def exclam() -> Glyph:
    bottom = DOT * 1.75
    return Glyph(
        contours=[
            cw([(0, bottom), (20, CAP), (STEM + 20, CAP), (STEM, bottom)]),
            _dot(STEM / 2.0 + 10, DOT / 2.0, DOT),
        ]
    )


def question() -> Glyph:
    width = 500
    glyph = Glyph()
    cy, ry = CAP * 0.755, CAP * 0.245
    bowl = ring_parts(width / 2.0, cy, width / 2.0, ry, glyph)
    bowl.holes.append(mask(-80, cy - ry - 80, width * 0.60, cy))
    glyph.contours.append(
        bar((width - STEM) / 2.0 + 46, (width + STEM) / 2.0 + 46, CAP * 0.30, cy)
    )
    glyph.contours.append(_dot(width / 2.0 + 46, DOT / 2.0, DOT))
    return glyph


def exclamdown() -> Glyph:
    top = CAP - DOT * 1.75
    return Glyph(
        contours=[
            cw([(0, top), (20, 0), (STEM + 20, 0), (STEM, top)]),
            _dot(STEM / 2.0 + 10, CAP - DOT / 2.0, DOT),
        ]
    )


def questiondown() -> Glyph:
    width = 500
    glyph = Glyph()
    cy, ry = CAP * 0.245, CAP * 0.245
    bowl = ring_parts(width / 2.0, cy, width / 2.0, ry, glyph)
    bowl.holes.append(mask(width * 0.40, cy, width + 80, cy + ry + 80))
    glyph.contours.append(
        bar((width - STEM) / 2.0 - 46, (width + STEM) / 2.0 - 46, cy, CAP * 0.70)
    )
    glyph.contours.append(_dot(width / 2.0 - 46, CAP - DOT / 2.0, DOT))
    return glyph


def hyphen() -> Glyph:
    return Glyph(contours=[bar(0, 300, CAP * 0.42, CAP * 0.42 + THIN)])


def endash() -> Glyph:
    return Glyph(contours=[bar(0, 480, CAP * 0.42, CAP * 0.42 + THIN)])


def emdash() -> Glyph:
    return Glyph(contours=[bar(0, 800, CAP * 0.42, CAP * 0.42 + THIN)])


def plus() -> Glyph:
    width, mid = 480, CAP * 0.46
    arm = width / 2.0
    return Glyph(
        contours=[
            bar(0, width, mid - THIN / 2.0, mid + THIN / 2.0),
            cw(
                [
                    ((width - THIN) / 2.0, mid - arm),
                    ((width - THIN) / 2.0, mid + arm),
                    ((width + THIN) / 2.0, mid + arm),
                    ((width + THIN) / 2.0, mid - arm),
                ]
            ),
        ]
    )


def multiply() -> Glyph:
    """The "3x" and "4x" of a reel. A real multiplication sign, rather than left
    to a lowercase x this caps face does not have.
    """
    width, mid, reach = 460, CAP * 0.46, 200.0
    return Glyph(
        contours=[
            diagonal(
                width / 2.0 - reach, mid + reach, width / 2.0 + reach, mid - reach, THIN
            ),
            diagonal(
                width / 2.0 + reach, mid + reach, width / 2.0 - reach, mid - reach, THIN
            ),
        ]
    )


def slash() -> Glyph:
    return Glyph(contours=[diagonal(420, CAP + 40, 0, -60, THIN + 10)])


def percent() -> Glyph:
    """The counters are punched marks in the chisel language, not small circles
    borrowed from a text face.
    """
    width, r = 780, 118.0
    glyph = Glyph(contours=[diagonal(width - 60, CAP + 30, 60, -30, THIN)])
    ring_parts(r + 40, CAP - r - 30, r, r, glyph)
    ring_parts(width - r - 40, r + 30, r, r, glyph)
    return glyph


def ampersand() -> Glyph:
    width = 700
    glyph = Glyph()
    upper_cy, upper_r = CAP * 0.775, CAP * 0.225
    lower_cy, lower_r = CAP * 0.285, CAP * 0.285
    ring_parts(width * 0.40, upper_cy, width * 0.28, upper_r, glyph)
    bowl = ring_parts(width * 0.40, lower_cy, width * 0.40, lower_r, glyph)
    bowl.holes.append(
        mask(width * 0.54, lower_cy - 30, width + 100, lower_cy + lower_r + 100)
    )
    glyph.contours.append(
        diagonal(width * 0.30, lower_cy + lower_r * 0.6, width, 0, THIN + 12)
    )
    return glyph


def numbersign() -> Glyph:
    width = 700
    return Glyph(
        contours=[
            bar(0, width, CAP * 0.30, CAP * 0.30 + THIN * 0.8),
            bar(0, width, CAP * 0.58, CAP * 0.58 + THIN * 0.8),
            diagonal(width * 0.44, CAP, width * 0.28, 0, THIN * 0.9),
            diagonal(width * 0.78, CAP, width * 0.62, 0, THIN * 0.9),
        ]
    )


def quotesingle() -> Glyph:
    return Glyph(
        contours=[
            cw(
                [
                    (0, CAP),
                    (THIN, CAP),
                    (THIN * 0.62, CAP - 210),
                    (THIN * 0.38, CAP - 210),
                ]
            )
        ]
    )


def quotedbl() -> Glyph:
    left = quotesingle().contours[0]
    right = [(x + THIN + 70, y) for x, y in left]
    return Glyph(contours=[left, right])


def parenleft() -> Glyph:
    width = 300
    glyph = Glyph()
    bowl = ring_parts(width, CAP / 2.0, width, CAP * 0.62, glyph)
    bowl.holes.append(mask(width, -CAP, width * 3, CAP * 2))
    return glyph


def parenright() -> Glyph:
    width = 300
    glyph = Glyph()
    bowl = ring_parts(0, CAP / 2.0, width, CAP * 0.62, glyph)
    bowl.holes.append(mask(-width * 3, -CAP, 0, CAP * 2))
    return glyph


def bracketleft() -> Glyph:
    width, top, bottom = 300, CAP * 1.06, -CAP * 0.06
    return Glyph(
        contours=[
            cw(
                [
                    (0, bottom),
                    (0, top),
                    (width, top),
                    (width, top - THIN * 0.9),
                    (THIN, top - THIN * 0.9),
                    (THIN, bottom + THIN * 0.9),
                    (width, bottom + THIN * 0.9),
                    (width, bottom),
                ]
            )
        ]
    )


def bracketright() -> Glyph:
    width = 300
    return Glyph(contours=[cw([(width - x, y) for x, y in bracketleft().contours[0]])])


PUNCTUATION = {
    "0": digit_zero,
    "1": digit_one,
    "2": digit_two,
    "3": digit_three,
    "4": digit_four,
    "5": digit_five,
    "6": digit_six,
    "7": digit_seven,
    "8": digit_eight,
    "9": digit_nine,
    ".": period,
    "\u2026": ellipsis,
    ",": comma,
    ":": colon,
    ";": semicolon,
    "!": exclam,
    "?": question,
    "\u00a1": exclamdown,
    "\u00bf": questiondown,
    "-": hyphen,
    "\u2013": endash,
    "\u2014": emdash,
    "+": plus,
    "\u00d7": multiply,
    "/": slash,
    "%": percent,
    "&": ampersand,
    "#": numbersign,
    "'": quotesingle,
    '"': quotedbl,
    "(": parenleft,
    ")": parenright,
    "[": bracketleft,
    "]": bracketright,
}
