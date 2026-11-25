# Rendering Architecture

Standard of Iron renders from published simulation snapshots. Gameplay state belongs to the simulation; the renderer receives a detached view of that state, resolves visibility and presentation, records draw work, and submits the result through a backend boundary.

That ownership model is the central rendering invariant. The renderer can cache meshes, shaders, animation presentation, GPU resources, and frame-local visibility decisions, but it does not become the authority for unit position, ownership, health, combat state, economy, or mission state.

`render_gl` links the simulation kernel (`game_sim`) so it can consume production simulation data. The simulation layer does not link renderer implementation.

## Source layout and layers

`render/` is a stack of folders. A file may include its own folder or any folder below it, never one above, so no two folders include each other. `scripts/check-render-layers.py` enforces this as a PR source-policy gate, and it also fails if a source file appears in the `render/` root. From the bottom up:

| Layer                           | Holds                                                                                                                                                                             |
| ------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `math`, `profiling`, `software` | plain math and hashing, frame and asset counters, the CPU triangle rasteriser                                                                                                     |
| `geom`                          | CPU data a backend uploads, with no GL: `Mesh`, `RiggedMesh`, `InstanceBlock`, `TextureImage`, unit primitives, procedural meshes and tessellation                                |
| `gl`                            | GL handles and wrappers: `Buffer`, `Texture`, `Shader`, `ResourceManager`, and the per-backend tables `MeshGpuTable`, `RiggedMeshGpuTable`, `InstanceBufferTable`, `TextureTable` |
| `frame`                         | what a frame is made of: draw commands, `DrawQueue`, `ISubmitter`/`FrameSubmitter`, `DrawContext`, render archetypes, graphics settings, the pass framework                       |
| `cache`                         | baked static building meshes, the static mesh pack, attachment specs                                                                                                              |
| `backend`                       | `Backend`, the pipelines and command executors, the software backend                                                                                                              |
| `ground`                        | terrain, roads, water, scatter, fog                                                                                                                                               |
| `creature`                      | skeletons, baking, animation, GPU creature assets, and the species (`humanoid`, `horse`, `elephant`, `wildlife`)                                                                  |
| `equipment`                     | armour, weapons, attachments, nation material palettes                                                                                                                            |
| `entity`                        | buildings, units, siege and effects per nation, and the entity renderer registry                                                                                                  |
| `overlay`                       | projectiles, rafts, range rings, order and target markers drawn over the world                                                                                                    |
| `scene`                         | `Renderer`, the scene walk, template prewarm, render bootstrap                                                                                                                    |

The domain layers (`ground` up to `overlay`) draw into a `FrameSubmitter` (`render/frame/frame_submitter.h`), not into the scene `Renderer`. That interface carries what a frame offers: per-entity commands, world surfaces and effects, the camera and visibility, and a `DrawResources` view (`render/frame/draw_resources.h`) of the shared GPU objects a command may point at: the stock quad, ground and unit meshes, the white texture, the banner mesh and shader, the troop shadow shader. The backend owns them and fills the view; `DrawContext::resources` is that view, so no domain layer includes anything from `render/gl` or `render/backend`. `Renderer` is its implementation, so the domain code does not depend on how the scene builds a frame. `DrawContext` (`render/frame/draw_context.h`) and the small value types it needs (`CreatureLOD`, `FacialHairStyle`) live in `frame` for the same reason.

The same gate keeps two more rules. Every layer except `gl`, `backend` and `scene` is GL-free: it includes nothing from `render/gl` or `render/backend` and no Qt OpenGL header. Generic `render/creature` code includes no species folder; it reaches a species through its `SpeciesContract`, and `render/creature/species/` is the one place that names them all. `scripts/check-render-file-size.py` keeps every file in `render/` at or under 1,000 lines; a file may go over only with a stated reason in its allow-list.

Includes into `render/` are always spelled from the repository root (`#include "render/frame/draw_queue.h"`). `render_gl` exports the repository root, not `render/`, as its include directory.

## Color ownership

A nation’s repeating materials live in `render/equipment/material_palette.h`. Infantry, riders and shields draw from the same swatches. In the humanoid renderer, `apply_uniform_palette` blends owner color into dyed cloth, gives leather a small tint, and keeps metal and wood independent of the owner. A unit can select a different dye for its job. Punic spearmen use saffron, while the archer tunic shares its indigo with Punic architecture. Stone, roof construction, heraldry and specialist clothing stay with the nation or role that owns them.

Maps supply grass, soil, rock and snow colors through `BiomeSettings`. The ground, riverbank and road shaders use `biome_palette.glsl` to vary the map’s vegetation color from fresh to cured. Each surface supplies its own coverage and dryness. Apply this treatment to vegetation before laying down exposed soil, paving or snow.

`environment_lighting.glsl` supplies sunlight, sky fill and shadow tint to world materials. It derives daylight from the incoming light and cloud cover. `tonemap.glsl` handles the final display grade. To review a palette change, Arena’s `allied_identity_lineup` groups allied infantry and riders under common lighting, and `lighting_sepulcher_readability` stages the night comparison. Map previews with `--map-preview-content` show authored terrain and settlements together.

## Production frame flow

The normal frame path is:

```text
fixed-step simulation
        │
        │ publishes presentation state
        ▼
World render snapshot
        │
        ▼
QSG / render-thread presentation
GameEngine::update_presentation(dt)
        │
        ▼
Renderer::render_world(...)
        │
        ├─ ensure/acquire published snapshot
        ├─ build camera/visibility state
        ├─ scene walk
        ├─ submit entities/environment/effects
        ▼
DrawQueue
        │
        ├─ sort
        ├─ batch
        ├─ pass/resource preparation
        ▼
IRenderBackend
        ├─ GL::Backend
        └─ SoftwareBackend
```

The frame is therefore separated into three broad layers:

1. simulation publishes what may be rendered;
2. scene traversal turns that snapshot into render commands; and
3. a backend executes those commands.

## Snapshot boundary

`Renderer::render_world()` calls `World::ensure_render_snapshot()` and then `acquire_render_snapshot()`.

If a render snapshot is not available, the renderer returns rather than falling back to mutable live-world entity reads.

That rule matters because a live-world fallback would create two presentation contracts:

- ordinary frames would read a stable published snapshot;
- exceptional frames would inspect mutable simulation state directly.

The current renderer keeps one contract instead: render what the simulation published, or skip the frame.

## Why render snapshots exist

Render snapshots provide several architectural properties at once.

### Stable frame input

The renderer sees a coherent view of the world for the frame rather than a collection of components that may change halfway through traversal.

### Simulation ownership

Gameplay systems remain the only authority for authoritative state. Render code does not need to lock or mutate the ECS simply to draw.

### Disposable presentation caches

Renderer-owned animation state, GPU handles, mesh caches, and pass data can be discarded and rebuilt because the simulation snapshot remains the source input.

### Thread separation

Simulation and presentation can advance at different cadences while exchanging explicit published state.

## Simulation thread and render thread

`GameEngine::simulate(dt)` advances authoritative simulation on the simulation side. Presentation work runs on the render/QSG side.

Presentation-side work includes things such as:

- camera presentation;
- renderer animation time;
- minimap/view-model synchronization;
- visual effect state; and
- actual scene submission/backend playback.

The live-state parts of simulation/presentation coordination use the game-engine frame lock, but the renderer does not keep the mutable world locked throughout the entire draw.

A `WorldFreeze` coordinates destructive world replacement or rebuild operations so simulation and presentation do not continue through a load/reset boundary.

## QSG render-thread stages

`ui/gl_view.cpp` owns the frame callback through `GLView::GLRenderer::render()`, which Qt runs on the QSG render thread with the FBO OpenGL context current. The simulation does not run inside that callback:

1. `GameEngine::simulate(dt)` runs at a fixed cadence on its own `QThread` (`SoISimulation`), started by the first successful `GLRenderer::render()` through `GameEngine::start_simulation_thread()`. `GameEngine::update(dt)` remains as `simulate` plus `update_presentation` for single-threaded callers.
2. `GameEngine::update_presentation(dt)` runs on the render thread at the top of every frame: camera follow, order markers, renderer animation time, visibility, minimap and view-model synchronization.
3. `GameEngine::render(width, height)` configures the per-frame camera copy (`m_render_camera`), submits terrain, and calls `Renderer::render_world(world)` against the published snapshot without holding `GameEngine::m_frame_mutex`. It then takes the frame lock with a bounded wait for the live-state effects pass (`FrameUiCoordinator::render_effects`).
4. `Renderer::end_frame()` sorts the `DrawQueue`, and `Backend::execute(...)` performs OpenGL playback.

`simulate`, `update_presentation`, and GUI-thread input serialise on the frame lock; scene walk and backend playback overlap the simulation tick. Frame phases are logged through `Render::Profiling::global_profile()`, and a GUI handler holding the frame lock must never wait on the render thread.

## Scene walk

`render/scene/scene_walk.cpp` is the main bridge between snapshot data and renderer submission. `Renderer::render_world` there orchestrates the frame; the stages it calls live beside it:

