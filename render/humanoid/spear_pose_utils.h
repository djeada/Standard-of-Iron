#pragma once

#include <QVector3D>

#include "../gl/humanoid/humanoid_types.h"
#include "animation/attack_pose_manifest.h"
#include "animation/pose_manifest.h"
#include "pose_primitives.h"

namespace Render::GL {

inline auto spear_qvec_from_pose(const Animation::PoseVec3& value) -> QVector3D {
  return {value.x, value.y, value.z};
}

inline auto resolve_spear_direction(const AnimationInputs& inputs,
                                    float attack_phase = 0.0F) -> QVector3D {
  QVector3D spear_dir =
      spear_qvec_from_pose(Animation::resolve_humanoid_spear_direction({
          .hold_blend = hold_transition_amount(inputs),
          .is_mounted = inputs.is_mounted,
          .is_attacking = inputs.is_attacking,
          .is_melee = inputs.is_melee,
          .attack_phase = attack_phase,
      }));
  if (spear_dir.lengthSquared() > 1.0e-6F) {
    spear_dir.normalize();
  } else {
    spear_dir = QVector3D(0.05F, 0.55F, 0.85F).normalized();
  }
  return spear_dir;
}

inline auto compute_offhand_spear_grip(const HumanoidPose& pose,
                                       const Animation::PoseVec3& spear_direction,
                                       const QVector3D& main_hand_pos,
                                       Side main_side,
                                       float along_offset,
                                       float y_drop = 0.05F,
                                       float lateral_offset = 0.05F) -> QVector3D {
  QVector3D spear_dir = spear_qvec_from_pose(spear_direction);
  if (spear_dir.lengthSquared() > 1.0e-6F) {
    spear_dir.normalize();
  } else {
    spear_dir = QVector3D(0.05F, 0.55F, 0.85F).normalized();
  }

  QVector3D offhand = main_hand_pos + spear_dir * along_offset;

  QVector3D const right_axis =
      Render::Humanoid::PosePrimitives::compute_right_axis(pose);

  offhand += ((main_side == Side::Left) ? right_axis : -right_axis) * lateral_offset;
  offhand.setY(offhand.y() - y_drop);

  QVector3D torso_center = (pose.shoulder_l + pose.shoulder_r) * 0.5F;
  torso_center.setY(offhand.y());
  offhand = offhand * 0.65F + torso_center * 0.35F;

  return offhand;
}

} // namespace Render::GL
