"""The Cyrillic capitals, drawn out of the same primitives as the Latin ones.

The game ships Russian, and `Typography.titleFamily` is bound to every heading
and outcome screen in it. Without these glyphs Qt falls back per glyph, so a
Russian title is set in whatever the host machine happens to offer -- which is
the one thing `assets/fonts/README.md` says this face exists to prevent.

Eleven capitals are shared outright with the Latin alphabet and are not redrawn
here: `ALIASES` points their code points at the Latin outline, so correcting O
corrects О as well and the two can never drift apart. What is left is drawn
below out of `stem`, `bar`, `diagonal` and `ring`, exactly as `glyph_shapes`
draws A-Z: the same wedge, the same chisel bite, the same thick/thin logic
(the heavy stroke goes on the diagonal, the light one on the verticals it
crosses), so ЗАМА and ZAMA read as one alphabet.

Two forms have no Latin ancestor and are worth naming:

    the descender feet of Д, Ц and Щ
        A short blunt leg below the baseline. Kept square rather than serifed:
        a wedge down there reads as a mistake at caption size, and the letters
        already carry their weight above the line.
    the mid-height junction of Ч, Ъ, Ь and Ы
        Every one of these hangs its bowl or arm off a stem at the same
        height. They are drawn against one constant, `WAIST`, so a change to
        one is a change to all four rather than four separate decisions.

The three constants below carry those two forms: `WAIST` is where a bowl or an
arm meets a stem, and `LEG_DROP`/`LEG_WIDTH` are how far the descender foot
hangs and how wide it is. One number each, so the letters sharing a form cannot
drift apart. `ALIASES`, at the foot of the module, is the shared-capital table.
"""

from __future__ import annotations

from glyph_geometry import (
    CAP,
    CURVE_OVERSHOOT,
    SIDEBEARING,
    STEM,
    THIN,
    bar,
    cut,
    cw,
    diagonal,
    foot_serif,
    head_serif,
    stem,
)
from glyph_shapes import Glyph, blade_mouth, mask, ring_parts, stem_bowl

WAIST = CAP * 0.44

LEG_DROP = 118.0
LEG_WIDTH = 104.0


def _leg(x_left: float) -> list:
    """A descender foot. Square: see the module docstring."""
    return cw(
        [
            (x_left, 0.0),
            (x_left, -LEG_DROP),
            (x_left + LEG_WIDTH, -LEG_DROP),
            (x_left + LEG_WIDTH, 0.0),
        ]
    )


def letter_be() -> Glyph:
    """Б is Г carrying P's bowl at the waist. The bowl is the lower one from B
    rather than P's upper one, so B and Б share a shape instead of merely a
    silhouette.
    """
    width = 566
    glyph = Glyph(contours=[stem(0, STEM)], advance=width + 2 * SIDEBEARING)
    glyph.contours.append(bar(0, width - 30, CAP - THIN, CAP, flare_right=True))
    stem_bowl(glyph, STEM, width, CAP * 0.25, CAP * 0.29)
    return glyph


def letter_ghe() -> Glyph:
    """An L turned over. The arm is a touch shorter than L's foot: an arm at
    the cap line carries further than one at the baseline, and matching them
    makes Г look overhung.
    """
    width = 486
    return Glyph(
        contours=[stem(0, STEM), bar(0, width, CAP - THIN, CAP, flare_right=True)],
        advance=width + 2 * SIDEBEARING,
    )


def letter_de() -> Glyph:
    """The left stroke splays and the right one stands. Both sit on a shelf
    that overhangs at each end, and the overhang drops two legs. Drawing the
    left stroke vertical -- which some faces do -- turns Д into a П on a
    plinth and loses the letter.
    """
    width = 700
    shelf_top = THIN
    right_stem_left = width - 190
    contours = [
        bar(0, width, 0, shelf_top),
        stem(
            right_stem_left,
            right_stem_left + STEM,
            y_bottom=shelf_top,
            foot=False,
            head=False,
        ),
        diagonal(258, CAP, 128, shelf_top, THIN + 16, THIN + 40),
        bar(212, right_stem_left + STEM, CAP - THIN, CAP),
        _leg(0),
        _leg(width - LEG_WIDTH),
    ]
    return Glyph(contours=contours, advance=width + 2 * SIDEBEARING)