- `render/scene/scene_collect.cpp` - world collection: unit and non-unit render entries, frustum and fog classification, cull radii;
- `render/scene/scene_unit_submit.cpp` - planning and submission: per-unit LOD, batching and preparation plans, prepared and rigged creature submission, non-unit dispatch;
- `render/scene/scene_overlays.cpp` - selection rings, activity indicators and construction previews;
- `render/scene/scene_walk_internal.h` - the per-frame entry and plan types shared by those files.

Creature submission reaches the renderer's mesh caches and shader state through `CreatureSubmissionContext` (`render/creature/pipeline/creature_submission_context.h`), which `ISubmitter::creature_submission()` exposes, rather than by downcasting the sink to `Renderer`. Sinks without a context (tests, capture tools) still submit rigged bodies from a per-thread, CPU-only `detached_rigged_mesh_cache()`; a sink that owns its caches can return its own context instead.

The scene walk is responsible for consuming the published world state and deciding which renderer paths receive which entities/environment elements.

Typical responsibilities include:

- camera/frustum visibility;
- fog/visibility filtering;
- terrain/environment submission;
- entity-type renderer dispatch;
- creature/rigged-body submission;
- structure and world-prop submission;
- effect/light collection; and
- draw-queue population.

Scene walk should answer “what needs to be drawn?” without becoming the backend that knows how an OpenGL draw call is executed.

## Draw queue

Entity and environment renderers submit typed commands into `Render::GL::DrawQueue`.

The draw queue separates scene traversal from backend execution. It can collect work first, then allow ordering, batching, and pass preparation to be performed consistently.

This is preferable to every entity renderer issuing immediate GL calls because it centralizes frame ordering and gives the backend a complete view of the work it must execute.

Conceptually:

```text
entity renderer ─┐
terrain renderer ├──► DrawQueue ─► sort/batch/pass prep ─► backend
VFX renderer     ┤
world props      ┘
```

The queue is frame-local presentation data. It is not persisted and is rebuilt every rendered frame.

## Backend interface

`Render::GL::IRenderBackend` defines the execution boundary:

```cpp
initialize()
begin_frame()
execute(const DrawQueue&, const Camera&)
set_viewport(...)
set_clear_color(...)
set_animation_time(...)
set_frame_budget(...)
```

`RenderBackendFactory::create()` currently selects:

- `SoftwareBackend` when `ShaderQuality::None` is requested;
- the OpenGL `Backend` for the other shader-quality levels.

The scene walk does not need a second gameplay pipeline for software rendering. Both backends consume the same higher-level render submission contract.

## Frame order and pass model

A world frame is built by `Renderer::render_frame(const WorldFrameSources&)`. The host (the game, the arena) supplies the world and two callbacks: `terrain`, which submits the terrain scene, and `overlays`, which submits projectiles, beams and markers. The renderer supplies the order. `build_world_frame_passes()` in `render/scene/world_frame.cpp` is the one place it is written down:

1. **Frame:** `terrain`, `world`, `overlays`, between `begin_frame()` and `end_frame()`.
2. **World stage:** the scene walk, then `primitive_flush` and `construction_previews`. These run while the render snapshot is still locked.

Every stage is an `IFramePass`, run by a `FramePassRunner` over one `Render::Pass::FrameContext`. `tests/render/pass/world_frame_order_test.cpp` pins the order. The commander portrait previews call `render_world()` alone, which is the world stage without terrain or overlays.

The ground renderers that feed the terrain stage are draw sources, not passes. Each one is an `IDrawSource` (`render/frame/i_draw_source.h`), and a scatter species is an `IScatterSource`. They submit commands into a `FrameSubmitter` when the terrain scene asks.

The backend fills the view half of the same `FrameContext` before it executes the draw queue: camera, view, projection, viewport, animation time, light and banner wind. Data that one pass produces and a later pass reads also lives there. For example, the terrain surface writes `frame.surface.ground_cover` and the grass pass reads it.

Each backend pipeline owns its shaders, uniform tables, meshes and scratch buffers, and draws through its own methods (`WaterPipeline::draw_terrain_features`, `TerrainPipeline::draw_grass`, `CylinderPipeline::draw`, ...). Those methods take a `PipelineDrawContext`: the queue, the `FrameContext`, and the backend's `GlStateCache`, which tracks bound shader, bound texture and polygon offset so pipelines skip redundant binds. `Backend::execute_*_commands` only dispatch a command run to the right pipeline. `scripts/check-pipeline-interface.py` (a PR source-policy gate) fails if a pipeline declares a public data member or anything reaches into one through `m_*_pipeline->m_*`.

## Shader quality vs graphics preset

Two related concepts exist and should not be conflated.

### `ShaderQuality`

Backend/shader capability uses:

- `Full`;
- `Reduced`;
- `Minimal`; and
- `None`.

`None` selects the software backend.

### User-facing `GraphicsProfile`

`render/frame/graphics_settings.h` defines the user-facing presets:

- `Low`;
- `Medium`;
- `High`; and
- `Ultra`.

A graphics profile contains many decisions beyond shader quality: LOD/culling, shadow cascades, post effects, batching, weather density, grass density, and resource budgets.

`High` is the current default through `k_default_graphics_quality`.

## Graphics profile contract

A profile controls:

- shader tier;
- creature LOD and cull policy;
- batching policy;
- contact-shadow budget;
- directional-shadow settings;
- post-processing switches;
- weather-particle scale;
- template/prewarm budget; and
- grass density.

`GraphicsSettings::set_quality()` swaps the active immutable profile and increments a generation counter. Consumers apply generation-sensitive changes at controlled boundaries instead of repeatedly rebuilding the profile from independent switches every frame.

## Low profile

Current Low settings include:

- Low shader tier;
- creature LOD/culling enabled;
- 120 m creature cull distance;
- forced batching;
- directional shadows disabled;
- bloom disabled;
- god rays disabled;
- ambient occlusion disabled;
- FXAA disabled;
- weather-particle scale `0.30`; and
- grass density `0.30`.

Low is therefore not merely “High with smaller textures.” It changes several rendering features and content densities together.

### Anti-aliasing

Every frame renders the world into the post-process scene target, a single-sample float texture, and only the final composite reaches the Qt framebuffer. Multisampling that framebuffer therefore smoothed nothing but one fullscreen triangle while Qt resolved it every frame, so the presets carry no MSAA setting; edges are smoothed by the FXAA pass. Multisampled world edges would need a multisampled scene target and a resolve before the post chain.

### Low on an old or software GPU

Low is the preset that has to run on a GL 3.3 Core driver with no 4.x features. That covers old Intel and AMD integrated GPUs, and Mesa llvmpipe.

- **First run picks a preset for the adapter.** If no graphics preset has been saved, `RenderBootstrap::initialize` caps the default by `classify_adapter()` (`render/backend/adapter_class.h`): software renderers (llvmpipe, softpipe, swrast, SwiftShader, Microsoft Basic Render, GDI Generic, Apple Software Renderer) and Intel HD/UHD start on Low; Iris Xe, integrated Arc, AMD APUs and GeForce MX start on Medium; anything else keeps High. Except on macOS, a context below GL 4.3 also means Low. macOS caps OpenGL at 4.1 on every GPU, so the version says nothing about a Mac's speed. `GraphicsSettings::quality_chosen_by_user()` stops a saved choice, or an explicit `--graphics-preset`, from being overridden. `SOI_IGNORE_ADAPTER_CLASS=1` skips the cap for testing.
- **The preset is decided once, on the first launch.** With no saved preset, `RenderBootstrap::initialize` caps the default for the adapter (software and low-end GPUs start on Low), and `GameEngine::remember_first_launch_graphics_quality` saves the result as the player's choice. Nothing changes the preset during play after that; only Settings does. Profiles from older builds that saved `graphics/auto_quality_level` start from that level once and are then locked the same way.
- **Low and Medium draw fewer pixels.** `RenderScaleSettings` sizes the gameplay framebuffer below the view: Low draws at 0.8 per axis and at most about one megapixel, Medium at most about 2.1 megapixels (so a HiDPI laptop panel no longer shades 4K on an integrated GPU). Qt stretches it under the full-resolution HUD; input maps through `ViewportState::map_input`, as on HiDPI.
- **Context.** The entry point still asks for 4.5 Core. On Linux, Qt falls back to what the driver grants, so a 3.3-only Mesa driver still gets a 3.3 Core context. Do not request 3.3 on Linux: the NVIDIA driver then returns exactly 3.3 and the GPU culling path turns off on capable hardware.
- **4.x features stay behind probes.** The only callers of compute, indirect draw, immutable storage and SSBO entry points are `rigged_cull_pipeline.cpp` and `platform_gl.h`, and 4.30 GLSL includes are used only by the optional 4.30 shaders. `scripts/validate_opengl_requirements.py` check 5 enforces both.
- **Cheaper Low tier.**
    - The post-process scene target is packed `R11F_G11F_B10F` when bloom, god rays and FXAA are all off. It keeps HDR range at half the bandwidth of RGBA16F.
    - Local lights are capped at four per pixel, and local specular is skipped.
    - The terrain noise atlas is capped at 2048² and the microdetail texture is 512².
- **Rigged creatures without GL 4.3** draw one command each through `RiggedCharacterPipeline`. Its bone-palette ring has 1024 slots, so it orphans the buffer about twice per frame at two thousand creatures instead of about thirty. When there is no persistent mapping, the fallback streaming ring maps unsynchronised because each slot is already fenced.

