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