def letter_zhe() -> Glyph:
    """K written twice against one stem. The arms stay thin and the legs stay
    heavy on both sides, so the letter keeps a direction of stroke instead of
    turning into a symmetrical asterisk.
    """
    width = 900
    centre = width / 2.0
    junction = CAP * 0.47
    arm_reach = 62.0
    contours = [
        stem(centre - STEM / 2.0, centre + STEM / 2.0),
        diagonal(
            width - arm_reach, CAP, centre + STEM / 2.0 - 34, junction, THIN, THIN + 20
        ),
        diagonal(arm_reach, CAP, centre - STEM / 2.0 + 34, junction, THIN, THIN + 20),
        diagonal(
            centre + STEM / 2.0 - 6, junction + 60, width - 54, 0, STEM - 46, STEM + 10
        ),
        diagonal(centre - STEM / 2.0 + 6, junction + 60, 54, 0, STEM - 46, STEM + 10),
        head_serif(width - arm_reach, THIN),
        head_serif(arm_reach, THIN),
        foot_serif(width - 54, STEM + 10),
        foot_serif(54, STEM + 10),
    ]
    return Glyph(contours=contours, advance=width + 2 * SIDEBEARING)


def letter_ze() -> Glyph:
    """Two bowls opening left, joined where they meet on the right -- a 3, not
    a mirrored S. Each mouth is cut on the slant so the four terminals come out
    sharpened, which is the only thing keeping З from reading as a numeral.
    """
    width = 556
    glyph = Glyph(advance=width + 2 * SIDEBEARING)
    upper_cy = CAP * 0.735 + CURVE_OVERSHOOT / 2.0
    upper_r = CAP * 0.265 + CURVE_OVERSHOOT / 2.0
    lower_cy = CAP * 0.285 - CURVE_OVERSHOOT / 2.0
    lower_r = CAP * 0.285 + CURVE_OVERSHOOT / 2.0
    cx = width * 0.52
    rx = width * 0.48

    upper = ring_parts(cx, upper_cy, rx, upper_r, glyph)
    lower = ring_parts(cx, lower_cy, rx, lower_r, glyph)
    upper.holes.append(
        cw(
            [
                (cx - rx * 0.10, upper_cy - upper_r * 1.4),
                (cx - rx * 0.62, upper_cy + upper_r * 1.4),
                (cx - rx * 3.0, upper_cy + upper_r * 1.4),
                (cx - rx * 3.0, upper_cy - upper_r * 1.4),
            ]
        )
    )
    lower.holes.append(
        cw(
            [
                (cx - rx * 0.10, lower_cy + lower_r * 1.4),
                (cx - rx * 0.62, lower_cy - lower_r * 1.4),
                (cx - rx * 3.0, lower_cy - lower_r * 1.4),
                (cx - rx * 3.0, lower_cy + lower_r * 1.4),
            ]
        )
    )
    return glyph


def letter_i() -> Glyph:
    """N with the diagonal reversed, and nothing else changed. They are the
    same letter cut the other way and any other difference between them is a
    bug.
    """
    width = 624
    return Glyph(
        contours=[
            stem(0, THIN),
            stem(width - THIN, width),
            diagonal(width - THIN * 0.5 - 34, CAP, THIN * 0.5 + 34, 0, STEM, STEM),
        ],
        advance=width + 2 * SIDEBEARING,
    )


def letter_el() -> Glyph:
    """A leg that splays and a stem that does not. The splay is what separates
    Л from П at a glance, so it is generous rather than polite.
    """
    width = 640
    contours = [
        stem(width - STEM, width),
        bar(150, width, CAP - THIN, CAP, flare_left=True),
        diagonal(238, CAP - THIN + 12, 74, 0, THIN + 16, THIN + 44),
        foot_serif(74, THIN + 44),
    ]
    return Glyph(contours=contours, advance=width + 2 * SIDEBEARING)


def letter_pe() -> Glyph:
    """H with the bar lifted to the cap line."""
    width = 624
    return Glyph(
        contours=[
            stem(0, STEM),
            stem(width - STEM, width),
            bar(0, width, CAP - THIN, CAP),
        ],
        advance=width + 2 * SIDEBEARING,
    )


