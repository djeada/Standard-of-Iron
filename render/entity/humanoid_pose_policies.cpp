#include "humanoid_pose_policies.h"

#include "render/entity/healer_renderer_common.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/asset/humanoid_spec.h"

namespace Render::GL::Carthage {
void apply_grave_priest_cast_pose(const Render::GL::HumanoidAnimationContext& anim,
                                  Render::GL::HumanoidPose& io_pose);
}

namespace Render::Entity {

void apply_humanoid_pose_policy(Render::Humanoid::HumanoidPosePolicy policy,
                                const HumanoidPosePolicyInputs& inputs,
                                Render::GL::HumanoidPose& io_pose) {
  using Render::Humanoid::HumanoidPosePolicy;

  switch (policy) {
  case HumanoidPosePolicy::None:
    return;
  case HumanoidPosePolicy::SkeletonProportions:
    Render::Humanoid::apply_skeleton_proportion_pose(io_pose);
    return;
  case HumanoidPosePolicy::HealerChannel:
    if (inputs.animation != nullptr) {
      Render::GL::apply_healer_channel_pose(*inputs.animation, io_pose);
    }
    return;
  case HumanoidPosePolicy::HealerStaff:
    if (inputs.animation != nullptr) {
      Render::GL::apply_healer_staff_pose(*inputs.animation, io_pose);
    }
    return;
  case HumanoidPosePolicy::GravePriestCast:
    if (inputs.animation != nullptr) {
      Render::GL::Carthage::apply_grave_priest_cast_pose(*inputs.animation, io_pose);
    }
    return;
  }
}

} // namespace Render::Entity
