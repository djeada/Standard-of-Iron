#include "render/humanoid/runtime/humanoid_pose_diagnostics.h"

#include <QVector3D>
#include <QtMath>

#include <algorithm>
#include <cmath>

#include "render/humanoid/schema/skeleton_schema.h"
#include "render/profiling/combat_animation_diagnostics.h"

namespace Render::Humanoid {

namespace {

constexpr auto bone_index(HumanoidBone bone) noexcept -> std::uint32_t {
  return static_cast<std::uint32_t>(bone);
}

} // namespace

void record_humanoid_body_pose(std::uint32_t entity_id,
                               std::uint16_t instance_index,
                               const QMatrix4x4& world,
                               std::uint32_t bone_count,
                               Render::Creature::Pipeline::BoneOriginFn origin,
                               const void* user) {
  if (origin == nullptr || bone_count <= bone_index(HumanoidBone::HandR)) {
    return;
  }

  auto local = [&](HumanoidBone bone) {
    return origin(user, bone_index(bone));
  };
  auto world_of = [&](HumanoidBone bone) {
    return world.map(local(bone));
  };

  QVector3D const pelvis_world = world_of(HumanoidBone::Pelvis);
  QVector3D const neck_world = world_of(HumanoidBone::Neck);
  QVector3D const visible_torso = neck_world - pelvis_world;
  float const body_up_y =
      visible_torso.lengthSquared() > 1.0e-8F ? visible_torso.normalized().y() : 1.0F;

  float const max_arm_reach =
      std::max((local(HumanoidBone::HandL) - local(HumanoidBone::ShoulderL)).length(),
               (local(HumanoidBone::HandR) - local(HumanoidBone::ShoulderR)).length());

  Render::Profiling::SubmittedBodyPose pose;
  pose.body_up_y = body_up_y;
  pose.max_arm_reach = max_arm_reach;

  if (bone_count > bone_index(HumanoidBone::FootR)) {
    pose.pelvis_world = pelvis_world;
    pose.hand_l_world = world_of(HumanoidBone::HandL);
    pose.hand_r_world = world_of(HumanoidBone::HandR);
    pose.foot_l_world = world_of(HumanoidBone::FootL);
    pose.foot_r_world = world_of(HumanoidBone::FootR);
    QVector3D const hip_axis =
        world_of(HumanoidBone::HipR) - world_of(HumanoidBone::HipL);
    if (hip_axis.lengthSquared() > 1.0e-8F) {
      pose.pelvis_yaw_degrees =
          qRadiansToDegrees(std::atan2(hip_axis.x(), hip_axis.z()));
      pose.joints_valid = true;
    }
  }

  Render::Profiling::CombatAnimationDiagnostics::instance().record_submitted_body_pose(
      entity_id, instance_index, pose);
}

} // namespace Render::Humanoid
