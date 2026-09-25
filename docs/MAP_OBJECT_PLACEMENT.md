# Map Object Placement

An object authored in map JSON has to become two things that agree with each other: a physical body on the ground and a model on the screen. This document explains the coordinate contract that keeps them aligned, how object footprints are validated, and how the placement audit detects geometry that intersects roads, water, steep terrain, or other authored objects.

## Dressing a repair is allowed to delete

`--drop-unplaceable` deletes world props and firecamps that no push could place. It never touches a structure or a spawn: a building is a decision, a spawn is a seat in the match, and a wall is geometry a settlement depends on.

It exists for generated content. `map_aurelia_magna.json` carried 1,454 defects and every push the ladder could make left 1,327 of them standing, because a planned city has no slack to push dressing into; 1,556 of the bodies named in those defects were world props — plants on broken ground, tents inside one another, trees over the forum. Deleting a body can only remove defects, never create one, so the pass runs after the pushes and takes the lower-priority side of each pair that is still in conflict. On that map it drops 831 of 2,074 props and brings 1,454 defects down to 145.

The 145 that survive are the generator's to fix, not the repair's: 129 fill buildings on ground that breaks under them, 14 spawns standing in water, 6 structures in a road and 5 structure-on-spawn overlaps, none of which has settled ground within 8 m to move to. `tools/city_export` checks placements with `stands_on_ground`, which accepts any walkable cell within six tiles of an object's centre — that is why a spawn can sit in a river and a house can straddle a break. A city planner that consulted the heightfield across each footprint, the water it writes and the roads it lays would not need this pass at all.

## The goods yard beside a building

A barracks draws a goods yard beside itself: `k_stockpile_center_x` in `game/systems/resource_stockpile.h` puts it 5.20 m along the building's own x with half extents 1.45 x 2.10, and `render/entity/barracks_stockpile.cpp` lays the crib and the wood, stone and iron bays out from there. The yard reaches 6.65 m while the barracks body itself stops at 4.325, so its last 2.3 m is ground that carries no object in the map file and was invisible to the audit. On `map_pinewater_cut` a firecamp stood inside the timber camp's yard and nothing reported it; once the yard was modelled, 33 defects of that class appeared across eleven maps, on maps that had audited clean for months.

`scripts/fix-map-prop-overlaps.py` derives the yard as a body keyed `structures[N].stockpile`, and two rules keep it honest:

- A body is exempt from anything derived from it, so a barracks does not overlap its own yard.
- A yard is measured against props, firecamps and spawns — the things that would visibly stand in it — and not against other buildings. A neighbouring house or a rampart shoulder touching the yard is a layout decision a settlement planner already took; a campfire inside the yard is the immersion break this audit exists to catch. Without that scope the audit reported a wall run and a settlement home as defects that nothing was allowed to move.

When the lower-priority body of an overlapping pair has nowhere legal to go, the repair now asks the other one to yield before giving up. Player 5's camp on `map_amber_delta` is why: a tent was pinned between a firecamp and a yard on ground where no legal spot existed within twelve metres, and the firecamp could step aside in one push. The tent itself was cleared by moving the barracks 1.5 m west, which is the authored fix — the yard, not the tent, was in the wrong place.

## The coordinate contract

Map JSON is authored in **grid coordinates**. `x` and `z` range from `0` to `grid.width` / `grid.height`, with `grid.tile_size` metres represented by each cell.

The rest of the engine reasons in **world coordinates**, centred on the map. The conversion is:

```text
world = (grid - (size × 0.5 - 0.5)) × tile_size
```

The `- 0.5` term is the half-cell offset that places a grid coordinate at the **centre** of its cell rather than at a corner. It is part of the coordinate contract, not a visual adjustment. Omitting it moves an object half a tile on both axes; with `tile_size: 1.0`, that is roughly 0.71 m of diagonal drift away from the terrain, roads, and buildings against which the object was authored.

Three code paths perform this conversion and must remain consistent:

