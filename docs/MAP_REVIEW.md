# Reviewing and Editing Maps from the Command Line

The map editor has a window, but most of a map review is not window work: read
what is on the map, look at it, change a handful of things, and prove nothing
else moved. This document covers the tools for that loop. They all go through
the editor's own document model (`tools/map_editor`, library `map_editor_core`),
so a change made here is the same change the editor would have made.

| Step                      | Tool                                                                               |
| ------------------------- | ---------------------------------------------------------------------------------- |
| Inventory and audit       | `build/bin/map_cli summary`, `list`, `audit`                                       |
| Plan view and free ground | `scripts/map-plan.py`                                                              |
| Beauty shot               | `arena_app --batch --terrain-map <map> --map-preview-content --capture-interval 0` |
| Batch edit                | `build/bin/map_cli apply <map> <ops.json>`                                         |
| Bend straight rivers      | `scripts/map-meander.py`                                                           |
| Dress a map               | `dressing` entries + `scripts/generate-map-dressing.py --only`                     |
| Placement check           | `scripts/fix-map-prop-overlaps.py --check --surface require`                       |

## Saving is lossless

Opening a map and saving it used to rewrite it: numbers were rounded to two
decimals, defaults were injected, legacy sections were migrated and fields the
editor does not model were dropped or reordered. Across the shipped maps a
no-op save changed about five thousand values.

`MapData::to_json_bytes()` now merges the edit into the text that was loaded
(`tools/map_editor/json_text.cpp`). Anything the edit did not touch is written
back byte for byte, in its original order and number spelling; an edited
element is merged field by field, so moving a forest rewrites two numbers.
`tests/tools/map_editor_lossless_save_test.cpp` holds every shipped map to a
byte-identical round trip.

The old schema-normalising writer is still there for when a migration is the
point: `MapData::save_canonical_json()` and `map_cli normalize`.

## `map_cli`

```sh
map_cli summary  <map>                 # counts per kind, grid, name
map_cli list     <map> [kind] [--json] # every element with index, id, position
map_cli audit    <map>                 # terrain topology audit, offenders named
map_cli apply    <map> <ops.json> [--out <path>] [--dry-run]
map_cli roundtrip <map> <out>          # load + save, for diffing
map_cli normalize <map> <out>          # rewrite in the editor's schema
```

Kinds are `terrain`, `prop`, `linear` (rivers, roads, bridges), `structure`,
`spawn`, `undead_zone`, `wildlife` and `forest`. An operations file is a JSON
array applied in order; an element is addressed by `index` or by `id`:

```json
[
    { "op": "move", "kind": "forest", "id": "west_wood", "by": [4, -2] },
    {
        "op": "set",
        "kind": "terrain",
        "id": "the_spur",
        "fields": { "taper": 0.5, "rotation": 87 }
    },
    {
        "op": "add",
        "kind": "forest",
        "json": { "id": "north_copse", "x": 30, "z": 12, "radius": 6 }
    },
    { "op": "remove", "kind": "prop", "index": 192 },
    { "op": "map", "biome": { "forest_density": 2.4 } },
    {
        "op": "root",
        "key": "dressing",
        "append": [{ "id": "ford_cairn", "kind": "cairn", "x": 40, "z": 12 }]
    }
]
```

`set` merges fields into the element (`null` removes one); `replace` swaps the
whole element. `map` merges into `biome`, `rain` and `environment`. `root`
edits root data the editor does not model - `dressing`, `landmarks`,
`settlements` - with `value`, `append` or `remove_ids`; it refuses keys that
have a kind of their own. A failed step is reported with its number and nothing
is written.

The audit prints which hill, river end, lake or road network it is unhappy
with, in map coordinates, instead of only a count.

## `scripts/map-plan.py`

```sh
scripts/map-plan.py assets/maps/map_forest.json --png plan.png
scripts/map-plan.py assets/maps/map_forest.json --open 12 --min-radius 8 --flat-only
```

Hills are not the ellipses they are authored as, so the plan asks the engine:
it runs `terrain_probe` for real walkability, terrain types and ramp cells,
then draws roads, bridges, forests, structures, spawns and props over them with
grid coordinates on the axes - the same x/z the map JSON and `map_cli` use.
`--open N` lists the N largest discs of free ground, which is where a new wood,
camp or landmark can go without sitting on something.

## `scripts/map-meander.py`

```sh
scripts/map-meander.py assets/maps/map_battle_cannae.json --ops bends.json
build/bin/map_cli apply assets/maps/map_battle_cannae.json bends.json
```

Resamples each straight river and pushes it sideways by two overlaid waves. The
push is zero at the river's ends, at every bridge, ford and confluence, and
eases in from there, so decks still span the channel squarely. A point is
pulled back wherever the channel would enter a hill or come too close to a
structure, spawn, prop, road, lake or another river. Rivers that already wander
are left alone (`--skip-above`). Always follow it with the audit and the
placement check.

## Thick woods without a thick map

`biome.tree_mix` scales a species everywhere, so using it to thicken the woods
also plants trees across the open ground a base needs for its fields.
`biome.forest_density` (default 1, up to 6) scales only the canopy inside
authored `forests`; open ground is untouched
(`MapLoaderTest.ForestDensityThickensTheWoodsAndLeavesOpenGroundAlone`).

A forest drawn over a hill is clipped to the flat ground beyond it, so a wood
that sits mostly on a slope stands almost no trees. Put woods on flat ground;
the plan view shows which is which.

## Dressing

Add entries to the map's `dressing` array with a `root` operation, then compose
only those pieces:

```sh
build/bin/map_cli apply <map> pieces_ops.json
scripts/generate-map-dressing.py --write --surface require --only id1,id2 <map>
scripts/fix-map-prop-overlaps.py --check --surface require <map>
```

`--only` matters: a full rerun re-rolls dressing that has since been nudged.
On mission maps keep pieces off the scripted spawn points, lanes and the
player's starting positions in `assets/missions/*.json`.
