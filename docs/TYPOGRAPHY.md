# Typography System

Standard of Iron controls its typography explicitly so the game, tools, and promotional output can render the same text consistently on every machine. UI code does not rely on an arbitrary operating-system font lookup for branded text, and the custom title face is built and tested as part of the repository.

The typography system has three distinct jobs: long-form readability, symbol-capable interface display text, and a tightly controlled brand face for titles and large figures.

## Why typography is controlled by the project

Promotional reels and in-game screenshots are reproducible only when the same text is rendered with the same face. Relying on whatever font happens to exist on a build machine can produce visibly different output from identical inputs.

The same concern applies inside the application. Family names resolved indirectly through the host font stack can change appearance or coverage across development machines, CI, and packaged builds.

The project therefore treats font selection as part of its visual contract rather than an environmental detail.

## The three font families

| Token                      | Face                      | Role                                                                    |
| -------------------------- | ------------------------- | ----------------------------------------------------------------------- |
| `Typography.family`        | Noto Sans                 | Body text, settings, debug UI, and text read at length                  |
| `Typography.displayFamily` | Noto Serif                | Serif headings and interface symbols such as `⚔ ⚑ ⚒ ♛ ⛏ ◈ ☾`            |
| `Typography.titleFamily`   | **Standard Iron Display** | Titles, outcome headlines, large numbers, and promotional reel captions |

`displayFamily` is deliberately separate from the brand face. It must carry command and faction symbols verified by `tests/ui/qml/tst_glyph_coverage.qml`, while the title face is optimized for capitals and figures rather than broad symbol coverage.

## The central rule: `titleFamily` is for capitals and figures

**Standard Iron Display does not contain lowercase letters.**

Qt can fall back per glyph. If mixed-case text is assigned to `titleFamily`, one word can therefore be assembled from two different physical faces while remaining technically legible. That kind of fallback is easy to miss in ordinary review and obvious in a polished screenshot.

Use the title face only for numeric text or pair it with forced uppercase:

```qml
font.family: Design.Typography.titleFamily
font.capitalization: Font.AllUppercase
```

`TitleFamilyUsageTest.EveryBindingIsUppercasedOrNumeric` in `tests/ui/brand_fonts_test.cpp` scans `ui/qml` and rejects title-family bindings that are neither numeric nor uppercased.

The rule is enforced in a test rather than a nearby comment because `make format` removes C++ and QML comments through `scripts/remove-comments.sh`. The invariant therefore lives somewhere formatting cannot erase it.

## How fonts are loaded

Three consumers load the project fonts through different runtime environments. All three paths must remain valid.

### QML application UI

QML loads the packaged fonts through `FontLoader` in `ui/qml/design/Typography.qml`. The font assets are therefore listed in `assets.qrc`.

### Qt Widgets tools

Arena and the map editor register fonts from disk through `Ui::BrandFonts::register_bundled()`, called by `UiShell::apply()`.

The path resolves through `Utils::Resources::resolve_resource_path`, which prefers the staged `build/bin/assets/fonts` copy used by a normal development build. A resource existing only inside qrc is therefore not sufficient for the tools path.

### Promotional reel compositor

`scripts/promo-edit.py` resolves the font from the repository and fails explicitly if it cannot find the required asset. It does not silently substitute a system font.

Arena's burned-in labels use `Arena::Typography` helpers such as `number` and `small_label`. Act cards and subtitles are added later by `promo-edit.py`, which owns its own sizing while resolving the same physical title-face asset.

## Promotional text rendering

The reel compositor renders text above final output resolution and downsamples once with Lanczos filtering. This preserves the unhinted display face's wedge serifs while reducing jagged edges.

A thin dark outline and close shadow separate text from footage without filling the counters. Text composition remains in 4:4:4 color before the final video encode.

The sampling level is configurable:

- `--text-scale 1` for faster drafts;
- the normal higher-resolution path for release output; and
- `--text-scale 3` for an even denser sampling option.

These choices affect reel compositing only. Recutting footage does not require rebuilding the game or regenerating the font.

## Test fallback and physical coverage separately

Font testing has to answer two different questions.

### Does the interface display the glyph somehow?

`QFontMetrics::inFont()` goes through the active font engine **with fallback available**.

That is appropriate for ordinary interface symbols. The important result is that the player sees the glyph, not necessarily which physical face provided it. `GlyphProbe.missing()` uses this behavior.