| Object family                               | Conversion path                                                                                      |
| ------------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| Structures, spawns, roads, rivers, wildlife | `authored_position()` in `game/map/map_loader.cpp`; converted **during load**                        |
| World props                                 | `TerrainService::world_prop_world_xz()`; converted **when used**, because props remain in grid space |
| Scatter through `SpawnValidator`            | `grid_to_world()` in `game/map/scatter/spawn_validator.cpp`                                          |

### World props are the exception

World props are the only authored objects that remain in grid space after map loading, so every consumer must convert them before using their position.

This distinction has caused real rendering bugs. Ten prop renderers once implemented the conversion independently and omitted the half-cell term. Tents, ruins, statues, carts, racks, camps, shrines, dead trees, abandoned homes, and plants were all rendered half a tile away from their authored position. Trees, boulders, and iron ore remained correct because they used the shared helper.

The rule is therefore explicit:

> Use `world_prop_world_xz()` or `world_prop_world_position()`. Never duplicate the conversion arithmetic.

`WorldPropClearanceIndex` itself is space-agnostic: it answers queries in whichever coordinate space was used to construct it. `shared_world_prop_clearance_index()` converts positions to world space during rebuild because scatter performs its queries in world space.

## Capturable props

The magic shrine and cursed gold vein are world props that receive a capturable `Barracks` entity at runtime through `UndeadAwakeningSystem` and `CursedGoldVeinSystem`.

The capturable entity uses the barracks collision body, which is larger than the visible prop footprint. Map authors should therefore leave several metres of clear ground around either object. See [IRON_SEPULCHER.md](IRON_SEPULCHER.md) and [CURSED_GOLD_VEIN.md](CURSED_GOLD_VEIN.md).

## Physical footprints

A world prop's ground body follows `world_prop_ground_half_extents()` in `game/map/map_definition.h`. Building bodies follow `BuildingCollisionRegistry`.

`tests/render/prop_model_footprint_test.cpp` measures rendered models and fails when the declared physical extents drift away from the geometry they are meant to represent.

### Tree canopies are intentionally different

Canopy trees separate blocking footprint from visual footprint. A pine blocks only a `0.22`-fraction stem so a forest remains walkable, but its crown renders out to the full model extent multiplied by render scale—about nine metres across for a scale-1 pine.

Grass, scatter, and soldiers may exist beneath the crown. Built objects may not. The audit therefore treats trunk collision and canopy clearance as separate concerns.

## Auditing authored placement

`scripts/fix-map-prop-overlaps.py` checks and repairs map-authored placement. A clean map has zero defects in all six categories:

| Kind      | Meaning                                              |
| --------- | ---------------------------------------------------- |
| `overlap` | two solid bodies intersect                           |
| `canopy`  | a tree crown covers something built                  |
| `road`    | a body occupies a road or bridge corridor            |
| `water`   | a body stands in a river or lake                     |
| `slope`   | ground relief under a body makes a high corner float |
| `ramp`    | a body blocks a hill entrance                        |

Run it in reporting mode or allow it to repair movable objects in place:

```sh
python3 scripts/fix-map-prop-overlaps.py --check
python3 scripts/fix-map-prop-overlaps.py
```

The `--check` form exits non-zero when defects are found.

## Ground shape must be measured from the engine

Four defect types—body overlap, canopy overlap, roads, and water—can be derived directly from authored map data. `slope` and `ramp` cannot.

Earlier versions of the script approximated each hill as its authored ellipse. The runtime terrain is considerably more complex:

- `Landform::sample_hill` warps boundaries with FBM, roughens them by as much as `+-roughness` of the radius, and smooth-unions an off-centre lobe into the result;
- at campaign scale (`is_campaign_landform_scale`, grid >= 128), round hills are widened by `k_campaign_hill_width_scale` and rotated from a hash of their grid position;
- mountain footprints come from `mountain_footprint_cells`, using `max(1.38r, r + 6)` cells along the ridge and `max(0.55r, 5)` across rather than the nominal radius; and
- hill entrances cut regraded ramp corridors from the crown beyond the foot of the hill, with `hill_entry_half_width_cells` starting at 7.25 cells of half-width on a campaign map and flaring further at the mouth.

Those effects mean that two hills with the same authored radius can break the ground in different places.