def letter_u() -> Glyph:
    """The right stroke runs the whole way through and out below the line; the
    left one stops on it. Drawn as a Y with a tail, the join sits too low and
    the descender loses its slant.
    """
    width = 636
    junction = CAP * 0.34
    left = THIN * 0.5 + 18
    right = width - THIN * 0.5 - 22
    contours = [
        diagonal(right, CAP, width * 0.24, -132, THIN, 78),
        diagonal(left, CAP, width * 0.55, junction, STEM, STEM - 52),
        head_serif(left, STEM),
        head_serif(right, THIN),
    ]
    return Glyph(contours=contours, advance=width + 2 * SIDEBEARING)


def letter_ef() -> Glyph:
    """A stem through a bowl. The bowl is shallower than O so the stem shows
    above and below it -- an F whose stem barely clears the bowl reads as a
    stray O with a scratch through it.
    """
    width = 780
    cx = width / 2.0
    glyph = Glyph(
        contours=[stem(cx - STEM / 2.0, cx + STEM / 2.0)],
        advance=width + 2 * SIDEBEARING,
    )
    ring_parts(cx, CAP * 0.52, width / 2.0, CAP * 0.30 + CURVE_OVERSHOOT, glyph)
    return glyph


def letter_tse() -> Glyph:
    """П stood on its head, with the leg hung under the right stem."""
    width = 624
    return Glyph(
        contours=[
            stem(0, STEM, y_bottom=THIN, foot=False),
            stem(width - STEM, width, y_bottom=THIN, foot=False),
            bar(0, width, 0, THIN),
            _leg(width - LEG_WIDTH - 26),
        ],
        advance=width + 2 * SIDEBEARING,
    )


def letter_che() -> Glyph:
    """The left arm stops at the waist, and the bowl of the letter is the open
    corner it leaves. One bite marks the inside of the junction, the way E is
    marked.
    """
    width = 590
    return Glyph(
        contours=[
            stem(width - STEM, width),
            stem(0, THIN, y_bottom=WAIST, foot=False),
            bar(0, width, WAIST, WAIST + THIN),
        ],
        holes=[cut(THIN + 44, WAIST + THIN, 40, facing="right")],
        advance=width + 2 * SIDEBEARING,
    )


def _sha_contours(width: float) -> list:
    inner = width / 2.0
    return [
        stem(0, THIN, y_bottom=THIN, foot=False),
        stem(inner - THIN / 2.0, inner + THIN / 2.0, y_bottom=THIN, foot=False),
        stem(width - STEM, width, y_bottom=THIN, foot=False),
        bar(0, width, 0, THIN),
    ]


def letter_sha() -> Glyph:
    """Three uprights on a shelf. The last one carries the weight, as it does
    in И and П, so a line of Cyrillic keeps its rhythm.
    """
    width = 880
    return Glyph(contours=_sha_contours(width), advance=width + 2 * SIDEBEARING)


def letter_shcha() -> Glyph:
    """Ш with Ц's leg."""
    width = 880
    contours = _sha_contours(width)
    contours.append(_leg(width - LEG_WIDTH - 26))
    return Glyph(contours=contours, advance=width + 2 * SIDEBEARING)


def letter_soft() -> Glyph:
    """A stem with B's lower bowl and nothing above it."""
    width = 540
    glyph = Glyph(contours=[stem(0, STEM)], advance=width + 2 * SIDEBEARING)
    stem_bowl(glyph, STEM, width, CAP * 0.22, CAP * 0.26)
    return glyph


def letter_hard() -> Glyph:
    """Ь with an arm reaching back over the shoulder. The arm is short: it
    exists to tell Ъ from Ь, not to balance the letter.
    """
    width = 700
    stem_left = 168.0
    glyph = Glyph(
        contours=[
            stem(stem_left, stem_left + STEM),
            bar(0, stem_left + STEM, CAP - THIN, CAP, flare_left=True),
        ],
        advance=width + 2 * SIDEBEARING,
    )
    stem_bowl(glyph, stem_left + STEM, width, CAP * 0.22, CAP * 0.26)
    return glyph


