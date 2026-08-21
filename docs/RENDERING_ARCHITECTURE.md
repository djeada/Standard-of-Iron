# How the Rendering System Actually Works

Picture this: you've got thousands of soldiers on screen, each with unique armor, weapons, and animations. Grass is swaying, rivers are flowing, and you need all of this running at 60 frames per second. How do you pull that off without your GPU catching fire?

This is the story of how Standard of Iron takes game state and turns it into pixels. We'll walk through the whole journey, from the moment Qt creates an OpenGL window to the final draw calls that paint soldiers on screen.

## What we'll cover

We'll start with where code is allowed to live and why the simulation must never depend on the renderer, then how the environment lighting and shadow system works. After that: how Qt bootstraps OpenGL, how we record what needs to be drawn, how the backend executes those commands efficiently, where OpenGL actually lives in the code, how different nations get their unique visual styles, and finally how our shaders generate infinite detail without eating all your VRAM.

## Which layer owns what

Before any of the frame mechanics, it's worth knowing where code is allowed to live, because this is the rule that keeps the simulation runnable without a GPU.

**The renderer reads game state. The simulation never reads the renderer.** `render/` includes `game/` freely -- terrain, visibility, nations, team colours -- because drawing means looking at the world. The reverse is forbidden: `game/` must not include `render/`.

That isn't stylistic. `tools/balance_sim` is a headless battle simulator with no window and no OpenGL, and it links `game_systems` without `render_gl`. Any gameplay code that reaches into the renderer breaks it.

So where do shared things go? Three leaf layers sit _below_ both:

| Layer             | Holds                                                                   | May depend on |
| ----------------- | ----------------------------------------------------------------------- | ------------- |
| `scene/`          | `Camera`, `EnvironmentLightingState` -- plain view and environment data | Qt only       |
| `animation/rig/`  | Skeleton proportions, attachment frames, gait, reach constants          | Qt only       |
| `animation/bpat/` | Baked pose data (BPAT format, reader, registry, playback)               | Qt only       |

The important idea: **animation is not part of rendering**. Pose evaluation is a peer subsystem. The renderer consumes bone transforms to skin a mesh; gameplay consumes the same transforms for weapon traces and attachment points. Neither owns them. That's why `HumanProportions` lives in `animation/rig/` and not in `render/humanoid/`, and why the combat weapon trace can sample baked sockets without touching the renderer.

`Camera` is worth calling out because it looks like a renderer type and isn't. It's `QMatrix4x4` maths with no GL. It used to pull map bounds from a gameplay singleton; it now takes a `Camera::MapBounds` pushed in by whoever loads the map, which is what let it move down to `scene/`.

Scene composition -- constructing renderers, wiring loaders -- belongs in `app/core/`, not `game/`. `world_bootstrap`, `level_loader`, `skirmish_loader` and `environment` live there for that reason.

### The guard

This is enforced at build time, not by convention. `scripts/check-layering.py` runs as part of every build via the `check_layering` target and fails with the offending file and line:

```
game/systems/camera_service.cpp:168: game/ must not include render/scene_renderer.h
```

`tests/architecture/layering_test.cpp` covers the same rules in the test suite. If you find yourself wanting to include `render/` from `game/`, the answer is almost always to move the shared type into one of the three leaf layers, or to invert the call so the renderer reads from the simulation.

## Environment lighting and shadows

Lighting is not per-shader constants any more. A single `EnvironmentLightingState` (`scene/environment_lighting.h`) describes the whole outdoor environment -- sun direction/colour/intensity, sky and ground bounce, fog, shadow tint and strength, exposure, cloud cover, wetness -- and is uploaded once per frame to a uniform block at binding 1.

Shaders pull from it through `assets/shaders/include/environment_lighting.glsl` rather than defining their own sun and sky colours. `#include` in GLSL is resolved by `Shader::load_from_files` (`resolve_shader_includes`), so a shader loaded any other way will not get its includes expanded.

Time of day is a continuous decimal hour with `locked`, `scripted` and `continuous` modes, not four fixed presets; the legacy `morning`/`day`/`afternoon`/`night` strings still load and alias onto hours. Weather feeds the same state, so rain and snow shift cloud cover, wetness, fog and sky tint coherently.

Directional shadows are cascaded (up to four, quality-dependent) with texel snapping and cascade blending. The cascades are fitted to what the camera can see, not to its near/far planes: `render/gl/shadow_cascade_fit.h` intersects the frustum corner rays with a ground slab (the terrain height range in the queue plus a caster allowance) and slices only that distance range, so an RTS camera 40 m up does not spend its first cascades on empty air. Each cascade's world texel size and depth span are uploaded alongside its matrix, and `directional_shadows.glsl` authors everything in metres against them -- the constant bias, the normal-offset that keeps shadows planted at the feet, the penumbra width (crisp on a clear day, wide when `shadow_softness` says overcast) -- so tuning does not drift when the cascade radius changes. Sampling is hardware depth-compare PCF (`sampler2DArrayShadow`, `GL_COMPARE_REF_TO_TEXTURE`) on a 3x3 or 5x5 grid, and the last cascade fades out before the shadow distance instead of ending on a line. Casters are culled against the cascade's light-space footprint only, never by camera distance, because a low sun throws a tall caster's shadow well outside the band it stands in. Two things the shadow pass has to do that are easy to break: the scatter species (trees, ruins, tents, boulders, statues, ore) read their view-projection from the `FrameData` block, so the pass writes the light matrix there for each cascade and restores the camera's afterwards; and the boundary mountain ring is a `TerrainSurfaceCmd` flagged `horizon_dressing`, which keeps it out of both the caster list and the ground slab. Every creature also carries a grounding blob (`troop_shadow*.frag`, `include/contact_shadow.glsl`): a soft occlusion ellipse centred on the creature's ground position, scaled to its footprint, elongated along its facing and tilted to the terrain slope (`build_contact_shadow_model` in `creature_prepared_state.cpp` probes four heights around the feet). It is drawn for every creature within `ContactShadowBudget::max_distance`, moving or idle, at every preset -- it is what keeps a marching formation on the ground when the sun is overhead and the cast shadow sits under the body. `MeshCmd::blend_batchable` lets the queue coalesce those blended quads into a few instanced draws through `troop_shadow_instanced`. Where the cascades are off (Low) the blob adds a sun-offset cast lobe (`u_cast_weight`); `render/contact_shadow.h` still supplies the distance fade.

Local lights are budgeted (`Render::k_max_local_lights`) and go through `Render::LocalLightFader`, which holds a slot until its light has ramped down. Entering and leaving the budget is a fade over `k_local_light_fade_seconds`, never a pop, and `Renderer::clear_entity_render_caches()` resets the fader so a new map never inherits the previous map's fires.

Two practical notes:

- **Texture units are a shared, program-wide namespace.** Two samplers of different types resolving to the same unit make every draw using that program raise `GL_INVALID_OPERATION`. The units in play are listed in `Render::GL::TextureUnit` (`render/gl/render_constants.h`) -- add new long-lived samplers there rather than picking a number.
- **Instanced emitters can't be recovered from draw commands.** Fire camps and shrines reach the GPU as instance buffers, so the backend cannot read their positions back to build local lights. They advertise themselves through `Renderer::local_light()` instead, and the backend budgets those alongside effect-driven lights.

## Graphics presets

There are four presets (`Render::GraphicsQuality` Low/Medium/High/Ultra) and each is one immutable `GraphicsProfile` in a constexpr table in `render/graphics_settings.h`. The table is the whole story: creature LOD, batching, contact and cascaded shadow settings, which post passes run, weather particle budget, MSAA, template-prewarm budget, grass density and the shader tier. `GraphicsSettings::set_quality()` swaps the active profile pointer and ticks a generation counter; nothing else happens on the caller's thread.

The rule for consumers is that the choice is made once, on a generation change, not per frame. `Backend::begin_frame()` compares `GraphicsSettings::generation()` with the one it last applied and, when it differs, calls `apply_graphics_profile()` exactly once: it copies the shadow settings it will use, tells the post-process pipeline which passes to run, and -- if the shader tier changed -- sets `Shader::set_global_defines("#define SOI_QUALITY_TIER n")` and calls `Shader::reload_all()`. `TerrainScatterManager::submit()` does the same generation check to regenerate the grass at the profile's density, and `GLView` recreates its framebuffer when the MSAA count moved. Per-frame code reads plain fields off `profile()` (the LOD configs in `creature_render_graph.cpp`, the batching ratio in the scene walk); none of it switches on `quality()`.

**Shader tiers are compiled, not branched.** Every GLSL stage gets `SOI_QUALITY_TIER` spliced in after its `#version` line (see `assets/shaders/include/quality.glsl` for the derived macros: `SOI_TERRAIN_NOISE_OCTAVES`, `SOI_SURFACE_DETAIL`, `SOI_ULTRA_EFFECTS`), so a tier is a different program, not a uniform tested per fragment. Low strips the layered noise, micro-relief, wear/grime, screen-space AO and the cascade lookup out entirely; Ultra compiles in PCSS contact-hardening shadows, shadowed and back-lit grass blades and the extra water and terrain octaves. `Shader::reload()` makes a live tier switch possible: uniform handles are stable indices into a per-shader table that is re-resolved against the new program, every value set through `set_uniform` and every uniform-block binding is replayed, so the pipelines' cached handles and one-time sampler bindings survive. `tests/render/shader_reload_test.cpp` exercises that on an offscreen context.

**Creatures have two rendered LODs, Full and Minimal, plus a cull distance.** `CreatureLOD::Culled` is not a third level; it marks a creature past `CreatureLodSettings::cull_distance`, which is not drawn. High and Ultra disable the LOD cut (every creature in range is Full) and never cull; Medium uses the authored full-detail distances; Low pulls them in and culls at 120 m.

What the presets mean: **High** is the game as designed (every shader feature, full LOD, four 4096 cascades, the whole post chain, MSAA 4x) and is the default; **Ultra** keeps all of that and adds the expensive extras (tier-3 shaders, MSAA 8x); **Medium** keeps shadows and post but small (two 1024 cascades, no godrays, 2x MSAA, tier-1 shaders, authored LOD); **Low** exists so weak hardware reaches 30 fps: no cascades, no bloom/godrays/AO/FXAA (the composite still runs for the tone grade and fog), 30% grass, tier-0 shaders, aggressive LOD.

## Precipitation

Every shipped map states its weather explicitly in a `"rain"` block -- `enabled`, `type` (`rain` or `snow`), `intensity` (a number, or the words `light`/`medium`/`heavy`), the cycle timings, and `wind_strength` plus `wind_direction` in compass degrees. `Game::Systems::RainManager` runs the cycle; its state, cycle position and transition progress round-trip through the save metadata's `weather` object, so loading mid-downpour resumes mid-downpour.

Particles live in one fixed pool owned by `RainPipeline` and are recycled in the vertex shader, positioned relative to the camera rather than across the map. How much of that pool is drawn each frame is `RainBatchParams::density`, computed by `Render::GL::weather_particle_density()` from the active intensity, the quality preset's `WeatherBudget::particle_scale`, and how far the camera has pulled back. Each particle carries a `rank` in the pool and fades out as the density cutoff approaches it, so raising or lowering the budget dissolves particles instead of popping them.

## QSG render-thread stages

`ui/gl_view.cpp` owns the frame callback through `GLView::GLRenderer::render()`. Qt runs that callback with the FBO OpenGL context current on the QSG render thread. The simulation no longer runs inside that callback:

