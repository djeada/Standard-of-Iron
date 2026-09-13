# Rendering Architecture

Standard of Iron renders from published simulation snapshots. The simulation owns gameplay state; the renderer consumes a detached view of that state, records draw commands, sorts/batches them, and hands the resulting queue to either the OpenGL backend or the software backend.

The renderer is a consumer of gameplay state, not a gameplay dependency. `render_gl` links the simulation kernel (`game_sim`), while the simulation layer does not depend on renderer types.

## Frame data flow

The production frame path is:

```text
fixed simulation thread
        │
        ▼
World publishes render snapshot
        │
        ▼
QSG render thread
GameEngine::update_presentation(dt)
        │
        ▼
Renderer::render_world(...)
        │
        ├─ ensure/acquire published snapshot
        ├─ visibility + scene walk
        ├─ entity/environment submission
        ▼
DrawQueue
        │
        ├─ sort / batch / pass preparation
        ▼
IRenderBackend
        ├─ GL::Backend      (shader path)
        └─ SoftwareBackend (ShaderQuality::None)
```

`Renderer::render_world()` calls `World::ensure_render_snapshot()` and then `acquire_render_snapshot()`. If no snapshot is available, the renderer returns rather than falling back to mutable live-world entity reads.

Renderer-owned animation/runtime presentation state is transferred between render snapshots where needed; authoritative gameplay components remain owned by the simulation.

## Simulation and render threads

`GameEngine::simulate(dt)` runs authoritative simulation work on the simulation thread. Presentation work such as camera presentation, renderer animation time, minimap/view-model synchronization, and visual effect state runs on the render thread.

The live-state parts of simulation/presentation access are serialized by the game-engine frame lock. World rendering and backend playback operate from published snapshot data and a render-camera copy rather than holding the mutable world for the duration of a frame.

A `WorldFreeze` coordinates operations that replace/rebuild the world so neither simulation nor presentation continues through a destructive restore/reload boundary.

## Draw queue and backend boundary

Entity and environment renderers submit typed commands into `Render::GL::DrawQueue`. The queue is the boundary between scene traversal and backend execution.

The backend interface is `Render::GL::IRenderBackend`:

```cpp
initialize()
begin_frame()
execute(const DrawQueue&, const Camera&)
set_viewport(...)
set_clear_color(...)
set_animation_time(...)
set_frame_budget(...)
```

`RenderBackendFactory::create()` maps `ShaderQuality::None` to `SoftwareBackend`; all other shader-quality values create the OpenGL `Backend`.

`ShaderQuality` currently has four values:

- `Full`;
- `Reduced`;
- `Minimal`; and
- `None`.

This backend choice is separate from the user-facing Low/Medium/High/Ultra graphics profile described below.

## Graphics profiles

`render/graphics_settings.h` defines four immutable `GraphicsProfile` entries:

- `Low`;
- `Medium`;
- `High`; and
- `Ultra`.

`High` is the default (`k_default_graphics_quality`).

A profile controls:

- shader tier;
- creature LOD/cull behavior;
- batching policy;
- contact-shadow budget;
- directional-shadow cascade settings;
- post-processing switches;
- weather-particle scale;
- MSAA sample count;
- template-prewarm budget; and
- grass density.

`GraphicsSettings::set_quality()` swaps the active profile and increments a generation counter. Consumers apply generation-sensitive changes at their own controlled boundary rather than rebuilding every setting on every frame.

### Low

The current Low profile uses:

- Low shader tier;
- creature LOD/culling enabled;
- 120 m creature cull distance;
- forced batching;
- directional shadows disabled;
- bloom, god rays, ambient occlusion, and FXAA disabled;
- no MSAA;
- 30% weather-particle scale; and
- 30% grass density.

### Medium

Medium uses:

- Medium shader tier;
- creature LOD/culling enabled;
- 200 m creature cull distance;
- two 1024 directional-shadow cascades;
- bloom, ambient occlusion, and FXAA;
- god rays disabled;
- 2× MSAA;
- 60% weather-particle scale; and
- 65% grass density.

### High

High uses:

- High shader tier;
- creature LOD disabled;
- no practical distance culling (`k_never_cull_distance`);
- batching disabled by profile;
- four 4096 directional-shadow cascades to 200 m;
- bloom, god rays, ambient occlusion, and FXAA;
- 4× MSAA;
- full weather-particle scale; and
- full grass density.

### Ultra

Ultra uses the same full-detail creature/shadow/post-process envelope as High, with Ultra shader tier and 8× MSAA.

These values are code-defined in `render/graphics_settings.h`; documentation should not substitute older preset descriptions for the current profile table.

## Creature rendering

Skinned creature animation uses baked BPAT data and compiled creature/rigged-body assets. Runtime gameplay/presentation selects clips and phases; the renderer consumes prepared pose data and submits rigged or snapshot-mesh commands according to the active profile and species path.

