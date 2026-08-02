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

Directional shadows are cascaded (up to four, quality-dependent) with texel snapping and cascade blending. Contact shadows remain as a cheap grounding effect and a low-quality fallback: `render/contact_shadow.h` derives their compass direction and length from the same `EnvironmentLightingState` sun rather than a baked constant, so a blob points and stretches the way the map's clock says it should. The same helper fades them out -- towards a subtle patch directly under the feet where a real cascade already covers the unit, and towards zero at the contact-shadow distance limit -- so nothing pops in or doubles up.

Local lights are budgeted (`Render::k_max_local_lights`) and go through `Render::LocalLightFader`, which holds a slot until its light has ramped down. Entering and leaving the budget is a fade over `k_local_light_fade_seconds`, never a pop, and `Renderer::clear_entity_render_caches()` resets the fader so a new map never inherits the previous map's fires.

Two practical notes:

- **Texture units are a shared, program-wide namespace.** Two samplers of different types resolving to the same unit make every draw using that program raise `GL_INVALID_OPERATION`. The units in play are listed in `Render::GL::TextureUnit` (`render/gl/render_constants.h`) -- add new long-lived samplers there rather than picking a number.
- **Instanced emitters can't be recovered from draw commands.** Fire camps and shrines reach the GPU as instance buffers, so the backend cannot read their positions back to build local lights. They advertise themselves through `Renderer::local_light()` instead, and the backend budgets those alongside effect-driven lights.

## Precipitation

Every shipped map states its weather explicitly in a `"rain"` block -- `enabled`, `type` (`rain` or `snow`), `intensity` (a number, or the words `light`/`medium`/`heavy`), the cycle timings, and `wind_strength` plus `wind_direction` in compass degrees. `Game::Systems::RainManager` runs the cycle; its state, cycle position and transition progress round-trip through the save metadata's `weather` object, so loading mid-downpour resumes mid-downpour.

Particles live in one fixed pool owned by `RainPipeline` and are recycled in the vertex shader, positioned relative to the camera rather than across the map. How much of that pool is drawn each frame is `RainBatchParams::density`, computed by `Render::GL::weather_particle_density()` from the active intensity, the quality preset's `WeatherBudget::particle_scale`, and how far the camera has pulled back. Each particle carries a `rank` in the pool and fades out as the density cutoff approaches it, so raising or lowering the budget dissolves particles instead of popping them.

## QSG render-thread stages

`ui/gl_view.cpp` owns the frame callback through `GLView::GLRenderer::render()`. Qt runs that callback with the FBO OpenGL context current on the QSG render thread. The callback intentionally performs two engine stages in order:

1. `GameEngine::update(dt)` advances simulation systems before rendering. `World::update(dt)` runs here, so combat query rebuilds, target searches, attack state updates, hit feedback, target direction, and mode flags are simulation costs even when a profiler groups them under `QSGRenderThread`.
2. `GameEngine::render(width, height)` records and plays back rendering work from state that already exists. `Renderer::render_world(world)` requests and consumes the latest detached render-world snapshot. After the first handoff, culling, sorting, cache updates, and entity renderer callbacks no longer hold the mutable simulation world's mutex. Renderer-owned animation and layout state is transferred from the previous snapshot before submission. The renderer must not rebuild combat query state or search for targets. `Renderer::end_frame()` sorts the `DrawQueue`, then `Backend::execute(...)` performs OpenGL playback.

When a capture needs stage markers, set `SOI_RENDER_STAGE_LOG=1`. The extra logs are first-use only and remain disabled during normal gameplay. They identify the simulation update, render submit, renderer setup/cache attachment, creature asset registry load, and first backend playback so one-time setup can be separated from steady-frame costs.

Rally and patrol markers are restricted to the local human player. Set
`SOI_RENDER_DEBUG_ORDER_MARKERS=1` to reveal non-local markers explicitly,
including while spectating.

## The two-phase dance

