# Hill Shapes and Raised Terrain

A `terrain` entry of type `hill` uses its `shape` field to define the tactical footprint of raised ground. Maps can author organic mounds, straight ridges, arcs, elbows, rings, traced paths, or exact painted cell masks.

The footprint feeds the same distance-field terrain logic used for crown height, slopes, rim walkability, entrance ramps, erosion, and line-of-fire blocking. Rendering operates on the resulting height map rather than on the authored shape type itself.

## Available hill shapes

| `shape`          | What it represents                                             | Sized by                                           |
| ---------------- | -------------------------------------------------------------- | -------------------------------------------------- |
| `blob` (default) | Organic mound                                                  | `radius`, or `width` / `depth`                     |
| `corridor`       | Straight capsule ridge along its long axis                     | `width` / `depth`, `thickness`                     |
| `arc`            | Elliptical band: boomerang, crescent, or corner wrap           | `width` / `depth`, `thickness`, `arc`, `arc_start` |
| `elbow`          | Two straight arms meeting at a hard corner                     | `width` / `depth`, `thickness`, `arc`              |
| `ring`           | Closed band around a hollow interior                           | `width` / `depth`, `thickness`                     |
| `path`           | Band following authored spine points                           | `points`, `thickness`                              |
| `mask`           | Exact cells listed in `cells`                                  | `cells`                                            |

Accepted aliases are:

- `boomerang`, `crescent`, and `horseshoe` → `arc`;
- `ridge` and `wall` → `corridor`;
- `crater` → `ring`; and
- `painted` → `mask`.

`path` and `mask` cover footprints that do not fit the named primitives: a path traces a centerline, while a mask authors occupied cells directly.

## Shape fields

### `width` and `depth`

These define the overall footprint extents in the hill's local frame before rotation. A shape's centreline is inset by the ridge thickness, so a `corridor` with `width: 40` and `depth: 10` occupies an overall 40-by-10 footprint.

### `thickness`

This controls the width of the raised band. It defaults to the short side for `corridor` and to roughly one third of the smaller extent for other banded shapes.

### `arc`

The meaning depends on the shape:

- for `arc`, it is the sweep in degrees;
- for `ring`, it is the sweep and defaults to 360°; and
- for `elbow`, it is the angle between the two arms.

Defaults are 120° for `arc`, 360° for `ring`, and 90° for `elbow`.

### `arc_start`

For an `arc`, `arc_start` defines where the sweep begins, measured in degrees from local `+x`. By default the sweep is centred on `+x`, so `rotation` alone can aim the feature.

### `taper`

`taper` ranges from 0 to 1 and narrows an open shape toward both ends. A tapered arc forms a crescent.

### `points`

`points` is used by `path` and defines a spine in map coordinates:

```json
[
  { "x": 40, "z": 20 },
  { "x": 52, "z": 26 }
]
```

A path consults `width` and `depth` when `thickness` is absent, so it needs either an explicit thickness or suitable extents.

### `cells`

`cells` is used by `mask`. Painted areas can be represented as row spans such as `[z, x_from, x_to]` or as individual `[x, z]` pairs. The map editor writes this representation directly.

### `rotation`

`rotation` turns the complete shape around its `x` / `z` centre. It is ignored for `mask`, whose cells already exist in map space.

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

The band wraps around the barracks at `(20, 100)` and faces the middle of the map. An attacker approaching from the centre meets raised ground, while the defender reaches it from the protected side. `assets/maps/map_rivers.json` contains this arrangement.

## Map-editor authoring

Double-clicking a hill opens its JSON dialog. The projection panel shows occupied cells, and the layer controls switch between the hill body and entrance cells.

Painting the body stores the hill as `shape: "mask"` with the exact cells drawn. Runtime terrain raises those cells and slopes inward from their boundary, so the projection corresponds directly to the terrain footprint.

Leaving the body untouched preserves the authored shape and its numeric parameters.

The terrain toolbox also places **Ridge**, **Boomerang**, **Elbow**, and **Ring** presets with appropriate shape values and starting extents.

## Flat features: terraces and raised ground

A `terrain` entry of type `flat` defines an absolute-height raised feature inside an ellipse.

Flat features do not use hill entrance rules or an unwalkable rim, so troops can approach them from any side. They are suitable for terraces, city shelves, raised forums, and broad traversable mountains.

| Field             | Meaning                                                                         |
| ----------------- | ------------------------------------------------------------------------------- |
| `width` / `depth` | Ellipse dimensions; `radius` is the fallback when neither is supplied          |
| `height`          | Absolute height inside the plateau                                              |
| `taper`           | Fraction of the radius occupied by the sloped rim, from `0` to `1`; default `0.20` |
| `raise`           | Keep the higher of existing ground and this feature instead of overwriting it   |

### `taper`

At `taper: 0.20`, the outer fifth of the radius forms the slope and the remaining area is plateau. The sacred mountain in `map_aurelia_magna` uses `taper: 0.49`, giving roughly half its radius to the climb.

A broad taper produces one continuous slope instead of the stepped profile created by stacked nested ellipses.

### `raise`

Without `raise`, a flat feature sets the height of every covered cell, so overlapping features are order-dependent.

With `"raise": true`, a feature can only increase terrain height. A terrace and a mountain can therefore overlap by taking the higher value regardless of their order in the data.

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

Shaped hills define tactical barriers and ridges; flat features define traversable raised surfaces. Both are authored as terrain data and resolved into the same heightfield used by navigation and rendering.
