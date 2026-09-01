# Map Object Placement

How an object authored in a map JSON becomes a body on the ground and a model on
the screen, and what has to agree for the two to land in the same place.

## The coordinate contract

Map JSON is authored in **grid coordinates**: `x` and `z` run from `0` to
`grid.width` / `grid.height`, with `grid.tile_size` metres per cell. Everything the
engine reasons about is in **world coordinates**, centred on the map:

```
world = (grid - (size * 0.5 - 0.5)) * tile_size
```

The `- 0.5` is the half-cell that puts a grid coordinate at the _centre_ of its
cell rather than its corner. It is not optional and it is not cosmetic: dropping
it slides an object half a tile in both axes, which at `tile_size 1.0` is 0.71 m
of diagonal drift away from the terrain, roads and buildings it was authored
against.

Three places own this conversion, and they must stay in step:

| What                                        | Where                                                                                                       |
| ------------------------------------------- | ----------------------------------------------------------------------------------------------------------- |
| Structures, spawns, roads, rivers, wildlife | `authored_position()` in `game/map/map_loader.cpp` — converted **at load**                                  |
| World props                                 | `TerrainService::world_prop_world_xz()` — converted **at use**, props stay in grid space in `MapDefinition` |
| Scatter (`SpawnValidator`)                  | `grid_to_world()` in `game/map/scatter/spawn_validator.cpp`                                                 |

World props are the odd one out: they are the only authored objects kept in grid
space after load, so every consumer has to convert them itself. Ten prop
renderers each rolled their own conversion and all ten dropped the half-cell
term, which drew every tent, ruin, statue, cart, rack, camp, shrine, dead tree,
abandoned home and plant half a tile off the map they stood on. Trees, boulders
and iron ore were correct because they went through the shared helper. **Call
`world_prop_world_xz()` or `world_prop_world_position()`; never open-code the
arithmetic.**

`WorldPropClearanceIndex` is space-agnostic — it answers in whatever space it was
built in. `shared_world_prop_clearance_index()` converts to world space on
rebuild, because scatter queries it in world space.

## Capturable props

The magic shrine and the cursed gold vein are props with a capturable Barracks
entity raised on top of them at runtime (`UndeadAwakeningSystem`,
`CursedGoldVeinSystem`). The entity uses the barracks collision body, which is
larger than the prop's own footprint, so leave a few metres of clear ground around
either. See `docs/IRON_SEPULCHER.md` and `docs/CURSED_GOLD_VEIN.md`.

## Bodies

A prop's ground body mirrors `world_prop_ground_half_extents()` in
`game/map/map_definition.h`; a building's mirrors
`BuildingCollisionRegistry`. `tests/render/prop_model_footprint_test.cpp`
measures the models and fails if the declared extents drift from them.

Canopy trees are the exception. A pine blocks a 0.22-fraction stem so a wood
stays walkable, but it _draws_ a crown out to its full model extent times its
render scale — nine metres across for a pine at scale 1. Scatter, grass and
soldiers belong under that crown. Nothing built does.

## `scripts/fix-map-prop-overlaps.py`

Audits and repairs authored placement. Six defect kinds, all of which must be
at zero:

| Kind      | Meaning                                                   |
| --------- | --------------------------------------------------------- |
| `overlap` | two solid bodies intersect                                |
| `canopy`  | a tree's crown has swallowed something built              |
| `road`    | a body stands in a road or bridge corridor                |
| `water`   | a body stands in a river or a lake                        |
| `slope`   | the ground breaks under a body, so its high corner floats |
| `ramp`    | a body stands in a hill entrance                          |

```sh
python3 scripts/fix-map-prop-overlaps.py --check     # report, exit non-zero
python3 scripts/fix-map-prop-overlaps.py             # repair in place
```

### The ground is measured, not modelled

`slope` and `ramp` are the two kinds that cannot be derived from the map JSON,
and for a long time the script tried anyway. It rastered each hill as the
ellipse it was authored as and asked whether a body straddled that boundary.
The engine builds something else:

- `Landform::sample_hill` warps the boundary with fbm and roughens it by up to
  `+-roughness` of the radius — 34% on a campaign-scale map — then smooth-unions
  an off-centre lobe into it. Two hills with the same authored radius break the
  ground in different places.
- At campaign scale (`is_campaign_landform_scale`, grid >= 128, which is every
  shipped map but three) a round hill is widened by `k_campaign_hill_width_scale`
  and rotated by a hash of its own grid position.
- A mountain's footprint is not its radius at all: `mountain_footprint_cells`
  makes it `max(1.38r, r + 6)` cells along the ridge and `max(0.55r, 5)` across.
- Every hill is then cut open by ramp corridors. `hill_entry_half_width_cells`
  starts at 7.25 cells of half width on a campaign map, the mouth flares wider
  still, and the corridor runs from the crown out past the foot of the hill —
  ground that is re-graded, is the only walkable way up, and appears nowhere in
  the JSON.

The authored-ellipse model saw none of that and reported **zero** slope defects
across `assets/maps` while the built terrain carried 451, including a tent with
8.2 m of ground break under its own footprint and 144 bodies standing in a hill
gateway.

So the audit asks the engine instead. `tools/terrain_probe` loads a map, builds
the heightfield with the same calls `TerrainService::initialize` makes, and
dumps the height plane and the hill-entrance mask; `scripts/map_surface_field.py`
reads them back and measures each body's own footprint against them.

```sh
cmake --build build --target terrain_probe -j4
python3 scripts/fix-map-prop-overlaps.py --check --surface require
```

`--surface require` fails if the probe is missing. The default, `auto`, warns
and falls back to the ellipse model so a checkout with no build directory still
runs the other four checks — but a run that has fallen back is not a slope
check, and the warning says so. `--surface off` skips the probe entirely.

`--ground-relief` (default 0.35 m) is how far the ground may break under one
body. A model settles on the lowest ground its footprint spans, so it is also
how far the high corner floats. Across the shipped maps relief is a continuum up
to about a quarter of a metre — ordinary gentle ground — and then flattens into
a separate population that barely thins between 0.30 m and 0.50 m; 0.35 sits in
the gap. `--ramp-coverage` (default 0.15) is the share of a footprint that may
stand in a gateway; a tent beside a ramp with one corner over it is scenery, a
tent with a sixth of itself on the ramp is in the way.

A canopy tree is measured across its trunk, not its crown. A pine on a hillside
is scenery; the `canopy` check is what keeps crowns off the tents.

Probe dumps are cached under the system temporary directory, keyed by map name
and invalidated by the mtime of either the map or the probe binary. `--surface-cache`
puts them somewhere else.

Repairs push the _lower priority_ object out: a tent that overlaps a wall moves,
the wall does not. Wall runs and anchor buildings (barracks, temple,
marketplace, farm) never move for each other; a road routed through an anchor is
trimmed back to the doorway instead.

`--canopy-overhang` (default `0.35`) sets how far a crown may lean over
something built, as a share of the crown's own reach along that bearing. It
scales with the tree, so a cypress may clip what a pine would swallow whole.

`--max-travel` (default 10 m) caps how far a prop may be nudged. A tree wedged
inside a walled camp can need more; raise it for that one map rather than for
every map.

### What the script cannot see

It audits authored JSON only. Procedural scatter — stones, grass, plants and the
trees that fill a `forests` entry — is generated at load and never written back,
so it is kept clear of authored props by `SpawnValidator` at runtime, not by this
script. A stone standing in a ruin is a scatter-clearance bug, not a map bug.

It also cannot move what a designer pinned. An anchor building steps aside for a
road or for water and for nothing else (`may_step_aside`), so a marketplace built
into a hillside is reported and left alone: a hall on broken ground is a decision
to revisit by hand, not something to nudge four metres across its own plaza.