To reproduce locally on an NVIDIA machine, force Mesa and cap it at 3.3:

```
__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 \
MESA_GL_VERSION_OVERRIDE=3.3 MESA_GLSL_VERSION_OVERRIDE=330 \
  build/bin/standard_of_iron --renderer-self-test
```

Point `XDG_CONFIG_HOME` at an empty directory to see the first-run Low selection.

## Medium profile

Current Medium settings include:

- Medium shader tier;
- creature LOD/culling enabled;
- 200 m creature cull distance;
- two 1024 directional-shadow cascades;
- bloom enabled;
- ambient occlusion enabled;
- FXAA enabled;
- god rays disabled;
- weather-particle scale `0.60`; and
- grass density `0.65`.

Medium retains the major lighting/post structure but uses a smaller shadow and scene-detail envelope than High.

## High profile

Current High settings include:

- High shader tier;
- creature LOD disabled;
- no practical creature distance culling through `k_never_cull_distance`;
- batching disabled by profile;
- four 4096 directional-shadow cascades out to 200 m;
- bloom enabled;
- god rays enabled;
- ambient occlusion enabled;
- FXAA enabled;
- full weather-particle scale; and
- full grass density.

This is the default full game profile.

## Ultra profile

Ultra keeps the full-detail High envelope and raises the expensive quality tier further:

- Ultra shader tier;
- the same full creature-detail policy;
- the same four-cascade directional-shadow envelope;
- the full post-processing chain; and
- full weather/grass density.

The exact profile tables in `render/frame/graphics_settings.h` are authoritative. Articles should not preserve older preset descriptions when the code table changes.

## Creature rendering

Creatures use baked BPAT animation/body data and renderer-side rig/mesh preparation.

Gameplay and presentation state select the current action/clip/phase. The renderer then evaluates the appropriate rigged/snapshot path for the species and active graphics profile.

The current creature detail model distinguishes full-detail and reduced/snapshot presentation, with a separate culled state when the active profile permits distance culling.

High and Ultra currently disable creature LOD in their profile definition.

Every creature kind takes its LOD distances from one function, `creature_lod_config(CreatureKind)` (`render/creature/pipeline/creature_render_graph.*`), which reads the active profile; a profile that turns creature LOD off therefore turns it off for humanoids, mounts, elephants and wildlife alike. `LodDistanceThresholds` defaults to never lowering detail, so a caller that forgets the settings draws full detail rather than a hidden cutoff. Wildlife used to evaluate a default-constructed config with a 12 m threshold, so on High and Ultra every sheep and wolf beyond 12 m dropped to the Minimal snapshot path and created a mesh for each new animation frame it showed, mid-battle.

The template prewarm also builds every creature species' base body (`prewarm_creature_species`, `render/scene/template_prewarm_runner.cpp`): each registered archetype that carries no equipment gets its rigged mesh and skin-palette buffer at every LOD the profile allows, whatever the match starts with. Ambient wildlife is not on any nation's roster and spawns during play, so without this a wolf at full detail built its mesh and palette buffer inside `render_world` and tripped the runtime-bake guard. The prewarm's troop rule is a single predicate, every troop type except the siege engines (which are not creatures), so a new species is warmed without editing a list. `CreatureSpeciesPrewarm.EverySpeciesBodyIsReadyBeforePlay` pins it.

### Minimal LOD and the prebaked snapshot blob

Species that ship a `*_minimal.bpsm` (horse, elephant, sheep, wolf) and carry no static attachments are marked `requires_prebaked_minimal_snapshot`: at the Minimal LOD they are only ever drawn from that blob, never skinned at runtime, so a distant herd costs one static mesh per body. The blob is baked from **every** clip in the species recipe, so any state can be served from it. The decision lives in `snapshot_mesh_serves_request()` (`render/creature/pipeline/lod_decision.h`): a prebaked Minimal request always takes the snapshot path; only the opt-in runtime-baked snapshot path (`creature_lod.snapshot_meshes`, off in every shipped profile) is restricted to states whose manifest `snapshot` flag is set.

It used to require the `snapshot` flag in both cases, and the clip manifest clears that flag for `Die` because a humanoid's runtime-baked fall would cost a mesh per frame. The prebaked species inherited the exclusion by accident, and the pipeline's answer for a non-snapshot state at a prebaked Minimal LOD was to submit nothing: a sheep or horse dying more than 20 m from the camera (12 m on Low) vanished for the whole fall and reappeared as a settled corpse. Measured in `wildlife_pack_takedown` at `--graphics-quality low`, all 144 `Die` submissions for the two sheep were dropped while the 122 `Dead` submissions were drawn. Humanoids were never affected: they have no `.bpsm`, so their Minimal LOD falls through to the rigged path in both states.

See [CREATURE_BPAT_FORMAT.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/CREATURE_BPAT_FORMAT.md) for the baked format and [HORSE_MODEL_ARCHITECTURE.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/HORSE_MODEL_ARCHITECTURE.md) for mounted creature asset structure.

## Animation ownership

Authoritative gameplay state does not depend on the renderer's interpolation or pose cache.

The simulation determines things such as:

- which gameplay action is active;
- authoritative unit/root transform;
- defensive-layout state;
- combat state; and
- current formation/traversal facts.

The renderer can interpolate pose/facing or preserve presentation animation state between snapshots, but those values remain presentation-owned.

This allows headless simulation and replay verification to remain valid without a renderer.

## Soldier-level anchors

Formation and combat systems can publish soldier-level presentation anchors. The renderer reads those published anchors rather than running its own independent positional simulation for each soldier.

That rule is especially important during traversal and combat, where presentation may need to show soldiers reflowing inside a unit while the authoritative troop root follows navigation.

The formation article documents the shared anchor precedence in detail: [FORMATION_ARCHITECTURE.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/FORMATION_ARCHITECTURE.md).

## Terrain and world geometry

Terrain, scatter, authored props, buildings, walls, and other world geometry feed the same queue/backend model as dynamic entities.

Terrain/world-prop positions are resolved from authored map/grid space through the terrain/map services before rendering. Renderer code should not invent a second coordinate convention for map objects.

See [MAP_OBJECT_PLACEMENT.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/MAP_OBJECT_PLACEMENT.md) for the placement contract.

### Terrain contact

Everything that stands on the ground samples one surface, and it is the surface on screen.

- **One height function.** The terrain mesh splits every grid quad along its
  (x+1, z)-(x, z+1) diagonal, and subdivided quads place their extra vertices on those same
  triangles. `Game::Map::sample_triangulated_height` (`game/map/terrain_surface.h`) returns
  exactly that height. `TerrainHeightMap::get_base_height_at`, `TerrainField`, the scatter
  spawn cache, stone ground fit, riverbank dressing, linear features, roads and the
  `ground_marker` shader (via `texelFetch`) all go through it. A bilinear patch can sit up
  to a quarter of the quad's twist away from the triangles, which is how feet sank and
  props floated on curved ground. `TerrainService::sample_ground_normal` is the smoothed
  up-vector used for anything that tilts.
- **Roads.** The road ribbon is draped over the highest terrain within
  `k_road_surface_envelope_tiles` of each vertex. `TerrainService` samples the same
  envelope for points on a road, so units walk on the paving rather than on the ground
  under it.
- **Structures stay upright and get a footing.** Buildings, walls, towers, construction
  sites and placement ghosts are seated at the height under their centre.
  `render/entity/structure_foundation` measures the drop across the drawn body (the
  `BuildingCollisionRegistry` body table × transform scale) and adds a fieldstone
  perimeter support down to the lowest ground, leaving the center open instead of filling
  the footprint with a slab. Map-object anchors suppress these supports. Completed
  buildings cache the support in `CachedUnitData` and recompute it only when the model
  matrix changes. Ghosts resolve it on the spot, using the same rule the finished
  structure will use.
- **Upright props are bedded, not tilted.** Trees, iron ore and plants sink by
  `slope_bed_depth` (contact radius × tan slope) so the downhill side of the trunk or base
  meets the ground. Rocks keep their tilt-and-sink ground fit (`stone_ground_fit.h`).
  Footprint props (tents, ruins, carts, shrines) sit at the lowest ground under their
  footprint. All of this is resolved when instances are built, never per frame.
- **Things that rest on the ground follow it.** Siege carriages tilt to the slope and each
  crew member stands on their own ground. Horses and elephants pitch along their heading
  (the rider inherits it) but stay upright across the slope. Fallen soldiers ease onto
  the slope as they go down. Selection rings drape over the terrain unless their owner is
  raised well above it (a bridge deck).

`grounding_flat`, `grounding_hill`, `grounding_ridge`, `grounding_riverbank`,
`grounding_road` and `grounding_scatter` put the same cast on each kind of ground for
arena captures. `tests/render/terrain_grounding_test.cpp` pins the surface, road,
foundation and tilt rules.

### Ground plane draws after the terrain

The ground plane (`GroundRenderer`, the map-plus-48-tile skirt at y = -0.08) and the terrain
chunks are both `TerrainSurfaceCmd`s. The plane uses sort key `0x00C0`, after the chunks and
the boundary mountains (`0x0080`). Both are opaque and nothing between them depends on the
plane, so the order is invisible in the image. What it changes is cost: drawn first, every
pixel under the terrain ran `ground_plane.frag` (18 fbm calls) and was then overdrawn;
drawn last, early depth rejects those pixels. GPU timers on Zama Ultra put the terrain
surface pass at 1.78 ms before and 1.23 ms after.

