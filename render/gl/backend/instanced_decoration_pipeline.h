#pragma once

// Stage 18 — InstancedDecorationPipeline facade.
//
// Unifies the entry point for static-decoration instanced draws
// (grass, stone, plant, pine, olive, firecamp). A single DrawCmd
// variant — DecorationBatchCmd — carries a Material* + per-kind
// params; this pipeline routes the call to the matching sub-pipeline
// based on DecorationBatchCmd::Kind.
//
// Rain is deliberately excluded (particle dynamics, see rain_gpu.h).
// Unifying the six decoration shaders into a single u_material_id
// mega-shader is deferred: each current shader has a distinct static
// mesh VBO and distinct uniforms, and a merged shader would pay the
// branch cost in every fragment. The dispatcher gives us the v3-plan
// "single variant + single entry" surface without the shader rewrite.

#include "../../draw_queue.h"

namespace Render::GL {
class Camera;
} // namespace Render::GL

namespace Render::GL::BackendPipelines {

class TerrainPipeline;
class VegetationPipeline;

class InstancedDecorationPipeline {
public:
  InstancedDecorationPipeline(TerrainPipeline *terrain_pipeline,
                              VegetationPipeline *vegetation_pipeline)
      : m_terrain_pipeline(terrain_pipeline),
        m_vegetation_pipeline(vegetation_pipeline) {}

  // The actual dispatch lives in backend.cpp — it touches a large
  // amount of GL state (depth mask scopes, VAO binds, shader rebinds,
  // uniform caches). Rather than re-plumb all of that through a thin
  // wrapper, backend.cpp calls into this pipeline via references to
  // the underlying sub-pipelines. This header exists to document the
  // abstraction and hold the pipeline pointers for future unification
  // work (shared VAO + u_material_id mega-shader).
  [[nodiscard]] auto terrain_pipeline() const -> TerrainPipeline * {
    return m_terrain_pipeline;
  }
  [[nodiscard]] auto vegetation_pipeline() const -> VegetationPipeline * {
    return m_vegetation_pipeline;
  }

private:
  TerrainPipeline *m_terrain_pipeline;
  VegetationPipeline *m_vegetation_pipeline;
};

} // namespace Render::GL::BackendPipelines
