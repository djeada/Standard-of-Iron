#pragma once

#include "rig.h"
#include <QVector3D>

namespace Render::GL {

class HumanoidPoseController;
struct HorseMountFrame;

enum class SpearGrip {
  OVERHAND,    // Spear overhead for thrusting down
  COUCHED,     // Spear under arm for charging (lance style)
  TWO_HANDED   // Both hands on spear shaft
};

class MountedPoseController {
public:
  MountedPoseController(HumanoidPose &pose,
                        const HumanoidAnimationContext &anim_ctx);

  // Mounting
  void mountOnHorse(const HorseMountFrame &mount);
  void dismount();

  // Riding postures
  void ridingIdle(const HorseMountFrame &mount);
  void ridingLeaning(const HorseMountFrame &mount, float forward_lean,
                     float side_lean);
  void ridingCharging(const HorseMountFrame &mount, float intensity);
  void ridingReining(const HorseMountFrame &mount, float left_tension,
                     float right_tension);

  // Mounted combat
  void ridingMeleeStrike(const HorseMountFrame &mount, float attack_phase);
  void ridingSpearThrust(const HorseMountFrame &mount, float attack_phase);
  void ridingBowShot(const HorseMountFrame &mount, float draw_phase);
  void ridingShieldDefense(const HorseMountFrame &mount, bool raised);

  // Equipment interaction
  void holdReins(const HorseMountFrame &mount, float left_slack,
                 float right_slack);
  void holdSpearMounted(const HorseMountFrame &mount, SpearGrip grip_style);
  void holdBowMounted(const HorseMountFrame &mount);

private:
  HumanoidPose &m_pose;
  const HumanoidAnimationContext &m_anim_ctx;

  void attachFeetToStirrups(const HorseMountFrame &mount);
  void positionPelvisOnSaddle(const HorseMountFrame &mount);
  void liftBodyToMountHeight(float saddle_height);
  void calculateRidingKnees(const HorseMountFrame &mount);

  auto solveElbowIK(bool is_left, const QVector3D &shoulder,
                    const QVector3D &hand, const QVector3D &outward_dir,
                    float along_frac, float lateral_offset, float y_bias,
                    float outward_sign) const -> QVector3D;

  auto solveKneeIK(bool is_left, const QVector3D &hip, const QVector3D &foot,
                   float height_scale) const -> QVector3D;

  auto getShoulder(bool is_left) const -> const QVector3D &;
  auto getHand(bool is_left) -> QVector3D &;
  auto getHand(bool is_left) const -> const QVector3D &;
  auto getElbow(bool is_left) -> QVector3D &;
  auto computeRightAxis() const -> QVector3D;
  auto computeOutwardDir(bool is_left) const -> QVector3D;
};

} // namespace Render::GL
