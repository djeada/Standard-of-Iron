# Hill Shapes and Raised Terrain

Hills are no longer limited to elliptical mounds. The `shape` field on a `terrain` entry of type `hill` lets map authors build terrain around tactical ideas: a ridge that channels a fight, a boomerang that protects a corner keep, a ring enclosing a bailey, a traced path, or the exact cells painted in the map editor.

Only the footprint changes. Crown height, slope, rim walkability, entrance ramps, erosion, and line-of-fire blocking are still derived from the same distance field. The renderer does not need to understand the authored shape at all; it renders the resulting height map.

## Available hill shapes

| `shape`          | What it represents                                             | Sized by                                         |
| ---------------- | -------------------------------------------------------------- | ------------------------------------------------ |
| `blob` (default) | The classic organic mound                                      | `radius`, or `width` / `depth`                   |
| `corridor`       | A straight capsule ridge along its long axis                   | `width` / `depth`, `thickness`                   |
| `arc`            | An elliptical band: boomerang, crescent, or corner wrap        | `width` / `depth`, `thickness`, `arc`, `arc_start` |
| `elbow`          | Two straight arms meeting at a hard corner                     | `width` / `depth`, `thickness`, `arc`            |
| `ring`           | A closed band around a hollow interior                         | `width` / `depth`, `thickness`                   |
| `path`           | A band following authored spine points                         | `points`, `thickness`                            |
| `mask`           | Exactly the cells listed in `cells`                            | `cells`                                          |

Several aliases are accepted for readability:

- `boomerang`, `crescent`, and `horseshoe` map to `arc`;
- `ridge` and `wall` map to `corridor`;
- `crater` maps to `ring`; and
- `painted` maps to `mask`.

`path` and `mask` are the escape hatches. If the named shapes cannot express a desired footprint, trace a spine or author the exact cells instead.

## Shape fields

### `width` and `depth`

These define the overall footprint extents in the hill's local frame, before rotation. A shape's centreline is inset by the ridge thickness, so a `corridor` with `width: 40` and `depth: 10` still occupies an overall 40-by-10 footprint.

### `thickness`

This controls the width of the raised band across the ridge. It defaults to the short side for a `corridor` and to roughly one third of the smaller extent for other banded shapes.

### `arc`

The meaning depends on the shape:

- for `arc`, it is the sweep in degrees;
- for `ring`, it is the sweep and defaults to 360°; and
- for `elbow`, it is the angle between the two arms.

Defaults are 120° for `arc`, 360° for `ring`, and 90° for `elbow`.

### `arc_start`

For an `arc`, `arc_start` defines where the sweep begins, measured in degrees from local `+x`. By default the sweep is centred on `+x`, which means `rotation` alone is enough to aim a boomerang.

### `taper`

`taper` ranges from 0 to 1 and narrows an open shape toward both ends. This is what turns a uniform arc into a crescent.

### `points`

`points` is used only by `path`. The points define a spine in map coordinates:

```json
[
  { "x": 40, "z": 20 },
  { "x": 52, "z": 26 }
]
```

A path still consults `width` and `depth` when `thickness` is absent, so author either an explicit thickness or suitable extents.

### `cells`

`cells` is used only by `mask`. Painted areas can be represented as row spans such as `[z, x_from, x_to]` or as individual `[x, z]` pairs. The map editor normally writes this representation; it is rarely hand-authored.

### `rotation`

`rotation` turns the complete shape around its `x` / `z` centre. It is ignored for `mask`, because mask cells already exist in map space.

## Example: a corner keep

The following hill creates a tapered 100° arc around a barracks and places both ramps on the protected side:

```json
{
  "type": "hill",
  "shape": "arc",
  "x": 20,
  "z": 100,
  "width": 52,
  "depth": 52,
  "thickness": 8,
  "arc": 100,
  "arc_start": -95,
  "taper": 0.35,
  "height": 2.6,
  "entrances": [
    { "x": 22.4, "z": 86.2 },
    { "x": 33.8, "z": 97.6 }
  ]
}
```

The band wraps around the barracks at `(20, 100)` and faces the middle of the map. An attacker approaching from the centre meets a raised wall, while the defender can reach the high ground from behind it. `assets/maps/map_rivers.json` ships this arrangement.

## Drawing custom hills in the map editor

Double-click a hill to open its JSON dialog. The projection panel on the right shows the cells occupied by the hill, while the layer controls above it switch between editing the hill body and its entrances.

Painting the body changes the hill to `shape: "mask"` and stores the exact cells that were drawn. At runtime, those cells are raised and sloped inward from their boundary, so the projection in the editor corresponds directly to the terrain built by the game.

If the body is left untouched, the existing authored shape and its numeric parameters are preserved.

The terrain toolbox can also place common shapes directly. **Ridge**, **Boomerang**, **Elbow**, and **Ring** create hills with appropriate `shape` values and sensible starting extents that can then be refined in the JSON dialog.

## Flat features: terraces and raised ground

A `terrain` entry of type `flat` provides the other half of the relief vocabulary.

Unlike a hill, a flat feature does not gate navigation. It has no entrance requirement and no unwalkable rim, so troops can approach from any side. Rather than raising a crown above the surrounding ground, it defines an absolute height inside an ellipse.

This makes `flat` suitable for terraces, city shelves, raised forums, and broad mountains that players are expected to traverse.

| Field             | Meaning                                                                       |
| ----------------- | ----------------------------------------------------------------------------- |
| `width` / `depth` | Ellipse dimensions; `radius` is the fallback when neither is supplied        |
| `height`          | Absolute height inside the plateau                                            |
| `taper`           | Fraction of the radius used by the sloped rim, from `0` to `1`; default `0.20` |
| `raise`           | Keep the higher of existing ground and this feature instead of overwriting it |

### Taper controls whether a feature reads as a shelf or a mountain

At the default `taper: 0.20`, the outer fifth of the radius forms the slope and the remaining area is a plateau. That produces a clear terrace or step.

The sacred mountain in `map_aurelia_magna` uses `taper: 0.49`, so roughly half of its radius is climb. Without a broad taper, long slopes have to be approximated with nested ellipses, which produces a visible “wedding cake” profile.

### `raise` makes overlapping terrain composable

By default, a flat feature sets the height of every cell it covers. When several features overlap, the last entry in the array therefore wins, and list order silently changes the terrain.

With `"raise": true`, a feature can only increase terrain height. A broad terrace and a mountain standing on it then combine by taking the higher value, regardless of their ordering in the data.

```json
{
  "type": "flat",
  "x": 0,
  "z": -306,
  "width": 300,
  "depth": 300,
  "height": 58,
  "taper": 0.49,
  "raise": true
}
```

Together, shaped hills and flat raised features let map authors describe tactical terrain directly instead of approximating it through stacks of ellipses or renderer-specific tricks.
