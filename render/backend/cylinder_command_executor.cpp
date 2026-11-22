#include "render/backend/command_executor_common.h"

namespace Render::GL {

void Backend::execute_cylinder_commands(const PreparedBatch& prepared,
                                        PipelineDrawContext& context) {
  if (m_cylinder_pipeline) {
    m_cylinder_pipeline->draw(prepared, context);
  }
}

} // namespace Render::GL
