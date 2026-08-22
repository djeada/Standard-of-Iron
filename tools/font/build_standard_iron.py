#!/usr/bin/env python3
"""Compile the Standard Iron display face from its source geometry.

    python3 tools/font/build_standard_iron.py

The .ttf under assets/fonts/ is a build artefact of this script and the four
modules beside it. It is committed because the game loads it at runtime and a
contributor should not need a font toolchain to run the game -- but it is
generated, so letters get fixed here and rebuilt, never by opening the binary
in a font editor. The build is reproducible: the same source produces the same
bytes, which is only true because the head timestamps are pinned below.

Requires fonttools.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import newTable
from fontTools.ttLib.tables import _k_e_r_n
from glyph_composites import ACCENTED, MARKS
from glyph_geometry import ASCENDER, CAP, DESCENDER, SIDEBEARING, UPM
from glyph_kerning import pairs as kerning_pairs
from glyph_punctuation import PUNCTUATION
from glyph_shapes import DIAGONAL_SIDES, LETTERS, ROUND_SIDES
from pathops import Path as SkiaPath
from pathops import PathOp, op

FAMILY = "Standard Iron Display"
STYLE = "Bold"
VERSION = "1.000"

OUTPUT = (
    Path(__file__).resolve().parents[2]
    / "assets"
    / "fonts"
    / "StandardIronDisplay-Bold.ttf"
)


CURVE_TOLERANCE = 0.6


BUILD_EPOCH = 3850070400


ROUND = set("OQCGDSU0368")
DIAGONAL = set("AVWXYTJ7")


GLYPH_NAMES = {
    ".": "period",
    "\u2026": "ellipsis",
    ",": "comma",
    ":": "colon",
    ";": "semicolon",
    "!": "exclam",
    "?": "question",
    "¡": "exclamdown",
    "¿": "questiondown",
    "-": "hyphen",
    "–": "endash",
    "—": "emdash",
    "+": "plus",
    "×": "multiply",
    "/": "slash",
    "%": "percent",
    "&": "ampersand",
    "#": "numbersign",
    "'": "quotesingle",
    '"': "quotedbl",
    "(": "parenleft",
    ")": "parenright",
    "[": "bracketleft",
    "]": "bracketright",
    "0": "zero",
    "1": "one",
    "2": "two",
    "3": "three",
    "4": "four",
    "5": "five",
    "6": "six",
    "7": "seven",
    "8": "eight",
    "9": "nine",
}


def glyph_name_for(name: str) -> str:
    if name in GLYPH_NAMES:
        return GLYPH_NAMES[name]
    if len(name) == 1 and name.isascii() and name.isalpha():
        return name
    if len(name) == 1:
        return f"uni{ord(name):04X}"
    return name


def _replay(contours, paths, pen) -> None:
    for contour in contours:
        pen.moveTo(contour[0])
        for point in contour[1:]:
            pen.lineTo(point)
        pen.closePath()
    for path in paths:
        for kind, args in path:
            if kind == "move":
                pen.moveTo(args)
            elif kind == "curve":
                pen.curveTo(*args)
            elif kind == "close":
                pen.closePath()


def record(glyph) -> RecordingPen:
    """Resolve a Glyph into a single set of non-overlapping contours.

    The strokes are unioned and the counters and chisel bites subtracted with
    real boolean geometry. Doing it here rather than in the drawing means a
    letter can be written the way it is built -- a stem, an arm, a bowl, a bite
    -- without anyone having to reason about where the pieces overlap.

    The union pass matters even for glyphs with nothing to subtract: TrueType
    renders overlapping contours correctly under the nonzero rule, but they
    confuse hinting and every downstream tool that measures the outline, and a
    stem crossing a bar is the normal case here rather than the exception.
    """

    def resolve(contours, paths, holes, hole_paths) -> SkiaPath:
        shape = SkiaPath()
        _replay(contours, paths, shape.getPen())
        shape.simplify(fix_winding=True, keep_starting_points=False)
        if holes or hole_paths:
            cut_away = SkiaPath()
            _replay(holes, hole_paths, cut_away.getPen())
            cut_away.simplify(fix_winding=True, keep_starting_points=False)
            shape = op(
                shape,
                cut_away,
                PathOp.DIFFERENCE,
                fix_winding=True,
                keep_starting_points=False,
            )
        return shape

    added = resolve(glyph.contours, glyph.paths, [], [])

    for part in glyph.parts:
        resolved = resolve(part.contours, part.paths, part.holes, part.hole_paths)
        added = op(
            added, resolved, PathOp.UNION, fix_winding=True, keep_starting_points=False
        )

    if glyph.holes or glyph.hole_paths:
        removed = SkiaPath()
        _replay(glyph.holes, glyph.hole_paths, removed.getPen())
        removed.simplify(fix_winding=True, keep_starting_points=False)
        added = op(
            added,
            removed,
            PathOp.DIFFERENCE,
            fix_winding=True,
            keep_starting_points=False,
        )

    pen = RecordingPen()
    added.draw(pen)
    return pen


def ink_bounds(pen: RecordingPen):
    bounds = BoundsPen(None)
    pen.replay(bounds)
    return bounds.bounds


def sidebearing_for(character: str) -> float:
    if character in ROUND:
        return SIDEBEARING * ROUND_SIDES
    if character in DIAGONAL:
        return SIDEBEARING * DIAGONAL_SIDES
    return SIDEBEARING


def build_kerning(builder, available: set) -> None:
    """Attach the legacy `kern` table.

    Legacy rather than GPOS because this font has no other OpenType layout and
    the table is far simpler to reason about; HarfBuzz, which is what Qt shapes
    with, reads `kern` when there is no GPOS kerning to prefer.

    Pairs naming a glyph the font does not have are dropped rather than
    ignored silently by the writer, and the count is reported, so that renaming
    a glyph cannot quietly empty the table.
    """
    table_pairs = {
        pair: value
        for pair, value in kerning_pairs().items()
        if pair[0] in available and pair[1] in available
    }
    dropped = len(kerning_pairs()) - len(table_pairs)
    if dropped:
        print(f"warning: dropped {dropped} kern pair(s) naming unknown glyphs")

    subtable = _k_e_r_n.KernTable_format_0()
    subtable.coverage = 1
    subtable.version = 0
    subtable.format = 0
    subtable.kernTable = table_pairs

    table = newTable("kern")
    table.version = 0
    table.kernTables = [subtable]
    builder.font["kern"] = table


def build() -> Path:
    sources: dict = {}
    sources.update(LETTERS)
    sources.update(PUNCTUATION)

    glyph_order = [".notdef", "space"]
    outlines: dict = {}
    advances: dict = {}
    cmap: dict = {}
    placement: dict = {}

    outlines[".notdef"] = TTGlyphPen(None).glyph()
    advances[".notdef"] = int(UPM * 0.5)
    outlines["space"] = TTGlyphPen(None).glyph()
    advances["space"] = int(UPM * 0.30)
    cmap[0x20] = "space"

    for character, factory in sources.items():
        glyph = factory()
        name = glyph_name_for(character)
        pen = record(glyph)
        bounds = ink_bounds(pen)
        if bounds is None:
            continue
        x_min, _, x_max, _ = bounds
        side = sidebearing_for(character)
        shift = side - x_min
        ink_width = x_max - x_min

        out = TTGlyphPen(None)
        pen.replay(TransformPen(Cu2QuPen(out, CURVE_TOLERANCE), (1, 0, 0, 1, shift, 0)))
        outlines[name] = out.glyph()
        advances[name] = int(
            round(glyph.fixed_advance if glyph.fixed_advance else ink_width + 2 * side)
        )
        placement[name] = (side, ink_width)
        cmap[ord(character)] = name
        glyph_order.append(name)

    for mark_name, factory in MARKS.items():
        pen = record(factory())
        bounds = ink_bounds(pen)
        out = TTGlyphPen(None)
        pen.replay(Cu2QuPen(out, CURVE_TOLERANCE))
        outlines[mark_name] = out.glyph()
        advances[mark_name] = 0
        placement[mark_name] = (bounds[0], bounds[2] - bounds[0])
        glyph_order.append(mark_name)

    composites: dict = {}
    for character, (base, mark, fraction, dy) in ACCENTED.items():
        name = f"uni{ord(character):04X}"
        base_name = glyph_name_for(base)
        base_left, base_width = placement[base_name]
        mark_left, mark_width = placement[mark]

        dx = base_left + (base_width * fraction) - (mark_width / 2.0) - mark_left
        composites[name] = (base_name, mark, dx, dy)
        advances[name] = advances[base_name]
        cmap[ord(character)] = name
        glyph_order.append(name)

    builder = FontBuilder(UPM, isTTF=True)
    builder.setupGlyphOrder(glyph_order)
    builder.setupCharacterMap(cmap)

    for name, (base_name, mark, dx, dy) in composites.items():

        pen = TTGlyphPen(outlines)
        pen.addComponent(base_name, (1, 0, 0, 1, 0, 0))
        pen.addComponent(mark, (1, 0, 0, 1, round(dx), round(dy)))
        outlines[name] = pen.glyph()

    builder.setupGlyf(outlines)
    builder.setupHorizontalMetrics({name: (advances[name], 0) for name in glyph_order})
    builder.setupHorizontalHeader(ascent=ASCENDER, descent=DESCENDER)
    builder.setupNameTable(
        {
            "familyName": FAMILY,
            "styleName": STYLE,
            "uniqueFontIdentifier": f"{FAMILY} {STYLE} {VERSION}",
            "fullName": f"{FAMILY} {STYLE}",
            "psName": f"{FAMILY.replace(' ', '')}-{STYLE}",
            "version": f"Version {VERSION}",
            "copyright": (
                "Copyright 2026 Standard of Iron contributors. "
                "Licensed under the SIL Open Font License 1.1."
            ),
            "designer": "Standard of Iron contributors",
            "description": (
                "Roman monumental capitals cut in damaged iron. "
                "The display face of Standard of Iron."
            ),
            "licenseDescription": (
                "This Font Software is licensed under the SIL Open Font License, Version 1.1."
            ),
            "licenseInfoURL": "https://scripts.sil.org/OFL",
        }
    )
    builder.setupOS2(
        sTypoAscender=ASCENDER,
        sTypoDescender=DESCENDER,
        sTypoLineGap=0,
        usWinAscent=ASCENDER + 260,
        usWinDescent=-DESCENDER,
        sxHeight=int(CAP * 0.72),
        sCapHeight=CAP,
        usWeightClass=700,
        fsSelection=0x20,
        panose={
            "bFamilyType": 2,
            "bSerifStyle": 4,
            "bWeight": 9,
            "bProportion": 4,
            "bContrast": 8,
            "bStrokeVariation": 0,
            "bArmStyle": 0,
            "bLetterForm": 0,
            "bMidline": 0,
            "bXHeight": 0,
        },
    )
    build_kerning(builder, set(glyph_order))

    builder.setupPost(isFixedPitch=0, underlinePosition=-120, underlineThickness=90)
    builder.font["head"].macStyle = 1

    builder.font["head"].created = BUILD_EPOCH
    builder.font["head"].modified = BUILD_EPOCH

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    builder.save(str(OUTPUT))
    return OUTPUT


if __name__ == "__main__":
    print(f"wrote {build()}")