1. `GameEngine::simulate(dt)` runs on its own `QThread` (`SoISimulation`), started by the first successful `GLRenderer::render()` through `GameEngine::start_simulation_thread()` and stopped by `~GLRenderer` / `~GameEngine`. The loop ticks at a fixed 60 Hz cadence independent of vsync and carries only authoritative work: mission waves, stages and commander messages, then `RuntimeFrameOrchestrator::advance_simulation` (`SessionContext::advance` with `World::update` and the environment clock). Its per-tick cost is accumulated in `GameEngine::take_simulation_tick_us()` and charged to the `sim` phase of the next rendered frame.
   `GameEngine::update_presentation(dt)` runs on the QSG render thread at the top of every frame (the `frame` phase): camera follow, order markers, renderer animation time, rain, visibility, minimap, victory checks and the `sync_*` view-model pushes. `simulate`, `update_presentation`, and the live-state parts of `GameEngine::render` (selection-id sync, `render_effects`) serialise on `GameEngine::m_frame_mutex` (a `std::recursive_mutex`); `render_world` and backend playback run unlocked against the published snapshot and a per-frame copy of the camera (`GameEngine::m_render_camera`, taken under the lock; the renderer and `CameraVisibility` point at the copy), and overlap the tick. GUI-thread input entries take the same lock through `ClientHost::lock_frame()` after `ensure_initialized()`, so selection, picking and placement no longer race the tick. The commander camera is written by `CommanderControlController::update_camera_presentation` from `update_presentation`, never from the sim step. `WorldFreeze` gates both worker threads: a tick or frame is refused while a world rebuild is in progress, and the freeze waits for any in-flight one. A GUI handler that holds the frame lock must never wait on the render thread (no nested event loops, no synchronous grabs) - the render thread may be parked on that lock. `GameEngine::update(dt)` still exists as `simulate` + `update_presentation` for single-threaded callers.
2. `GameEngine::render(width, height)` records and plays back rendering work from state that already exists. `Renderer::render_world(world)` requests and consumes the latest detached render-world snapshot. After the first handoff, culling, sorting, cache updates, and entity renderer callbacks no longer hold the mutable simulation world's mutex. Renderer-owned animation and layout state is transferred from the previous snapshot before submission. The renderer must not rebuild combat query state or search for targets. `Renderer::end_frame()` sorts the `DrawQueue`, then `Backend::execute(...)` performs OpenGL playback.

### The renderer never reads the live world

`Renderer::render_world` reads the published snapshot and nothing else. There is
no fallback to the simulation world: `World` publishes an empty snapshot in its
constructor, and `World::ensure_render_snapshot()` publishes the live contents
synchronously the first time the renderer asks, so `acquire_render_snapshot()`
never returns null for a non-snapshot world. If it somehow did, `render_world`
returns without drawing rather than reaching for live entities.

This used to be a conditional. `world` began as the live simulation world and
was only reassigned when a snapshot existed, so the first frame after attaching
to a world walked live entities — harmless while the simulation shared this
thread, and a data race the moment it does not.

`PersistentRenderRegistry` existed only to feed that fallback with classified
id lists. It has been deleted. It observed the live world's component and
entity-destroyed callbacks, and its `remove_from_lists` did three linear scans
over world-sized vectors on every entity death, to maintain lists the snapshot
path never read. Deleting it also empties `World::m_component_observers`, which
`World::on_component_changed` copies on every component add and remove — an
allocation per component change in the combat hot path, now gone with its only
production registrant.

The classified id lists the renderer walks (`render_unit_ids()`,
`render_building_ids()`, `render_other_ids()`) are built by
`publish_render_snapshot()` and only exist on a snapshot world.

Rally and patrol markers are restricted to the local human player, and are
hidden entirely while spectating.

## The two-phase dance

The renderer works like a recording studio. In the first phase, we record: game logic tells us "there are 5000 soldiers here, some trees over there, a river running through." The SceneRenderer listens to all of this and writes down lightweight commands into something called a DrawQueue. No actual OpenGL happens yet—we're just taking notes.

In the second phase, we play it back. We sort all those commands by material, shader, and transparency so that similar things get drawn together. Then the Backend walks through that sorted list and actually talks to the GPU. This separation is the key insight that makes everything else work. By splitting "what to draw" from "how to draw it," we can sort for optimal GPU performance, we can record frame N+1 while the GPU is still rendering frame N, and we can test our rendering logic without needing OpenGL at all.

Here's how a single frame flows through the system:

```
#
                           ┌─────────────────────────────────────┐
                           │           Qt Render Thread          │
                           │ (prefers GL 4.5; requires 3.3 Core) │
                           └──────────────┬──────────────────────┘
                                          │
                                          ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                              PHASE 1: RECORDING                              │
│                                                                              │
│   ┌─────────────┐      ┌──────────────────┐      ┌───────────────────────┐   │
│   │ GameEngine  │─────▶│  SceneRenderer   │─────▶│  Entity Renderers     │   │
│   │  ::render() │      │  ::begin_frame() │      │  (spearman, archer,   │   │
│   └─────────────┘      └──────────────────┘      │   terrain, trees...)  │   │
│                                                   └───────────┬───────────┘  │
│                                                               │              │
│                                                               ▼              │
│                                                   ┌───────────────────────┐  │
│                                                   │      DrawQueue        │  │
│                                                   │  (just data, no GL)   │  │
│                                                   │                       │  │
│                                                   │  • MeshCmd            │  │
│                                                   │  • CylinderCmd        │  │
│                                                   │  • TerrainChunkCmd    │  │
│                                                   │  • GrassBatchCmd      │  │
│                                                   │  • 20+ more types...  │  │
│                                                   └───────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────────┘
                                          │
                                          ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                              PHASE 2: PLAYBACK                               │
│                                                                              │
│   ┌──────────────────┐         ┌─────────────────────────────────────────┐   │
│   │  SceneRenderer   │────────▶│              Backend                    │   │
│   │  ::end_frame()   │  sort   │          ::execute()                    │   │
│   │  (sorts queue,   │  then   │                                         │   │
│   │   swaps buffer)  │  hand   │  Dispatches to specialized pipelines:   │   │
│   └──────────────────┘  off    │                                         │   │
│                                │  ┌─────────────┐  ┌─────────────────┐   │   │
│                                │  │  Cylinder   │  │   Vegetation    │   │   │
│                                │  │  Pipeline   │  │   Pipeline      │   │   │
│                                │  └─────────────┘  └─────────────────┘   │   │
│                                │  ┌─────────────┐  ┌─────────────────┐   │   │
│                                │  │  Terrain    │  │   Character     │   │   │
│                                │  │  Pipeline   │  │   Pipeline      │   │   │
│                                │  └─────────────┘  └─────────────────┘   │   │
│                                │  ┌─────────────┐  ┌─────────────────┐   │   │
│                                │  │  Effects    │  │   Mesh          │   │   │
│                                │  │  Pipeline   │  │   Instancing    │   │   │
│                                │  └─────────────┘  └─────────────────┘   │   │
│                                └─────────────────────────────────────────┘   │
│                                                   │                          │
│                                                   ▼                          │
│                                     ┌─────────────────────────┐              │
│                                     │    OpenGL Draw Calls    │              │
│                                     │  glDrawElements(...)    │              │
│                                     │  glDrawElementsInstanced│              │
│                                     └─────────────────────────┘              │
└──────────────────────────────────────────────────────────────────────────────┘
                                          │
                                          ▼
                              ┌───────────────────────┐
                              │   Framebuffer         │
                              │   (presented by Qt)   │
                              └───────────────────────┘
```

The key files in this flow are [scene_renderer.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/scene_renderer.cpp) for the recording phase and [backend.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/backend.cpp) for playback.

## How Qt gets OpenGL running

Our 3D view lives inside a QML interface. Qt Quick provides something called QQuickFramebufferObject that handles all the threading complexity of running OpenGL alongside a declarative UI. We subclass it in [gl_view.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/ui/gl_view.cpp) to hook in our renderer.

The relationship between Qt and our rendering code looks like this:

```
┌────────────────────────────────────────────────────────────────────┐
│                         QML Layer                                  │
│                                                                    │
│   main.qml                                                         │
│   └── GLView {                                                     │
│           id: viewport                                             │
│           engine: gameEngine    ◄─── binds to GameEngine instance  │
│       }                                                            │
└────────────────────────────────────────────────────────────────────┘
                                   │
                                   │ Qt creates FBO, calls createRenderer()
                                   ▼
┌────────────────────────────────────────────────────────────────────┐
│  GLView : QQuickFramebufferObject        [ui/gl_view.h]            │
│                                                                    │
│  └── GLRenderer : QQuickFramebufferObject::Renderer                │
│          │                                                         │
│          ├── render()  ───────────▶  GameEngine::render()          │
│          │                                                         │
│          └── createFramebufferObject()                             │
│                  │                                                 │
│                  └──▶ Creates FBO with depth attachment            │
└────────────────────────────────────────────────────────────────────┘
```

When Qt's render thread starts up, it requests OpenGL 4.5 Core on Linux and
Windows and 4.1 Core on macOS. The renderer still has a strict 3.3 Core portable
floor. `GLView` notices the current context and creates a `GLRenderer` that holds
a pointer to the `GameEngine`. From then on, every frame Qt calls our render
method, which calls into `GameEngine` and kicks off the whole pipeline.

The actual creation happens in [gl_view.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/ui/gl_view.cpp) around line 30:

```cpp
auto GLView::createRenderer() const -> QQuickFramebufferObject::Renderer * {
  QOpenGLContext *ctx = QOpenGLContext::currentContext();
  if ((ctx == nullptr) || !ctx->isValid()) {
    qCritical() << "GLView::createRenderer() - No valid OpenGL context";
    return nullptr;
  }
  return new GLRenderer(m_engine);
}
```

We keep OpenGL 3.3 Core as the baseline so older hardware and Apple's driver can
render the complete game. A 4.3 context enables compute/SSBO/indirect GPU crowd
culling, 4.4 (or `ARB_buffer_storage`) enables persistent mapped streaming, and
4.5 is the preferred release tier. The Core profile means there is no legacy
fixed-function path—everything goes through shaders.

If you're ever debugging why nothing renders, the first place to check is whether the OpenGL context is actually valid. The code logs a warning if there's no context available, which usually means you're running in software mode where 3D won't work. Look for "No valid OpenGL context" in your logs.

## Recording what to draw

Here's the problem with naive rendering: imagine you have 10,000 entities with different meshes, textures, and shaders. If you draw them in whatever order the game logic hands them to you, you'll be constantly switching GPU state. Bind shader A, draw one mesh, bind shader B, draw one mesh, bind shader A again... Each state change costs about a microsecond, and 10,000 of them means 10 milliseconds gone just on switching. At 60 FPS you only have 16ms per frame, so you've already burned most of your budget on bookkeeping.

The solution is to record everything first, then sort it, then draw in the optimal order. That's what the DrawQueue is for. It's essentially a big list of command structs—things like "draw this mesh with this transform and this color" or "draw a cylinder from here to there." Each command is tiny, maybe 50-100 bytes, and contains no OpenGL calls. Just data.

The commands are defined in [draw_queue.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/draw_queue.h). Here's what a mesh command looks like:

```cpp
struct MeshCmd {
  Mesh *mesh = nullptr;
  Texture *texture = nullptr;
  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{1, 1, 1};
  float alpha = 1.0F;
  int material_id = 0;
  Shader *shader = nullptr;
};
```

There are over 20 command types: CylinderCmd for debug lines and spear shafts, TerrainChunkCmd for ground tiles, GrassBatchCmd for instanced vegetation, HealingBeamCmd for visual effects, and so on. They're all stored in a std::variant so the queue can hold any mix of them.

The SceneRenderer implements an interface called ISubmitter that entity renderers use to submit their draw requests. This interface is defined in [submitter.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/submitter.h):

```cpp
class ISubmitter {
public:
  virtual void mesh(Mesh *mesh, const QMatrix4x4 &model, const QVector3D &color,
                    Texture *tex = nullptr, float alpha = 1.0F,
                    int material_id = 0) = 0;
  virtual void cylinder(const QVector3D &start, const QVector3D &end,
                        float radius, const QVector3D &color, float alpha = 1.0F) = 0;
  virtual void selection_ring(const QMatrix4x4 &model, float alpha_inner,
                              float alpha_outer, const QVector3D &color) = 0;
  // ... about 15 more methods for different visual elements
};
```

### Wrapping a submitter

Some passes want to watch or adjust submissions on their way through without becoming
the destination. `DamageStateSubmitter` folds a damage tier into the material id;
`RiggedBodyProbeSubmitter` counts how many rigged bodies a unit renderer actually
emitted, so the tracing build can warn when one emits none.

