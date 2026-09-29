# map_loader split (WP-F, issue #1559)

`MapLoader::load_from_json_file` (257 lines) is now a 30-line orchestrator in
`game/map/map_loader.cpp`: read file -> reject retired keys -> read sections ->
return. The section readers moved to three units that share one internal header,
`map_loader_internal.h` (namespace `Game::Map::loader_detail`, not a public API).

| Unit                      | Owns                                                                                                                                               |
| ------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| `map_loader_env.cpp`      | header (name, coordinate system, troop cap, grid, camera), biome, victory, rain, time of day, environment, starting resources, `authored_position` |
| `map_loader_entities.cpp` | spawns, structures, world props, fire camps, undead zones and their fog, forests, fog zones, wildlife                                              |
| `map_loader_terrain.cpp`  | terrain features (hills, entrances, mask cells), forest terrain, rivers, lakes, roads, bridges                                                     |

## Ordering that must not change

The section order in the orchestrator is the original order of the monolith:
header, units (spawns, structures), scenery (props, zones, forests), terrain
(features, lake lifting, forest terrain, rivers, lakes, river trim, roads,
bridges fitted to riverbanks), fog (fog zones, then undead-zone fog), biome,
victory, rain, authored wildlife, time of day and environment, resources, and last the
wildlife defaults and missing spawn areas, because those read the finished map.
The error paths keep their original wording: unopenable file, JSON parse error,
non-object root, retired `buildings`/`walls`, invalid grid, structure listed in
`spawns`, non-array `structures`, structure geometry errors.

## Split giant functions

`read_terrain` (213 lines) is split into scalar fields, hill fields (mask cells,
shape points) and entrance sampling (grid and world variants). `read_biome`
(187 lines) keeps the ground-type/seed prologue and delegates to grass, colour and
terrain-shading, scatter and surface sections. All arithmetic expressions were
kept verbatim (float and double mixing matters to the digests).

## Verification

A scratch tool loaded every map in `assets/maps` plus six malformed inputs and
hashed the resulting `MapDefinition` field by field. The old loader (compiled
with the library's flags) and the new one give identical digests. Wildlife
spawn-area coordinates are compared at 0.01 m: the unchanged wildlife placement
code is inlined differently under LTO and differs in the last float bit
otherwise.
