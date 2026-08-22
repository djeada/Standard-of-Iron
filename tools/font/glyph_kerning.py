"""Kerning pairs.

The face is set in short all-caps lines, which is the worst case for optical
spacing: the pairs that open up -- a diagonal beside a straight stem, a round
beside a diagonal -- appear constantly in words like SURVIVE, CAVALRY and
VICTORY, and there is no lowercase to hide them among. Uniform sidebearings
cannot fix this, because the gap comes from the shape of the letter above and
below the baseline rather than from its width.

Values are in font units against a 1000-unit em and are all negative: kerning
here only ever closes a gap, never opens one. They are deliberately modest,
because the reel presets add tracking on top and an aggressive pair that looks
right at default spacing collides once tracking is applied.

Grouped by cause rather than alphabetically, so that adding a letter later
means finding the group it behaves like instead of guessing a number.
"""

from __future__ import annotations

DIAGONAL_BESIDE_UPRIGHT = {
    ("A", "V"): -52,
    ("A", "W"): -46,
    ("A", "Y"): -56,
    ("A", "T"): -58,
    ("V", "A"): -52,
    ("W", "A"): -46,
    ("Y", "A"): -56,
    ("T", "A"): -58,
    ("L", "V"): -60,
    ("L", "W"): -54,
    ("L", "Y"): -64,
    ("L", "T"): -62,
    ("F", "A"): -50,
    ("P", "A"): -48,
    ("K", "V"): -34,
    ("K", "Y"): -38,
    ("R", "V"): -30,
    ("R", "W"): -26,
    ("R", "Y"): -34,
    ("R", "T"): -28,
    ("B", "V"): -20,
    ("D", "V"): -22,
    ("D", "Y"): -24,
}


ROUND_BESIDE_DIAGONAL = {
    ("T", "O"): -30,
    ("T", "C"): -30,
    ("T", "G"): -30,
    ("T", "Q"): -30,
    ("O", "V"): -24,
    ("O", "W"): -20,
    ("O", "Y"): -26,
    ("O", "A"): -26,
    ("V", "O"): -24,
    ("W", "O"): -20,
    ("Y", "O"): -26,
    ("L", "O"): -26,
    ("L", "C"): -24,
    ("L", "G"): -24,
    ("P", "O"): -14,
    ("R", "O"): -14,
    ("D", "O"): -12,
}


DIAGONAL_OVER_STEM = {
    ("T", "S"): -22,
    ("T", "U"): -20,
    ("T", "Y"): -18,
    ("T", "W"): -18,
    ("V", "S"): -18,
    ("Y", "S"): -20,
    ("W", "S"): -16,
    ("T", "R"): -16,
    ("T", "N"): -14,
    ("T", "M"): -14,
}


PUNCTUATION_PAIRS = {
    ("A", "period"): -70,
    ("A", "comma"): -70,
    ("V", "period"): -80,
    ("V", "comma"): -80,
    ("W", "period"): -70,
    ("W", "comma"): -70,
    ("Y", "period"): -86,
    ("Y", "comma"): -86,
    ("T", "period"): -84,
    ("T", "comma"): -84,
    ("L", "quotedbl"): -60,
    ("L", "quotesingle"): -60,
    ("period", "quotedbl"): -50,
}


FIGURE_PAIRS = {
    ("one", "one"): -30,
    ("seven", "one"): -34,
    ("seven", "four"): -40,
    ("seven", "zero"): -22,
    ("two", "four"): -22,
    ("four", "one"): -26,
    ("one", "four"): -24,
    ("nine", "four"): -20,
    ("three", "four"): -18,
}


def pairs() -> dict[tuple[str, str], int]:
    combined: dict[tuple[str, str], int] = {}
    for group in (
        DIAGONAL_BESIDE_UPRIGHT,
        ROUND_BESIDE_DIAGONAL,
        DIAGONAL_OVER_STEM,
        PUNCTUATION_PAIRS,
        FIGURE_PAIRS,
    ):
        combined.update(group)
    return combined
