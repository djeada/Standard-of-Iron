#pragma once

#include <cstdint>

#include "render/creature/humanoid/humanoid_types.h"
#include "render/creature/pipeline/creature_render_graph.h"
#include "render/creature/render_request.h"

namespace Render::Creature::Pipeline {

struct PreparedHumanoidBodyState {
  CreatureGraphOutput graph{};
  Render::GL::HumanoidPose pose{};
  Render::GL::HumanoidVariant variant{};
  Render::GL::HumanoidAnimationContext animation{};
};

} // namespace Render::Creature::Pipeline
