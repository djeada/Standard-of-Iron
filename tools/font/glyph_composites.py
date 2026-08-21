"""Diacritical marks, and the accented capitals built from them.

The game ships German, Spanish and Brazilian Portuguese alongside English, and
a title face that drops out from under an accented capital is worse than one
that was never used: Qt falls back per glyph, so a single missing Ó turns
CAMPAÑA into two typefaces in one word.

The accented forms are composite glyphs -- a reference to the base capital and
a reference to the mark -- rather than redrawn outlines. Correcting O then
corrects Ó, Ò, Ô, Õ and Ö at the same time, which is the only way a face this
size stays consistent under edits.

Arabic is not covered here and is not meant to be. It falls to the bundled
text face; a Roman inscriptional display alphabet has nothing to say about it.
"""

from __future__ import annotations

from glyph_geometry import CAP, THIN, cw, ellipse
from glyph_shapes import BOWL_THIN, Glyph

# Marks sit above the cap line, so accented capitals are taller than plain
# ones. That is correct and intended -- the alternative is squashing the base
# letter, which makes MÁS shorter than MAS in the same line of a title.
MARK_Y = CAP + 40
MARK_WEIGHT = THIN * 0.92
MARK_REACH = 150.0


def acute() -> Glyph:
    return Glyph(
        contours=[
            cw(
                [
                    (0, MARK_Y),
                    (MARK_REACH * 0.55, MARK_Y + 170),
                    (MARK_REACH * 0.55 + MARK_WEIGHT, MARK_Y + 170),
                    (MARK_WEIGHT, MARK_Y),
                ]
            )
        ]
    )


def grave() -> Glyph:
    return Glyph(
        contours=[
            [(MARK_REACH * 0.55 + MARK_WEIGHT - x, y) for x, y in acute().contours[0]]
        ]
    )


def circumflex() -> Glyph:
    span = MARK_REACH * 1.5
    return Glyph(
        contours=[
            cw(
                [
                    (0, MARK_Y),
                    (span / 2.0, MARK_Y + 180),
                    (span, MARK_Y),
                    (span - MARK_WEIGHT * 0.9, MARK_Y),
                    (span / 2.0, MARK_Y + 180 - MARK_WEIGHT * 1.15),
                    (MARK_WEIGHT * 0.9, MARK_Y),
                ]
            )
        ]
    )


def tilde() -> Glyph:
    span, wave = MARK_REACH * 1.7, 78.0
    top = MARK_Y + 60
    return Glyph(
        contours=[
            cw(
                [
                    (0, top),
                    (span * 0.30, top + wave),
                    (span * 0.62, top + wave * 0.25),
                    (span, top + wave),
                    (span, top + wave - MARK_WEIGHT),
                    (span * 0.62, top + wave * 0.25 - MARK_WEIGHT),
                    (span * 0.30, top + wave - MARK_WEIGHT),
                    (0, top - MARK_WEIGHT),
                ]
            )
        ]
    )


def dieresis() -> Glyph:
    size = MARK_WEIGHT * 1.15
    gap = size * 1.9

    def square(x0: float):
        return cw(
            [
                (x0, MARK_Y),
                (x0, MARK_Y + size),
                (x0 + size, MARK_Y + size),
                (x0 + size, MARK_Y),
            ]
        )

    return Glyph(contours=[square(0), square(gap)])


def ring() -> Glyph:
    r = MARK_WEIGHT * 1.35
    return Glyph(
        paths=[ellipse(r, MARK_Y + r + 20, r, r)],
        hole_paths=[
            ellipse(r, MARK_Y + r + 20, r - BOWL_THIN * 0.62, r - BOWL_THIN * 0.62)
        ],
    )


def cedilla() -> Glyph:
    """Hangs below the baseline off the C. Drawn as a chiselled hook -- a stub
    down and a foot to the left -- rather than a comma borrowed from a text
    face, so it belongs to the same cut language as the rest of the alphabet.
    The stub deliberately starts *above* the baseline. A mark that begins at
    y=0 floats free: the C's bowl has already curved away from the baseline at
    the x the mark is centred on, so the two never meet.
    """
    stub_w = MARK_WEIGHT * 1.15
    return Glyph(
        contours=[
            cw([(0, 60), (0, -90), (stub_w, -90), (stub_w, 60)]),
            cw(
                [
                    (-stub_w * 0.9, -90),
                    (-stub_w * 0.9, -178),
                    (stub_w, -178),
                    (stub_w, -90),
                ]
            ),
        ]
    )


MARKS = {
    "acute": acute,
    "grave": grave,
    "circumflex": circumflex,
    "tilde": tilde,
    "dieresis": dieresis,
    "ring": ring,
    "cedilla": cedilla,
}

# character -> (base letter, mark, dx, dy)
#
# dx is resolved against the base glyph's own width at build time; the number
# here is a fraction of that width, so a mark stays centred if a letter is
# later redrawn wider. dy is absolute, because marks all sit on one line.
ACCENTED = {
    "Á": ("A", "acute", 0.50, 0.0),
    "À": ("A", "grave", 0.50, 0.0),
    "Â": ("A", "circumflex", 0.50, 0.0),
    "Ã": ("A", "tilde", 0.50, 0.0),
    "Ä": ("A", "dieresis", 0.50, 0.0),
    "Å": ("A", "ring", 0.50, 0.0),
    "Ç": ("C", "cedilla", 0.46, 0.0),
    "É": ("E", "acute", 0.46, 0.0),
    "È": ("E", "grave", 0.46, 0.0),
    "Ê": ("E", "circumflex", 0.46, 0.0),
    "Ë": ("E", "dieresis", 0.46, 0.0),
    "Í": ("I", "acute", 0.50, 0.0),
    "Ì": ("I", "grave", 0.50, 0.0),
    "Î": ("I", "circumflex", 0.50, 0.0),
    "Ï": ("I", "dieresis", 0.50, 0.0),
    "Ñ": ("N", "tilde", 0.50, 0.0),
    "Ó": ("O", "acute", 0.50, 0.0),
    "Ò": ("O", "grave", 0.50, 0.0),
    "Ô": ("O", "circumflex", 0.50, 0.0),
    "Õ": ("O", "tilde", 0.50, 0.0),
    "Ö": ("O", "dieresis", 0.50, 0.0),
    "Ú": ("U", "acute", 0.50, 0.0),
    "Ù": ("U", "grave", 0.50, 0.0),
    "Û": ("U", "circumflex", 0.50, 0.0),
    "Ü": ("U", "dieresis", 0.50, 0.0),
    "Ý": ("Y", "acute", 0.50, 0.0),
}
