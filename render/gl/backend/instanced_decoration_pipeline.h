#pragma once

#include "../../draw_queue.h"

namespace Render::GL {
class Camera;
}

namespace Render::GL::BackendPipelines {

class TerrainPipeline;
class VegetationPipeline;

class InstancedDecorationPipeline {
public:
  InstancedDecorationPipeline(TerrainPipeline *terrain_pipeline,
                              VegetationPipeline *vegetation_pipeline)
      : m_terrain_pipeline(terrain_pipeline),
        m_vegetation_pipeline(vegetation_pipeline) {}

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
