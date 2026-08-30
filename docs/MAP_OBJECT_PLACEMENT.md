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

Audits and repairs authored placement. Five defect kinds, all of which must be
at zero:

| Kind      | Meaning                                                     |
| --------- | ----------------------------------------------------------- |
| `overlap` | two solid bodies intersect                                  |
| `canopy`  | a tree's crown has swallowed something built                |
| `road`    | a body stands in a road or bridge corridor                  |
| `water`   | a body stands in a river or a lake                          |
| `slope`   | a body straddles the rim of a hill, where the ground breaks |

```sh
python3 scripts/fix-map-prop-overlaps.py --check     # report, exit non-zero
python3 scripts/fix-map-prop-overlaps.py             # repair in place
```

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
