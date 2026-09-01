"""Diacritical marks, and the accented capitals built from them.

The game ships German, Spanish, Brazilian Portuguese and Turkish alongside
English, and a title face that drops out from under an accented capital is
worse than one that was never used: Qt falls back per glyph, so a single
missing Ó turns CAMPAÑA into two typefaces in one word.

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

MARK_Y = CAP + 40
MARK_WEIGHT = THIN * 0.84
MARK_REACH = 138.0


def acute() -> Glyph:
    return Glyph(
        contours=[
            cw(
                [
                    (0, MARK_Y),
                    (MARK_REACH * 0.55, MARK_Y + 152),
                    (MARK_REACH * 0.55 + MARK_WEIGHT, MARK_Y + 152),
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
                    (span / 2.0, MARK_Y + 158),
                    (span, MARK_Y),
                    (span - MARK_WEIGHT * 0.9, MARK_Y),
                    (span / 2.0, MARK_Y + 158 - MARK_WEIGHT * 1.15),
                    (MARK_WEIGHT * 0.9, MARK_Y),
                ]
            )
        ]
    )


def tilde() -> Glyph:
    span, wave = MARK_REACH * 1.7, 62.0
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
    size = MARK_WEIGHT
    gap = size * 1.7

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


def breve() -> Glyph:
    """The cup over Turkish G. Cut as a chiselled arc rather than a curve so it
    reads at title size next to the circumflex, whose angle it answers."""
    span, depth, weight = MARK_REACH * 1.62, 108.0, MARK_WEIGHT * 0.62
    top = MARK_Y + 104
    return Glyph(
        contours=[
            cw(
                [
                    (0, top),
                    (0, top - depth * 0.55),
                    (span * 0.32, top - depth),
                    (span * 0.68, top - depth),
                    (span, top - depth * 0.55),
                    (span, top),
                    (span - weight, top),
                    (span - weight, top - depth * 0.48),
                    (span * 0.66, top - depth + weight),
                    (span * 0.34, top - depth + weight),
                    (weight, top - depth * 0.48),
                    (weight, top),
                ]
            )
        ]
    )


def dotaccent() -> Glyph:
    """One half of the dieresis, for Turkish dotted capital I."""
    size = MARK_WEIGHT
    return Glyph(
        contours=[
            cw(
                [
                    (0, MARK_Y),
                    (0, MARK_Y + size),
                    (size, MARK_Y + size),
                    (size, MARK_Y),
                ]
            )
        ]
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
    "breve": breve,
    "dotaccent": dotaccent,
}


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
    "Ğ": ("G", "breve", 0.50, 0.0),
    "İ": ("I", "dotaccent", 0.50, 0.0),
    "Ş": ("S", "cedilla", 0.46, 0.0),
}
