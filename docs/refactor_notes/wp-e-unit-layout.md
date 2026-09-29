# WP-E: unit_layout.cpp

Per-nation built-in style tables moved out of `game/formation/unit_layout.cpp`
(1235 -> 583 lines) into their own translation units. Logic (`UnitLayoutLibrary`,
`UnitLayoutSystem`, state modifiers, string conversion) stays.

- `unit_layout_style_tables.h`: internal header (namespace `style_tables`)
  declaring `make_style`, the per-group `register_*` functions and
  `register_default_styles`.
- `unit_layout_styles_generic.cpp`: generic tables split by role
  (infantry, mounted, support, sepulcher) so no function exceeds ~100 lines;
  `register_default_styles` fixes registration order.
- `unit_layout_styles_rome.cpp`, `..._carthage.cpp`, `..._sepulcher.cpp`.

Registration order is unchanged, so `UnitLayoutId` values are identical. The
style bodies were moved verbatim (field text compared before/after).
`scripts/sync-unit-layouts.py` now parses `unit_layout_styles_*.cpp`; its
`--check` output is unchanged (0 written, 0 removed).
