# Baked Meshes

Every mesh the game assembles out of primitive parts (unit spheres, cylinders, cones and boxes placed by C++ code) is assembled **once, at build time**, and merged into a single mesh. The running game loads the merged result; it never builds a procedural mesh out of parts, and it never keeps the parts around.

This document explains why, how the bake is organised, what keeps a new building or piece of equipment from slipping past it, and how geometry is tessellated.

## Why

Profiling real battles (Siege of Aurelia Magna, four-AI skirmishes; see [FRAME_PACING.md](FRAME_PACING.md)) showed procedural assembly costing presentation time in three ways:

- **First-use spikes.** A building's merged mesh was built the first time the building was drawn. On Aurelia Magna that put a 538 ms stall on the first playable frame.
- **Mid-battle creation.** Equipment and creature meshes were merged and uploaded lazily, so new troops or wildlife arriving in view created GL buffers during play (over 400 in a 40-second AI match).
- **Per-part submission.** Anything not merged went to the GPU one primitive at a time.

Baking at build time removes all three: loading reads finished meshes, the loading screen uploads them, and gameplay never creates geometry.

## The bake

`tools/bpat_baker` runs as part of the build (`bake_creature_assets`; the game and the test binaries depend on it). It links the renderer, so it runs exactly the C++ that describes each mesh. It writes:

| File                                | Contents                                                                          |
| ----------------------------------- | --------------------------------------------------------------------------------- |
| `assets/creatures/*.bpat`, `*.bprm` | creature animation and rigged bodies (see `CREATURE_BPAT_FORMAT.md`)              |
| `assets/meshes/buildings.smpk`      | every catalog building in every damage state, merged into one static mesh         |
| `assets/meshes/attachments.rmpk`    | every troop's equipment set (helmets, armour, weapons, cloaks), merged and rigged |

The packs are build outputs: gitignored, regenerated whenever the renderer changes, copied into the source tree so tests running from the repository root find them.

Both packs share one container, `render/mesh_pack_format.h`: named entries of one vertex type, with indices, optional index ranges and a bounding sphere. Each family has its own magic number and the header records the vertex size, so a pack can never be read as the wrong kind of mesh or by a build with a different vertex layout.

## Buildings

`building_archetype_catalog()` lists every building: each nation's home, barracks, tower, marketplace, temple, five farm stages, six wall variants and the gate. It is the **only** way to get a building:

- The baker merges each catalog entry in each `BuildingState` and writes it as `"<name>/<state>"` (for example `roman_home/damaged`).
- At runtime, `building_archetype_set(name)` (`render/entity/building_archetype_library.*`) returns archetypes that hold only the baked mesh. Renderers ask for buildings by catalog name; nothing in the game builds a building's parts.
- `GameEngine::prewarm_overlay_gpu_resources` uploads the baked meshes of every building the match uses while the loading screen is up.
- Translucency (a construction placement ghost, a fade) is screen-door coverage on the same baked mesh, carried per instance; ghosts cast no shadow.
- Tools and tests that inspect building geometry (the building preview, silhouette and seam tests) get the source parts through `building_source_parts()`; the default `ISubmitter::render_instance` expands baked buildings that way. The game's `Renderer` never does.

`tests/architecture/humanoid_layering_test.cpp` (`StaticMeshBakeBoundary`) fails if anything outside the library builds a building archetype or merges a static mesh. `tests/render/static_mesh_pack_test.cpp` fails if a catalog building is missing from the pack, and (extended lane) if the pack differs from a fresh merge.

## Equipment

Troop equipment is merged per **attachment set**: the full list of pieces a creature archetype carries. Which sets exist is decided by the renderers, so the baker discovers them the way the game does: it builds a world with one unit of every troop type for every nation, runs the game's template prewarm on a renderer with no GL context, and writes every set the prewarm merged.

Sets are keyed by `"<species>/<lod>/<skin>/<creature archetype name>"`, for example `humanoid/0/3/troops/roman/swordsman_short_beard`. The key is a name, never an address or a hash of computed floats: the release build uses `-ffast-math`, which lets two executables compute the same transform one ulp apart, so float hashes do not survive from the baker into the game.

At runtime `RiggedMeshCache::create_rigged_asset` takes a named set from `AttachmentMeshLibrary` before it would merge one itself, and counts the sets it had to merge (`runtime_attachment_bakes()`). The tests require that count to be zero for the prewarm, and (extended lane) compare a fresh merge of every set with the pack.

### Equipment caches key on exact inputs

Generated equipment is cached by the values it is built from (`render/equipment/equipment_cache_key.h`). Those keys must be the exact inputs. A rounded key hands every later caller the geometry built from the first caller's values, so what is drawn, and what is baked, would depend on the order things happened to be built in. That made the baker and the game disagree about cloaks, sashes and crests until the keys were made exact.

## Tessellation

Primitives are tessellated to one **geometric accuracy**, not one segment count (`bake_tessellated_mesh`, `render/gl/primitives.*`). Each unit sphere, cylinder, cone, capsule and tapered cylinder gets the fewest segments that keep every chord within `k_bake_chord_tolerance` (1.5 mm) of the true surface at its baked size, and never more segments than its author asked for. Segment counts are rounded up to multiples of four, and sphere latitude bands kept even, so each primitive keeps a vertex at both ends of every axis: its extents, and everything measured from them, are exact.

The result is the same mesh at every distance; this is not a level of detail. Large curved surfaces keep their smoothness, while a two-centimetre crest tuft stops costing a thousand triangles. Measured effect: the humanoid body went from 27,520 to 12,066 triangles; the building pack from 4.85 M to 1.34 M triangles; rigged triangles per frame on Aurelia Magna from 18.8 M to 10.4 M.

## Adding content

- **A new building:** add its descriptor to `building_archetype_catalog()` and fetch it with `building_archetype_set("<name>")`. It is baked on the next build; there is no other way to draw it.
- **New equipment or a new troop:** register it as usual. The prewarm that the baker runs picks it up; the coverage test fails if the game would have to merge it at runtime.
- **A new procedural mesh family:** give it an entry type in `mesh_pack_format.h`, a writer in `tools/bpat_baker`, a runtime library that only loads, and a test that fails when the runtime would assemble it itself.