The old ellipse approximation reported **zero** slope defects across `assets/maps`. The built terrain contained 451, including a tent with 8.2 m of relief under its footprint and 144 bodies occupying hill gateways.

### Terrain probe

The audit now asks the engine for the surface it actually builds.

`tools/terrain_probe` loads a map, constructs the same heightfield used by `TerrainService::initialize`, and exports both the height plane and hill-entrance mask. `scripts/map_surface_field.py` reads those results and measures each object's real footprint against them.

Build the probe and require engine-backed surface data with:

```sh
cmake --build build --target terrain_probe -j4
python3 scripts/fix-map-prop-overlaps.py --check --surface require
```

`--surface require` fails when the probe is unavailable.

The default, `--surface auto`, warns and falls back to the old ellipse approximation so a checkout without a build directory can still run the other four checks. A fallback run is **not** a valid slope audit, and the warning states that explicitly.

`--surface off` disables the probe entirely.

## Surface thresholds

### Ground relief

`--ground-relief`, default `0.35` m, defines how much height variation may exist beneath one object's footprint. A model settles on the lowest terrain spanned by its footprint, so the same value also describes how far its highest corner can appear to float.

Across shipped maps, normal gentle terrain produces a continuum up to roughly a quarter metre. A separate population of visibly broken placements appears between roughly 0.30 m and 0.50 m. The default of `0.35` m sits in that gap.

### Ramp coverage

`--ramp-coverage`, default `0.15`, defines how much of an object's footprint may overlap a hill gateway. A tent placed beside a ramp can have one corner over the corridor and still read as scenery; a tent with roughly one sixth of its body in the corridor blocks the route.

Tree slope checks use the trunk footprint rather than the crown. A pine on a hillside can therefore remain valid scenery, while the independent `canopy` check keeps its crown away from buildings.

## Surface-probe caching

Probe dumps are cached under the system temporary directory. Cache keys include the map name, and entries are invalidated when either the map or the probe binary changes modification time.

Use `--surface-cache` to place those dumps somewhere else.

## How automatic repairs choose what moves

Repairs move the **lower-priority** object. If a tent intersects a wall, the tent moves; the wall does not.

Wall runs and anchor buildings such as barracks, temples, marketplaces, and farms do not move for one another. When a road intersects an anchor, the road is trimmed back to the doorway instead.

`--canopy-overhang`, default `0.35`, controls how far a tree crown may extend over something built as a fraction of the crown's reach along that bearing. Because the threshold scales with the tree, a narrow cypress may be allowed to clip a space that a large pine would dominate.

`--max-travel`, default 10 m, caps how far a prop may be nudged during repair. If a particular tree is trapped inside a walled camp and requires a larger move, raise the limit for that map rather than weakening the global default.

## What the audit deliberately cannot fix

The script inspects authored JSON only. Procedural scatter—stones, grass, plants, and trees generated from a `forests` entry—is created during loading and never written back to the map. Runtime `SpawnValidator` is responsible for keeping that scatter clear of authored props.

A procedural stone inside a ruin is therefore a scatter-clearance bug, not an authored-map bug.

Ground that must stay open for building - a camp floor where a player lays
farms - is authored as a `flat` with `"fields": true`. Generated scatter is kept
off its level core (the inner `1 - taper` of the ellipse, grown by each prop's
ground radius), so the field is not refused by a pine the scatter pass happened
to drop on it. The core is one mask, `TerrainHeightMap::is_fields`, and the
forest pass and the navigation grid's forest cells both read it too: fields laid
inside a wood are a clearing, not tilled ground that still refuses cavalry.

The script also refuses to move objects a designer has effectively pinned. An anchor building may step aside for a road or water, but not for other geometry. If a marketplace is authored into a hillside, the tool reports it and leaves it in place. A major building on broken ground is a design decision to revisit manually, not something an automated repair should slide several metres across its plaza.

The central placement rule is simple: authored positions, physical footprints, rendered geometry, and built terrain must all describe the same world. The shared coordinate helpers and terrain-backed audit exist to keep those representations from drifting apart.
