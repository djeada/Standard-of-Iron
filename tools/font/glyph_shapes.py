"""The Standard Iron alphabet, drawn out of the primitives in glyph_geometry.

Every function returns a `Glyph`: some number of straight-sided contours, some
number of cubic paths, and an advance width. Nothing here does boolean
geometry -- overlapping same-wound contours union under the nonzero rule and
opposite-wound ones cut, so a letter is assembled the way a mason would build
it up, not the way a vector editor would.

Proportions are semi-condensed and heavy on purpose: this face exists to be
legible at 70px on a phone, over moving footage, behind a dark border.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from glyph_geometry import (
    CAP,
    EXT,
    SIDEBEARING,
    STEM,
    THIN,
    bar,
    cut,
    cw,
    diagonal,
    ellipse,
    foot_serif,
    head_serif,
    stem,
)


@dataclass
class Part:
    """A piece of a letter that gets trimmed before it joins the rest.

    A bowl is drawn whole and then cut back to meet its stem. If that cut ran
    against the finished letter it would take the stem with it, so the bowl
    resolves on its own first and only then merges. This is why B, D, P and R
    are parts and H is not.
    """

    contours: list = field(default_factory=list)
    paths: list = field(default_factory=list)
    holes: list = field(default_factory=list)
    hole_paths: list = field(default_factory=list)


@dataclass
class Glyph:
    """One letter, as material added and material taken away.

    `contours` and `paths` are the strokes the letter is built from; `holes`
    are the counters, the chisel bites and the parts of a bowl that get opened
    up. The builder unions the first two and subtracts the third with real
    boolean geometry.

    An earlier version leaned on the nonzero winding rule instead -- reversed
    contours to cut, same-wound ones to join -- which is cheaper and wrong: a
    reversed contour that extends past the shape it was meant to cut has
    winding -1 out there, and nonzero fills anything that is not zero. Cutting
    the mouth of a C painted a rectangle beside it.

    Fields:
        contours, paths
            Material to add: straight-sided contours and cubic paths.
        holes, hole_paths
            Material to remove from the *finished* letter, after every part
            below has merged. The chisel bites belong here; a bowl's trim does
            not, because it would take the stem with it.
        parts
            Pieces that resolve on their own before joining, each carrying its
            own holes. This is what B, D, P and R are built out of.
        advance
            Ignored; spacing is measured off the drawn ink so that redrawing a
            letter respaces it automatically.
        fixed_advance
            An explicit advance, for the one case where the ink deliberately
            runs outside it: Q's tail is meant to reach under the letter that
            follows, and auto-spacing would push that letter away instead.
    """

    contours: list = field(default_factory=list)
    paths: list = field(default_factory=list)
    holes: list = field(default_factory=list)
    hole_paths: list = field(default_factory=list)
    parts: list = field(default_factory=list)
    advance: float = 0.0
    fixed_advance: float | None = None


# Ring weights. Bowls carry slightly less weight at the top and bottom than the
# stems do, which is what stops a heavy caps face from looking like a slab.
BOWL_SIDE = STEM - 8
BOWL_THIN = THIN

# Optical sidebearing multipliers. A round letter set on the same flat
# sidebearing as H looks loose, and a diagonal one looks looser still, because
# the ink retreats from the advance as the eye travels up the stroke. These are
# applied by the builder rather than baked into each glyph so that the numbers
# sit together where they can be compared.
ROUND_SIDES = 0.80
DIAGONAL_SIDES = 0.66


def ring(cx: float, cy: float, rx: float, ry: float) -> tuple[list, list]:
    """An O, as (added bowl, counter to subtract)."""
    return (
        [ellipse(cx, cy, rx, ry)],
        [ellipse(cx, cy, rx - BOWL_SIDE, ry - BOWL_THIN)],
    )


def ring_parts(cx: float, cy: float, rx: float, ry: float, into: "Glyph") -> Part:
    """Attach a bowl to a glyph as its own part, and hand it back.

    Returned so the caller can trim it -- `part.holes.append(mask(...))` cuts
    the bowl and nothing else.
    """
    added, cut_away = ring(cx, cy, rx, ry)
    part = Part(paths=list(added), hole_paths=list(cut_away))
    into.parts.append(part)
    return part


def stem_bowl(
    glyph: "Glyph",
    stem_right: float,
    right_edge: float,
    y_center: float,
    ry: float,
) -> Part:
    """A bowl that grows out of the right edge of a stem: B, D, P, R.

    Centred exactly on the stem's right edge and then cut back to it, so the
    half that survives meets the stem along a straight face and the counter
    closes against it. Getting that centre right is what removes the need for
    patching bars at the shoulder and the foot -- an earlier version put the
    centre inside the stem and then papered over the resulting gap with two
    horizontal bars, which is what left B and D with ledges sticking out past
    the curve.
    """
    part = ring_parts(stem_right, y_center, right_edge - stem_right, ry, glyph)
    part.holes.append(mask(-400, y_center - ry - 60, stem_right, y_center + ry + 60))
    return part


def blade_mouth(cx: float, cy: float, rx: float, ry: float, opening: float):
    """The wedge cut out of a C, a G or an S to open its bowl.

    Angled rather than square: a vertical cut leaves two blunt slabs, and the
    terminals are supposed to read as sharpened -- the `point` of the three
    forms this face is built from.
    """
    return cw(
        [
            (cx + rx * 0.30, cy - opening),
            (cx + rx * 2.0, cy - opening - ry * 0.55),
            (cx + rx * 2.0, cy + opening + ry * 0.55),
            (cx + rx * 0.30, cy + opening),
        ]
    )


def mask(x0: float, y0: float, x1: float, y1: float):
    """A box of material to remove. Used to open a bowl into a C, G or S."""
    return cw([(x0, y0), (x0, y1), (x1, y1), (x1, y0)])


def apex_pair(
    apex_x: float,
    overshoot: float,
    left_foot: float,
    right_foot: float,
    left_width: float,
    right_width: float,
    y_bottom: float = 0.0,
    y_top: float = CAP,
):
    """The two strokes of an A or a V, cut flat where the point would be.

    `overshoot` is how far past `y_top` the strokes would have met. It is the
    only control that matters here: at zero the letter comes to a true point,
    and every unit of overshoot widens the flat left behind. A small flat is
    the chipped spearhead the face is after; a large one just looks unfinished.
    """
    apex_y = y_top + overshoot
    span = apex_y - y_bottom
    ratio = overshoot / span if span else 0.0
    left_x = apex_x + (left_foot - apex_x) * ratio
    right_x = apex_x + (right_foot - apex_x) * ratio
    return [
        diagonal(left_x, y_top, left_foot, y_bottom, left_width, left_width + 26),
        diagonal(right_x, y_top, right_foot, y_bottom, right_width, right_width + 26),
    ]


def letter_a() -> Glyph:
    """The crossbar sits low -- around a quarter of the cap height rather than
    the usual half. It is the single loudest proportion in the alphabet:
    dropping it opens the counter into a broad triangle and makes ROMA and
    CARTHAGE read as monumental rather than as ordinary type. One bite out of
    the thin stroke, at the height of the bar.
    """
    width = 664
    apex, over = width / 2.0, 42.0
    left_foot, right_foot = 96.0, width - 96.0
    contours = apex_pair(apex, over, left_foot, right_foot, THIN, STEM)
    contours.append(foot_serif(left_foot, THIN + 26))
    contours.append(foot_serif(right_foot, STEM + 26))
    bar_bottom = CAP * 0.27
    contours.append(bar(140, width - 140, bar_bottom, bar_bottom + THIN))
    counter = [
        (apex - 58, CAP - 96),
        (apex + 58, CAP - 96),
        (apex + 150, bar_bottom + THIN),
        (apex - 150, bar_bottom + THIN),
    ]
    return Glyph(
        contours=contours,
        holes=[cw(counter), cut(206, CAP * 0.47, 70, facing="right")],
        advance=width + 2 * SIDEBEARING,
    )


def letter_b() -> Glyph:
    """The lower bowl is wider and deeper than the upper one. Drawing them equal
    is the classic tell of a B that was constructed rather than cut.
    """
    width = 588
    glyph = Glyph(contours=[stem(0, STEM)], advance=width + 2 * SIDEBEARING)
    stem_bowl(glyph, STEM, width - 40, CAP * 0.74, CAP * 0.26)
    stem_bowl(glyph, STEM, width, CAP * 0.26, CAP * 0.29)
    return glyph


def letter_c() -> Glyph:
    """Open the ring on the right, with an angled mouth so the terminals read as
    two blade tips rather than as a broken O.
    """
    width = 624
    glyph = Glyph(advance=width + 2 * SIDEBEARING)
    cx, cy = width / 2.0, CAP / 2.0
    rx, ry = width / 2.0, CAP / 2.0
    bowl = ring_parts(cx, cy, rx, ry, glyph)
    bowl.holes.append(blade_mouth(cx, cy, rx, ry, ry * 0.30))
    return glyph


def letter_d() -> Glyph:
    width = 624
    glyph = Glyph(contours=[stem(0, STEM)], advance=width + 2 * SIDEBEARING)
    stem_bowl(glyph, STEM, width, CAP / 2.0, CAP / 2.0)
    return glyph


def letter_e() -> Glyph:
    """The middle arm is deliberately short, and stops short of the others by
    more than a hair: it is the difference between an E that looks cut and an
    E that looks set.
    """
    width = 524
    contours = [stem(0, STEM)]
    contours.append(bar(0, width, CAP - THIN, CAP, flare_right=True))
    contours.append(bar(0, width, 0, THIN, flare_right=True))
    mid = CAP * 0.5
    contours.append(
        bar(0, width - 104, mid - THIN * 0.46, mid + THIN * 0.46, flare_right=True)
    )
    return Glyph(
        contours=contours,
        holes=[cut(STEM + 46, mid + THIN * 0.46, 54, facing="right")],
        advance=width + 2 * SIDEBEARING,
    )


def letter_f() -> Glyph:
    width = 500
    mid = CAP * 0.54
    return Glyph(
        contours=[
            stem(0, STEM),
            bar(0, width, CAP - THIN, CAP, flare_right=True),
            bar(0, width - 100, mid - THIN * 0.46, mid + THIN * 0.46, flare_right=True),
        ],
        advance=width + 2 * SIDEBEARING,
    )


def letter_g() -> Glyph:
    """A C with a bar, which is the only construction that reads as a G at
    caption size. Earlier versions opened the bowl at the top and hung a short
    spur off the right wall; that produced a C with a nub attached, because the
    thing the eye actually uses to tell G from C is the horizontal crossing the
    counter, not the presence of extra ink.
    """
    width = 648
    glyph = Glyph(advance=width + 2 * SIDEBEARING)
    cx, cy = width / 2.0, CAP / 2.0
    rx, ry = width / 2.0, CAP / 2.0
    bowl = ring_parts(cx, cy, rx, ry, glyph)
    # Open the upper right only. The lower arc has to survive all the way to
    # mid height, because that is where the bar meets the right wall.
    bowl.holes.append(
        cw(
            [
                (cx + rx * 0.26, cy + THIN * 0.58),
                (cx + rx * 2.2, cy - ry * 0.16),
                (cx + rx * 2.2, cy + ry * 2.5),
                (cx + rx * 0.26, cy + ry * 2.5),
            ]
        )
    )
    # The bar, projecting inward from the right wall, wedge-ended where it
    # stops in open counter.
    glyph.contours.append(
        bar(cx + rx * 0.14, cx + rx, cy - THIN * 0.5, cy + THIN * 0.5, flare_left=True)
    )
    return glyph


def letter_h() -> Glyph:
    width = 604
    return Glyph(
        contours=[
            stem(0, STEM),
            stem(width - STEM, width),
            bar(0, width, CAP * 0.5 - THIN / 2, CAP * 0.5 + THIN / 2),
        ],
        advance=width + 2 * SIDEBEARING,
    )


def letter_i() -> Glyph:
    """Full serifs top and bottom, always. An I that could be mistaken for a
    lowercase l is a bug in a face used for mission names and unit counts.
    """
    return Glyph(contours=[stem(0, STEM)], advance=STEM + 2 * (SIDEBEARING + EXT))


def letter_j() -> Glyph:
    """The hook has to be deep enough to be a hook. At the shallow depth this
    started with, the bowl's own stroke weight ate most of it and what showed
    was a stem with a curl -- indistinguishable from an I at caption size,
    which is the size that matters.
    """
    width = 486
    hook_cy = CAP * 0.30
    glyph = Glyph(advance=width + 2 * SIDEBEARING)
    stem_centre = width - STEM / 2.0
    bowl = ring_parts(stem_centre - 178, hook_cy, 178 + STEM / 2.0, hook_cy, glyph)
    bowl.holes.append(mask(-240, hook_cy, width + 120, CAP + 120))
    glyph.contours.append(
        stem(width - STEM, width, y_bottom=hook_cy, head=True, foot=False)
    )
    return glyph


def letter_k() -> Glyph:
    """The leg is the blade: heavier than the arm, running straight out."""
    width = 624
    junction = CAP * 0.47
    arm_top, leg_foot = width - 44.0, width - 54.0
    contours = [
        stem(0, STEM),
        diagonal(arm_top, CAP, STEM - 40, junction, THIN, THIN + 20),
        diagonal(STEM - 10, junction + 60, leg_foot, 0, STEM - 40, STEM + 16),
        head_serif(arm_top, THIN),
        foot_serif(leg_foot, STEM + 16),
    ]
    return Glyph(
        contours=contours,
        holes=[cut(width - 128, junction + 148, 56, facing="right")],
        advance=width + 2 * SIDEBEARING,
    )


def letter_l() -> Glyph:
    width = 500
    return Glyph(
        contours=[stem(0, STEM), bar(0, width, 0, THIN, flare_right=True)],
        advance=width + 2 * SIDEBEARING,
    )


def letter_m() -> Glyph:
    """Broad, and the central vee comes almost to the baseline. This is the glyph
    that decides whether ROME looks monumental, so it gets the width.
    """
    width = 748
    splay = 28.0
    left_head, right_head = THIN / 2.0, width - STEM / 2.0
    left_foot, right_foot = left_head - splay, right_head + splay
    contours = [
        diagonal(left_head, CAP, left_foot, 0, THIN, THIN),
        diagonal(right_head, CAP, right_foot, 0, STEM, STEM),
        diagonal(THIN, CAP, width / 2.0, 104, STEM - 26, STEM - 50),
        diagonal(width - THIN, CAP, width / 2.0, 104, THIN, STEM - 50),
        head_serif(left_head, THIN),
        head_serif(right_head, STEM),
        foot_serif(left_foot, THIN),
        foot_serif(right_foot, STEM),
    ]
    return Glyph(contours=contours, advance=width + 2 * SIDEBEARING)


def letter_n() -> Glyph:
    width = 624
    return Glyph(
        contours=[
            stem(0, THIN),
            stem(width - THIN, width),
            diagonal(THIN * 0.5 + 34, CAP, width - THIN * 0.5 - 34, 0, STEM, STEM),
        ],
        advance=width + 2 * SIDEBEARING,
    )


def letter_o() -> Glyph:
    """Nearly circular, a touch taller than it is wide. Hand-cut stone, not a
    geometric ellipse -- the stress comes from the counter, not the outline.
    """
    width = 664
    glyph = Glyph(advance=width + 2 * SIDEBEARING)
    ring_parts(width / 2.0, CAP / 2.0, width / 2.0, CAP / 2.0, glyph)
    return glyph


def letter_p() -> Glyph:
    width = 566
    glyph = Glyph(contours=[stem(0, STEM, head=True)], advance=width + 2 * SIDEBEARING)
    stem_bowl(glyph, STEM, width, CAP * 0.725, CAP * 0.275)
    return glyph


def letter_q() -> Glyph:
    """The tail. Long, straight and sharpened -- the one glyph allowed to break
    the baseline, and the letter most likely to be recognised on its own.
    """
    width = 664
    glyph = letter_o()
    cx = width / 2.0
    glyph.contours.append(
        diagonal(cx + 30, CAP * 0.30, width + 76, -150, STEM - 10, 72)
    )
    glyph.fixed_advance = width + 2 * SIDEBEARING * ROUND_SIDES
    glyph.advance = 0.0
    return glyph


def letter_r() -> Glyph:
    """The leg: a spear, not a curve. It leaves the bowl where the bowl closes
    and runs straight to the baseline, heavier than the bowl's own stroke.
    """
    width = 614
    glyph = Glyph(contours=[stem(0, STEM, head=True)], advance=width + 2 * SIDEBEARING)
    cy, ry = CAP * 0.735, CAP * 0.265
    stem_bowl(glyph, STEM, width - 90, cy, ry)
    glyph.contours.append(
        diagonal(STEM + 40, cy - ry + 30, width, 0, STEM - 20, STEM + 20)
    )
    glyph.holes.append(cut(width - 30, 120, 62, facing="left"))
    return glyph


def letter_s() -> Glyph:
    """Not geometric. The two bowls are deliberately unequal -- the lower one is
    wider and sits lower than a mirrored S would -- because a perfectly even S
    is the one thing that makes a classical face look manufactured. Each bowl
    loses a different quadrant, and each cut has to reach only its own bowl --
    run either against the finished letter and it takes the other bowl's spine
    with it. The upper bowl opens to the right and the lower to the left; the
    two cuts are angled so the terminals come out sharpened. The upper bowl
    loses its lower right, the lower bowl its upper left. Each cut is angled
    so the terminal it leaves comes out sharpened rather than sawn off square.
    """
    width = 548
    glyph = Glyph(advance=width + 2 * SIDEBEARING)
    upper_cy, upper_r = CAP * 0.73, CAP * 0.27
    lower_cy, lower_r = CAP * 0.285, CAP * 0.285
    upper = ring_parts(width * 0.50, upper_cy, width * 0.50, upper_r, glyph)
    lower = ring_parts(width * 0.50, lower_cy, width * 0.50, lower_r, glyph)
    upper.holes.append(
        cw(
            [
                (width * 0.46, upper_cy + upper_r * 0.16),
                (width * 1.8, upper_cy - upper_r * 0.30),
                (width * 1.8, upper_cy - upper_r * 3.0),
                (width * 0.46, upper_cy - upper_r * 3.0),
            ]
        )
    )
    lower.holes.append(
        cw(
            [
                (width * 0.54, lower_cy - lower_r * 0.16),
                (-width * 0.8, lower_cy + lower_r * 0.30),
                (-width * 0.8, lower_cy + lower_r * 3.0),
                (width * 0.54, lower_cy + lower_r * 3.0),
            ]
        )
    )
    return glyph


def letter_t() -> Glyph:
    """An unusually broad top bar. Titles are set in this letter constantly and
    the width is what makes them read as carved into a lintel.
    """
    width = 616
    return Glyph(
        contours=[
            stem(
                (width - STEM) / 2.0, (width + STEM) / 2.0, y_top=CAP - THIN, head=False
            ),
            bar(0, width, CAP - THIN, CAP, flare_left=True, flare_right=True),
        ],
        advance=width + 2 * SIDEBEARING,
    )


def letter_u() -> Glyph:
    width = 628
    bowl_cy = CAP * 0.305
    glyph = Glyph(advance=width + 2 * SIDEBEARING)
    bowl = ring_parts(width / 2.0, bowl_cy, width / 2.0, bowl_cy, glyph)
    bowl.holes.append(mask(-100, bowl_cy, width + 100, CAP + 100))
    glyph.contours.append(stem(0, STEM, y_bottom=bowl_cy - 10, foot=False))
    glyph.contours.append(stem(width - THIN, width, y_bottom=bowl_cy - 10, foot=False))
    return glyph


def letter_v() -> Glyph:
    width = 644
    left, right = THIN * 0.5 + 14, width - THIN * 0.5 - 14
    contours = [
        diagonal(left, CAP, width / 2.0, 0, STEM, STEM - 66),
        diagonal(right, CAP, width / 2.0, 0, THIN, THIN - 34),
        head_serif(left, STEM),
        head_serif(right, THIN),
    ]
    return Glyph(contours=contours, advance=width + 2 * SIDEBEARING)


def letter_w() -> Glyph:
    width = 936
    inner = width / 2.0
    outer_left, outer_right = THIN * 0.5, width - THIN * 0.5
    contours = [
        diagonal(outer_left, CAP, width * 0.235, 0, STEM, STEM - 66),
        diagonal(inner - 18, CAP, width * 0.235, 0, THIN, THIN - 34),
        diagonal(inner + 18, CAP, width * 0.765, 0, STEM, STEM - 66),
        diagonal(outer_right, CAP, width * 0.765, 0, THIN, THIN - 34),
        head_serif(outer_left, STEM),
        head_serif(outer_right, THIN),
        head_serif(inner - 18, THIN),
        head_serif(inner + 18, STEM),
    ]
    return Glyph(contours=contours, advance=width + 2 * SIDEBEARING)


def letter_x() -> Glyph:
    width = 628
    left, right = THIN * 0.5 + 14, width - THIN * 0.5 - 14
    contours = [
        diagonal(left, CAP, right, 0, STEM, STEM),
        diagonal(right, CAP, left, 0, THIN, THIN),
        head_serif(left, STEM),
        head_serif(right, THIN),
        foot_serif(right, STEM),
        foot_serif(left, THIN),
    ]
    return Glyph(contours=contours, advance=width + 2 * SIDEBEARING)


def letter_y() -> Glyph:
    width = 624
    junction = CAP * 0.43
    left, right = THIN * 0.5 + 14, width - THIN * 0.5 - 14
    contours = [
        diagonal(left, CAP, width / 2.0, junction, STEM, STEM - 44),
        diagonal(right, CAP, width / 2.0, junction, THIN, THIN - 24),
        stem(
            (width - STEM) / 2.0, (width + STEM) / 2.0, y_top=junction + 50, head=False
        ),
        head_serif(left, STEM),
        head_serif(right, THIN),
    ]
    return Glyph(contours=contours, advance=width + 2 * SIDEBEARING)


def letter_z() -> Glyph:
    width = 568
    spur = THIN * 0.82
    contours = [
        bar(0, width, CAP - THIN, CAP),
        bar(0, width, 0, THIN),
        diagonal(width - 100, CAP - THIN + 24, 104, THIN - 24, STEM + 24, STEM + 24),
        cw(
            [
                (0, CAP - THIN - EXT),
                (0, CAP - THIN),
                (spur, CAP - THIN),
                (spur, CAP - THIN - EXT),
            ]
        ),
        cw(
            [
                (width - spur, THIN),
                (width - spur, THIN + EXT),
                (width, THIN + EXT),
                (width, THIN),
            ]
        ),
    ]
    return Glyph(contours=contours, advance=width + 2 * SIDEBEARING)


LETTERS = {
    "A": letter_a,
    "B": letter_b,
    "C": letter_c,
    "D": letter_d,
    "E": letter_e,
    "F": letter_f,
    "G": letter_g,
    "H": letter_h,
    "I": letter_i,
    "J": letter_j,
    "K": letter_k,
    "L": letter_l,
    "M": letter_m,
    "N": letter_n,
    "O": letter_o,
    "P": letter_p,
    "Q": letter_q,
    "R": letter_r,
    "S": letter_s,
    "T": letter_t,
    "U": letter_u,
    "V": letter_v,
    "W": letter_w,
    "X": letter_x,
    "Y": letter_y,
    "Z": letter_z,
}
