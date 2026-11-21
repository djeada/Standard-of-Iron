#pragma once

#include <string>

#include "render/creature/humanoid/humanoid_types.h"
#include "render/entity/registry.h"

namespace Render::GL::Carthage {

void apply_grave_priest_cast_pose(const Render::GL::HumanoidAnimationContext& anim,
                                  Render::GL::HumanoidPose& io_pose);

void register_healer_renderer(EntityRendererRegistry& registry);
void register_grave_priest_renderer(EntityRendererRegistry& registry);

} // namespace Render::GL::Carthage
