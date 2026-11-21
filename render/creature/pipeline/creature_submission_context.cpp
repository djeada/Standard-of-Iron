#include "creature_submission_context.h"

#include "render/rigged_mesh_cache.h"
#include "render/submitter.h"

namespace Render::Creature::Pipeline {

auto resolve_creature_submission(Render::GL::ISubmitter& out) noexcept
    -> CreatureSubmissionContext* {
  Render::GL::ISubmitter* sink = out.unwrap_submitter();
  return sink != nullptr ? sink->creature_submission() : nullptr;
}

auto detached_rigged_mesh_cache() -> Render::GL::RiggedMeshCache& {
  thread_local Render::GL::RiggedMeshCache detached;
  return detached;
}

} // namespace Render::Creature::Pipeline
