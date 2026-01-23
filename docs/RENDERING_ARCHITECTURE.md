# How the Rendering System Actually Works

Picture this: you've got thousands of soldiers on screen, each with unique armor, weapons, and animations. Grass is swaying, rivers are flowing, and you need all of this running at 60 frames per second. How do you pull that off without your GPU catching fire?

This is the story of how Standard of Iron takes game state and turns it into pixels. We'll walk through the whole journey, from the moment Qt creates an OpenGL window to the final draw calls that paint soldiers on screen.

## What we'll cover

We'll start with the big picture of how data flows through the system, then dig into each layer: how Qt bootstraps OpenGL, how we record what needs to be drawn, how the backend executes those commands efficiently, where OpenGL actually lives in the code, how different nations get their unique visual styles, and finally how our shaders generate infinite detail without eating all your VRAM.


## The two-phase dance

The renderer works like a recording studio. In the first phase, we record: game logic tells us "there are 5000 soldiers here, some trees over there, a river running through." The SceneRenderer listens to all of this and writes down lightweight commands into something called a DrawQueue. No actual OpenGL happens yet—we're just taking notes.

In the second phase, we play it back. We sort all those commands by material, shader, and transparency so that similar things get drawn together. Then the Backend walks through that sorted list and actually talks to the GPU. This separation is the key insight that makes everything else work. By splitting "what to draw" from "how to draw it," we can sort for optimal GPU performance, we can record frame N+1 while the GPU is still rendering frame N, and we can test our rendering logic without needing OpenGL at all.

Here's how a single frame flows through the system:

```
                           ┌─────────────────────────────────────┐
                           │           Qt Render Thread          │
                           │  (creates OpenGL 3.3 Core context)  │
                           └──────────────┬──────────────────────┘
                                          │
                                          ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                              PHASE 1: RECORDING                              │
│                                                                              │
│   ┌─────────────┐      ┌──────────────────┐      ┌───────────────────────┐  │
│   │ GameEngine  │─────▶│  SceneRenderer   │─────▶│  Entity Renderers     │  │
│   │  ::render() │      │  ::begin_frame() │      │  (spearman, archer,   │  │
│   └─────────────┘      └──────────────────┘      │   terrain, trees...)  │  │
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

| What we use | Why |
|-------------|-----|
| Vertex arrays (VAO) | Group vertex attribute state |
| Instanced rendering | Draw 1000 trees in 1 call |
| Depth testing | Hidden surface removal |
| Alpha blending | Transparent effects |
| Polygon offset | Fix z-fighting on terrain |
| GLSL 330 shaders | All visual computation |

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

Each nation also gets its own shader files. You can see the pattern in the shader lookup:

```cpp
auto lookup_spearman_shader_resources(const QString &shader_key)
    -> std::optional<SpearmanShaderResourcePaths> {
  if (shader_key == QStringLiteral("spearman_carthage")) {
    return SpearmanShaderResourcePaths{
        QStringLiteral(":/assets/shaders/spearman_carthage.vert"),
        QStringLiteral(":/assets/shaders/spearman_carthage.frag")};
  }
  if (shader_key == QStringLiteral("spearman_roman_republic")) {
    return SpearmanShaderResourcePaths{
        QStringLiteral(":/assets/shaders/spearman_roman_republic.vert"),
        QStringLiteral(":/assets/shaders/spearman_roman_republic.frag")};
  }
  return std::nullopt;
}
```

The Carthage spearman shader knows how to render bronze with appropriate patina. The Roman shader knows how to render steel with rust patterns. This per-nation customization extends all the way down to the GPU.


## Procedural shaders

Here's a memory problem: if 5000 soldiers each need unique 4K textures for their rust, dirt, and wear patterns, that's around 80 gigabytes of VRAM. Obviously impossible. So instead we generate all that detail procedurally in the shader.

The shaders in [assets/shaders](https://github.com/djeada/Standard-of-Iron/blob/main/assets/shaders) use hash functions and noise to create variation. Looking at [spearman_carthage.frag](https://github.com/djeada/Standard-of-Iron/blob/main/assets/shaders/spearman_carthage.frag), you'll see the building blocks:

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

The vertex shader sometimes does geometry modifications too. For example, in [spearman_carthage.vert](https://github.com/djeada/Standard-of-Iron/blob/main/assets/shaders/spearman_carthage.vert), shields get a curved surface:

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

We have about 90 shader files in the [assets/shaders](https://github.com/djeada/Standard-of-Iron/blob/main/assets/shaders) folder, covering everything from terrain and rivers to individual unit types and special effects. The ShaderCache in [shader_cache.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/shader_cache.cpp) loads them on demand and keeps them around so we don't recompile every frame.


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

| What you want to do | Where to look |
|---------------------|---------------|
| Add a new unit type | [render/entity/registry.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/entity/registry.cpp) for registration, create new renderer in [render/entity/nations](https://github.com/djeada/Standard-of-Iron/blob/main/render/entity/nations) |
| Change a nation's look | [render/entity/nations/carthage](https://github.com/djeada/Standard-of-Iron/blob/main/render/entity/nations/carthage) or [roman](https://github.com/djeada/Standard-of-Iron/blob/main/render/entity/nations/roman) folders |
| Modify shaders | [assets/shaders](https://github.com/djeada/Standard-of-Iron/blob/main/assets/shaders) folder |
| Debug GL errors | [render/gl/mesh.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/mesh.cpp) has error checking after draws |
| Change draw order | [render/draw_queue.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/draw_queue.h) for command definitions, sort logic in draw_queue.cpp |
| Add a new effect | Create new Cmd struct in draw_queue.h, add pipeline in render/gl/backend |
| Debug the frame | Use RenderDoc to capture and step through |
| Tune battle performance | [render/battle_render_optimizer.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/battle_render_optimizer.h) for temporal culling and animation throttling |

The most common mistakes are calling OpenGL from the wrong thread (Qt's render thread is the only safe place), forgetting to bind the VAO before drawing (nothing appears), uploading instance data but calling the non-instanced draw function (only one object appears), or getting matrix conventions mixed up (everything is inside-out or flipped).

The RAII state scopes in [state_scopes.h](https://github.com/djeada/Standard-of-Iron/blob/main/render/gl/state_scopes.h) help prevent state leakage bugs—use them whenever you need to temporarily change GL state. The uniform cache in Shader prevents per-frame overhead from name lookups.

When in doubt, fire up RenderDoc and trace a frame. You'll see exactly what gets bound, what gets drawn, and where time goes. Most rendering bugs become obvious once you can see the actual GPU work.