The current LOD model distinguishes full-detail and minimal/snapshot rendering, with a separate culled state when the active profile allows culling. High and Ultra disable creature LOD in their current profiles.

See [CREATURE_BPAT_FORMAT.md](CREATURE_BPAT_FORMAT.md) and [HORSE_MODEL_ARCHITECTURE.md](HORSE_MODEL_ARCHITECTURE.md).

## Environment lighting

`scene/environment_lighting.h` defines the environment-lighting state shared across outdoor rendering. It carries values such as sun direction/colour/intensity, sky/ground contribution, fog, shadow parameters, exposure, cloud cover, and wetness.

Shaders use the shared environment-lighting include/state instead of maintaining unrelated sun/fog constants per material.

Time-of-day and weather systems update the environment state; render passes consume the resulting snapshot/presentation values.

## Directional and contact shadows

Directional shadows are controlled by `DirectionalShadowSettings` in the active graphics profile. Medium, High, and Ultra enable cascaded directional shadows with their profile-specific cascade count/resolution/distance. Low disables directional cascades.

Creature grounding also uses contact-shadow rendering. `ContactShadowBudget` controls caster count/distance independently from the directional-shadow profile.

Shadow-cascade fitting, bias, filtering, and light-space culling are implemented under `render/gl/` and the shared shader includes. Changes to shadow geometry/tuning should use those sources as the contract rather than numbers copied into unrelated articles.

## Weather and precipitation

Weather presentation uses the same authored/runtime weather state as gameplay/environment systems. `RainPipeline` owns a fixed particle pool; draw density is derived from weather intensity and the active `WeatherBudget::particle_scale`.

The four graphics profiles therefore scale precipitation without changing the authored weather itself.

## Terrain, scatter, and world props

Terrain and authored world props are prepared by the terrain/scatter rendering path and submitted to the same queue/backend architecture as other renderable content.

Terrain scatter has an explicit GPU-readiness state used by mission startup: the loading overlay can remain active while terrain scatter exists but is not GPU-ready. See [MISSION_STARTUP.md](MISSION_STARTUP.md).

World-prop positions are authored in grid space and converted through the terrain-service position helpers before rendering. See [MAP_OBJECT_PLACEMENT.md](MAP_OBJECT_PLACEMENT.md).

## Nation and equipment appearance

Gameplay identifies troops/buildings by nation, spawn type, equipment/configuration, and presentation state. Rendering resolves those values into nation-specific renderers, materials, equipment archetypes, and creature-body variants.

Equipment/archetype registries are renderer-side caches/lookup tables. Gameplay does not use the renderer as the authority for what equipment or nation a unit owns.

## Shaders

The OpenGL backend compiles shader programs from the assets under `assets/shaders/`. Shared shader includes provide common lighting, quality, shadow, and material behavior.

The active `GraphicsProfile::shader_tier` selects the quality tier supplied to shader compilation. Tier-dependent features are compiled into shader variants rather than represented only as per-fragment runtime branches.

The shader system supports reloading; backend/profile code reapplies the state needed by a newly compiled program.

## Local lights

Local lights from entities/effects are collected separately from ordinary mesh commands and are constrained by the renderer's local-light budget/fader. Instanced world props that need a light expose the light through their renderer rather than attempting to recover emitter positions from GPU instance data.

## Minimap and other presentation consumers

The minimap also consumes render/world snapshots for unit overlay updates rather than scanning the mutable world at presentation cadence. Its architecture is documented in [MINIMAP.md](MINIMAP.md).

Other presentation systems follow the same ownership rule: simulation owns authoritative state, presentation gets a snapshot/read model, and renderer/UI caches are disposable.

## Layering enforcement

The source tree keeps rendering dependencies one-way. Layering checks and architecture tests reject simulation code that reaches upward into renderer implementation.

Shared non-OpenGL concepts live below the renderer where appropriate—for example scene camera/environment data and animation/BPAT data—so both gameplay/headless tools and rendering can use them without creating a renderer dependency inside simulation.

## Performance instrumentation

The render path exposes frame phases, asset/GL counters, post-load asset-work counters, GPU timings where available, and budget reports used by the performance suite.

See [PERFORMANCE_INSTRUMENTATION.md](PERFORMANCE_INSTRUMENTATION.md) and [FRAME_PACING.md](FRAME_PACING.md) for the measurement/reporting contracts.

## Source of truth

The rendering contract is defined by the current implementation:

- `render/graphics_settings.h` — user-facing graphics profiles;
- `render/i_render_backend.h` and `render/render_backend_factory.cpp` — backend interface/selection;
- `render/scene_walk.cpp` — snapshot consumption and scene submission;
- `render/gl/` — OpenGL backend and passes;
- `render/software_backend.*` — software fallback;
- `scene/` — non-GL scene primitives/environment data;
- `animation/` — render-independent animation data and BPAT runtime.

Historical refactor notes and “next steps” are not renderer capabilities. Current code and tests are authoritative.