The damage tier rides in the tens digit: `basic.frag` and `basic_instanced.frag` read
`u_material_id % 10` as the surface material (wood, metal, cloth) and
`u_material_id / 10` as the tier, so a sooted plank still shades as wood. It used to
substitute the whole id, which meant only untagged parts ever caught soot — every
plank, band, and awning on a burning building stayed factory-fresh.

Both derive from `ForwardingSubmitter`, which implements every `ISubmitter` method by
passing it to an inner submitter. A decorator then overrides only what it changes —
three methods for the damage one, one for the probe — instead of restating all sixteen.
Before that, each wrapper spelled out every method, and thirteen of the sixteen were
pure pass-throughs that told you nothing.

Read a decorator's override list as its specification: what is listed is what it does.
`BatchSubmitterAdapter` in `equipment_submit.h` is deliberately _not_ a
`ForwardingSubmitter` — it is a sink that collects into an `EquipmentBatch` rather than
forwarding anywhere, so it implements the interface directly.

When a Carthaginian spearman renderer wants to draw a torso, it calls the mesh method on the submitter. That method just packs the parameters into a MeshCmd struct and pushes it onto the queue. Fast and simple.

We use double-buffering on these queues. While the GPU is busy rendering the previous frame's queue, the CPU is filling up the next frame's queue. The swap happens in [scene_renderer.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/scene_renderer.cpp) at the frame boundary:

```cpp
void Renderer::end_frame() {
  if (m_paused.load()) {
    return;
  }
  if (m_backend && (m_camera != nullptr)) {
    std::swap(m_fill_queue_index, m_render_queue_index);
    DrawQueue &render_queue = m_queues[m_render_queue_index];
    render_queue.sort_for_batching();
    m_backend->set_animation_time(m_accumulated_time);
    m_backend->execute(render_queue, *m_camera);
  }
}
```

We swap pointers—the GPU gets the fresh queue, and we start recording into the now-empty old one. No locks needed because CPU and GPU never touch the same queue at the same time.

## Sorting for speed

Before we hand the queue to the backend, we sort it. The sorting has a few priorities:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         SORTING PRIORITY                                    │
│                                                                             │
│   1. Opaque objects first, transparent objects last                         │
│      (transparent needs back-to-front order for correct blending)          │
│                                                                             │
│   2. Within opaque: group by shader                                         │
│      (switching shader programs is expensive)                               │
│                                                                             │
│   3. Within same shader: group by texture                                   │
│      (texture binds are moderately expensive)                               │
│                                                                             │
│   4. Within same texture: group by mesh                                     │
│      (enables instancing - draw 1000 trees in 1 call)                       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

Before sorting:                          After sorting:
┌────────────────────────┐               ┌────────────────────────┐
│ soldier (shader A)     │               │ soldier (shader A)     │
│ tree (shader B)        │               │ soldier (shader A)     │
│ soldier (shader A)     │               │ soldier (shader A)     │
│ grass (shader C)       │     ───▶      │ tree (shader B)        │
│ soldier (shader A)     │               │ tree (shader B)        │
│ tree (shader B)        │               │ grass (shader C)       │
│ river (transparent)    │               │ river (transparent)    │
└────────────────────────┘               └────────────────────────┘
      7 state changes                          3 state changes
```

This sorting pass is what transforms a random pile of draw requests into something the GPU can chew through efficiently. The difference between sorted and unsorted can easily be 2-3x in frame time.

### The pass byte outranks the transparency bucket

Priority 1 above is only true *within* a render pass. `SortIdentity` packs `pass` at bit 56 and `pipeline` at bit 48, both above `transparency_bucket` at bit 44, and `RenderPassOrder` puts `TerrainScatter = 2` a long way ahead of `Mesh = 5`. So every scatter command — grass, stones, tents, camp fires — drew before every wall, building and creature, whatever its alpha. A camp fire standing in front of a wall was blended into the framebuffer first, wrote no depth (correctly, it is transparent), and then the wall drew over it and covered it. `scatter_species_is_blended()` in `render/draw_commands.h` is the fix: a blended scatter species is re-homed into the `Mesh` pass with `transparency_bucket = 1`, so it sorts after every opaque pipeline in that pass. Add a species to that predicate whenever its executor turns blending on.

The mirror-image rule is that **opaque geometry must write depth**. `Stone` was the one opaque scatter species running under `DepthMaskScope(false)`, so it left the depth buffer holding the terrain behind it; every tree and prop in the later `Mesh` pass then passed its own depth test and painted straight over stones that were plainly in front. `tests/render/draw_queue_sort_order_test.cpp` pins both halves.

## The backend and its pipelines

The Backend class in [backend.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/backend.cpp) is where OpenGL finally gets involved. It inherits from QOpenGLFunctions_3_3_Core, which gives it access to all the GL functions without polluting the global namespace.

Rather than having one giant loop that handles every command type, we split things into specialized pipelines:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            Backend Pipelines                                │
│                                                                             │
│  ┌─────────────────────┐   ┌─────────────────────┐   ┌──────────────────┐   │
│  │  CylinderPipeline   │   │  VegetationPipeline │   │ TerrainPipeline  │   │
│  │                     │   │                     │   │                  │   │
│  │  • spear shafts     │   │  • instanced grass  │   │  • ground chunks │   │
│  │  • debug lines      │   │  • trees (pine,     │   │  • roads         │   │
│  │  • selection rings  │   │    olive)           │   │  • riverbeds     │   │
│  └─────────────────────┘   │  • plants           │   └──────────────────┘   │
│                            └─────────────────────┘                          │
│  ┌─────────────────────┐   ┌─────────────────────┐   ┌──────────────────┐   │
│  │  CharacterPipeline  │   │  EffectsPipeline    │   │ BannerPipeline   │   │
│  │                     │   │                     │   │                  │   │
│  │  • humanoid bodies  │   │  • healing beams    │   │  • unit banners  │   │
│  │  • horses           │   │  • combat dust      │   │  • flags         │   │
│  │  • elephants        │   │  • rain             │   │                  │   │
│  └─────────────────────┘   │  • auras            │   └──────────────────┘   │
│                            └─────────────────────┘                          │
│  ┌─────────────────────┐   ┌─────────────────────┐                          │
│  │  WaterPipeline      │   │ MeshInstancingPipe  │                          │
│  │                     │   │                     │                          │
│  │  • rivers           │   │  • batched meshes   │                          │
│  │  • riverbanks       │   │  • buildings        │                          │
│  └─────────────────────┘   └─────────────────────┘                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

`Backend::initialize()` brings fifteen of these up through one `create_pipeline()` template: it constructs the pipeline into its `unique_ptr` slot, calls `initialize()`, logs the same way for every one, and returns false so the caller can bail. Adding a pipeline is one three-line block rather than a seven-line copy of the create-check-log dance. `RiggedCullPipeline` stays outside the helper on purpose — it has a different constructor, takes its shader cache through a setter, and is optional, so a failure resets the slot and carries on instead of failing the whole backend.

Each pipeline understands the specific needs of its command type and can optimize accordingly. The main execute loop walks through the sorted queue and delegates to the appropriate pipeline. Here's a simplified view from [backend.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/backend.cpp):

```cpp
void Backend::execute(const DrawQueue &queue, const Camera &cam) {
  const QMatrix4x4 view_proj = cam.get_projection_matrix() * cam.get_view_matrix();

  const std::size_t count = queue.size();
  std::size_t i = 0;
  while (i < count) {
    const auto &cmd = queue.get_sorted(i);
    switch (cmd.index()) {
    case CylinderCmdIndex: {
      // Batch all consecutive cylinders together
      m_cylinderPipeline->m_cylinderScratch.clear();
      do {
        const auto &cy = std::get<CylinderCmdIndex>(queue.get_sorted(i));
        // ... pack into instance buffer ...
        ++i;
      } while (i < count && queue.get_sorted(i).index() == CylinderCmdIndex);

      // Draw all cylinders in one instanced call
      m_cylinderPipeline->draw_cylinders(instance_count);
      continue;
    }
    // ... handle other command types ...
    }
  }
}
```

When it hits a run of cylinder commands, it collects them all into a scratch buffer and draws them all in one instanced call. This is where the earlier sorting pays off—similar commands cluster together so batching opportunities are easy to spot. Drawing 1000 cylinders individually would be 1000 draw calls. Instanced, it's just 1.

For managing OpenGL state, we use RAII wrappers defined in [state_scopes.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/state_scopes.h). There's a DepthMaskScope that saves the current depth write setting, applies a new one, and restores the old one when it goes out of scope:

```cpp
struct DepthMaskScope {
  GLboolean prev;
  DepthMaskScope(bool enableWrite) {
    glGetBooleanv(GL_DEPTH_WRITEMASK, &prev);
    glDepthMask(enableWrite ? GL_TRUE : GL_FALSE);
  }
  ~DepthMaskScope() { glDepthMask(prev); }
};
```

Same pattern for blending, depth testing, polygon offset. This prevents the classic bug where you disable depth writes for some transparent effect and forget to turn them back on, breaking everything that draws afterward.

## Where OpenGL actually lives

All the low-level OpenGL code is concentrated in the [render/gl](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl) folder:

```
render/gl/
├── backend.cpp/.h          # Main command executor, pipeline coordinator
├── mesh.cpp/.h             # VAO/VBO/EBO wrapper
├── shader.cpp/.h           # GLSL program wrapper with uniform caching
├── texture.cpp/.h          # Texture loading and binding
├── buffer.cpp/.h           # Generic buffer abstraction
├── camera.cpp/.h           # View/projection matrices
├── resources.cpp/.h        # Built-in meshes (quad, cube, cylinder)
├── shader_cache.cpp/.h     # Loads and caches shader programs
├── state_scopes.h          # RAII wrappers for GL state
├── persistent_buffer.h     # Persistent mapped buffers for streaming
└── backend/                # Individual pipeline implementations
    ├── mesh_buffers.cpp/.h  # StaticMeshBuffers: the GL handles one mesh owns
    ├── cylinder_pipeline.cpp/.h
    ├── terrain_pipeline.cpp/.h
    ├── prop_mesh_builder.cpp/.h        # append_* geometry helpers, shared
    ├── vegetation_pipeline.cpp/.h      # plumbing: init, shutdown, uniforms, upload
    ├── vegetation_pipeline_natural.cpp     # stone, plant, pine, olive, dead tree, ore
    ├── vegetation_pipeline_settlement.cpp  # camp, tent, cart, rack, ruins, home, statue, shrine
    └── ...
```

`VegetationPipeline` is one class across four translation units. It had reached ~3000
lines in a single file by stacking three unrelated concerns: the pipeline plumbing every
prop shares, a general-purpose mesh-building library, and the per-prop authoring of
fourteen `initialize_*_pipeline()` methods. The mesh helpers were file-static, so nothing
else could call or test them despite being pure functions over a vertex/index pair; they
now live in `prop_mesh_builder.h` behind the named types `PropMeshVerts` and
`PropMeshIndices`. The authoring methods split by domain — natural scatter versus
settlement props — because those two groups are edited for different reasons and almost
never together.

A pipeline that owns static geometry keeps it in `StaticMeshBuffers` — the VAO, the vertex buffer, the index buffer, an optional instance buffer, and the two counts — and tears it down with `release_mesh_buffers()`, which no-ops safely when no GL context is current. That one struct replaced twenty-six hand-written groups of loose `GLuint`/`GLsizei` members and their per-mesh `shutdown_*()` functions across the vegetation, combat dust, healer aura, healing beam, primitive batch and cylinder pipelines. `VegetationPipeline::shutdown()` and `CombatDustPipeline::release_geometry()` now just walk their mesh list.

Three pipelines deliberately stay off it because their handles are a different shape: `terrain_pipeline`'s grass draws from arrays and has no index buffer, `rain_pipeline` names its buffers differently, and `rigged_cull_pipeline` owns compute-shader SSBOs rather than a mesh. Forcing those into the struct would make it mean less, not more.

### Billboard effects all take one path

Combat dust, building flames, burning flames, fireballs, stone impacts and metal sparks are the same draw: one camera-facing billboard mesh, instanced, with a per-instance position, colour, radius, intensity, clock and — for sparks — a direction. The only thing that varies is which `EffectType` the fragment shader branches on. So `effects_command_executor.cpp` maps `EffectBatchCmd::Kind` to `EffectType` once in `billboard_effect_type()`, builds the instance in `make_dust_instance()`, and runs every one of those six kinds through a single `case` block calling `render_dust_batch()`. The batched path (a run of consecutive effect commands) and the single-command path share both helpers, so they cannot drift apart.

Blood pools stay separate: they carry rotation, aspect ratio and a seed instead of colour and intensity, and draw from their own mesh with their own shader.

Every class that touches OpenGL inherits from QOpenGLFunctions_3_3_Core. This is Qt's way of giving you function pointers to OpenGL without relying on a global loader:

```cpp
class Mesh : protected QOpenGLFunctions_3_3_Core { ... };
class Shader : protected QOpenGLFunctions_3_3_Core { ... };
class Backend : protected QOpenGLFunctions_3_3_Core { ... };
```

The Mesh class in [mesh.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/mesh.cpp) wraps VAOs, VBOs, and index buffers. You give it vertex data and indices, and it lazily uploads them to the GPU on first draw:

```cpp
void Mesh::draw() {
  if (!prepare_draw("Mesh::draw")) {
    return;
  }
  glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()),
                 GL_UNSIGNED_INT, nullptr);
  m_vao->unbind();
}

