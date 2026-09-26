# Rendering Architecture

Standard of Iron renders from published simulation snapshots. Gameplay state belongs to the simulation; the renderer receives a detached view of that state, resolves visibility and presentation, records draw work, and submits the result through a backend boundary.

That ownership model is the central rendering invariant. The renderer can cache meshes, shaders, animation presentation, GPU resources, and frame-local visibility decisions, but it does not become the authority for unit position, ownership, health, combat state, economy, or mission state.

`render_gl` links the simulation kernel (`game_sim`) so it can consume production simulation data. The simulation layer does not link renderer implementation.

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

`render/scene_walk.cpp` is the main bridge between snapshot data and renderer submission.

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

`render/graphics_settings.h` defines the user-facing presets:

- `Low`;
- `Medium`;
- `High`; and
- `Ultra`.

A graphics profile contains many decisions beyond shader quality: LOD/culling, shadow cascades, post effects, batching, MSAA, weather density, grass density, and resource budgets.

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
- MSAA sample count;
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
- MSAA disabled;
- weather-particle scale `0.30`; and
- grass density `0.30`.

Low is therefore not merely “High with smaller textures.” It changes several rendering features and content densities together.

### Low on an old or software GPU

Low is the preset that has to run on a GL 3.3 Core driver with no 4.x features. That covers old Intel and AMD integrated GPUs, and Mesa llvmpipe.

- **First run picks Low on weak adapters.** If no graphics preset has been saved, `RenderBootstrap::initialize` switches to Low when the adapter is a software renderer (llvmpipe, softpipe, swrast, SwiftShader, Microsoft Basic Render, GDI Generic, Apple Software Renderer), or, except on macOS, when the context is below GL 4.3. macOS caps OpenGL at 4.1 on every GPU, so the version says nothing about a Mac's speed. `GraphicsSettings::quality_chosen_by_user()` stops a saved choice from being overridden.
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
- 2× MSAA;
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
- 4× MSAA;
- full weather-particle scale; and
- full grass density.

This is the default full game profile.

## Ultra profile

Ultra keeps the full-detail High envelope and raises the expensive quality tier further:

- Ultra shader tier;
- the same full creature-detail policy;
- the same four-cascade directional-shadow envelope;
- the full post-processing chain;
- 8× MSAA; and
- full weather/grass density.

The exact profile tables in `render/graphics_settings.h` are authoritative. Articles should not preserve older preset descriptions when the code table changes.

## Creature rendering

Creatures use baked BPAT animation/body data and renderer-side rig/mesh preparation.

Gameplay and presentation state select the current action/clip/phase. The renderer then evaluates the appropriate rigged/snapshot path for the species and active graphics profile.

The current creature detail model distinguishes full-detail and reduced/snapshot presentation, with a separate culled state when the active profile permits distance culling.

High and Ultra currently disable creature LOD in their profile definition.

### Minimal LOD and the prebaked snapshot blob

Species that ship a `*_minimal.bpsm` (horse, elephant, sheep, wolf) and carry no static attachments are marked `requires_prebaked_minimal_snapshot`: at the Minimal LOD they are only ever drawn from that blob, never skinned at runtime, so a distant herd costs one static mesh per body. The blob is baked from **every** clip in the species recipe, so any state can be served from it. The decision lives in `snapshot_mesh_serves_request()` (`render/creature/pipeline/lod_decision.h`): a prebaked Minimal request always takes the snapshot path; only the opt-in runtime-baked snapshot path (`creature_lod.snapshot_meshes`, off in every shipped profile) is restricted to states whose manifest `snapshot` flag is set.

It used to require the `snapshot` flag in both cases, and the clip manifest clears that flag for `Die` because a humanoid's runtime-baked fall would cost a mesh per frame. The prebaked species inherited the exclusion by accident, and the pipeline's answer for a non-snapshot state at a prebaked Minimal LOD was to submit nothing: a sheep or horse dying more than 20 m from the camera (12 m on Low) vanished for the whole fall and reappeared as a settled corpse. Measured in `wildlife_pack_takedown` at `--graphics-quality low`, all 144 `Die` submissions for the two sheep were dropped while the 122 `Dead` submissions were drawn. Humanoids were never affected: they have no `.bpsm`, so their Minimal LOD falls through to the rigged path in both states.

