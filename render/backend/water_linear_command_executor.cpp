#include "render/backend/command_executor_common.h"

namespace Render::GL {

void Backend::execute_water_linear_commands(const PreparedBatch& prepared,
                                            PipelineDrawContext& context) {
  const auto& cmd = context.queue.get_sorted(prepared.start);
  if (cmd.index() != TerrainFeatureCmdIndex || m_water_pipeline == nullptr) {
    return;
  }
  m_water_pipeline->draw_terrain_features(prepared, context);
}

} // namespace Render::GL