void Mesh::draw_instanced(std::size_t instance_count) {
  if (instance_count == 0) {
    return;
  }
  if (!prepare_draw("Mesh::draw_instanced")) {
    return;
  }
  glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(m_indices.size()),
                          GL_UNSIGNED_INT, nullptr,
                          static_cast<GLsizei>(instance_count));
  m_vao->unbind();
}
```

The Shader class in [shader.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/shader.h) wraps GLSL programs and caches uniform locations. Looking up a uniform location is a string hash operation on the GPU driver side—not catastrophically slow, but slow enough that you don't want to do it every frame for every uniform. So we cache:

```cpp
class Shader : protected QOpenGLFunctions_3_3_Core {
  GLuint m_program = 0;
  std::unordered_map<std::string, UniformHandle> m_uniform_cache;

  // Cached lookup - fast path after first access
  auto uniform_handle(const char *name) -> UniformHandle;

  // Set uniforms by cached handle (fast) or by name (convenience)
  void set_uniform(UniformHandle handle, const QMatrix4x4 &value);
  void set_uniform(const char *name, const QMatrix4x4 &value);
};
```

The rest of the rendering code doesn't call OpenGL directly. It talks through these abstractions, which means we could theoretically swap backends someday (though OpenGL is deeply baked in, so this is more of an architectural nicety than a real possibility).

The renderer is split into explicit capability tiers:

| What we use                   | Why                                                             |
| ----------------------------- | --------------------------------------------------------------- |
| Vertex arrays (VAO)           | Group vertex attribute state                                    |
| Instanced rendering           | Draw 1000 trees in 1 call                                       |
| Depth testing                 | Hidden surface removal                                          |
| Alpha blending                | Transparent effects                                             |
| Polygon offset                | Fix z-fighting on terrain                                       |
| GLSL 330 shaders              | All visual computation                                          |
| GLSL 430 compute/SSBO shaders | GPU crowd culling on OpenGL 4.3+                                |
| Persistent buffer storage     | Lower-overhead streaming on OpenGL 4.4+ or `ARB_buffer_storage` |

The 4.3 crowd path dispatches compute shaders and submits an indirect indexed
draw. It is capability-gated and falls back to the 3.3 instanced path. We do not
use geometry or tessellation shaders, and no draw path currently depends on a
4.5-only API.

## How buildings rot

`get_building_state()` in `entity/building_state.h` maps a building's health ratio onto
three states: `Normal` above 70%, `Damaged` above 30%, `Destroyed` below it. Every
building resolves its state per frame and picks an archetype out of a
`BuildingArchetypeSet` — three pre-built `RenderArchetype`s, one per state, all baked
from one `BuildingArchetypeDesc`. Nothing is recomputed while a building burns; the
switch is a pointer swap.

There are three layers to the degraded look, and it matters that they stack:

1. **Parts drop out.** Each part carries a `BuildingStateMask`. Merlons, roof standards,
   awnings, and market wares are masked to the states where they still exist. This is
   authored per building, and it is what shapes the silhouette.
2. **Surviving parts are re-tinted.** `build_building_archetype()` runs every
   non-palette colour through `decayed_color()` (`entity/building_decay.h`) before it
   reaches the builder. The ramp desaturates, darkens brightness-proportionally, then
   mixes patchily toward ash, rot-green, and stone dust using a per-part hash. The
   darkening is scaled by luminance on purpose: a flat multiply turned dark timber into
   near-black mud while barely touching white limestone. `Normal` returns the authored
   colour untouched, so a healthy building is byte-identical to what its author drew.
3. **Ruin dressing is added.** `add_ruin_dressing()` scatters a rubble field, charred
   beams, scorch patches, and collapsed roof slabs, all masked to `Damaged` /
   `Destroyed`. Damaged rubble uses `ring_bias` to hug the outside of the footprint at
   terrain height, where it is visible; destroyed rubble fills the shell.

Before this, degradation was subtractive only: a ruined home was a shorter, roofless,
_pristine white_ box that read as unfinished construction rather than as a ruin.

Walls and gates were worse — `build_wall_archetype_set()` and
`build_wall_gate_archetype()` both hard-coded `BuildingState::Normal`, so the most
frequently attacked structures in the game rendered identically at full health and at
one hit from collapse. They now build the full three-state set: stakes snap at
deterministic indices, rails and braces fall, merlons break off their coping, and the
masonry core drops to a stump.

Two things deliberately do _not_ decay: palette parts (team colour must stay readable
for identification) and the shader-side soot, which is a separate signal applied through
the material tier described above.

The whole matrix is reviewable in seconds with
`building_preview --states <outdir>`, which renders every type × nation × state into one
contact sheet.

## How nations get their look

Roman legionaries wear red cloaks and carry rectangular shields. Carthaginian infantry have purple tunics and round shields. The underlying skeleton is the same, but the visual details differ. We handle this with a renderer hierarchy.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       Humanoid Renderer Hierarchy                           │
│                                                                             │
│                    ┌─────────────────────────┐                              │
│                    │  HumanoidRendererBase   │  [humanoid/rig.h]            │
│                    │                         │                              │
│                    │  • compute_pose()       │  ◄── shared animation logic  │
│                    │  • draw_common_body()   │  ◄── shared body rendering   │
│                    │  • render()             │  ◄── orchestrates everything │
│                    │                         │                              │
│                    │  virtual:               │                              │
│                    │  • get_variant()        │  ◄── colors, equipment       │
│                    │  • draw_armor()         │  ◄── nation-specific armor   │
│                    │  • draw_helmet()        │  ◄── nation-specific helmet  │
│                    └───────────┬─────────────┘                              │
│                                │                                            │
│              ┌─────────────────┼─────────────────┐                          │
│              │                 │                 │                          │
│              ▼                 ▼                 ▼                          │
│  ┌───────────────────┐  ┌───────────────┐  ┌───────────────┐                │
│  │  Carthage         │  │  Roman        │  │  (future      │                │
│  │  SpearmanRenderer │  │  Spearman...  │  │   nations)    │                │
│  │                   │  │               │  │               │                │
│  │  purple tunics    │  │  red cloaks   │  │               │                │
│  │  round shields    │  │  rectangular  │  │               │                │
│  │  bronze helmets   │  │  steel helms  │  │               │                │
│  └───────────────────┘  └───────────────┘  └───────────────┘                │
│                                                                             │
│  Located in: render/entity/nations/carthage/                                │
│              render/entity/nations/roman/                                   │
└─────────────────────────────────────────────────────────────────────────────┘
```

The base class HumanoidRendererBase in [humanoid/rig.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/humanoid/rig.h) handles everything that's common to all humanoids: computing the pose from animation state, drawing the basic body parts, coordinating the rendering sequence. But it has virtual methods for the nation-specific bits.

Each nation has derived classes that override these methods. Looking at the Carthaginian spearman in [spearman_renderer.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/entity/nations/carthage/spearman_renderer.cpp), you'll see it sets up purple tunics, bronze helmets, and the distinctive Carthaginian visual style.

The entity system stores a unit type string like "spearman_carthage" on each unit. The EntityRendererRegistry in [registry.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/entity/registry.cpp) maps these strings to renderer functions. When it's time to draw, we look up the right renderer and call it. If a unit type isn't registered, it just doesn't render—that's usually the first thing to check when soldiers are mysteriously invisible.

Troop bodies no longer get per-nation shader files. The rigged creature backend owns the shared singleton `character_skinned` fallback and `character_skinned_gpu_instanced` full-detail program, while nation, role, and equipment variation is supplied as declarative render data: palette values, material IDs, visual specs, equipment records, and texture slots.

## Centralized pose selection

Every humanoid entity produces exactly one `AnimationInputs` struct per render frame—a snapshot of its state from game components (is it attacking? dying? constructing?). Previously, each downstream system had its own if/else chain to map these flags to the animation it needed. We replaced all of those with a single function.

```
AnimationInputs  ──►  resolve_pose_intent()  ──►  PoseIntent
                          [pose_intent.h]
                                │
               ┌────────────────┼────────────────┐
               ▼                ▼                ▼
   to_humanoid_state()  to_animation_state_id()  ArchetypeVariantTable
   [clip driver]        [BPAT key builder,        [creature_render_graph.cpp]
                         render graph]
```

`PoseIntent` is a small enum (17 values) that names the _canonical_ animation intent—`Idle`, `Walk`, `Run`, `AttackMelee`, `Dying`, etc.—in strict priority order. The resolver applies that priority once: dying beats dead, dead beats hit-reaction, hit-reaction beats attacking, and so on. All downstream systems convert from this single value instead of re-implementing the priority logic.

**ArchetypeVariantTable** is the data-driven replacement for the old runtime hook function pointers. Each `UnitVisualSpec` can carry an optional pointer to a `constexpr`-constructible table that maps:

- `archetype_for_pose[intent]` / `state_for_pose[intent]` — override archetype or clip state for a specific intent
- `archetype_for_variant[k]` / `state_for_variant[k]` — per-variant overrides (e.g. by seed for builders, by `FacialHairStyle` for spearmen), optionally gated on a trigger intent

Nation renderers that need per-variant animation (builders with different tools, spearmen with beards) simply fill in the table's arrays at static init time instead of registering a function pointer.

### Equipment archetypes resolve through one engine

Putting equipment on a creature means deriving a new archetype from a base one:
take the base descriptor, append each item's bake attachments, extend the role-colour
count, and register the result — memoised on (base archetype, debug name, handle list)
so a loadout is only built once.

That logic is identical for riders and for mounts, and it used to exist as two
169-line files that differed only in the word "humanoid" versus "horse". It now lives
once in `EquipmentArchetype::Resolver` (`render/equipment/equipment_archetype_resolver.h`).
`humanoid_equipment_archetype.cpp` and `horse_equipment_archetype.cpp` are thin wrappers
that own one `Resolver` each and keep their existing public functions, so no call site
changed.

The two resolvers stay separate instances on purpose: each owns its own contribution
registry, memo cache and mutex. Merging them into one registry would put rider and mount
equipment handles in the same key space, and a handle registered for one domain would
silently become visible to the other.

**Caching.** `CreatureRenderBatch::add_humanoid` calls `resolve_pose_intent` once and passes the result into both `humanoid_state_for_anim` (via its two-argument overload) and the variant-table dispatch block. No system in the critical render path calls the resolver more than once per entity per frame.

### Sword points are flattened cones, not spikes

Every sword in the game — both nations, every commander variant, the mounted riders, and
the baked static attachment — comes out of the single `sword_archetype` builder in
`render/equipment/weapons/sword_renderer.cpp`. The blade below the point is a stack of
oriented boxes: wide in the guard axis, thin (about a sixth of the blade width) in the
flat axis.