See [CREATURE_BPAT_FORMAT.md](CREATURE_BPAT_FORMAT.md) for the baked format and [HORSE_MODEL_ARCHITECTURE.md](HORSE_MODEL_ARCHITECTURE.md) for mounted creature asset structure.

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

The formation article documents the shared anchor precedence in detail: [FORMATION_ARCHITECTURE.md](FORMATION_ARCHITECTURE.md).

## Terrain and world geometry

Terrain, scatter, authored props, buildings, walls, and other world geometry feed the same queue/backend model as dynamic entities.

Terrain/world-prop positions are resolved from authored map/grid space through the terrain/map services before rendering. Renderer code should not invent a second coordinate convention for map objects.

See [MAP_OBJECT_PLACEMENT.md](MAP_OBJECT_PLACEMENT.md) for the placement contract.

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
- **Structures stay upright and get a foundation.** Buildings, walls, towers, construction
  sites and placement ghosts are seated at the height under their centre.
  `render/entity/structure_foundation` measures the drop across the drawn body (the
  `BuildingCollisionRegistry` body table × transform scale) and adds a fieldstone
  foundation down to the lowest ground. Completed buildings cache it in `CachedUnitData`
  and recompute it only when the model matrix changes. Ghosts resolve it on the spot, using
  the same rule the finished structure will use.
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

Most instanced props pick their materials in the shader from the model-space position (bands keyed to where a part happens to sit) or from a length packed into the normal (the tent). The weapon rack (`render/gl/backend/weapon_rack_mesh.cpp`) has too many small, overlapping parts for that to work. Every vertex therefore carries a fourth attribute at location 4, `(material, u, v, seed)`, uploaded by `VegetationPipeline::upload_prop_mesh_with_surface_impl`. Location 4 is free on prop VAOs because the instance stream only binds locations 2 and 3.

- `material` is one of the `WeaponRackMaterial` ids, which match the `k_mat_*` constants in `weapon_rack_instanced.frag`: oak, ash, steel, iron, bronze, leather, yew, linen, shield paint, shield back, bone and feather.
- `u, v` depend on how the part was built:
    - Boxes and beams are projected so that `v` runs along the member. The shader lays grain, forging streaks and wear along it.
    - Swept parts use `u` for the distance around the section (0 to 1) and `v` for the distance along the sweep. Blades and spearheads have `v` normalised from 0 at the base to 1 at the tip, and their four-facet diamond section puts the edges at `cos(u·2π) = ±1`.
    - Shield faces use the flat face coordinates in [-1, 1]. The scutum outline is the superellipse `|s|^2.4 + |t|^2.4 = 1`, and the shader paints its border against that curve. On the parma the coordinates are polar.
- `seed` varies each part. For shield paint it also chooses the design: below 0.5 is a scutum, 0.5 and above is a parma.

The frame parts in `weapon_rack_parts.h` are still the source for `PropModelFootprintTest`. Weapons and shields are built only in the mesh builder and stay inside the declared `{0.88, 0.54}` half extents. The shader builds a height field for each material and turns it into a bump normal from screen-space derivatives. Every derivative is taken outside the material branches, and the bump fades out beyond about 48 m. Lighting is GGX specular plus a sky/ground reflection. Directional shadow blocks sun specular completely but only tints the ambient and diffuse terms, so metal in shadow does not glint.

## Terrain scatter readiness

Terrain scatter has an explicit GPU-readiness state.

Mission startup can keep the loading overlay visible while terrain scatter exists but is not yet ready for presentation. That state is one of the startup readiness gates described in [MISSION_STARTUP.md](MISSION_STARTUP.md).

The startup system therefore distinguishes “simulation world exists” from “the main visual environment is ready enough to reveal the battle.”

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

### Curvature without a height or field texture

`terrain_chunk.frag` takes curvature from the baked field texture or the height texture. Meshes with neither (the boundary mountain ring drawn with `horizon_dressing`) fell back to `curvature_from_normal_field()`, the screen-space derivative of interpolated vertex normals. On a coarse mesh that derivative is constant per triangle, so the gully mask shaded each concave triangle as a dark wedge and the ring read as vertical streaks. `k_soi_normal_field_curvature_trust` scales that fallback to zero. Terrain chunks with baked fields are unaffected.

### Building foot

