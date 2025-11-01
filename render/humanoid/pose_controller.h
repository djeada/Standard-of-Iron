#pragma once

#include "rig.h"
#include "humanoid_specs.h"
#include <QVector3D>
#include <cstdint>

namespace Render::GL {

/**
 * High-level pose controller API that encapsulates low-level joint manipulation logic.
 * Unit renderers should call methods like kneel(), aimBow(), or meleeStrike() instead
 * of directly setting pose coordinates.
 */
class HumanoidPoseController {
public:
  /**
   * Construct a pose controller operating on the given pose and animation context.
   * 
   * @param pose Reference to the HumanoidPose to manipulate
   * @param anim_ctx Animation context providing motion state and timing information
   */
  HumanoidPoseController(HumanoidPose &pose,
                         const HumanoidAnimationContext &anim_ctx);

  // ---- Basic Stance Methods ----

  /**
   * Set the humanoid to a standing idle pose.
   * This is the default neutral stance.
   */
  void standIdle();

  /**
   * Make the humanoid kneel down.
   * 
   * @param depth Kneel depth factor (0.0 = standing, 1.0 = full kneel)
   */
  void kneel(float depth);

  /**
   * Lean the body in a specific direction.
   * 
   * @param direction Direction vector to lean towards (normalized)
   * @param amount Lean amount (0.0 = upright, 1.0 = maximum lean)
   */
  void lean(const QVector3D &direction, float amount);

  // ---- Hand Positioning ----

  /**
   * Position a hand at a target position, computing elbow via IK.
   * 
   * @param is_left True for left hand, false for right hand
   * @param target_position World position to place the hand at
   */
  void placeHandAt(bool is_left, const QVector3D &target_position);

  // ---- Low-level IK Utilities ----

  /**
   * Solve elbow position using inverse kinematics for torso-relative arms.
   * This is the low-level IK logic extracted from the base renderer.
   * 
   * @param is_left True for left arm, false for right arm
   * @param shoulder Shoulder position
   * @param hand Hand position
   * @param outward_dir Outward direction vector for elbow bend
   * @param along_frac Fraction along the arm (0.0-1.0) to place elbow
   * @param lateral_offset Lateral offset for elbow bend
   * @param y_bias Vertical bias for elbow position
   * @param outward_sign Sign multiplier for outward direction (+1.0 or -1.0)
   * @return Computed elbow position
   */
  auto solveElbowIK(bool is_left, const QVector3D &shoulder,
                    const QVector3D &hand, const QVector3D &outward_dir,
                    float along_frac, float lateral_offset, float y_bias,
                    float outward_sign) const -> QVector3D;

  /**
   * Solve knee position using inverse kinematics for leg.
   * This is the low-level IK logic extracted from the base renderer.
   * 
   * @param is_left True for left leg, false for right leg
   * @param hip Hip position
   * @param foot Foot position
   * @param height_scale Height scaling factor for proportions
   * @return Computed knee position
   */
  auto solveKneeIK(bool is_left, const QVector3D &hip, const QVector3D &foot,
                   float height_scale) const -> QVector3D;

private:
  HumanoidPose &m_pose;
  const HumanoidAnimationContext &m_anim_ctx;

  // Helper to get shoulder position for the specified side
  auto getShoulder(bool is_left) const -> const QVector3D &;
  
  // Helper to get hand position reference for the specified side
  auto getHand(bool is_left) -> QVector3D &;
  auto getHand(bool is_left) const -> const QVector3D &;
  
  // Helper to get elbow position reference for the specified side
  auto getElbow(bool is_left) -> QVector3D &;
  
  // Helper to compute right axis from shoulders
  auto computeRightAxis() const -> QVector3D;
  
  // Helper to compute outward direction for arm
  auto computeOutwardDir(bool is_left) const -> QVector3D;
};

} // namespace Render::GL
