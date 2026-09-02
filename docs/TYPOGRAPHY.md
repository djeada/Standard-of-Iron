# Typography

Standard of Iron ships its own type. Nothing in the game or its tools asks the
operating system for a font by name.

## Why it is bundled

The reels are the most-seen thing this project produces, and they used to be
lettered by whatever the capture machine happened to have installed.
`scripts/promo-edit.py` walked an eight-deep list of `/usr/share/fonts` paths —
EB Garamond, then Latin Modern, then Linux Libertine, down to Liberation Serif —
and captioned the video with the first hit. The same promo spec cut on a build
box, a laptop and CI could publish three differently-lettered videos from
identical inputs, and nothing in the output said which face it got.

The game had a quieter version of the same problem: `Typography.family` and
`Typography.displayFamily` were bare family names resolved by fontconfig.

## The three families

| Token                      | Face                                | Job                                                                      |
| -------------------------- | ----------------------------------- | ------------------------------------------------------------------------ |
| `Typography.family`        | Noto Sans (system)                  | Body text, settings, debug, anything the player reads at length.         |
| `Typography.displayFamily` | Noto Serif (system)                 | Serif headings, **and the interface's symbol glyphs** — `⚔ ⚑ ⚒ ♛ ⛏ ◈ ☾`. |
| `Typography.titleFamily`   | **Standard Iron Display** (bundled) | Titles, outcome headlines, big numbers, reel captions.                   |

`displayFamily` is deliberately _not_ the brand face. It carries the command and
faction glyphs that `tests/ui/qml/tst_glyph_coverage.qml` asserts, and a
caps-and-figures display face has none of them.

## The one rule: `titleFamily` is capitals and figures only

**The display face has no lowercase.** Qt falls back per glyph, so binding
`titleFamily` to mixed-case text renders half a word in one typeface and half in
another — legible enough that nobody notices in review, and obviously wrong in a
screenshot.

Bind it only where the text is numeric, or alongside:

```qml
font.family: Design.Typography.titleFamily
font.capitalization: Font.AllUppercase
```

`TitleFamilyUsageTest.EveryBindingIsUppercasedOrNumeric` in
`tests/ui/brand_fonts_test.cpp` scans `ui/qml` and fails on any binding that
does neither. That guard exists in a test rather than in a comment next to the
property because `make format` strips C++ and QML comments
(`scripts/remove-comments.sh`), so a comment there would not survive.

## How each consumer loads the fonts

Three paths, all of which must keep working:

- **QML** loads from qrc via `FontLoader` in `ui/qml/design/Typography.qml`.
  This is why the files are listed in `assets.qrc`.
- **The Qt Widgets tools** (arena, map editor) register from disk in
  `Ui::BrandFonts::register_bundled()`, called from `UiShell::apply()`. It
  resolves through `Utils::Resources::resolve_resource_path`, which finds the
  staged `build/bin/assets/fonts` copy before the qrc one — the staged copy is
  what a dev build actually reads, so a qrc-only load is not enough.
- **The reel cutter** `scripts/promo-edit.py` resolves repo-relative and fails
  loudly rather than falling back to a system font.

Burned-in arena text goes through `Arena::Typography` (`number`,
`small_label`). Act cards and subtitles are not there: those are composited
afterwards by `promo-edit.py`, which owns its own sizing and resolves the same
file by path.

## Testing coverage: fallback or no fallback

Two different questions, two different instruments, and using the wrong one
makes a test pass forever:

- `QFontMetrics::inFont()` answers through the font engine, **fallbacks
  included**. Right for the interface glyphs — what matters is that the player
  sees the mark, not which family drew it. This is `GlyphProbe.missing()`.
- `QRawFont::supportsCharacter()` asks one physical face with nothing behind
  it. Right for the brand face, where a fallback _is_ the failure. This is
  `GlyphProbe.missingWithoutFallback()` and `missing_from()` in
  `tests/ui/brand_fonts_test.cpp`.

The display face's own coverage test was written with `QFontMetrics` first and
passed while proving nothing, because the system serif quietly supplied every
character it asked about.

## The display face is generated

`assets/fonts/StandardIronDisplay-Bold.ttf` is compiled from geometry in
`tools/font/`, not drawn in an editor:

```
python3 -m venv .venv
.venv/bin/pip install -r tools/font/requirements.txt
.venv/bin/python tools/font/build_standard_iron.py     # rebuild the .ttf
.venv/bin/python tools/font/proof.py proof.png         # look at it
```

The build is deterministic — same source, same bytes. Fix letters in
`tools/font/glyph_shapes.py` and rebuild; editing the binary in a font editor
loses the change the next time anyone regenerates.

The `.ttf` is committed anyway, because the game loads it at runtime and a
contributor should not need a font toolchain to run the game.

### How the letters are built

Every glyph is assembled from three recurring forms and nothing else — a
**wedge** (the flared Roman serif), a **cut** (a 35° chisel bite, one angle for
the whole font), and a **point** (a blade terminal). That is what makes the
alphabet read as one alphabet; adding a fourth form is a decision about the
game's identity, not a drawing convenience.

Every terminal is served, including the diagonal ones. `stem()` grows its own
serifs, but a diagonal cannot — its terminal is cut horizontally at whatever x
the slope reaches — so `foot_serif()` and `head_serif()` place them explicitly
on A, V, W, X, Y, K and M. Without those, X and V had bare cuts and read as a
second, sans-serif alphabet sitting inside the serifed one, which was the
single most visible flaw in the first draft.

A glyph is material added and material taken away, resolved with real boolean
geometry (`skia-pathops`) at build time. Bowls that grow out of a stem are
`Part`s that resolve _before_ they merge, so trimming a bowl back to its stem
cannot eat the stem — that scoping is what B, D, P and R depend on.

Kerning is a legacy `kern` table (`tools/font/glyph_kerning.py`), not GPOS:
the font has no other OpenType layout, and HarfBuzz — which is what Qt shapes
with — reads `kern` when there is no GPOS kerning to prefer. All 72 pairs are
negative, because kerning here only ever closes a gap; the reel presets add
tracking on top, and a pair aggressive enough to look right at default spacing
collides once tracking is applied.
`BrandFontsTest.QtAppliesTheDisplayFacesKerning` asserts the pairs actually
reach Qt, because a shaper that ignored them would cost nothing visible except
a hole in the middle of SURVIVE.

Coverage is A–Z, 0–9, punctuation, the Latin-1 accented capitals German,
Spanish and Brazilian Portuguese need, `Ğ İ Ş` for Turkish, `Ą Ć Ę Ł Ń Ś Ź Ż`
for Polish and the Russian capitals `А–Я` — all outside Latin-1, so the breve,
the dot above, the ogonek and the Ł stroke are drawn here rather than borrowed.
Accented forms are composites (base + mark), so correcting `O` corrects
`Ó Ò Ô Õ Ö` at once. Eleven Cyrillic capitals — `А В Е К М Н О Р С Т Х` — are
not drawn at all: `ALIASES` in `tools/font/glyph_cyrillic.py` points their code
points at the Latin outline, so `О` and `O` cannot drift apart. Arabic is not
covered and is not meant to be; it falls to the bundled text face.

## Licensing

Both bundled faces are OFL-1.1 and neither restricts commercial use. See
[THIRD_PARTY_LICENSES.md](../THIRD_PARTY_LICENSES.md) and the license files in
[assets/fonts/](../assets/fonts/).