`building_merged.vert` passes each merged building's instance origin (its ground contact) as `v_ground_height`, and `basic_instanced.frag` darkens and warms wall faces within `k_plinth_height` of it, so houses sit in the soil instead of on it. `basic_instanced.vert` draws individual parts whose origin is not the ground, so it passes `k_no_ground_contact` and the effect is off. Buildings outside the static batch (under construction, damaged, or on a backend without it) do not get the foot.

## Directional shadows

Directional shadows are configured by `DirectionalShadowSettings` in the active graphics profile.

Current profile behavior is:

| Profile | Directional shadows                         |
| ------- | ------------------------------------------- |
| Low     | disabled                                    |
| Medium  | 2 cascades, 1024 resolution                 |
| High    | 4 cascades, 4096 resolution, 200 m envelope |
| Ultra   | full High-style cascade envelope            |

Cascade fitting, bias, filtering, light-space culling, and sampling are implemented in `render/gl/` and shared shader includes.

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

## Nation and equipment appearance

Gameplay identifies units/buildings through nation, troop/building type, equipment/configuration, and presentation state.

Renderer-side registries resolve those facts into:

- nation-specific materials;
- equipment archetypes;
- mesh/body variants;
- heraldic/team presentation; and
- renderer-specific cached resources.

The renderer does not become the gameplay registry for what equipment a unit owns. If a simulation rule needs equipment data, it must read simulation/content data rather than reverse-querying a render asset choice.

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

Every GL name records the share group it was created in (`current_gl_share_group()`), and is released through one policy in `render/gl/gl_lifetime.*`:

- If the owning share group is current, the name is deleted immediately.
- Otherwise it is queued with `defer_gl_delete`. The queue drains only into its own group, and is discarded (`forget_gl_share_group`) when that group is destroyed.
- No destructor issues a GL call without a current context.

`Buffer`, `VertexArray`, `Texture`, `Shader` (as `DeferredGlObject::Program`) and the resource manager's 3D wear volume all use `release_gl_object`. Pipeline objects in `render/gl/backend/*` still delete their own names. For that reason `Backend::~Backend()`, when it runs without a context, releases (abandons) the pipelines rather than destroying them, but it still clears the shared geometry cache: cached meshes defer their names safely.

`Mesh::prepare_draw` re-uploads when its vertex array belongs to a different share group than the current one. The process-global `SharedGeometryCache` can therefore survive a context change without handing out dead VAO names.

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

See [MINIMAP.md](MINIMAP.md).

## Resource generation and quality changes

Graphics quality changes increment a settings generation. Consumers can observe that generation and apply expensive changes at controlled boundaries.

This is preferable to re-reading dozens of mutable options for every entity every frame.

A quality change may require operations such as:

- shader recompilation/reselection;
- MSAA target recreation;
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

[FRAME_PACING.md](FRAME_PACING.md) defines the current presentation-budget gate. [PERFORMANCE_INSTRUMENTATION.md](PERFORMANCE_INSTRUMENTATION.md) covers the broader runtime measurement system.

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

See [ARCHITECTURE.md](ARCHITECTURE.md) for the repository-wide dependency boundary.

## Failure modes by layer

A rendering bug is easier to diagnose when separated by stage.

### Snapshot is wrong or missing

Symptoms: whole entities absent, stale authoritative transforms, old ownership/team state.

Inspect simulation publication and snapshot acquisition first.

### Scene walk is wrong

Symptoms: snapshot contains the entity but no renderer receives it, wrong culling/visibility class, wrong renderer dispatch.

Inspect `render/scene_walk.cpp` and visibility inputs.

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

| Concern                | Source                                               |
| ---------------------- | ---------------------------------------------------- |
| Graphics profiles      | `render/graphics_settings.h`                         |
| Backend interface      | `render/i_render_backend.h`                          |
| Backend selection      | `render/render_backend_factory.cpp`                  |
| Scene traversal        | `render/scene_walk.cpp`                              |
| OpenGL backend/passes  | `render/gl/`                                         |
| Software backend       | `render/software_backend.*`, `render/software/`      |
| Scene/environment data | `scene/`                                             |
| Animation/BPAT         | `animation/`                                         |
| Shaders                | `assets/shaders/`                                    |
| Frame pacing/profiling | `render/profiling/`, `scripts/check-frame-pacing.py` |

The current implementation is the source of truth for renderer capability and quality behavior. Historical refactor notes, old preset descriptions, and proposed follow-up work are not part of the runtime contract.
