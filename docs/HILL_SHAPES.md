# Hill shapes

A hill used to be one thing: an ellipse. `shape` on a `terrain` entry of type
`hill` turns the footprint into something a map can be designed around — a
corridor that forces a fight along its length, a boomerang tucked into a corner
so the pocket behind it is a natural keep, a ring with a bailey inside, or the
exact set of cells you painted in the map editor.

Everything else about a hill is unchanged. The crown, the slope, the rim
walkability and the entrance ramps all fall out of the same distance field, so a
shaped hill carries entrances, blocks line of fire and erodes exactly like a
round one. The renderer never sees the shape at all: it draws the height map.

## The shapes

| `shape`          | What it is                                                     | Sized by                                         |
| ---------------- | -------------------------------------------------------------- | ------------------------------------------------ |
| `blob` (default) | The classic organic mound.                                     | `radius`, or `width`/`depth`                     |
| `corridor`       | A straight capsule ridge running along its long axis.          | `width`/`depth`, `thickness`                     |
| `arc`            | An elliptical band. A boomerang, a crescent, or a corner wrap. | `width`/`depth`, `thickness`, `arc`, `arc_start` |
| `elbow`          | Two straight arms meeting at a hard corner.                    | `width`/`depth`, `thickness`, `arc`              |
| `ring`           | A closed band around a hollow interior.                        | `width`/`depth`, `thickness`                     |
| `path`           | A band following authored spine points.                        | `points`, `thickness`                            |
| `mask`           | Exactly the cells listed in `cells`.                           | `cells`                                          |

`boomerang`, `crescent` and `horseshoe` are accepted spellings of `arc`;
`ridge` and `wall` of `corridor`; `crater` of `ring`; `painted` of `mask`.

## Fields

- `width`, `depth` — the overall footprint extents in the hill's own frame,
  before rotation. A shape's centreline is inset from them by the thickness, so
  a `corridor` with `width: 40, depth: 10` really is 40 by 10.
- `thickness` — how wide the band is across the ridge. Defaults to the short
  side for `corridor` and to about a third of the smaller extent otherwise.
- `arc` — sweep in degrees for `arc` and `ring`; the angle between the two arms
  for `elbow`. Defaults to 120 (arc), 360 (ring) and 90 (elbow).
- `arc_start` — where an arc begins, in degrees measured from local `+x`.
  Defaults to a sweep centred on `+x`, so `rotation` alone aims a boomerang.
- `taper` — 0 … 1. Narrows an open shape towards its two ends, which is what
  turns a plain arc into a crescent.
- `points` — `path` only. Spine points in map coordinates:
  `[{"x": 40, "z": 20}, {"x": 52, "z": 26}]`. A `path` still reads `width` and
  `depth` when `thickness` is absent, so give it one or the other.
- `cells` — `mask` only. Painted rows as `[z, x_from, x_to]` spans, or single
  `[x, z]` pairs. Written by the map editor; rarely hand authored.
- `rotation` — rotates the whole shape about `x`/`z`. Ignored by `mask`, whose
  cells are already in map space.

`mask` and `path` are the escape hatches: anything the named shapes cannot
express can be drawn cell by cell or traced as a polyline.

## A corner keep

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

The band sweeps 100 degrees around the barracks at `20, 100`, facing the middle
of the map, and both ramps sit on the inside. An attacker coming from the centre
meets a wall; the defender walks up from behind it. `assets/maps/map_rivers.json`
ships this pair.

## Drawing a shape in the map editor

Double-click a hill to open its JSON dialog. The projection panel on the right
draws the hill's cells; the layer buttons above it switch between painting the
hill body and painting its entrances.

Paint the body and the hill is saved as `shape: "mask"` with the exact cells you
drew — the runtime raises precisely those cells and slopes inward from their
edge, so what the panel shows is what the map builds. Leave the body alone and
the authored shape and its numbers are preserved untouched.

The terrain tool box also places the common shapes directly: **Ridge**,
**Boomerang**, **Elbow** and **Ring** drop a hill already carrying the right
`shape` and sensible extents, ready to be nudged in the JSON dialog.
