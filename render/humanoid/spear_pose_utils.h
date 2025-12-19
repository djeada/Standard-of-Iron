#pragma once

#include "../entity/renderer_constants.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../gl/humanoid/humanoid_types.h"

#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Render::GL {

inline auto
computeSpearDirection(const AnimationInputs &anim_inputs) -> QVector3D {

  auto normalize = [](QVector3D dir) {
    if (dir.lengthSquared() > 1e-6F) {
      dir.normalize();
    }
    return dir;
  };

  QVector3D spear_dir = normalize(QVector3D(0.05F, 0.55F, 0.85F));

  if (anim_inputs.is_in_hold_mode || anim_inputs.is_exiting_hold) {
    float const t = anim_inputs.is_in_hold_mode
                        ? 1.0F
                        : (1.0F - anim_inputs.hold_exit_progress);

    QVector3D const braced_dir = normalize(QVector3D(0.05F, 0.40F, 0.91F));
    spear_dir = normalize(spear_dir * (1.0F - t) + braced_dir * t);
  } else if (anim_inputs.is_attacking && anim_inputs.is_melee) {
    float const attack_phase =
        std::fmod(anim_inputs.time * SPEARMAN_INV_ATTACK_CYCLE_TIME, 1.0F);
    if (attack_phase >= 0.30F && attack_phase < 0.50F) {
      float const t = (attack_phase - 0.30F) / 0.20F;

      QVector3D const attack_dir = normalize(QVector3D(0.03F, -0.15F, 1.0F));
      spear_dir = normalize(spear_dir * (1.0F - t) + attack_dir * t);
    }
  }

  return spear_dir;
}

inline auto compute_offhand_spear_grip(const HumanoidPose &pose,
                                    const HumanoidAnimationContext &anim_ctx,
                                    const QVector3D &main_hand_pos,
                                    bool main_is_left, float along_offset,
                                    float y_drop = 0.05F,
                                    float lateral_offset = 0.05F) -> QVector3D {
  QVector3D const spear_dir = computeSpearDirection(anim_ctx.inputs);

  QVector3D offhand = main_hand_pos + spear_dir * along_offset;

  QVector3D right_axis = pose.shoulder_r - pose.shoulder_l;
  right_axis.setY(0.0F);
  if (right_axis.lengthSquared() < 1e-6F) {
    right_axis = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    right_axis.normalize();
  }

  offhand += (main_is_left ? right_axis : -right_axis) * lateral_offset;
  offhand.setY(offhand.y() - y_drop);

  QVector3D torso_center = (pose.shoulder_l + pose.shoulder_r) * 0.5F;
  torso_center.setY(offhand.y());
  offhand = offhand * 0.65F + torso_center * 0.35F;

  return offhand;
}

} // namespace Render::GL