The point used to be `cone_from_to(blade3, blade4, tip_half_w)`, a _round_ cone whose
radius came from `blade_tip_width_scale`. On the Roman gladius that is a 0.010 radius
stuck on the end of a blade 0.065 half-wide, so the last fifth of every sword read as a
needle welded to a blade. The fix is `oriented_cone_between`, which takes the same basis
as `oriented_box_between` but doubles the length column, because the unit cone spans
±0.5 in Y while the unit cube spans ±1. Feeding it the blade's own half-width and
half-depth makes the point an elliptical cone whose base exactly matches the cross
section it grows out of: a triangle seen from the flat side, a thin wedge seen edge-on,
continuous silhouette from both.

Consequence for the config: `blade_tip_width_scale` no longer sets the width of anything.
It only shifts `tip_start_dist`, so a smaller value means the point starts further back
and the triangle is longer and keener. The point's width is always the blade's width —
anything else reintroduces the shoulder step that made the old tip look like a pin.

> For the full animation/pose pipeline — bake step, BPAT clip resolution and sampling,
> the procedural locomotion shaper, the combat visual state machine with marker-driven
> melee damage sync, and the shared quadruped gait — see
> [ANIMATION_ARCHITECTURE.md](ANIMATION_ARCHITECTURE.md).

## Procedural shaders

Here's a memory problem: if 5000 soldiers each need unique 4K textures for their rust, dirt, and wear patterns, that's around 80 gigabytes of VRAM. Obviously impossible. So instead we generate all that detail procedurally in the shader.