The renderer works like a recording studio. In the first phase, we record: game logic tells us "there are 5000 soldiers here, some trees over there, a river running through." The SceneRenderer listens to all of this and writes down lightweight commands into something called a DrawQueue. No actual OpenGL happens yet—we're just taking notes.

In the second phase, we play it back. We sort all those commands by material, shader, and transparency so that similar things get drawn together. Then the Backend walks through that sorted list and actually talks to the GPU. This separation is the key insight that makes everything else work. By splitting "what to draw" from "how to draw it," we can sort for optimal GPU performance, we can record frame N+1 while the GPU is still rendering frame N, and we can test our rendering logic without needing OpenGL at all.

Here's how a single frame flows through the system:

```
#
                           ┌─────────────────────────────────────┐
                           │           Qt Render Thread          │
                           │  (creates OpenGL 3.3 Core context)  │
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

When Qt's render thread starts up, it creates an OpenGL 3.3 Core context for us. Our GLView class notices this and creates a GLRenderer that holds a pointer to the GameEngine. From then on, every frame Qt calls our render method, which calls into GameEngine, which kicks off the whole pipeline.

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

We picked OpenGL 3.3 Core as a balance between running on older hardware and having modern features like instancing. The Core profile means we don't have any of the legacy fixed-function baggage—everything goes through shaders.

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
    ├── cylinder_pipeline.cpp/.h
    ├── terrain_pipeline.cpp/.h
    ├── vegetation_pipeline.cpp/.h
    └── ...
```

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

We use a fairly conservative subset of OpenGL 3.3:

| What we use         | Why                          |
| ------------------- | ---------------------------- |
| Vertex arrays (VAO) | Group vertex attribute state |
| Instanced rendering | Draw 1000 trees in 1 call    |
| Depth testing       | Hidden surface removal       |
| Alpha blending      | Transparent effects          |
| Polygon offset      | Fix z-fighting on terrain    |
| GLSL 330 shaders    | All visual computation       |

What we don't use: geometry shaders (compatibility issues on some drivers), compute shaders (require OpenGL 4.3), tessellation (not needed for our art style), multi-draw indirect (instancing is enough).

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

Troop bodies no longer get per-nation shader files. The rigged creature backend owns the shared `character_skinned` and `character_skinned_instanced` programs, while nation, role, and equipment variation is supplied as declarative render data: palette values, material IDs, visual specs, equipment records, and texture slots.

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

**Caching.** `CreatureRenderBatch::add_humanoid` calls `resolve_pose_intent` once and passes the result into both `humanoid_state_for_anim` (via its two-argument overload) and the variant-table dispatch block. No system in the critical render path calls the resolver more than once per entity per frame.

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

When part of a scatter prop shades almost black while the rest of the same mesh looks fine, suspect the face normals rather than the lighting. `append_oriented_box` and `append_barrel_yaxis` in [vegetation_pipeline.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/backend/vegetation_pipeline.cpp) emit inward-facing normals on some faces; prop rendering runs with culling disabled, so the geometry still draws but shades as if it were facing away from every light. Use `append_prop_beam` and `append_prop_taper` for new geometry—they are the same shapes with outward normals. `append_box` and `append_vert_prism` were always correct. The background and the full list of touchpoints for a new prop are in [docs/SETTLEMENT_ASSETS.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/SETTLEMENT_ASSETS.md).

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

`FrameProfile` is compiled out unless `SOI_RUNTIME_TRACING` is on, so a default Release
build reports 0.000 ms for `animation_sampling`, `humanoid_preparation`, `bpat_playback`
and `layout_generation` in the Arena trace. Configure a second build directory with
`-DSOI_RUNTIME_TRACING=ON` before drawing any conclusion from those numbers.

The Arena's `render_execute` bucket is sampled straight after `Renderer::end_frame()`, so
it covers queue sort plus backend execution — it is not animation playback. It was called
`playback` for a while, which sent at least one investigation in the wrong direction.

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