def letter_yery() -> Glyph:
    """Ь and I, set as one letter. The gap between them is the letter: close it
    and Ы reads as Ь followed by a stray stem.
    """
    width = 830
    glyph = Glyph(contours=[stem(0, STEM)], advance=width + 2 * SIDEBEARING)
    stem_bowl(glyph, STEM, 540, CAP * 0.22, CAP * 0.26)
    glyph.contours.append(stem(width - STEM, width))
    return glyph


def _mirrored(contour: list, axis: float) -> list:
    return [(2.0 * axis - x, y) for x, y in contour]


def letter_e() -> Glyph:
    """G read backwards: the same bowl, the same mouth, the same bar crossing
    the counter. The bar is what tells Э from a reversed C, exactly as it is
    what tells G from C.
    """
    width = 624
    glyph = Glyph(advance=width + 2 * SIDEBEARING)
    cx, cy = width / 2.0, CAP / 2.0
    rx, ry = width / 2.0, CAP / 2.0 + CURVE_OVERSHOOT
    bowl = ring_parts(cx, cy, rx, ry, glyph)
    bowl.holes.append(cw(_mirrored(blade_mouth(cx, cy, rx, ry, ry * 0.30), cx)))
    glyph.contours.append(
        bar(cx - rx * 0.34, cx + rx, cy - THIN * 0.5, cy + THIN * 0.5, flare_left=True)
    )
    return glyph


def letter_yu() -> Glyph:
    """A stem, a bowl, and the bar that ties them. The bar sits at the middle
    of the bowl rather than the middle of the letter, which is the same rule H
    follows.
    """
    width = 940
    glyph = Glyph(contours=[stem(0, STEM)], advance=width + 2 * SIDEBEARING)
    bowl_left = STEM + 96.0
    rx = (width - bowl_left) / 2.0
    cx = bowl_left + rx
    ring_parts(cx, CAP / 2.0, rx, CAP / 2.0 + CURVE_OVERSHOOT, glyph)
    glyph.contours.append(
        bar(STEM - 8, bowl_left + 30, CAP / 2.0 - THIN / 2.0, CAP / 2.0 + THIN / 2.0)
    )
    return glyph


def letter_ya() -> Glyph:
    """R read backwards. The bowl hangs off the left face of the stem and the
    leg runs out to the baseline on the other side, which is why the stem
    keeps its head flare and loses nothing else.
    """
    width = 614
    glyph = Glyph(
        contours=[stem(width - STEM, width, head=True)],
        advance=width + 2 * SIDEBEARING,
    )
    bowl_bottom = CAP * 0.47
    bowl_top = CAP + CURVE_OVERSHOOT
    cy, ry = (bowl_bottom + bowl_top) / 2.0, (bowl_top - bowl_bottom) / 2.0
    stem_left = width - STEM
    part = ring_parts(stem_left, cy, stem_left - 90.0, ry, glyph)
    part.holes.append(mask(stem_left, cy - ry - 60, width + 400, cy + ry + 60))
    glyph.contours.append(
        diagonal(stem_left - 40, cy - ry + 30, 0, 0, STEM - 20, STEM + 20)
    )
    glyph.holes.append(cut(30, 120, 44, facing="right"))
    return glyph


CYRILLIC = {
    "Б": letter_be,
    "Г": letter_ghe,
    "Д": letter_de,
    "Ж": letter_zhe,
    "З": letter_ze,
    "И": letter_i,
    "Л": letter_el,
    "П": letter_pe,
    "У": letter_u,
    "Ф": letter_ef,
    "Ц": letter_tse,
    "Ч": letter_che,
    "Ш": letter_sha,
    "Щ": letter_shcha,
    "Ъ": letter_hard,
    "Ы": letter_yery,
    "Ь": letter_soft,
    "Э": letter_e,
    "Ю": letter_yu,
    "Я": letter_ya,
}


ALIASES = {
    "А": "A",
    "В": "B",
    "Е": "E",
    "К": "K",
    "М": "M",
    "Н": "H",
    "О": "O",
    "Р": "P",
    "С": "C",
    "Т": "T",
    "Х": "X",
}


CYRILLIC_ROUND = set("ЗЭЮФ")
CYRILLIC_DIAGONAL = set("ЛУЖ")
