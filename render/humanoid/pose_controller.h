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

  void aimBow(float draw_phase);
  void meleeStrike(float strike_phase);
  void graspTwoHanded(const QVector3D &grip_center, float hand_separation);
  void spearThrust(float attack_phase);
  void swordSlash(float attack_phase);
  void mountOnHorse(float saddle_height);
  void hold_sword_and_shield();
  void look_at(const QVector3D &target);

  auto solveElbowIK(bool is_left, const QVector3D &shoulder,
                    const QVector3D &hand, const QVector3D &outward_dir,
                    float along_frac, float lateral_offset, float y_bias,
                    float outward_sign) const -> QVector3D;

  auto solveKneeIK(bool is_left, const QVector3D &hip, const QVector3D &foot,
                   float height_scale) const -> QVector3D;

  auto get_shoulder_y(bool is_left) const -> float;
  auto get_pelvis_y() const -> float;

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