### Does this exact brand face contain the glyph?

`QRawFont::supportsCharacter()` queries one physical face without fallback.

That is the correct test for Standard Iron Display because fallback is precisely the failure mode being guarded against. `GlyphProbe.missingWithoutFallback()` and `missing_from()` in `tests/ui/brand_fonts_test.cpp` use this path.

A brand-face coverage test that allows fallback can pass while proving nothing about the bundled face itself.

## Standard Iron Display is generated from source

`assets/fonts/StandardIronDisplay-Bold.ttf` is produced from deterministic geometry in `tools/font/` rather than maintained manually in a font editor.

Create the font-tool environment and rebuild the asset with:

```sh
python3 -m venv .venv
.venv/bin/pip install -r tools/font/requirements.txt
.venv/bin/python tools/font/build_standard_iron.py
.venv/bin/python tools/font/proof.py proof.png
```

The build is deterministic: identical source produces identical font bytes.

Make outline changes in `tools/font/glyph_shapes.py` and regenerate the `.ttf`. Editing the compiled binary directly creates changes that disappear the next time the source-driven build runs.

The generated `.ttf` is still committed because the game needs it at runtime and contributors should not need the font toolchain merely to launch the project.

## Visual language of the title face

Version 1.200 uses a small vocabulary of repeated forms to give the alphabet one coherent identity:

- a **wedge** for flared Roman serifs;
- a **cut** using a consistent 35° chisel angle; and
- a **point** for blade-like terminals.

The face combines those forms with open interiors and bold silhouettes suitable for titles and caption-sized use. Character-specific details include the spear-shaped `A`, deep central `M`, swept foot on `R`, and longer tapered wedge serifs.

Adding a fourth recurring construction form should be treated as a visual-language decision, not merely as a convenient way to solve one difficult glyph.

### Diagonal terminals need explicit treatment

Vertical stems can generate their own serifs through `stem()`. Diagonal strokes cannot: their terminal is cut horizontally wherever the slope reaches the boundary.

`foot_serif()` and `head_serif()` therefore add explicit terminals to diagonal-heavy letters such as `A`, `V`, `W`, `X`, `Y`, `K`, and `M`.

Without those shared terminal forms, diagonal letters read like a separate sans-serif alphabet embedded inside the serif face.

### Glyphs are constructive geometry

Each glyph is built from material added and removed through boolean geometry using `skia-pathops`.

Bowls attached to a stem are represented as scoped `Part`s that resolve before being merged with the stem. That ordering allows the bowl to be trimmed without accidentally carving into the structural stem, which is important for letters such as `B`, `D`, `P`, and `R`.

## Kerning

The face uses a legacy `kern` table generated by `tools/font/glyph_kerning.py` rather than GPOS positioning.

The font contains no other OpenType layout system, and HarfBuzz—the shaper used by Qt—honors the `kern` table when no GPOS kerning takes precedence.

All 72 pairs are negative because kerning in this face is used only to close visually excessive gaps. Promotional presets add tracking separately, so the pair adjustments must remain conservative enough not to collide once tracking is applied.

`BrandFontsTest.QtAppliesTheDisplayFacesKerning` verifies that the kerning information reaches Qt rather than merely existing in the binary.

## Character coverage

Standard Iron Display covers:

- `A–Z`;
- `0–9`;
- required punctuation;
- Latin-1 accented capitals used by supported German, Spanish, and Brazilian Portuguese text;
- `Ğ İ Ş` for Turkish;
- `Ą Ć Ę Ł Ń Ś Ź Ż` for Polish; and
- Russian capitals `А–Я`.

Accented letters are composed from a base glyph and mark where possible, so correcting the base outline also fixes its accented variants.

Several Cyrillic capitals share the same form as Latin characters. `ALIASES` in `tools/font/glyph_cyrillic.py` maps those code points to the Latin outline so visually identical forms cannot drift apart.

Arabic is intentionally outside the title face's coverage and falls back to the bundled text family.

## Licensing

The bundled third-party font families use OFL-1.1 and permit commercial use under that license. See [THIRD_PARTY_LICENSES.md](../THIRD_PARTY_LICENSES.md) and the license files in [assets/fonts/](../assets/fonts/).

The typography system is successful when the same content looks intentional in a game build, an editor tool, a screenshot, and a promotional reel—and when a missing glyph or accidental fallback fails loudly instead of becoming machine-dependent visual drift.
