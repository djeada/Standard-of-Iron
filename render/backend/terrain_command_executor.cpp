#include "render/backend/command_executor_common.h"

namespace Render::GL {

void Backend::execute_terrain_commands(const PreparedBatch& prepared,
                                       PipelineDrawContext& context) {
  const auto& cmd = context.queue.get_sorted(prepared.start);
  switch (cmd.index()) {
  case TerrainSurfaceCmdIndex:
    m_terrain_pipeline->draw_surfaces(prepared, context, m_active_local_lights);
    break;
  default:
    break;
  }
}

} // namespace Render::GL