### Baked terrain shader variant

`terrain_chunk.frag` is compiled twice. `terrain_chunk` keeps the procedural fallbacks for
when the per-map noise atlas or microdetail texture is missing (a failed
bake). `terrain_chunk_baked` is compiled with `SOI_TERRAIN_BAKED`, which turns
`HAS_NOISE_ATLAS` and `HAS_MICRODETAIL` into compile-time `true`, so the compiler drops the
fallback code instead of carrying it behind a uniform branch. The executor picks the baked
program per draw when the command's height resources carry all three textures, and
`TerrainPipeline::terrain_uniforms_for()` hands back the matching uniform table; the baked
table resolves every handle as optional because the stripped program no longer has the
fallback-only uniforms. Measured on Zama Ultra: 1.23 ms to 0.97 ms for the terrain pass on
top of the ground-plane change.

### Weapon rack carries a per-vertex surface stream

Most instanced props pick their materials in the shader from the model-space position (bands keyed to where a part happens to sit) or from a length packed into the normal (the tent). The weapon rack (`render/backend/weapon_rack_mesh.cpp`) has too many small, overlapping parts for that to work. Every vertex therefore carries a fourth attribute at location 4, `(material, u, v, seed)`, uploaded by `VegetationPipeline::upload_prop_mesh_with_surface_impl`. Location 4 is free on prop VAOs because the instance stream only binds locations 2 and 3.

- `material` is one of the `WeaponRackMaterial` ids, which match the `k_mat_*` constants in `weapon_rack_instanced.frag`: oak, ash, steel, iron, bronze, leather, yew, linen, shield paint, shield back, bone and feather.
- `u, v` depend on how the part was built:
    - Boxes and beams are projected so that `v` runs along the member. The shader lays grain, forging streaks and wear along it.
    - Swept parts use `u` for the distance around the section (0 to 1) and `v` for the distance along the sweep. Blades and spearheads have `v` normalised from 0 at the base to 1 at the tip, and their four-facet diamond section puts the edges at `cos(u·2π) = ±1`.
    - Shield faces use the flat face coordinates in [-1, 1]. The scutum outline is the superellipse `|s|^2.4 + |t|^2.4 = 1`, and the shader paints its border against that curve. On the parma the coordinates are polar.
- `seed` varies each part. For shield paint it also chooses the design: below 0.5 is a scutum, 0.5 and above is a parma.

The frame parts in `weapon_rack_parts.h` are still the source for `PropModelFootprintTest`. Weapons and shields are built only in the mesh builder and stay inside the declared `{0.88, 0.54}` half extents. The shader builds a height field for each material and turns it into a bump normal from screen-space derivatives. Every derivative is taken outside the material branches, and the bump fades out beyond about 48 m. Lighting is GGX specular plus a sky/ground reflection. Directional shadow blocks sun specular completely but only tints the ambient and diffuse terms, so metal in shadow does not glint.

## Terrain scatter readiness

Terrain scatter has an explicit GPU-readiness state.

Mission startup can keep the loading overlay visible while terrain scatter exists but is not yet ready for presentation. That state is one of the startup readiness gates described in [MISSION_STARTUP.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/MISSION_STARTUP.md).

The startup system therefore distinguishes “simulation world exists” from “the main visual environment is ready enough to reveal the battle.”

## Scatter layers: generate, sync, submit

Every scatter species (grass, stones, plants, the four tree species, fire camps, boulders and the ten world-prop layers) is one layer with three stages.

