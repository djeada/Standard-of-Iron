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
map_cli backdrop <map> <out.png>       # the editor's painted world
map_cli new      <out> <width> <height> [name]   # a blank world-space map
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

## Building a town

A town is too many structures to place one at a time, so the operations file
has verbs for the pieces a town is made of. Each is also a tool in the editor.

```json
[
    {
        "op": "block",
        "id": "subura_3",
        "origin": [44, -128],
        "lots": [7, 2],
        "rotation": 6,
        "alley": 3,
        "style": "tenement",
        "player_id": 1,
        "nation": "roman_republic"
    },
    {
        "op": "terrace",
        "id": "basilica_aemilia",
        "from": [-2, -36.75],
        "to": [64, -36.75],
        "facing": 0,
        "type": "marketplace",
        "style": "basilica",
        "player_id": 1,
        "nation": "roman_republic"
    },
    {
        "op": "paving",
        "rect": [-3.5, -35.5, 65.5, -6.5],
        "style": "flagstone",
        "strip": 12
    },
    {
        "op": "wall",
        "id": "servian",
        "points": [
            [-64, -140],
            [172, -140],
            [172, 78],
            [130, 78],
            [130, 146],
            [-64, 146]
        ],
        "style": "masonry",
        "tower_spacing": 46,
        "player_id": 1,
        "nation": "roman_republic"
    },
    { "op": "gate", "id": "porta_esquilina", "at": [172, -21] }
]
```

- `terrace` lays buildings of one `type` along `from` -> `to`, one lot apart
  and centred on the run, fronts turned to `facing` (0 is +z, 90 is +x).
  `skip` lists lots to leave open. The lot is the type's navigation footprint:
  4.3 units for a home, 5.5 for a marketplace.
- `block` lays the four terraces of a perimeter block of `lots: [wide, deep]`
  from its low corner, turned by `rotation` about that corner, with one lot of
  the first terrace left open as the `alley` into the court.
- `paving` covers a rectangle with parallel road strips of `style` and one
  across them, so the square is a single road network.
- `wall` builds a circuit through `points` (legs along x or z only) with a
  tower on every corner and every `tower_spacing` along a leg. With
  `"style": "masonry"` it is built in stone; without it the wall is the
  nation's own.
- `gate` cuts a gate into the wall run under `at`; it takes the run's style.

A `style` has to be one the building type has (`tenement` for a home;
`portico`, `basilica` or `dock` for a marketplace; `masonry` for a wall); the
operation is refused otherwise.

The two cities are built this way and nothing else:
`tools/map_editor/plans/rome.ops.json` and `carthage.ops.json` applied to a
`map_cli new` map give `assets/maps/map_rome.json` and `map_carthage.json`.

## Water level and outline

A lake carries two optional fields, and the editor writes back exactly what it
was given:

- `height`: the water level in world units, the ground being 0. Unset, the
  water lies with the ground at the lake's centre. `-0.6` is the level a sea
  or a harbour needs to give a quay a bank; a river running into it takes the
  same `height`.
- `shape`: `natural` (the default) is an ellipse with a wandering shore;
  `round` a true ellipse; `basin` a rectangle with eased corners. A round
  harbour with an island in it is a ring river.

Where lakes overlap they draw as one water. A dock stands with its centre 2.25
units inland of the water's edge and its front to the water.

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

## The painted canvas

The editor's main area used to be a blueprint: ellipses for hills, a blue stroke
for a river, lettered discs for buildings. It now paints the land the map
becomes (`tools/map_editor/world_backdrop.cpp`, `world_sprites.cpp`):

- The ground is the engine's own height map, built in a private game session
  from the open document, lit from the top left with cast shadows, coloured from
  the map's biome (grass, dry grass, soil, rock, snow) and tinted for a night map.
- Rivers and lakes come from the terrain types, with a shoreline, shallows and
  depth. Roads and bridges are painted over them.
- Woods are the game's actual scattered trees, boulders and ore - the same
  procedural pass the game runs - so thickening a forest shows as more trees.
- Buildings, camps, palisades, gates, ruins, fires and shrines are drawn at
  their real footprint in place of lettered badges; an owner's colour sits under
  each building. Troops stay as tokens.
- Editing shapes appear when you need them: a hill's outline and ramps on hover
  or selection, a river's centre line when selected, a forest's dashed edge.
- Cloud shadows drift across the map (View > Drifting Cloud Shadows) and pause
  themselves when a paint takes longer than 14 ms.

The backdrop is repainted in the background 160 ms after an edit; the canvas
keeps the last picture until the new one is ready. `F6` (View > Painted World)
switches to the blueprint. `SOI_EDITOR_PAINT_STATS=1` logs each paint's time.

A map in world coordinates (Aurelia Magna) is framed where its content is; the
blueprint used to draw half of it outside the grid.

A ridge authored as a spine has no centre of its own. Dragging one used to
measure from (0, 0) and throw it off the map; it now moves by the distance the
cursor travelled.
