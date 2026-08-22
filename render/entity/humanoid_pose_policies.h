#pragma once

#include "render/humanoid/schema/pose_policy.h"

namespace Render::GL {
struct HumanoidPose;
struct HumanoidAnimationContext;
struct HumanoidVariant;
} // namespace Render::GL

namespace Render::Entity {

struct HumanoidPosePolicyInputs {
  const Render::GL::HumanoidAnimationContext* animation{nullptr};
  const Render::GL::HumanoidVariant* variant{nullptr};
  std::uint32_t seed{0U};
};

void apply_humanoid_pose_policy(Render::Humanoid::HumanoidPosePolicy policy,
                                const HumanoidPosePolicyInputs& inputs,
                                Render::GL::HumanoidPose& io_pose);

} // namespace Render::Entity
