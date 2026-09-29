# Terrain split (WP-F, #1559)

`game/map/terrain.h` was 1,226 lines of enums, biome tables, feature value types, bridge geometry
and the `TerrainHeightMap` class; `terrain.cpp` was 1,913 lines and one function,
`build_from_features`, was 840 of them.

## Headers

| Header               | Holds                                                                                                                                     | Depends on                                                                    |
| -------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------- |
| `ground_type.h`      | `TerrainType`, `GroundType`, string codecs                                                                                                | none                                                                          |
| `biome_settings.h`   | `BiomeSettings`, surface/scatter/climate/wind profiles, scatter rules, `apply_ground_type_defaults` (a data table; kept whole on purpose) | `ground_type.h`                                                               |
| `terrain_features.h` | `TerrainFeature`, `HillNavigation`, `RiverSegment`, `Lake` and lake geometry, `RoadSegment`, road-surface constants                       | `ground_type.h`, `hill_shape.h`                                               |
| `bridge_geometry.h`  | `Bridge`, river reach and bank widths, span fitting, deck height and arch curve                                                           | `terrain_features.h`, `river_ribbon.h`                                        |
| `terrain.h`          | `TerrainField`, `TerrainHeightMap` only                                                                                                   | the three above; forward-declares `BiomeSettings` and `TerrainSurfaceProfile` |

There is no umbrella header. Every includer was migrated to the narrowest set of headers whose
symbols it names. A file that uses `TerrainHeightMap` keeps `terrain.h` (which brings the ground,
feature and bridge types its public API mentions) and adds `biome_settings.h` only if it names
biome types. `terrain.h` deliberately does not include `biome_settings.h`.

## Sources

| Unit                        | Responsibility                                                                                               |
| --------------------------- | ------------------------------------------------------------------------------------------------------------ |
| `terrain.cpp`               | `TerrainField`, constructor, height/walkability/type/field queries, `restore_from_data`                      |
| `terrain_build.cpp`         | `build_from_features` orchestration; mountain, forest/river and flat-plateau stamps; erosion settle          |
| `terrain_hill_build.cpp`    | `HillStamp`: footprint, shape, entrance resolution, body raise, entrance clustering, walkable trim and flood |
| `terrain_hill_ramp.cpp`     | `HillStamp` ramp phases: axis, layout, per-cell sampling and application                                     |
| `terrain_hill_stamp.h`      | `HillStamp` declaration (internal to the build units)                                                        |
| `terrain_hill_nav.cpp`      | hill entrance queries and `hill_navigation()`                                                                |
| `terrain_biome.cpp`         | `apply_biome_variation` (ground irregularity, legacy height noise)                                           |
| `terrain_water_bridges.cpp` | rivers, lakes, bridges, water-blocked and bridge precompute, bridge queries                                  |
| `terrain_build_internal.h`  | `GridLayers` (references to the height map's arrays), `BuildContext`, `FeatureFrame`, stamp signatures       |
| `terrain_value_noise.h`     | hash and value noise used by build and biome                                                                 |

`terrain_noise.h` already existed (mountain fbm used by the arena viewport and fog renderer), so
the new helpers live in `terrain_value_noise.h`.

## How `build_from_features` is ordered now

`make_build_context` snapshots the base heights, sizes the erosion layers and resets the grid layers.
For each feature `frame_feature` converts it to grid space, then exactly one stamp runs: mountain,
hill, forest/river, or the flat-plateau default (which also serves lake and flat features, as before).
`settle_erosion` finishes with the walkable protection pass and constrained erosion.

The stamps get their state through `BuildContext`; `GridLayers` holds references to the height map's
own vectors, so the terrain class exposes no accessors and nothing befriends it.

The hill stamp is an object rather than a function because a hill has about twenty derived
quantities (footprint, rotation, crown profile, shape geometry, extents, landform config, masks)
that every phase reads. `HillStamp` is non-copyable because `hill_config.shape` points into its own
`shape_geometry`. Phases are: `place_footprint`, `build_shape`, `measure_extents`,
`configure_landform` (constructor); `resolve_entrances`, `raise_body`, `cluster_entrances`, per
cluster `carve_entrance` (axis, layout, centerline, sculpt), `trim_walkable_to_crown`,
`trim_walkable_edges`, `flood_walkable_from_entrances`.

Two pitfalls hit during the move, both caught before merge: a `const std::vector<float>
ground_before_hill` declared inside a phase shadowed the member and left it empty (segfault in the
ramp), and a hard-coded existing header name (`terrain_noise.h`) was overwritten and restored.

## Behaviour proof

The build is bit-for-bit the same computation. A scratch tool (kept out of the repo) loaded every
shipped map, ran `build_from_features`, rivers, lakes, bridges and biome variation, and dumped every
height, terrain type, entrance, walkable, field, bridge and river flag plus a dense sample of
`get_height_at`, bridge and hill-entrance queries. Original and split sources compiled at `-O2`
without fast-math produced identical dumps for all 19 maps. Under the release flags
(`-ffast-math -O3`) the two differ by at most one to a few float ulps (under 2e-4 after erosion) in
some height cells and never in any discrete value, because reassociation follows function
boundaries; the headless replay digest and the terrain tests are the release-flag check.
