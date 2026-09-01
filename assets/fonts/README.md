# Bundled typography

Every face the game and its tools draw with ships here. Nothing reads a font
by family name off the host machine.

That rule exists because the reels are the most-seen thing this project
produces, and they used to be lettered by whatever fontconfig turned up:
`scripts/promo-edit.py` walked an eight-deep list of `/usr/share/fonts` paths
and took the first hit, so the same promo spec cut on a build box, a laptop and
CI could publish three differently-lettered videos from identical inputs, with
nothing in the output saying which face it got.

## What is here

| File | Role |
| --- | --- |
| `EBGaramond12-Bold.ttf` | The text and fallback face. Covers lowercase, accents and everything the display face has no glyph for. OFL-1.1, Georg Duffner. |
| `StandardIronDisplay-Bold.ttf` | The brand face: titles, headings, outcome screens, big numbers, reel captions. **Caps, digits, punctuation, Latin-1 accented capitals and the Turkish `Ğ İ Ş` only — no lowercase.** Generated; see below. |

`OFL-EBGaramond.txt` is the license the bundled EB Garamond is redistributed
under; it has to travel with the file.

## How each consumer reaches them

Three paths, and all three must keep working:

- **QML** loads from qrc through `FontLoader` in `ui/qml/design/Typography.qml`,
  which is why the files are listed in `assets.qrc`. `Typography.titleFamily`
  is the token; `family` and `displayFamily` are unchanged and still carry the
  interface's symbol glyphs, which the display face does not have.
- **The widget tools** (arena, map editor) register from disk in
  `Ui::BrandFonts::register_bundled()`, called from `UiShell::apply()`. It
  resolves through `Utils::Resources::resolve_resource_path`, so it finds the
  staged `build/bin/assets/fonts` copy before the qrc one.
- **The reel cutter** `scripts/promo-edit.py` resolves repo-relative and fails
  loudly rather than falling back to a system font.

`tests/ui/brand_fonts_test.cpp` and the title-face cases in
`tests/ui/qml/tst_glyph_coverage.qml` fail if any of that stops resolving. A
silent fallback is readable enough that nobody notices by eye, which is exactly
why it is asserted.

## The display face is generated, not drawn in an editor

`StandardIronDisplay-Bold.ttf` is compiled from the geometry in `tools/font/`:

```
python3 tools/font/build_standard_iron.py     # rebuild the .ttf
python3 tools/font/proof.py proof.png         # look at it
```

Fix letters in `tools/font/glyph_shapes.py` and rebuild. Editing the binary in
a font editor loses the change the next time anyone regenerates.

It has **no lowercase**, and Qt falls back per glyph, so binding
`Typography.titleFamily` to mixed-case text renders half a word in one face and
half in another. Bind it only to digits, or alongside
`font.capitalization: Font.AllUppercase`. `tests/ui/brand_fonts_test.cpp`
asserts the caps-only range so the constraint is discoverable rather than
folklore.

## Adding a face

Drop the file here, add it to `assets.qrc`, and give it a row above. CMake
globs `assets/` so the staged copy follows on the next build. If it should
become the title face, add its family to `k_title_preference` in
`ui/brand_fonts.cpp` -- the list is walked in order and the first registered
family wins, so a new face takes over with no other code change.
