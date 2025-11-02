#pragma once

#include "humanoid_specs.h"
#include "rig.h"
#include <QVector3D>
#include <cstdint>

namespace Render::GL {

class HumanoidPoseController {
public:
  HumanoidPoseController(HumanoidPose &pose,
                         const HumanoidAnimationContext &anim_ctx);

  void standIdle();

  void kneel(float depth);

  void lean(const QVector3D &direction, float amount);

  void placeHandAt(bool is_left, const QVector3D &target_position);

  auto solveElbowIK(bool is_left, const QVector3D &shoulder,
                    const QVector3D &hand, const QVector3D &outward_dir,
                    float along_frac, float lateral_offset, float y_bias,
                    float outward_sign) const -> QVector3D;

  auto solveKneeIK(bool is_left, const QVector3D &hip, const QVector3D &foot,
                   float height_scale) const -> QVector3D;

private:
  HumanoidPose &m_pose;
  const HumanoidAnimationContext &m_anim_ctx;

  auto getShoulder(bool is_left) const -> const QVector3D &;

  auto getHand(bool is_left) -> QVector3D &;
  auto getHand(bool is_left) const -> const QVector3D &;

  auto getElbow(bool is_left) -> QVector3D &;

  auto computeRightAxis() const -> QVector3D;

  auto computeOutwardDir(bool is_left) const -> QVector3D;
};

} // namespace Render::GL