- **Generate.** A free function in `render/ground/scatter/<species>_scatter.cpp`, declared in `scatter_generators.h`, turns a `ScatterRequest` into the layer's instances: the terrain grid, the ground-ecology field, the biome (whose `seed` drives placement), the terrain service, the world props and, for a worker, the `ScatterWorld` snapshot that replaces every live service. It is GL-free and deterministic. `tests/render/scatter_generator_test.cpp` runs every generator without a GL context and checks determinism, one instance per prop for world-prop layers, seed sensitivity, plant and grass density, and that stones, plants and grass stay off a mountain.
- **Sync.** `FilteredRendererState` chunks the instances, filters them by fog of war and packs each chunk's visible instances into an `InstanceBlock`.
- **Submit.** `submit_visible_chunks` (or `submit_visible_chunks_lod` for grass's screen-size LOD) emits a `TerrainScatterCmd` per visible chunk.

`ScatterRendererBase<Instance, Params>` owns stages two and three for every layer, grass included. A layer only configures its parameters, calls its generator and hands the result to `adopt_generated`.

**Instance budget.** `TerrainScatterManager` gives each layer a budget before configuring it (`scatter_budget.h`). Grass gets 4.4 M blades at full grass density, scaled by the profile's grass density with a floor of 10%: Low 1.32 M, Medium 2.86 M, High and Ultra 4.4 M. Stones get 1 per grid cell, plants and each tree species 4, and every world-prop layer 1. No layer gets fewer than 4,096. These are ceilings, not detail settings, and sit well above what the generators produce. `NoShippedMapReachesAScatterBudget` checks that no map in `assets/maps` reaches one, so the cut never changes a shipped map. A layer that would exceed its budget keeps the first instances in generation order (grass is shuffled first, so it thins evenly) and reports `budget_hit()`. The benchmark JSON's `scatter_memory` reports, per layer and in total, the instances, CPU bytes, reserved GPU bytes, the budget in instances and bytes, and whether it was hit.

**Threads.** At map load, generation runs on the loader thread before the renderer resumes. Every later regeneration runs on a worker.

- **Snapshot.** The live map is written by the simulation without a lock. That covers building footprints (`BuildingCollisionRegistry`), roads, bridges, water and world props (`TerrainService`), and the world-prop clearance index, and `TerrainService` also fills a mutable surface cache from const lookups. So a worker never reads them. `Game::Map::ScatterWorld` (`game/map/scatter/scatter_world.h`) freezes all of it: a `TerrainService::clone()`, a `BuildingFootprintSnapshot`, and the clearance index of that revision. A `ScatterRequest` with `world` set reads only the snapshot. The spawn validator and the composition context take it in place of the live services. Byte-identical output for the same inputs is pinned by `tests/render/scatter_world_test.cpp`.
- **Capture.** `EnvironmentRuntime::sync_scatter_world_props` runs in `GameEngine::update_presentation`, under the frame lock, so the simulation is not writing the map there. It captures at most one snapshot per call, and only when something changed. It schedules the world-prop refresh (`schedule_runtime_world_props_refresh`), the anchored building trees (`schedule_anchored_trees`), and the grass regeneration the render thread asked for (`wants_world_snapshot` / `provide_world_snapshot`).
- **Generate and adopt.** `TerrainScatterManager` asks each affected layer to plan its regeneration. A plan is a `ScatterJob` that copies the layer's inputs (the shared grid, biome, props, ecology, anchored trees) and returns a `ScatterAdoption`. All jobs of one request run in a single `QThreadPool::globalInstance()` task, one task at a time. Newer requests merge and run against the newest snapshot afterwards. The adoption runs at the start of the next `submit` on the render thread, in one swap. Until then every layer draws its previous instances, so nothing pops or empties.
- **Superseded work.** `configure` and `clear` drop a job still in flight; an epoch keeps its result from landing on the new map.
- **Graphics-profile change.** The render thread notices it in `submit`. It cannot read the live map, so it marks the grass pending and waits for the next sync point to provide a snapshot.
- **Synchronous calls.** Callers that own the map can still refresh on the spot: `refresh_runtime_world_props`, `set_anchored_trees` and `refresh_grass` (the arena tool, tests, load). A host that never schedules or provides snapshots keeps the synchronous grass refresh.

## Scatter variation

Every cell scatter already jitters each instance anywhere in its cell, so repetition never came from a grid. It came from the following:

- **Even density.** `ScatterCompositionSample::cluster_bias` is value noise, and its lattice is seeded with the caller's `salt`. The render-only scatters (plants, stones) used to pass `state ^ K`, the per-cell RNG state. That reseeded the lattice on every sample and made the "cluster" field white noise. They now pass one constant salt per species, so cell density and per-instance traits read one coherent patch field. `scatter_patch_multiplier()` turns it into clumps and clearings (0.24 + 3·bias²; the mean stays near the old 1.1). Grass gets its own field, `grass_patch_density()` in `render/ground/scatter/grass_scatter.cpp`, which scales background blades and rejects cluster centres in sparse patches.
- **Gameplay props keep their placement.** Trees, boulders, ore and dead trees are `WorldProp`s that the simulation and save files own, and they still pass `state ^ K`. Giving them the same coherent salt was tried, together with smooth value noise in place of the tree walk's 8/4-tile hash blocks. It turned the densest patches into impassable copses and failed three simulation scenes: `FormationPropClearance.SoldiersMarchAroundSolidProps` (a unit stranded 96 m out), `AiEstateEconomyTest.ItsBuildersAreNeverLeftWithNothingToDo` and `MissionWaveAssaultTest.WaveCannotWalkThroughAnIntactRampart`. All three pass with HEAD placement. Clustering gameplay props has to be tuned against those scenes, not just against the look.
- **One mesh, one shape.** Dead trees and iron ore get seeded non-uniform stretch, cant and (dead trees) twist in their vertex shaders, with normals corrected by the inverse stretch. Pine and cypress decouple height from width. Scatter props have no shadow pass, so the deformation cannot desync a shadow.
- **Identical neighbours.** Tree looks (tint, silhouette, sway phase) used to be hashed from the position rounded to 1 m, so two trees in the same metre were twins. They now hash at ⅛ m through a mixer (`mix_tree_look_hash`).
- **Rows along rivers.** Riverbank dressing used to spawn every 0.8 m at a flat 30% chance. The chance now follows a value-noise field along each bank (`k_bank_clump_frequency`), along-river jitter covers the full step, and props crowd the waterline (`pow(rand, 1.6)`).

## Water

- **River UVs.** `river.vert` read `a_tex_coord` from location 1. `Render::GL::Vertex` puts the normal there, so every river got `tex_coord = (0, 1)`. Shore distance was 0 everywhere, so the depth gradient, shore band and foam never worked and the whole river shaded as shore. It now reads location 2, pinned by `ShaderSource.WaterShadersReadTexCoordsFromTheMeshVertexLayout`.
- **Shallow bed.** Near the shore (`k_shallow_bed_reach`) the water shows the biome's wet sediment through it, and the deep channel is darker. Reflection weight is `0.05 + fresnel·0.34`.
- **Junction caps.** Caps fill the gaps between river segments at bends. Their radial UV makes the whole rim read as shore, so drawn over the ribbon they showed as rings and shards mid-river. River caps now sit `junction_sink` (3 cm) below the ribbon, so the ribbon wins wherever both exist.
- **Wet bank.** `riverbank.frag` darkens a wet band (`k_wet_band_reach`, `k_wet_band_darkening`) and keeps the core opaque over a wider strip, so water meets darker soil instead of dry grass.

### River mist

Valley fog (`valley_fog()`) is a thin bank that gathers in patches: its opacity is capped at `k_ground_fog_max_opacity` and broken up by the square of a drifting noise field, because a uniform veil over all low ground tinted whole river and forest maps teal.

`post_composite.frag` lays water mist over every pixel within `k_mist_bank_reach` (7 m) of a river or lake and below `k_mist_water_ceiling` (1.7 m) above the bank, at up to `k_water_mist_strength` (0.42). The amount is applied at the surface the pixel shows, so it also has to account for how much mist lies between the lens and that surface: it is scaled by `1 - exp(-view_distance / k_mist_view_reach)` with a 9 m reach. Without that, a chase camera standing on a bridge saw the commander and the men beside him, three metres away, through the same milky veil as the far bank.

## Environment lighting

`scene/environment_lighting.h` defines the shared outdoor environment-lighting state.

It carries the common scene inputs used across materials/passes, including values such as:

- sun direction;
- sun colour/intensity;
- sky and ground contribution;
- fog;
- shadow parameters;
- exposure;
- cloud cover; and
- wetness.

Time-of-day and weather systems update this environment state. Rendering consumes the resulting presentation values through shared lighting/shader code rather than letting each material invent an unrelated sun/fog model.

### Shared fill terms

`environment_lighting.glsl` adds two fill terms that every world material picks up through `environment_ambient_light()` and `soi_surface_lighting*()`:

- **Sun bounce.** `environment_sun_bounce()` is sunlight reflected off the sunlit ground: ground-bounce colour × sun colour × sun intensity × sun height × `k_soi_sun_bounce_gain`. It lights faces that point sideways or down, weighted by `1 - hemisphere`, so flat ground is unchanged. Without it the ground bounce was scaled only by the _sky_ ambient intensity (about 0.27 at noon), and every wall, rock, log and trunk facing away from the sun rendered near-black while units beside it, which have their own readable floor in `character_shading.glsl`, stayed bright.
- **Horizon fill.** `environment_ambient_light()` adds sky colour × ambient intensity × `k_soi_horizon_fill_gain`, weighted by `1 - |n.y|`. A wall sees the horizon band, which is the brightest part of a clear sky; the hemisphere term alone gave a sun-away wall about a seventh of its lit side's light, so village facades read black. Flat ground and roofs are unchanged.
- **Canopy scatter.** `soi_canopy_scatter()` is sunlight transmitted through a leaf mass into its shaded side. Tree crowns are volumes, not shells, so the unlit hemisphere of a crown still glows. Olive and pine foliage add it on top of `soi_key_light()`, masked to foliage so trunks do not glow. Pines scale it by `k_needle_scatter` because dense needles transmit far less than olive leaves; at full strength the whole tree went flat lime.

### Display encode

`post_composite.frag` ends with `soi_display_encode()` from `tonemap.glsl`, a `1 / k_soi_display_gamma` power applied after the grade and time-of-day grade. The pipeline has no sRGB framebuffer or encode anywhere, so the tonemapped value used to reach an 8-bit display buffer as-is: daylight battle frames averaged about 65/255 below the horizon and 16% of a backlit frame was crushed below 26/255. The promo edit grade had been compensating with its own gamma and brightness. The encode is deliberately partial (1.2, not 2.2) because every albedo in the game was authored against the unencoded output; `k_soi_grade_saturation` was raised with it to restore the chroma a power curve removes.

Measured on the Cannae spotlight battle (six shots, crushed means pixels below 26/255):

|        | mean luma | crushed, backlit shot | saturation |
| ------ | --------- | --------------------- | ---------- |
| before | 60.8–71.6 | 15.9%                 | 0.37–0.40  |
| after  | 79.3–90.3 | about 2%              | 0.34–0.38  |

Any change to these constants needs the night and dusk check: the `lighting_moonlit_night` batch frame and a 19.5 h shot, compared on mean and standard deviation against HEAD shaders.

### Meadow drift

At gameplay zoom (40–90 m) the terrain's fine detail is damped for readability (`ground_tactical_distance()`), and the regional and patch fields vary over hundreds of metres, so a battlefield read as one flat green. `terrain_chunk.frag` blends the grass between a sun-cured and a deep-sward tone using two extra microdetail samples at about 33 m and 12 m wavelengths (`k_soi_meadow_frequency`), warped by the existing `domain_warp`. It changes colour only, never relief, so troops stay as readable as before.

### Painted sward

Low-contrast noise read as green felt from the play camera, so the grass surface is drawn as overlapping tufts instead. `soi_painted_sward()` in `terrain_chunk.frag` places one fan of four to six blades per jittered cell, dark at the root and light at the tip, all leaning along `k_soi_sward_lean` so the tips point up-screen at the default yaw. A coarse layer (`k_soi_sward_frequency`, about half a metre per tuft) carries the look at gameplay zoom and a fine layer adds blades up close; both fade with the pixel footprint. In the dry, rocky and alpine ground types the same fans become straw tufts over open earth (`steppe`), which is what separates those biomes from the green ones at a glance.

### Rock strata

Steep faces multiply the rock colour by two beds of height bands (`k_soi_strata_frequency` and a finer one at 3.3 times that), each layer with its own tone, a light lip and a dark undercut, wandering with two low-frequency noise samples. A slope that faces away from the sun is already unlit, so it takes only `k_soi_unlit_shadow_share` of the cast shadow; applying the full shadow tint on top of an unlit face turned hillsides into black slabs.

### Rock projection, snow and ground patchwork

- **Triplanar rock.** Rock used to sample its noise at `mix(world_coord, wall_coord, wall_blend)`. Lerping _coordinates_ between two projections smears every pattern into swirls on mid-steep faces. The altitude-keyed strata also drew topographic contour lines across smooth mountain slopes. `sample_rock_pattern()` now samples each projection (top, x-side, z-side) separately and blends the _results_ by `pow(|n|, 8)`. Projections under `k_soi_projection_epsilon` (0.12) are dropped and the rest renormalised, so most pixels pay for one projection. Relief normals use the same weights through `relief_field()`. Strata are warped and only weight steep faces. Weathering streaks run down the fall line. The Voronoi fracture network is damped on gentle rock, where it read as paving.
- **Snow follows the terrain, not the noise.** Mountain snow sheds off slopes steeper than about 0.3–0.5 (`steep_shed`), so faces show rock and ledges hold snow. Alpine ground snow is driven by aspect: lee slopes hold it, sun slopes melt, with hollows and gullies favoured. Noise only ragged-edges it. A thin, dirtier fringe lets grass tufts poke through. Relief normals are damped under snow (`snow_cover`).
- **Ground patchwork at RTS zoom.** About 1.5 m tufts (`k_soi_tuft_*`), whose contrast varies between cropped and tussocky ground, about 6 m sod tone patches (`k_soi_sod_amount`) and bare-earth flecks in tuft gaps (`k_soi_earth_fleck_amount`). All are mipmapped microdetail samples, so they hold at 40–90 m without shimmer. The distance damping of grain and relief was relaxed for the same reason.
- **Mountain uplands.** Mountain-typed surfaces dry toward cured turf with altitude and grow rock outcrops on crests. This is gated by `mountain_surface`, not absolute height, because some maps raise flat-typed ground (Aurelia's dome) or towns.
- **Vegetation chroma ceiling.** `ground_vegetation_chroma()` in `ground_readability.glsl` softly compresses saturation above 0.44, for green-dominant colours only (soils and Copper Canyon's red rock are untouched). Both `terrain_chunk.frag` and `grass_instanced.frag` apply it, so blades still match the ground. It tames the lime presets (`SoilFertile`, `ForestMud`) and map palettes like Ticino's.

Cost, offscreen Crossing the Alps at 720p on the RTX 5060, three interleaved pairs: GPU colour pass p50 went from 3.37 ms to 3.51–3.55 ms. Review these with `arena_app --batch --terrain-map <scratch copy with a close camera>`. Rock, snow and patchwork read clearly at 40–100 m camera distance.

### Curvature without a height or field texture

`terrain_chunk.frag` takes curvature from the baked field texture or the height texture. Meshes with neither (the boundary mountain ring drawn with `horizon_dressing`) fell back to `curvature_from_normal_field()`, the screen-space derivative of interpolated vertex normals. On a coarse mesh that derivative is constant per triangle, so the gully mask shaded each concave triangle as a dark wedge and the ring read as vertical streaks. `k_soi_normal_field_curvature_trust` scales that fallback to zero. Terrain chunks with baked fields are unaffected.

### Building foot

`building_merged.vert` passes each merged building's instance origin (its ground contact) as `v_ground_height`, and `basic_instanced.frag` darkens and warms wall faces within `k_plinth_height` of it, so houses sit in the soil instead of on it. `basic_instanced.vert` draws individual parts whose origin is not the ground, so it passes `k_no_ground_contact` and the effect is off. Buildings outside the static batch (under construction, damaged, or on a backend without it) do not get the foot.

### Opaque and ghost buildings

The static batch draws first so the terrain behind buildings fails the depth test. That only pays if building pixels themselves pass through early depth testing, and a `discard` anywhere in a program turns early testing off for it. Ghost instances (construction previews and fading buildings, `state[2]` coverage above zero) need the Bayer-dither `discard` in `soi_resolve_ghost_alpha`; opaque instances never take that branch. The batch therefore links `basic_instanced.frag` twice: `building_merged` with `SOI_OPAQUE_ONLY` defined, which compiles the dither out, and a ghost program with it. `StaticBuildingBatch::finish_frame` sorts ghosts after the opaque instances and gives them their own draws. On Siege of Aurelia Magna at 1920x1080 Ultra the building pass fell from 5.5 to 3.3 ms of GPU time.

## Directional shadows

Directional shadows are configured by `DirectionalShadowSettings` in the active graphics profile.

Current profile behavior is:

| Profile | Directional shadows                         | Rigged casters per cascade |
| ------- | ------------------------------------------- | -------------------------- |
| Low     | disabled                                    | 60                         |
| Medium  | 2 cascades, 1024 resolution                 | 100                        |
| High    | 4 cascades, 4096 resolution, 200 m envelope | 160                        |
| Ultra   | full High-style cascade envelope            | 240                        |

Cascade fitting, bias, filtering, light-space culling, and sampling are implemented in `render/gl/` and shared shader includes.

`max_rigged_casters_per_cascade` caps how many soldiers, horses and elephants cast into each cascade. For every cascade the pass ranks the rigged commands that fall in that cascade's range by distance to the camera and keeps the nearest ones, across all rigged batches, so the budget is spent on the soldiers that are large on screen; the rest keep only their contact shadow, which is blended against the directional term anyway and reads as the same grounding at that size. A duel or a small fight never reaches the cap, so nothing changes there. On the Sepulcher Bridge's last rising (about 280 soldiers in view at preset High, 1280x720) the unlimited shadow pass cost 4.6 ms average and 7.6 ms at the 95th percentile; at 220 casters it was 3.2 / 3.9, at 120 it was 2.7 / 3.2 and at 60 it was 2.6 / 3.0, where the remaining cost is static casters and terrain. The benchmark report counts the dropped casters in `shadow_rigged_casters_dropped`.

Terrain scatter (trees, plants, rocks and camp props) casts with its own colour shaders, because their vertex stages place and sway each instance. The pass uploads `FrameData` with `u_frame_depth_only` set, and every scatter fragment shader returns through `soi_depth_only_pass()` before its lighting, keeping only the cutouts that shape the shadow: plant leaves and the trunk base of pines and olives. Without that, each 4096² cascade ran the full foliage lighting per shadow texel; on Cannae at 1920x1080 Ultra the scatter shadows cost 4.4 ms of a 5.8 ms shadow pass and 0.8 ms after. Each scatter command also carries its chunk's bounding sphere, so a chunk is drawn only into the cascades whose receivers its shadow can reach; on Crossing the Alps that took scatter shadows from 0.46 to 0.26 ms.

Those implementation sources are the correct place to read exact shadow mechanics. The profile table documents the quality envelope rather than duplicating every internal shadow constant into multiple articles.

## Contact shadows and grounding

Creature/world grounding also uses contact-shadow presentation with its own `ContactShadowBudget`.

This budget is separate from the directional-cascade settings. A profile can therefore reduce or disable the expensive directional shadow path without requiring all local grounding/occlusion cues to disappear in the same way.

The renderer owns these visual grounding effects; the simulation does not need contact-shadow state to decide gameplay collision or visibility.

## Local lights

Entity/effect local lights are collected separately from ordinary mesh commands and constrained by the renderer's local-light budget/fader.

Instanced props that emit light expose those lights through renderer-side submission rather than relying on a later pass to recover emitter positions from opaque GPU instance buffers.

That keeps light ownership explicit at scene-submission time.

## Weather and precipitation

Authored/runtime weather is a gameplay/environment fact; precipitation density is a presentation budget.

`RainPipeline` owns a fixed particle pool. The number/intensity of particles presented depends on current weather state and the active `WeatherBudget::particle_scale` from the graphics profile.

Thus Low/Medium can show the same storm with fewer rendered particles without changing the authored weather itself.

This separation is useful throughout rendering: quality settings change presentation cost, not simulation rules.

## Grass and environmental density

Grass density is likewise part of the graphics profile rather than mission state.

The current profile values are:

- Low: `0.30`;
- Medium: `0.65`;
- High: `1.00`;
- Ultra: `1.00`.

Reducing this density should not alter walkability, cover rules, terrain height, or other simulation facts.

### Ground cover

`BiomeRenderer` fills one instance stream with two kinds of plant, both drawn from the seven-blade tuft mesh built in `TerrainPipeline::initialize_grass_geometry()`:

- grass tufts, a couple per square metre in drifts shaped by `grass_patch_density()`, on flat ground and on hills thinned by slope;
- wildflowers, gathered into drifts and clumps by `scatter_flowers()` from a small palette per ground type (`flower_palette_for()`).

The kind rides in the integer part of the sway strength (`k_instance_kind_stride`; `k_kind_stride` in `grass_instanced.vert`). For a flower the vertex shader folds the same mesh into a stem and a flat rosette of petals, which is the shape a top-down camera actually sees.

The whole stream is capped at `k_grass_instance_budget` scaled by the profile's grass density, so an 800-cell campaign map thins its sward rather than growing its buffers.

Buildings are placed after the grass is generated. The grass shader therefore samples the terrain cover mask (`u_cover_tex`, stashed per frame in `TerrainPipeline::m_frame_ground_cover`) and collapses any tuft or flower that stands on a yard or at a wall foot.

## Entity renderers: authoring, presentation, submission

`render/entity` keeps three jobs in separate files.

- **Authoring** builds a building's geometry into a `BuildingArchetypeDesc`: the palette and the parts, in plain code, with no draw call and no frame state. Each nation building has a `nations/<nation>/<kind>_authoring.{h,cpp}`. Shared parts live in `market_goods`, `farm_field`, `home_yard`, `wall_authoring`, `wall_gate_authoring` and `temple_layout`. A palette or layout constant that the renderer also needs sits in the authoring header's `<kind>_detail` namespace. `building_archetype_catalog.cpp` lists every description, and `building_archetype_library` is the single baker that turns them into archetype meshes.
- **Presentation** turns snapshot state into parameters: hearth, lamps and doorstep visits (`HomeActivity`), farm workers (`farm_activity`), the barracks drill (`barracks_activity`), building decay and collapse. These files take a `World` and time and return values; they never see a submitter. `tests/render/home_activity_test.cpp` and `farm_activity_test.cpp` exercise them without a GL context.
- **Submission** is the per-frame code that reads those outputs and emits commands: `<kind>_renderer.cpp`, `*_activity_submit.cpp`, `combat_dust_renderer.cpp`, `rpg_telegraph_renderer.cpp`.

`EntityRendererRegistry` (`render/entity/registry.h`) ties the three together per kind (`troops/<nation>/<type>`, `wildlife/sheep`, and so on):

- **Descriptions.** Every `BuildingArchetypeCatalogEntry` names the kinds that draw it (`kinds`), and `registry.descriptions(kind)` reads them back from the one catalog the baker uses.
- **Presentation systems.** `EntityPresentationSystem` (`render/entity/entity_presentation.h`) has a name and a `present(ctx, EntityPresentation&)` that writes plain values, with no submitter and no GL. The built-in systems are:
    - `building_state`: the damage state with its hysteresis. Decay colours, torches, smoke and incense follow it.
    - `home_activity`: none, the residents who fell, or the live hearth, doorstep and window plan with its screen metrics.
    - `farm_activity`: the field a farm works and the metrics its workers are drawn at.
    - `barracks_activity`: whether the yard is alive and whether it is training.

    Every building kind runs `building_state` first. Homes, farms and the Roman and Carthaginian barracks add their activity system. `registry.presentation(kind)` lists them in order.

- **Submission.** `register_presented_renderer` stores a submission that takes the `EntityPresentation`. `registry.get()` returns one function per kind that runs the systems and then the submission. The scene's per-entity path is therefore: look up the kind, present, submit. Kinds registered with a plain `RenderFunc` (units, creatures, siege) submit directly. Units and creatures prepare through their `IParallelPreparer`.

Some presentation stays inside submission, because moving it would reorder what is drawn:

- The per-resident and per-worker choices inside the home and farm activity (doorstep plans, gags, worker rosters) spend the frame's actor and plume budgets as they draw.
- The stockpile, ambient people and banner code resolve their state while they emit.

Building collapse is resolved once per building in the scene's collect step (`resolve_building_collapse`), because culling and the draw model need it before any kind is looked up. `tests/render/entity_presentation_test.cpp` checks, without GL:

- the kind-to-systems table;
- that every building kind draws catalog descriptions, and that every catalog entry names a registered kind;
- that dispatch presents before it submits;
- each built-in system against a plain `World`.

`scripts/check-render-layers.py` pins the authoring side (entity authoring rule). No `*_authoring` file and no shared authoring module may reach `frame/submitter.h`, `frame/frame_submitter.h`, `frame/draw_context.h` or `render/scene`, not even through another header.

## Nation and equipment appearance

Gameplay identifies units/buildings through nation, troop/building type, equipment/configuration, and presentation state.

Renderer-side registries resolve those facts into:

- nation-specific materials;
- equipment archetypes;
- mesh/body variants;
- heraldic/team presentation; and
- renderer-specific cached resources.

The renderer does not become the gameplay registry for what equipment a unit owns. If a simulation rule needs equipment data, it must read simulation/content data rather than reverse-querying a render asset choice.

### Limb geometry and skinning

A span primitive's `anchor_bone` and `tail_bone` locate its endpoints. Its
`SpanSkinning` policy separately controls deformation during the rigged bake:
`LinearBlend` distributes influence along the span, while `AnchorRigid` attaches
the entire surface to the anchor's frame. Custom span geometry uses the same
policy; `MeshSkinning` continues to describe standalone mesh weights.

Humanoid arm and leg segments use `AnchorRigid` at both detail levels, including
the skeleton body. Their anchor frame already aims along the segment: `Forearm`
runs from elbow to wrist and `Knee` from knee to ankle. The hand and foot frames
control the extremities and equipment sockets independently. Blending toward
those frames across the limb shaft makes a wrist turn bow or pinch the forearm
and an ankle rotation twist the calf. Joint spheres cover articulation while the
two smooth sections of each limb retain their shared surface and normal.

Pose solvers and the skeleton evaluator share `PosePrimitives::hip_position`.
Leg-length correction must use that actual pelvis-relative joint, not infer a
hip from the moving knee. The latter stretched the held pose's thigh by about
26% and left a gap once its mesh stopped deforming toward the knee. Baked held
poses are checked against the rig's hip, knee, and ankle origins on every frame.

The humanoid mesh regression bends and twists every downstream joint and checks
the skinned positions and normals on both sides and at both detail levels.
After changes, rebuild `bake_creature_assets` and inspect the Arena's
`humanoid_gait_review`, `humanoid_gait_review_carthage`, `sword_duel`, and
`promo_humanoid_showcase` scenarios; the bind pose alone cannot expose this fault.

## Shader system

The OpenGL backend compiles shader programs from `assets/shaders/`.

Shared shader includes centralize behavior such as:

- environment lighting;
- quality-tier selection;
- shadows;
- material helpers; and
- common rendering conventions.

`GraphicsProfile::shader_tier` selects the quality tier provided during shader compilation. Tier-specific paths are compiled into variants rather than represented only as expensive per-fragment dynamic branches.

The shader system supports reload. Backend/profile state required by a newly compiled program is reapplied after the program is replaced.

## OpenGL backend

The OpenGL backend consumes the draw queue and owns the GL-specific execution details:

- buffers and vertex arrays;
- textures and samplers;
- shader programs;
- framebuffer/pass sequencing;
- shadow passes;
- post-processing;
- instancing/batching;
- GPU timing/counters; and
- resource lifetime.

Higher layers submit render intent. They should not depend on raw GL state layout unless they are explicitly part of the backend implementation.

### GL object lifetime

The process has one GL context that is never recreated. The gameplay `GLView` and the commander portrait are both `QQuickFramebufferObject` items in the same window, so they share the scene-graph context and render thread. The portrait still owns a second `Renderer` and `Backend`, and that is why per-renderer state matters even with one context.

Every GL object under `render/` is owned by a move-only handle from `render/gl/gl_handle.h`: `BufferHandle`, `VertexArrayHandle`, `TextureHandle`, `ProgramHandle`, `ShaderHandle`, `FramebufferHandle`, `RenderbufferHandle`, `SamplerHandle`, `QueryHandle`, and `FenceHandle` for sync objects. `XHandle::create()` makes the object on the current context and records its share group (`current_gl_share_group()`). `reset()` or the destructor releases it through one policy in `render/gl/gl_lifetime.*`:

- If the owning share group is current, the object is deleted immediately.
- Otherwise it is queued with `defer_gl_delete`. The queue drains only into its own group, and is discarded (`forget_gl_share_group`) when that group is destroyed. A name released into a group that is already gone is counted as deleted and never queued.
- No destructor issues a GL call without a current context, so a handle can be destroyed on any thread and a pipeline's `shutdown()` is just a list of `reset()` calls. `Backend::~Backend()` destroys its pipelines whether or not a context is current.

A member that only reads a name owned elsewhere (a shadow cascade's texture, the wear volume a pipeline binds) is declared `BorrowedGlName`.

`gl_lifetime` keeps a live-object census per kind (`gl_object_census`): handles count creation, and the queue counts every deletion or discard. `tests/render/gl_ownership_leak_test.cpp` initialises a `Backend`, draws frames with profiling on (timer queries and fences included), destroys it, does that again, and asserts that every kind is back to its starting count. CI runs it on Mesa under Xvfb with `SOI_REQUIRE_GL=1`, so a missing context fails it instead of skipping it.

`scripts/check-gl-ownership.py` (a PR source-policy gate) keeps it that way. It allows `glGen*`/`glCreate*`/`glDelete*`/`glFenceSync` only in `gl_handle.cpp` and `gl_lifetime.cpp`, no `gl*` call anywhere in `render/` outside `render/gl/` and `render/backend/`, and no raw `GLuint` member named like an owned object.

A `Mesh` is geometry only: vertices, indices, bounds, a process-unique `id()` and a lifetime token. Each backend owns a `RenderContext` (`render/gl/render_context.h`) that holds its `ResourceManager`, its `ShaderCache` and a `MeshGpuTable`. The table uploads a vertex array and buffers for a mesh the first time that backend binds it. It keeps them, keyed by mesh id, until the `Mesh` is destroyed (`collect_retired()` runs at `begin_frame`) or the backend is. So the process-wide `SharedGeometryCache` holds no GL objects, two renderers in one process (the game and the commander portrait, or two tests) never share or fight over a vertex array, and destroying a backend releases everything it uploaded. Domain renderers only list their meshes through a `MeshVisitor` (`for_each_mesh`, `for_each_projectile_mesh`); the caller uploads them with `prewarm_meshes(backend.render_context().meshes(), visit_meshes)`, so no domain folder sees the table.

Instance data and images follow the same split. A scatter chunk, the fog and the ambient fog hand the backend an `InstanceBlock` (`render/geom/instance_block.h`): bytes, an id, a version and a lifetime token. The terrain heights, terrain fields and ground cover, the visibility mask and the fog mask are `TextureImage`s (`render/geom/texture_image.h`): texels, a format, a version and the region the last change touched. Draw commands point at these CPU objects. Each backend keeps its own `InstanceBufferTable` and `TextureTable` in its `RenderContext`. At the start of every frame, before the shadow pass, `Backend::sync_frame_resources` walks the queue and uploads whatever changed. That is the resources stage. A mask that changed in one region is uploaded as that region only. The terrain noise atlas and microdetail are baked by the backend's terrain pipeline from a `TerrainNoiseRequest` the terrain command carries, once per terrain revision. Pipelines then only look the objects up. Creatures follow it too. A `RiggedMesh` (`render/geom/rigged_mesh.h`) is bone-local geometry with an id and a lifetime token; each backend uploads it through its `RiggedMeshGpuTable`. A skin atlas's baked palettes are an `InstanceBlock` with a uniform target whose bytes a writer produces from the atlas storage at upload time, so the atlas is never held twice on the CPU. `RiggedCreatureCmd::palette_block` points at it, and the backend binds its own uniform buffer at the frame offset. The cache builds palette blocks only when the scene renderer has a GL backend (`set_palette_blocks_enabled`); without one, creatures upload their palette per draw, as before. So `render/ground` builds plain data, testable without a context (`tests/render/gpu_data_test.cpp`, the scatter partition tests), and owns no GL object.

`check-gl-ownership.py` enforces both halves: no header that declares an `instance()` singleton may hold a GL object, and `render/geom/mesh.h` holds none. `tests/render/gl_ownership_leak_test.cpp` runs two backends side by side and checks that both release everything.

`current_gl_share_group()` is called per draw, so it caches the last context group per thread. The cache is invalidated by a generation counter bumped whenever a group is destroyed, so a reused `QOpenGLContextGroup` address cannot resolve to a stale id.

### Renderer-scoped state

- **Graphics quality.** `GraphicsSettings` publishes quality, profile pointer and backend kind as atomics. The profiles are immutable `constexpr` tables, so a reader on the render thread or a prepare worker always sees a whole profile while the GUI thread changes quality.
- **Shader reload.** `Shader::set_global_defines` bumps a generation only when the defines actually change. `Shader::reload_all()` recompiles only programs that were compiled under an older generation and that belong to the current share group. When both backends apply the same tier, the second one recompiles nothing.
- **Runtime-bake barrier.** A `Renderer` lifts the global no-runtime-bake barrier on `initialize()`/`shutdown()` only if it raised it. Initializing or tearing down the portrait renderer no longer re-enables runtime bakes for the gameplay renderer. The barrier itself remains process-global because bake sites deep in the creature caches read it without a renderer in hand. Both renderers run one after the other on the render thread, and the portrait wraps its frames in `RuntimeBakeAllowScope`.
- **Frame profile.** The portrait wraps its frames in `ScopedFrameProfileRedirect`, so its draw counts and phase timings go to a local `FrameProfile` instead of overwriting the gameplay frame shown by F10 and the benchmark.
- **Parallel prepare.** `prepare_unit_plans` freezes the render world's registry (`Registry::StructureFreeze`) while worker threads prepare creatures. Adding or removing components, creating storages, or creating or destroying entities while frozen is counted in `structure_violations()` and asserts in debug builds. A preparer that forgets to add a component in `ensure_prepare_components` fails there instead of racing.

Not changed, deliberately:

- `CameraVisibility` and `VisibilityBudgetTracker` stay process-global. The portrait renderer draws no dust or contact shadows that consult them, and `begin_frame` resets run one after the other on one thread.
- `Renderer::shutdown()` is terminal. Every caller destroys the renderer right after it, so there is no re-initialize contract to honour.
- Draw commands keep borrowed raw pointers. The queue is filled, sorted and played back within one `end_frame`, and the only reader of a previous frame's queue (`render_software_preview`) is test-only.

## Software backend

`SoftwareBackend` is selected when shader quality is `None` or the application is explicitly configured to use the CPU renderer.

The software path is a diagnostic/reduced-fidelity fallback, but it is a real backend in the repository. Documentation should not state that software rendering is unsupported or absent.

Because it consumes the backend boundary, it also serves as a useful architecture check: scene submission is not supposed to require direct OpenGL calls to express every drawable.

## Minimap and secondary presentation consumers

The minimap follows the same ownership principle as the main renderer. It consumes published/read-model state rather than scanning and mutating the live world at arbitrary presentation cadence.

Other UI/presentation systems use the same pattern: simulation publishes facts; presentation derives visual state; presentation caches remain disposable.

See [MINIMAP.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/MINIMAP.md).

## Resource generation and quality changes

Graphics quality changes increment a settings generation. Consumers can observe that generation and apply expensive changes at controlled boundaries.

This is preferable to re-reading dozens of mutable options for every entity every frame.

A quality change may require operations such as:

- shader recompilation/reselection;
- shadow resource changes;
- LOD/cull policy changes;
- post-process chain changes; or
- density/budget updates.

The profile is therefore an immutable description of one coherent quality mode rather than a loose bag of unrelated booleans.

## Performance instrumentation

The rendering path exposes profiling information used by repository performance tooling.

Measured/reportable areas include:

- render-thread frame phases;
- GPU timings where available;
- GL resource/upload counters;
- post-playable asset work;
- frame interval timing; and
- budget verdicts.

[FRAME_PACING.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/FRAME_PACING.md) defines the current presentation-budget gate. [PERFORMANCE.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/PERFORMANCE.md) covers the broader runtime measurement system.

## Playable-frame resource discipline

The frame-pacing gate distinguishes ordinary per-frame transfer work from asset/resource construction occurring after the battle is considered playable.

That distinction is important because a frame can look acceptable in steady state while still hitching the first time an asset, shader, buffer, or texture is created in combat.

The repository therefore tracks `post_playable_asset_work` separately from upload bytes.

## Layering enforcement

Rendering dependencies remain one-way.

Simulation code should not include renderer implementation simply to ask a gameplay question. Shared non-GL concepts belong in lower libraries when both sides need them—for example:

- scene camera/environment data;
- animation/BPAT structures; and
- simulation-published render snapshot types.

The build graph and architecture/source checks enforce selected parts of this direction.

See [ARCHITECTURE.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/ARCHITECTURE.md) for the repository-wide dependency boundary.

## Failure modes by layer

A rendering bug is easier to diagnose when separated by stage.

### Snapshot is wrong or missing

Symptoms: whole entities absent, stale authoritative transforms, old ownership/team state.

Inspect simulation publication and snapshot acquisition first.

### Scene walk is wrong

Symptoms: snapshot contains the entity but no renderer receives it, wrong culling/visibility class, wrong renderer dispatch.

Inspect `render/scene/scene_collect.cpp` (collection and visibility class), `render/scene/scene_unit_submit.cpp` (renderer dispatch) and the visibility inputs.

### Draw queue is wrong

Symptoms: renderer submits something but ordering/batching/pass data is malformed.

Inspect queue contents and command preparation.

### Backend is wrong

Symptoms: queue is correct but GL/software output is wrong, shader state is missing, resource lifetime is broken.

Inspect `IRenderBackend` implementation and pass state.

### Presentation cache is stale

Symptoms: authoritative entity is correct but pose/material/interpolation lags behind.

Inspect renderer-owned cache keys, content epoch/generation, and snapshot-to-presentation transfer.

This separation keeps a visual defect from automatically being treated as a simulation defect.

## Testing and review surfaces

Renderer behavior is covered by several kinds of checks:

- headless/unit tests for data preparation and profiling logic;
- shader/source validation;
- Arena scenarios for integrated rendering states;
- packaged-game self-tests;
- frame-pacing runs on qualified hardware; and
- screenshot/video/manual review for visual changes that are inherently perceptual.

Arena is especially useful because it can isolate a known authored state without requiring a full campaign path to reproduce it.

### Creature pipeline goldens

`HumanoidGoldenParity` and `QuadrupedGoldenParity` replay fixed poses through the
creature pipeline and compare every rigged draw with
`tests/render/creature/golden/*_pipeline_golden.txt` (regenerate with
`SOI_WRITE_HUMANOID_GOLDEN=1` / `SOI_WRITE_QUADRUPED_GOLDEN=1`, run from the
repository root). Each `draw` line carries:

- `topology`, a hash of vertex and index counts and the index buffer;
- `binding`, a hash of each vertex's bone indices and colour role, all integers;
- `weights`, two moments of the skin weights (the bone-index-weighted mean and the
  mean square per vertex), compared with the scalar tolerance like the matrices.

The weights used to be rounded to 1/4096 and hashed. That made the golden depend on
the build type: the bake's weights are dyadic fractions (2308.5/4096, 15/16) and
float blends, so thousands of them sit on or next to a rounding boundary, and the
last bit that Debug and Release codegen disagree on decided the hash. CI (Debug) and
local Release builds recorded different `binding` values, and either golden failed
on the other build. No rounding grid or offset avoided it (every one tried left
weights within float noise of a boundary), which is why the weights are compared as
tolerant moments instead. Any golden change must still pass under both build types.

## Architectural invariants

The current renderer depends on these invariants:

- simulation publishes renderable state; renderer does not own gameplay state;
- there is no mutable-live-world fallback for ordinary rendering;
- scene traversal records work before backend execution;
- quality profiles change presentation cost, not gameplay rules;
- renderer caches are disposable;
- software and GL paths consume the same higher-level submission contract;
- animation interpolation does not feed authoritative movement/combat; and
- performance counters measure presentation work without becoming gameplay state.

## Source map

| Concern                | Source                                                                                                                                   |
| ---------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| Graphics profiles      | `render/frame/graphics_settings.h`                                                                                                       |
| Backend interface      | `render/frame/i_render_backend.h`                                                                                                        |
| Backend selection      | `render/backend/render_backend_factory.cpp`                                                                                              |
| Scene traversal        | `render/scene/scene_walk.cpp`, `render/scene/scene_collect.cpp`, `render/scene/scene_unit_submit.cpp`, `render/scene/scene_overlays.cpp` |
| OpenGL backend/passes  | `render/gl/`                                                                                                                             |
| Software backend       | `render/software_backend.*`, `render/software/`                                                                                          |
| Scene/environment data | `scene/`                                                                                                                                 |
| Animation/BPAT         | `animation/`                                                                                                                             |
| Shaders                | `assets/shaders/`                                                                                                                        |
| Frame pacing/profiling | `render/profiling/`, `scripts/check-frame-pacing.py`                                                                                     |

The current implementation is the source of truth for renderer capability and quality behavior. Historical refactor notes, old preset descriptions, and proposed follow-up work are not part of the runtime contract.