The shaders in [assets/shaders](https://github.com/djeada/Standard-of-Iron/blob/main/assets/shaders) use hash functions and noise for pipeline-owned effects such as terrain, water, vegetation, particles, banners, and shared rigged bodies.

Those helpers live in `assets/shaders/include/noise.glsl` and are named after the algorithm
they implement, not the role they play: `soi_hash_82bbee`, `soi_hash_f8bd2f` and so on, with
the suffix being a hash of the body. That looks odd until you know the history — the same
name meant two different functions in different shaders (`hash12` was a sine-dot hash in the
prop shaders and a p3-fract hash in `grid`/`combat_dust`), so consolidating under the plain
names would have silently restyled half of them. The suffix makes "same name, same maths" an
invariant. Prefer an existing entry over adding a fourth spelling of the same hash. The
include resolver in `shader.cpp` guards against double inclusion, so including it is free.

A typical procedural fragment block looks like this:

```glsl
// Hash function - turns any position into a pseudo-random number
float hash2(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

// Multi-octave noise - combines multiple frequencies for natural-looking patterns
float fbm(vec2 p) {
  float v = 0.0;
  float a = 0.5;
  mat2 rot = mat2(0.87, 0.50, -0.50, 0.87);
  for (int i = 0; i < 5; ++i) {
    v += a * noise(p);
    p = rot * p * 2.0 + vec2(100.0);
    a *= 0.5;
  }
  return v;
}
```

Each soldier's world position plus a random seed produces unique wear patterns. High-frequency noise creates scratches. Low-frequency noise creates larger rust patches. Sine waves create fabric weave patterns. All of this costs some GPU compute time but zero extra memory.

A typical fragment shader checks the material ID to know what kind of surface it's shading:

```glsl
if (u_materialId == 2) { // Metal armor
  // Procedural rust based on world position
  float rust = fbm(v_worldPos.xz * 10.0);
  vec3 rustColor = vec3(0.5, 0.3, 0.1);
  baseColor = mix(baseColor, rustColor, rust * 0.3);
}
else if (u_materialId == 1) { // Cloth
  // Fabric weave pattern
  vec2 uv = v_worldPos.xz * 50.0;
  float weave = sin(uv.x) * sin(uv.y) * 0.1 + 0.9;
  baseColor *= weave;
}
```

Geometry variation now belongs in mesh/spec generation instead of per-troop GLSL. When a vertex shader needs procedural deformation, it should be owned by a backend pipeline rather than selected by a troop renderer:

```glsl
if (u_materialId == 4) {  // Shield
  float curveRadius = 0.52;
  float curveAmount = 0.46;
  float angle = position.x * curveAmount;

  float curved_x = sin(angle) * curveRadius;
  float curved_z = position.z + (1.0 - cos(angle)) * curveRadius;
  position = vec3(curved_x, position.y, curved_z);
}
```

The shader directory is intentionally small and pipeline-owned. The ShaderCache in [shader_cache.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/shader_cache.cpp) preloads shared backend programs and keeps them around so we don't recompile every frame.

### Ground detail lives in three frequency bands

[`terrain_chunk.frag`](https://github.com/djeada/Standard-of-Iron/blob/main/assets/shaders/terrain_chunk.frag) is the biggest procedural shader in the project, and the thing that makes it read as ground rather than as a painted plane is which _scale_ each layer works at. One world tile is one metre, and `world_coord` is measured in tiles, so a noise frequency of `f` means a feature wavelength of `1/f` metres.

There are three bands and each one has a job:

- **Biome fields**, driven by `u_macro_noise_scale` (~0.03), run from six metres to fifty. They decide where the map is wet, grazed, exposed or stony. At the battle camera you see two or three of these at once, so on their own they read as soft blobs.
- **The ground mosaic** (`k_mosaic_frequency`, `k_fleck_frequency`) fills roughly 0.3 m to 4.5 m. This is the band the battle camera actually resolves, and it carries the dry/lush patchwork and the small bare scuffs that make the surface look like a material.
- **Surface grain** (`u_detail_noise_scale` and up) lives below half a metre. It only shows when the camera is close.

Every layer runs through `band_limit(footprint, frequency)`, which fades a frequency out once it approaches one cycle per pixel. That is what keeps the ground from shimmering when the camera pulls back, but it is also a trap: anything authored above roughly one cycle per metre is invisible at normal play distance, so pushing detail _up_ in frequency to make the ground look busier does nothing except cost ALU. If the ground looks flat, the fix is almost always a layer in the middle band, not a finer one.

Shading uses the height texture for more than normals. `terrain_sky_openness()` walks six directions at three radii and keeps the steepest horizon in each, which gives a sky-occlusion term for hollows and cliff feet. Open ground still measures some horizon, so it is remapped through `smoothstep(0.35, 0.92, ...)` before use -- otherwise the whole map would darken slightly rather than just the sheltered parts. That same sheltered term feeds drainage, exposure and snow drift, so hollows are damper, less scoured, and collect more snow.

Both that walk and `compute_curvature()` are pure functions of a height map that
never changes during a match -- together 23 dependent texture taps per pixel,
which thrash the texture cache in a way arithmetic does not. They are baked
instead. `TerrainRenderer::bake_terrain_fields()` evaluates the same formulas on
the CPU straight out of `m_height_data` at map load, one texel per tile, and
uploads them as the two channels of an `RG16F` texture; the fragment shader
reads `u_field_tex` once and skips both functions. The bake is on the CPU rather
than in an offscreen GL pass only because the height data is already there in
`std::vector<float>` form -- there is nothing to read back.

Baking at height-map resolution is not a compromise. The openness walk steps in
units of _texels_ (radii 1.5, 4 and 9) and the curvature stencil is +/-2 texels,
so neither field carries detail finer than the grid it is sampled from; the
bilinear filter that reads the baked texture is smoother than evaluating per
pixel, not coarser. Measured against a clean build of `ed7d5b71`: identical mean
brightness on Zama, Crossing the Alps and the mountain map, with 0.008% of
pixels differing by more than 8/255 on the two hilly maps, and terrain-surface
GPU time (draw-command type 8, `SOI_GPU_BREAKDOWN=1`) down from 6.20 ms to
5.75 ms averaged over six interleaved arena runs -- about 7%.

The larger half of the terrain shader's cost is still the `gradient_fbm` calls,
and those are **not** baked. Two things have to be understood before attempting
it, because neither is what the plan assumed.

First, `gradient_fbm` fades its octaves by `fwidth(p)`, so it is a function of
the screen footprint as well as the world position. A bake has to fix the octave
count and let the baked texture's own mip chain do the distance fade instead.

Second, and more limiting: **a third of the noise in this shader is not a
function of world position at all.** Three coordinate systems are in play, and
only one of them is bakeable.

| Coordinate     | Derived from                                  | Bakeable |
| -------------- | --------------------------------------------- | -------- |
| `world_coord`  | `v_world_pos.xz / tile_size + u_noise_offset` | yes      |
| `rock_coord`   | `mix(world_coord, wall_coord, wall_blend)`    | no       |
| `rill_coord`   | projected onto `flow_down = normal.xz`        | no       |
| `relief_coord` | `mix(world_coord, wall_coord, wall_blend)`    | no       |

`wall_blend` is a `smoothstep` of the slope and `wall_coord` picks its axis from
`step(abs(normal.x), abs(normal.z))`, so everything in the rock, rill and relief
families moves with the shading normal. `rock_detail`, `rock_grain`,
`rock_strata`, `bedding`, `scrub_field`, `rill_field`, `rill_shifted` and the
three `relief_octave()` calls are all off the table -- not because they need
per-pixel resolution, which is the reason the plan gave, but because a
world-space texture cannot represent them.

What is left is bakeable, and it is the expensive part -- these are `gradient_fbm`
at five octaves each, where the un-bakeable detail noise is mostly single-octave
`gradient_noise`. Frequencies are cycles per tile at the default
`macro_scale` 0.035 / `detail_scale` 0.14:

| Field               | Frequency | Field              | Frequency |
| ------------------- | --------- | ------------------ | --------- |
| `domain_warp` (x2)  | 0.015     | `grass_weave`      | 0.16      |
| `earth_field`       | 0.016     | `rock_breakup`     | 0.19      |
| `regional_field`    | 0.020     | `snow_edge_noise`  | 0.19      |
| `hue_field`         | 0.022     | `mosaic_field`     | 0.24      |
| `meadow_field`      | 0.037     | `sward_drift`      | 0.30      |
| `moisture_field`    | 0.063     | `mosaic_warp` (x2) | 0.31      |
| `thatch_field`      | 0.091     | `alpine_snow_*`    | 0.10-0.32 |
| `material_patch`    | 0.098     | `surface_detail`   | 0.39      |
| `alpine_snow_large` | 0.10      | `graze_drift`      | 0.77      |
| `soil_field`        | 0.168     | `tussock`          | 0.90      |
|                     |           | `fleck_field`      | 1.15      |

`fleck_field` at 1.15 cycles/tile sets the bake resolution: Nyquist wants 2.3
texels/tile, so 4 texels/tile with a mip chain. Everything else is an order of
magnitude coarser, so the cheap first cut is the sixteen fields at or below
0.4 cycles/tile in one atlas at 1 texel/tile, leaving `tussock`, `graze_drift`
and `fleck_field` procedural.

Keep grain, granular and speck procedural regardless: at 1.5, 4.3 and 12.9
cycles/tile they are per-pixel detail and already band-limited by
`band_limit(coord_footprint, ...)`.

## Fog of war: three states, one mask

The player sees the battlefield in three states, and all of them are driven by a
single low-resolution mask rather than by geometry.

`VisibilityService` owns the truth: one byte per terrain tile, `Unseen`,
`Explored` or `Visible`, recomputed on a worker thread at the interval in
`GameConfig::gameplay().visibility_update_interval` (not per frame). Once per
frame `Renderer::visibility_mask()` turns the current snapshot into an RGBA
texture, one texel per tile: red is "under live sight now", green is "seen at
some point". Both channels pass through a 3x3 tent kernel and the texture is
sampled with linear filtering, which is what turns the tile grid into a
feathered edge instead of a staircase.

Everything that belongs to the permanent map -- terrain chunks, roads, rivers,
riverbanks, and the scattered props -- includes
[`visibility_mask.glsl`](https://github.com/djeada/Standard-of-Iron/blob/main/assets/shaders/include/visibility_mask.glsl)
and calls `apply_visibility_memory()`. That one call discards fragments the
player has never seen and, on ground that is explored but not currently
watched, replaces the lit colour with a drained, cooled, dimmed version of
itself. Remembered terrain is therefore the real terrain rendered from memory,
props included -- not a grey sheet laid over it.

Only never-seen ground gets a fog layer. `FogRenderer` submits one instanced
batch of chunk-sized quads covering the unexplored regions and hands the
shader its own mask, so cost scales with the unexplored area in chunks rather
than with map size or unit count. Newly revealed tiles dissolve rather than
pop, and the layer subtracts live sight from its own opacity: soldiers stand
above the fog plane, so fog creeping past the sight edge would show them
apparently standing in it.

Two rules keep hidden activity hidden. Enemy units, enemy construction
previews and combat effects are gated with `FogExtent::Anchor`, which asks
about the tile the thing stands on rather than its whole bounding sphere -- the
footprint rule would render an entire formation whose flank merely clipped the
sight edge. Animated scatter (campfires) stays `ScatterMemoryMode::VisibleOnly`
so nothing keeps playing where nobody is looking, while static props use
`Remembered`.

Per-update cost stays small because the mask is uploaded by dirty rectangle:
the helper compares against the previous grid, grows a box by the blur's reach,
and re-encodes only that slice. On a 650x650 map a typical update costs about
0.1 ms of comparison and a microsecond of encoding, against ~2.6 ms for a
whole-grid encode. `arena_app --fog-of-war` runs the real thing in the Arena,
and the `fog_of_war_recon` scenario walks a patrol out and back so all three
states appear in one frame.

## Ground markers: one ring system

Everything the game draws flat on the ground to say "this is the thing you mean"
is one system: the blue ring under a selected unit, the yellow one under a
hovered unit, the hostile outline attack mode puts on a valid target, and the
projectile range rings a selected archer or catapult publishes. They are one
command (`GroundMarkerCmd`), one shader (`assets/shaders/ground_marker.*`), one
annulus mesh, and — because the sort key groups them into a single prepared batch
— **one instanced draw call per frame for every marker on screen**, whatever
their colours, radii or patterns.

`ISubmitter::ground_marker` is the only entry point. `selection_ring` and
`selection_ring_styled` still exist as non-virtual adapters that decompose the
old model matrix into a centre and a radius, so the world walk that draws
selection rings did not have to change.

Three properties are the point of the design:

- **Markers follow the ground.** The vertex shader samples the terrain height
  texture the terrain renderer already uploads (`TextureUnit::terrain_height`,
  published to the renderer by `TerrainRenderer::submit`) and places every ring
  vertex on the surface. A range ring 21 units across climbs a hill and is
  occluded by one instead of slicing through it. Without a height texture — an
  arena scene with no terrain, say — markers fall back to the centre's own height.
- **Patterns are data, not geometry.** `render/geom/ground_marker_pattern.h`
  holds one spec per `TeamPattern` (dash count and duty, an optional inner band,
  optional ticks). C++ uploads that table as `u_pattern_table`; the fragment
  shader reads it. Adding a pattern is a row in the table, not a new mesh, and
  the accessibility contract — every pattern differs in _shape_, so hue is never
  the only signal — is asserted against the table in
  `ground_marker_pattern_test.cpp` rather than against six meshes.
- **The radial coordinate is band-relative.** A marker carries an outer radius
  and a band thickness in world units; the mesh's radial coordinate is measured
  in band widths (`world_radius = outer_radius + (t - 1) * thickness`), which is
  why a 0.3-unit selection ring and a 21-unit range ring are equally crisp and
  why the inner band of a double ring and the ticks of a chevron sit at the same
  visual offset at any size. `k_marker_geometry_inner/outer` must stay wide
  enough to contain every pattern feature; a test checks it.

The fragment shader adds what the old per-pattern meshes could not: an
antialiased band edge, a soft glow around the line, and a slow pulse on markers
flagged `focused` (a single selection, the hovered target, the unit whose range
ring is the one being read). Colour still carries meaning — blue for selected,
gold for reach, red for hostile — but never alone.

RPG commander combat is on the same system. `RpgTelegraphRenderer` used to draw
six kinds of ring that differed only in hue — an enemy winding up, a staggered
enemy, your aim candidate, your locked target, a strike flash, a landed hit —
which is exactly the case a colour-vision mode collapses. Each role now maps to
a pattern in `marker_style()`: telegraph chevrons, a locked target's double ring
(and the shader's pulse, since "the one you mean" is what `focused` means
everywhere else), dashed for aim, dotted for stagger, solid for the strike
flash, notched for the hit. Where the _rate_ of a pulse carries meaning — a
telegraph speeds up from 4 Hz to 10 Hz as the wind-up completes — that stays in
the submitted alpha, because the shader's pulse is a fixed cadence.

## Activity indicators

Every unit the local player owns carries one small 3D item above its head saying
what it is doing right now: crossed swords for fighting, a shield for guarding,
a planted standard for holding, an axe for felling trees, a pickaxe for
quarrying, a wrench for repairs, a load going into a crate for hauling, a warning
triangle when it is stuck -- sixteen in all. There is exactly one source for that
answer,
`classify_unit_activity()` in `game/systems/unit_activity.cpp`, the same function
the selection panel reads, so the item over a unit can never disagree with the
text in the HUD. The simulation writes the result into
`CreaturePresentationComponent::activity` while it builds the render snapshot,
which is why the renderer classifies nothing itself and touches no gameplay
component to decide what to draw.

Every one of them uses the same rendering treatment -- a soft drop shadow and a
dark contour carrying the activity colour -- with a different icon. The shared
glyph builder gives sixteen unrelated symbols one optical weight.

The icons themselves are generated, not authored. `render/geom/icon_glyph.cpp`
is a small vector builder -- bars, arcs, rings, convex polygons, arrow heads --
and `mode_indicator.cpp` describes each icon as a handful of calls against it.
`begin_glyph()`/`end_glyph()` records those flat shapes and turns them into a
solid. It first unions overlapping bars, discs and polygons into one clean
silhouette, then extrudes side walls and lays a narrow contour and offset shadow
around only the exterior boundary. Internal primitive edges therefore never
become dark seams. A new icon is still only a few lines of 2D drawing, and it
does not need to be drawn at the right size because `fit_since()` scales the
emitted geometry down to the shared footprint.

Four properties are worth knowing before changing anything here:

- **Every badge is the same size in the world, for every unit.**
  `indicator_world_size()` is one constant, and every item mesh is normalised to
  the same local footprint by `normalize_extent()`, so a worker's order and an
  elephant's order are drawn at identical scale -- they say the same kind of
  thing, so they get the same voice. Only the anchor height still varies, enough
  to clear each unit's head. They do not grow when the camera pulls back either:
  an earlier revision held them at a constant pixel size and they read as UI
  stickers rather than objects in the world.
- **They face the camera, tilted.** The model matrix is built from the view
  matrix's right and up vectors with a small pitch (`k_indicator_tilt_radians`)
  so the extrusion catches perspective instead of presenting flat on.
- **They ignore depth.** The pass runs last with depth test and depth write off,
  so the order the mesh is emitted in is the order it composites in: shadow,
  halo, contour, plaque, rim, then the icon's outline, walls and face. A status
  read that a friendly unit can stand in front of is worse than useless.
- **State is colour, not geometry.** Queued, unavailable and interrupted tint the
  same mesh, because the instanced path carries only a colour and an alpha per
  instance and widening that vertex format would cost every other mesh batch.

Lighting is local to the item: `activity_indicator.glsl` shades against a fixed
light in the item's own space, so a camera-facing badge keeps a stable studio
highlight instead of flickering as the camera turns.

Cost is kept down in three places. Anchor height comes from the troop profile
once, cached in `CachedUnitData` and refreshed only when the spawn type or nation
changes, instead of a `TroopProfileService` lookup per unit per frame.
`ModeIndicatorCmd`s sort by kind, so the queue hands the backend one instanced
run per item rather than one draw per unit. And the meshes are built lazily, so a
match that never sees a dismantle order never uploads that item.

`arena_app --batch --scenario unit_activity_showcase` drives every gathering,
building and repair state in one scene. Note that the scenario spawns its units
under a non-local owner, so the order-marker indicators correctly stay hidden;
and its camera sits far enough out that world-sized items are only a few pixels
tall, so pass `--scenario-distance 0.34` to review them at gameplay zoom.

## Common problems and how to fix them

When nothing renders at all and you're just seeing a black screen, walk through this checklist:

1. Check if the OpenGL context is valid. Look for "No valid OpenGL context" in logs. If you see it, you're probably running in software mode.

2. Check if shaders compiled. The shader loading code in [shader.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/shader.cpp) logs errors, but you might want to add more verbose output.

3. Put a breakpoint in DrawQueue::submit to see if anything's actually being recorded. If the queue is empty, the problem is in the game logic, not the renderer.

4. Check the camera. Entities might be outside the view frustum. Print out the camera's position and view matrix.

5. Make sure the depth function isn't backwards. GL_GREATER instead of GL_LESS will flip everything.

When performance tanks, it's usually one of three things:

- Draw call explosion means batching isn't working. Check if you're using draw_instanced where you should be. A single non-instanced draw where instancing should happen can fragment your batches.

- State thrashing means commands aren't sorted properly. Fire up RenderDoc or Nsight and look at the call sequence. If you see shader/texture binds alternating rapidly, the sort isn't working.

- Vertex bloat means meshes are too detailed for how small they appear on screen. This points to the LOD system in [rig.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/humanoid/rig.h)—check the distance thresholds.

When specific units don't render but debug shapes do, the renderer probably isn't registered. Check [entity/registry.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/entity/registry.cpp) and make sure there's a registration call for that unit type. Missing registrations are the most common cause of invisible units.

Transparent objects rendering as opaque usually means blending got disabled somewhere, or the draw order is wrong so transparent stuff draws before what's behind it. Make sure the queue sorts transparent objects to the back and that the BlendScope RAII wrapper is being used.

When trees look frozen even though the biome states a wind profile, the sway is probably being applied in the wrong space or at the wrong magnitude. `pine_instanced.vert` and `olive_instanced.vert` displace the vertex after `model_pos * scale`, so the offset has to be multiplied by the instance `scale` to stay proportional — an absolute offset that reads as a breeze on a sapling is a couple of centimetres on a 5-unit olive and vanishes. The offset also has to be added _after_ the per-instance yaw `rot`, because anything added before it gets turned by each tree's random rotation and the grove ends up leaning in every direction at once instead of downwind. Both shaders bend by `height_factor²` so the trunk base stays planted, mix a wider displacement into the canopy than the trunk via `foliage_mask`, and share the same `normalize(vec2(0.78, 0.62))` wind direction so the whole treeline moves together. Peak tip travel lands near 5% of tree height at the default biome `sway_strength`. Shadows follow for free: the shadow pass replays the same scatter commands through the same shaders and reads `time` from `FoliageBatchParams`, not from the context's animation clock.

When part of a scatter prop shades almost black while the rest of the same mesh looks fine, suspect the face normals rather than the lighting. `append_oriented_box` and `append_barrel_yaxis` in [prop_mesh_builder.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/backend/prop_mesh_builder.cpp) emit inward-facing normals on some faces; prop rendering runs with culling disabled, so the geometry still draws but shades as if it were facing away from every light. Use `append_prop_beam` and `append_prop_taper` for new geometry—they are the same shapes with outward normals. `append_box` and `append_vert_prism` were always correct. The background and the full list of touchpoints for a new prop are in [docs/SETTLEMENT_ASSETS.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/SETTLEMENT_ASSETS.md).

### When the process dies inside the GPU driver

A `SIGSEGV` whose backtrace bottoms out in `libnvidia-glcore` (or any driver `.so`) during `QRhi::endFrame` on the `QSGRenderThread` is almost never a bug in the driver call you can see on the stack. The driver executes our recorded command stream when the frame is flushed, so the faulting call was made earlier in the same frame. Two shapes of app bug produce it:

- An instanced draw asking for more instances than its instance buffer holds. The GPU fetches past the end of the buffer, and the driver faults while walking its own bookkeeping. Every instanced pipeline therefore records how many instances it actually uploaded and routes its draw count through `InstanceDrawGuard` ([instance_draw_guard.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/backend/instance_draw_guard.h)), which clamps the draw to what is resident and logs the first overflow per buffer. Growing the buffer is capped (`k_max_instances_per_batch`), so `MeshInstancingPipeline::flush` splits an oversized batch into capacity-sized chunks rather than drawing past the cap.
- Deleting a GL object with no current context. `glDeleteBuffers` through `QOpenGLFunctions` with no context is undefined; on NVIDIA it corrupts driver state and the crash lands in an unrelated later frame. `Buffer` and `VertexArray` now check `QOpenGLContext::currentContext()` and leak the name with a warning instead. Leaking at teardown is free — the driver reclaims everything when the context dies.

Release builds compile out every `glGetError` check in the render layer, so misuse is silent in the build people actually run. To locate one, build with `-DCMAKE_BUILD_TYPE=Debug`, which keeps the checks in and names the offending call at the point it is made.

## Battle render optimizations

When more than 15 units are visible on screen, the `BattleRenderOptimizer` kicks in to keep rendering fresh without sacrificing visual quality. This system provides several tricks that work independently of LOD:

### Temporal culling

Static or idle units are rendered on alternating frames. If a unit isn't moving, selected, or hovered, it may be skipped on odd or even frames based on its entity ID. This effectively cuts the render load for idle units in half while remaining imperceptible to the player.

```
Frame 1: Render units with (entity_id + frame) % 2 == 0
Frame 2: Render units with (entity_id + frame) % 2 == 0  (different set)
```

Moving units, selected units, and hovered units always render every frame to maintain responsiveness.

### Animation throttling

When the visible unit count exceeds 30 and units are far from the camera (>40 units away), animation updates are throttled. Instead of computing new poses every frame, distant units update their animations every 2-3 frames. This saves significant CPU time during large battles while keeping close-up units fully animated.

### Enhanced batching

The batching ratio is boosted proportionally when more units are visible. This pushes more units into the primitive batching path, reducing draw call overhead during intense battles.

The optimizer can be configured via `BattleRenderConfig`:

- `temporal_culling_threshold`: Unit count that triggers temporal culling (default: 15)
- `animation_throttle_threshold`: Unit count that triggers animation throttling (default: 30)
- `animation_throttle_distance`: Distance beyond which animations are throttled (default: 40.0)
- `animation_skip_frames`: How many frames to skip for distant animations (default: 2)

See [battle_render_optimizer.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/battle_render_optimizer.h) for the implementation.

### Creature parts are a bake-time description, not a runtime one

`k_full_parts` in `humanoid_spec.cpp` (and the horse/elephant equivalents) is a list of
primitives — pectoral, elbow, calf, hand — but **none of it is submitted per frame**.
`tools/bpat_baker` runs as a build step, walks the part graph through
`render/rigged_mesh_bake.cpp`, and writes one skinned mesh per species/LOD to
`assets/creatures/<species>_<lod>.bprm`. Those files are gitignored build artifacts; they
regenerate whenever `render_gl` changes, so editing a part spec is enough to change what
ships.

At runtime `RiggedMeshRegistry::load_all()` loads the blobs and every creature is drawn
through a single `ISubmitter::rigged()` command. `submit_part_graph()` in
`render/creature/part_graph.cpp` — the per-part submission path — has no production
caller; it exists for the bake, for tooling and for tests.

The one exception is `RiggedMeshCache::get_or_bake_prehashed()`: a creature carrying
**bake attachments** (healer staves and robe overlays, for example) cannot use the shared
prebaked blob, so its mesh is baked on first use and cached by attachment hash. The
`--prewarm` flag does that up front and then sets `runtime_bake_forbidden()`, which turns
any later bake into a reported violation. `arena_app --batch --scenario performance_30v30
--prewarm` passing clean is the check that nothing bakes during rendering.

Practical consequence: adding detail to a part spec costs baked vertices and bake time,
not draw calls. It does **not** put per-part draws in the frame.

`render/creature/primitive_geometry.{h,cpp}` is the single source of truth for turning a
`PrimitiveInstance` into a unit mesh plus a model matrix. Both the bake and
`submit_part_graph()` call it, so a new `PrimitiveShape` is wired up in exactly one place;
the two used to carry separate copies of the same switch and could drift apart.

Every unit primitive obeys one convention: **radius 1, height 1, centred on the origin**,
because `Geom::cylinder_between(a, b, r)` scales x/z by the caller's radius verbatim and y
by the head-to-tail span. `capsule_between()` and `cone_from_to()` are deliberately the
same transform under names that document which primitive is being placed. Breaking the
convention is silent — the capsule mesh was authored at radius 0.25 and drew the whole
minimal-LOD humanoid body four times too thin for as long as it existed.
`tests/render/unit_primitive_convention_test.cpp` now measures every unit mesh and fails
if one drifts.

### Profiling stage timings

`FrameProfile` is compiled into every build and gated at runtime by
`FrameProfile::enabled`, which the profiling HUD toggles. While it is off the
phase scopes take no clock samples, so `animation_sampling`,
`humanoid_preparation`, `bpat_playback` and `layout_generation` read 0.000 ms.
Turn the HUD on before drawing any conclusion from those numbers.

Switch the HUD on in-game with **F10**, or start with `SOI_PROFILING_HUD=1` to
have it enabled from the first frame. `ui/qml/ProfilingOverlay.qml` is mounted
at the top of `Main.qml` above every other layer; without it the
`profiling_hud` context property registered in `main.cpp` has no reader and the
overlay cannot be switched on at all.

The render-thread phases (`frame` through `present`) tile the whole
render-thread frame. `sim` is the exception since the simulation moved to its
own thread: it is the CPU the `SoISimulation` thread spent in ticks since the
previous rendered frame, and it overlaps `present` rather than adding to the
frame. `total_us()` is therefore the wall-clock frame interval plus the
off-thread simulation cost:

| Phase      | Scope                                                                    |
| ---------- | ------------------------------------------------------------------------ |
| `sim`      | `GameEngine::simulate(dt)` ticks on `SoISimulation` since the last frame |
| `frame`    | `GameEngine::update_presentation(dt)` on the render thread               |
| `snapshot` | `ensure_render_snapshot()` + `acquire_render_snapshot()`                 |
| `collect`  | `Renderer::begin_frame()`                                                |
| `submit`   | `Renderer::render_world()` less the snapshot handoff                     |
| `sort`     | `DrawQueue::sort_for_batching()`                                         |
| `shadow`   | `Backend::render_directional_shadows()` CPU time                         |
| `play`     | `Backend::execute()` less the shadow pass                                |
| `present`  | the gap between one `render()` returning and the next starting           |

`PhaseScope` is self-exclusive: a nested scope subtracts its own elapsed time
from its parent, so `shadow` inside `play` and `snapshot` inside `submit` are
each counted once. `present` is the compositor and vsync wait; when it dominates
the frame is not CPU-bound and none of the other phases are worth optimising.

There is deliberately no `cull` phase. Culling is not a span — every entity
renderer tests visibility inline as it submits, so the only honest place to
attribute it is `submit`. The enum used to carry a `Culling` entry that read
0.00 ms in every frame ever recorded, which repeatedly suggested culling was
free.

A frame is opened by whichever of `GLView::GLRenderer::render()` or
`Renderer::begin_frame()` runs first and is closed by `Renderer::end_frame()`.
The guard exists so the `sim` phase, which is recorded before the renderer is
entered, survives into the same frame's report instead of being reset away.

### The runtime benchmark

`SOI_RUNTIME_BENCHMARK_SECONDS` (or `--benchmark-seconds`) measures the directly
started mission after a two-second warm-up and writes the JSON report named by
`--benchmark-output`. The report carries `graphics_preset` read from
`GraphicsSettings::instance().quality()` — it is not assumed to be Ultra — and a
`valid` flag. A run that collects fewer than 30 measured frames reports
`valid: false` with an `error` instead of statistics, because a handful of
frames on a loaded box produces numbers that look like measurements and are not.

The frame-continuity probe is a separate switch, `SOI_RUNTIME_CONTINUITY=1`. It
does a full-framebuffer `glReadPixels` plus a `QImage` allocation every frame on
campaign missions, so leaving it coupled to the benchmark meant the benchmark
was measuring the probe. Its `continuity_*` fields only appear in the report
when it ran.

The Arena's `render_execute` bucket is sampled straight after `Renderer::end_frame()`, so
it covers queue sort plus backend execution — it is not animation playback. It was called
`playback` for a while, which sent at least one investigation in the wrong direction.

### The selected-entity set is pushed on change, not per frame

`GameEngine::render` filters the live selection through
`CommanderViewModel::should_render_selected_entity` into a reused member vector
and only calls `Renderer::set_selected_entities` when the resulting list
differs from the one already pushed. That call clears and refills an
`unordered_set`, so doing it unconditionally rebuilt the set every frame for
data that changes when the player clicks.

### The minimap's dirty hash is global, and that is the problem

`MinimapManager::update_units` runs at 20 Hz (`k_minimap_unit_update_interval`)
on the render thread and is charged to the `sim` phase, where it measured
1.05 ms/frame at Zama scale -- 14% of that phase. It walks every unit in the
world, builds a marker per unit, hashes them, and rasterises when the hash
changed.

Quantising that hash to the minimap pixel grid looks like the obvious fix and
**does not work**: the hash covers every unit at once, so in a battle it is
enough for any single one of ~6000 units to cross a pixel boundary in 50 ms for
the whole overlay to redraw, which happens on essentially every tick. Tried and
reverted on 19 Aug 2026; it also broke
`RuntimeFrameOrchestratorTest.MovingUnitMarkersUpdateAtMinimapCadence`, where a
one-tile move on a 12-tile map is under one minimap pixel and must still count.

What would actually pay: hoisting the visibility cull out of
`UnitLayer::update` and into the marker loop, so fogged units never become
markers and never get rasterised; or moving the whole thing off the render
thread, since it produces a `QImage` for QML rather than anything in the frame.

### Cascade resolutions are split

The directional shadow cascades do not all live in one texture. Cascades 0 and 1
go into a `GL_TEXTURE_2D_ARRAY` at the preset's `resolution`; cascades 2 and 3
go into a second array at half that, which cuts the texels cleared and drawn
every frame by about 37% at Ultra (4x4096 squared is 67 M texels; 2x4096 plus
2x2048 is 42 M). `near_cascade_split()` only splits when the preset asks for more
than two cascades above 512, so Low and Medium are untouched.
`SOI_SHADOW_CASCADE_SPLIT=0` turns it off to bisect against.

The far cascades sample `u_directional_shadow_map_far`, a second
`sampler2DArrayShadow` bound to the same comparison sampler object as the near
one; `u_shadow_camera_position.w` carries the near-cascade count and
`u_shadow_bias.w` the far texel size, both previously padding.
**The depth bias has to scale with the texel size.** It does so implicitly now
that biases are authored in metres: `u_shadow_cascade_texel_world[cascade]` is
computed in `render_directional_shadows()` against that cascade's own
resolution, so the halved far cascades get a proportionally larger world bias
without a second scaling term. Before biases moved into world units the fixed
depth bias no longer cleared the coarser far quantisation and every distant
surface shadowed itself: mean frame brightness on the Zama terrain view fell
from 62 to 25 and the whole map read as being in shade. PCSS blocker search is
near-atlas only -- the raw-depth sampler (`u_directional_shadow_depth`) is bound
to the near array, and at far-cascade distances the contact-hardening term is
below a texel anyway.

`Backend::execute_scene` keeps at most two frames in flight: it fences the end of every
frame and waits on the fence from two frames back before starting the next, so a CPU
that outruns the GPU stalls a little every frame instead of for seconds when the driver
runs out of queue. The wait, plus `GL_TIMESTAMP` brackets around the shadow and colour
passes, are reported in `Backend::PlaybackStats` (`gpu_wait_ms`, `gpu_shadow_ms`,
`gpu_color_ms`) and land in the Arena trace as `gpu_ms`. When `gpu_wait_ms` is large the
frame is GPU-bound and the CPU phases are not what to optimise.

## Projectiles and siege engines

Every arrow, bolt and ballista round in the game is the same three meshes, built once in
[render/geom/arrow.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/geom/arrow.cpp)
and scaled per call site: a capped, barrelled shaft with a flared nock, a socketed
broadhead, and three helical feather vanes. All three are authored in one shared local
space where `+Z` is the direction of flight and the arrow spans `z = 0` (nock) to
`z = Arrow::k_total_length` (point), with the head occupying the last `k_tip_length`.
That is what lets a single model matrix drive all three meshes, and it is what the
`arrow_cloud_render_test` z-bound assertions pin down. If you change the split between
shaft and head, keep both meshes meeting exactly at `k_shaft_length` or the arrow comes
apart in flight.

Colour carries meaning here and is deliberately **not** all team colour:

- the shaft is wood with only a 12% team tint, so it never reads as a glowing stick,
- the head is steel and is the brightest part of the arrow,
- the fletching is the team colour, and it is the widest part of the silhouette — at
  gameplay camera distance the feathers are what the player actually sees.

Arrows carry a glow, but a shaped one. It used to be a translucent copy of the whole shaft
scaled 2.45x, which made a volley look like a swarm of glowing darts. It is now two draws:
a sheath hugging the shaft at `k_shaft_glow_xy_scale` and a brighter highlight over the
head only, scaled about `k_head_center_z` so the head mesh does not slide off its socket.
The rule the `ArrowGlowHugsTheShaft` test pins is that the sheath stays narrower than the
fletching — the moment the glow is wider than the feathers it stops reading as light on an
arrow and starts reading as a glowing stick. Colour comes from `Arrow::glow_color`, which
is mostly neutral warm white with only a fifth of the team hue mixed in; push that ratio up
and the whole arrow turns the team colour again. Marker-style arrows skip the glow because
they are a UI aid, not a projectile. Motion trails are shaft-only ghosts at 62% radius, so
they read as a streak rather than as duplicated arrows.

Impacts deliberately have no starburst. `Renderer::metal_spark` draws ten flat radiating
quads in world space, and an arrow impact holds for `impact_lifetime` — two thirds of a
second of a stationary ten-ray fan, which from the gameplay camera reads as a spider web
lying on the grass. The melee callers in `combat_dust_renderer` keep it because theirs live
for 80 ms at a blade contact, where it reads as a spark. Arrow impacts now show nothing
beyond the spent shaft that appears; only ballista bolts get a dust puff. Do not add a
per-arrow puff — a volley lands dozens of arrows a second and the puffs stack into a haze
over the whole engagement.

Arrows that land do not vanish. `ProjectileSystem::record_spent_projectile` drops a
`SpentProjectile` at every arrow or bolt impact, clamped to the terrain surface, planted at
a jittered angle derived from the incoming direction, and scattered inside a small radius
so a repeatedly-hit spot grows a cluster instead of a stack. They fade out over the last
few seconds of their lifetime and the list is capped at `k_max_spent_projectiles`, oldest
evicted first, which bounds the draw cost of a long siege. The buried head is skipped
rather than drawn underground. Spent projectiles are visual-only and are not serialized —
neither are in-flight projectiles.

The siege engines in `render/entity/nations/*/{ballista,catapult}_renderer.cpp` are built
from the same primitive helpers as everything else (`draw_box`, `draw_cyl`,
`get_unit_sphere`), and both machines share a rule worth knowing before you move anything:
the anim context drives geometry that other parts have to follow. On the ballista the
bowstring, the bolt and the trigger carriage all read `slide_travel(anim_ctx)`, so the
string visibly draws back with the bolt; on the catapult the arm angle comes from
`arm_swing_rad(anim_ctx)`, which cocks the arm backwards over the frame and slams it
forward into a padded buffer beam. Both machines load real ammunition — the ballista puts
an actual bolt in its groove and the catapult a rock mesh in its sling bowl — rather than a
scaled unit cube.

The catapult's loaded round is visibly the round it will actually throw.
`ammunition_for_target` in `siege_special_processor.cpp` already picks
`ProjectileKind::FlamingStone` whenever a catapult is aimed at something with a
`BuildingComponent`, so `CatapultAnimContext::incendiary_round` carries that choice through
to the renderer: the rock goes pitch-dark, gains bound rags, and burns in the sling. The
flames need the concrete `Renderer` (`fireball` is not on `ISubmitter`), so they come from a
`dynamic_cast` on `unwrap_submitter()` and are simply skipped when a submitter cannot
provide one — an offscreen preview harness still gets the geometry, just no fire.

## The phases inside `render_world`

`Renderer::render_world` in [scene_walk.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/scene_walk.cpp) is the widest function in the renderer, so it is worth knowing its shape before editing it. It runs six phases in order, and each one is a separate private method so the boundaries are visible rather than implied by blank lines:

1. **Snapshot and setup** — acquire the detached render world, take the entity mutex, attach the persistent render registry, and resolve the id spans. `compute_rpg_lens_gap()` returns the RPG lens exclusion that the submission visibility policy then applies.
2. **Collection** — `collect_unit_entries()` and `collect_non_unit_entries()` walk the id spans and produce flat `UnitRenderEntry` / `RenderEntry` vectors. Everything that can reject an entity (pending removal, dead without a death animation, invisible, out of frustum, fogged) happens here, so the submission phase never has to ask again. Units are then sorted front to back.
3. **Batching setup** — prune the caches, ask `BattleRenderOptimizer` for a batching ratio, size the `PrimitiveBatcher`, and derive the full-detail distance threshold.
4. **Planning** — `plan_unit_entry()` handles one unit: resolve its renderer (falling back to the profile renderer key), decide whether its animation ticks this frame, fill a `DrawContext`, pick an LOD tier, and record the result in a `UnitDrawPlan`. If the renderer registered an `IParallelPreparer` (every humanoid-family renderer does, through `register_humanoid_renderer`), the plan pass also calls `ensure_prepare_components()` so any render-side components the preparer needs are added here, on the main thread, and never from a worker.
5. **Preparation** — `prepare_unit_plans()` runs the humanoid preparers. The first use of each renderer handle runs serially (that warms the renderer's lazily baked caches); everything after that is distributed over the persistent `PrepareWorkerPool` (at most three workers plus the caller) into per-unit `CreaturePreparationResult`s. Only CPU work happens here — no GL, no submitter, no world lock beyond what the caller already holds — and the pass is skipped in favour of serial rendering while combat animation diagnostics are recording. `FrameProfile::humanoid_preparation_us` is the wall time of this pass.
6. **Submission** — `submit_unit_entry()` walks the plans in the original front-to-back order and either submits the prepared result (`submit_preparation`) or, for renderers without a preparer, calls the registry `RenderFunc` directly, through the batching submitter or the renderer itself. Non-unit entries take a shorter version of the same path.

The two things a preparer must promise are that `prepare()` touches only the entity it was given plus read-only registries, and that it does not add components (component insertion notifies the world under the entity mutex the render thread holds during the walk, so a worker doing it deadlocks; that is what `ensure_prepare_components()` exists for).

Everything phases 4-6 need from phases 1-3 travels in one `UnitSubmitContext` — the world, the resource manager, the batching submitter, the optimizer, the batching ratio, the LOD distance threshold and the frame's flags. That is the point of the struct: the submission path used to read a dozen locals out of an enclosing scope, so there was no way to see what it actually depended on. Now adding a dependency means adding a field.

The split matters because phase 2 is pure collection against renderer state, while phases 4-6 own the submitter, the batcher and the LOD decisions. Keeping a filter in phase 2 keeps it out of the hot submission loop; adding renderer state to phase 2 means adding it to an explicit parameter list rather than to a growing capture set.

## The full journey

Let's trace a frame from start to finish. Qt's render thread calls our GLRenderer::render method in [gl_view.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/ui/gl_view.cpp). That calls GameEngine::render, which calls SceneRenderer::begin_frame to clear the draw queue and reset frame state.

Game systems iterate through all entities. For each entity that needs rendering, they look up the appropriate renderer in the EntityRendererRegistry and call it. The renderer submits commands to the draw queue: mesh commands for body parts, cylinder commands for spear shafts, whatever's needed.

After all entities are processed, SceneRenderer::end_frame sorts the queue by the criteria we discussed (opacity, shader, texture, mesh), swaps the double buffer so the GPU gets the fresh queue, and calls Backend::execute with the freshly sorted commands.

Backend walks through commands in order. When it sees a run of similar commands, it batches them and hands them to the appropriate pipeline. CylinderPipeline gets all the cylinders and draws them instanced. TerrainPipeline handles ground chunks. Each pipeline binds its shader, sets uniforms, uploads any instance data, and issues draw calls.

The shaders run on the GPU, pulling in procedural details—rust patterns on armor, weave on fabric, scratches on shields. Simple Lambertian lighting gives everything shape. The fragment shader writes final colors to the framebuffer.

OpenGL rasterizes everything. Qt presents the framebuffer to the screen. And then we do it all again, 60 times a second.

The whole architecture optimizes for minimal state changes, parallel CPU/GPU work, and memory efficiency through procedural generation. There's still room for improvement—we don't do frustum culling yet, and occlusion culling would help in complex scenes—but the foundation is solid.

## Finding your way around

Here's a quick reference for common tasks:

| What you want to do                      | Where to look                                                                                                                                                                                                                                              |
| ---------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Add a new unit type                      | [render/entity/registry.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/entity/registry.cpp) for registration, create new renderer in [render/entity/nations](https://github.com/djeada/Standard-of-Iron/blob/main/render/entity/nations) |
| Change a nation's look                   | [render/entity/nations/carthage](https://github.com/djeada/Standard-of-Iron/blob/main/render/entity/nations/carthage) or [roman](https://github.com/djeada/Standard-of-Iron/blob/main/render/entity/nations/roman) folders                                 |
| Modify shaders                           | [assets/shaders](https://github.com/djeada/Standard-of-Iron/blob/main/assets/shaders) folder                                                                                                                                                               |
| Debug GL errors                          | [render/gl/mesh.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/mesh.cpp) has error checking after draws                                                                                                                               |
| Change draw order                        | [render/draw_queue.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/draw_queue.h) for command definitions, sort logic in draw_queue.cpp                                                                                                      |
| Add a new effect                         | Create new Cmd struct in draw_queue.h, add pipeline in render/gl/backend                                                                                                                                                                                   |
| Debug the frame                          | Use RenderDoc to capture and step through                                                                                                                                                                                                                  |
| Tune battle performance                  | [render/battle_render_optimizer.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/battle_render_optimizer.h) for temporal culling and animation throttling                                                                                    |
| Share a type between gameplay and render | Put it in `scene/` (view data), `animation/rig/` (skeleton, reach) or `animation/bpat/` (baked poses) -- never include `render/` from `game/`                                                                                                              |
| Change lighting or time of day           | `scene/environment_lighting.h` for the state, `game/map/environment_lighting.cpp` for the curves, `assets/shaders/include/environment_lighting.glsl` for shader access                                                                                     |
| Tune unit blob shadows                   | `render/contact_shadow.h` -- direction, length and fade all derive from the environment sun and the directional-shadow settings                                                                                                                            |
| Change a map's weather                   | The map's `"rain"` block (or the map editor's Weather panel); particle budgets live in `Render::GL::weather_particle_density` and `GraphicsSettings::weather_budget`                                                                                       |
| Add a shader sampler                     | Register the unit in `Render::GL::TextureUnit` ([render/gl/render_constants.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/render_constants.h)) so it cannot collide                                                                    |
| Make a prop cast light                   | Call `Renderer::local_light()` from its renderer; instanced props cannot be recovered from draw commands                                                                                                                                                   |
| Review props or lighting visually        | `arena_app --batch --scenario world_prop_lineup` (or any `lighting_*` scenario) `--clean-capture --artifact-dir <dir>`; compare with `scripts/compare-arena-captures.py`                                                                                   |

The most common mistakes are calling OpenGL from the wrong thread (Qt's render thread is the only safe place), forgetting to bind the VAO before drawing (nothing appears), uploading instance data but calling the non-instanced draw function (only one object appears), or getting matrix conventions mixed up (everything is inside-out or flipped).

The RAII state scopes in [state_scopes.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/state_scopes.h) help prevent state leakage bugs—use them whenever you need to temporarily change GL state. The uniform cache in Shader prevents per-frame overhead from name lookups.

When in doubt, fire up RenderDoc and trace a frame. You'll see exactly what gets bound, what gets drawn, and where time goes. Most rendering bugs become obvious once you can see the actual GPU work.
