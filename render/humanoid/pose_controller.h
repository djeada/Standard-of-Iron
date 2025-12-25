#pragma once

#include "humanoid_specs.h"
#include "rig.h"
#include <QVector3D>
#include <cstdint>

namespace Render::GL {

enum class AmbientIdleType : std::uint8_t {
  None = 0,
  SitDown,
  ShuffleFeet,
  TapFoot,
  ShiftWeight,
  StepInPlace,
  BendKnee,
  RaiseWeapon,
  Jump
};

class HumanoidPoseController {
public:
  HumanoidPoseController(HumanoidPose &pose,
                         const HumanoidAnimationContext &anim_ctx);

  void stand_idle();

  void apply_micro_idle(float time, std::uint32_t seed);

  void apply_ambient_idle(float time, std::uint32_t seed, float idle_duration);

  void apply_ambient_idle_explicit(AmbientIdleType idle_type, float phase);

  static auto get_ambient_idle_type(float time, std::uint32_t seed,
                                    float idle_duration) -> AmbientIdleType;

  void kneel(float depth);
  void kneelTransition(float progress, bool standing_up);

  void lean(const QVector3D &direction, float amount);

  void place_hand_at(bool is_left, const QVector3D &target_position);

  void aim_bow(float draw_phase);
  void meleeStrike(float strike_phase);
  void graspTwoHanded(const QVector3D &grip_center, float hand_separation);
  void spearThrust(float attack_phase);
  void spearThrustFromHold(float attack_phase, float hold_depth);
  void sword_slash(float attack_phase);
  void sword_slash_variant(float attack_phase, std::uint8_t variant);
  void spear_thrust_variant(float attack_phase, std::uint8_t variant);
  void mount_on_horse(float saddle_height);
  void hold_sword_and_shield();
  void look_at(const QVector3D &target);
  void hit_flinch(float intensity);
  void tilt_torso(float side_tilt, float forward_tilt);

  auto solve_elbow_ik(bool is_left, const QVector3D &shoulder,
                      const QVector3D &hand, const QVector3D &outward_dir,
                      float along_frac, float lateral_offset, float y_bias,
                      float outward_sign) const -> QVector3D;

  auto solve_knee_ik(bool is_left, const QVector3D &hip, const QVector3D &foot,
                     float height_scale) const -> QVector3D;

  auto get_shoulder_y(bool is_left) const -> float;
  auto get_pelvis_y() const -> float;

private:
  HumanoidPose &m_pose;
  const HumanoidAnimationContext &m_anim_ctx;

  auto get_shoulder(bool is_left) const -> const QVector3D &;

  auto get_hand(bool is_left) -> QVector3D &;
  auto get_hand(bool is_left) const -> const QVector3D &;

  auto get_elbow(bool is_left) -> QVector3D &;

  auto compute_right_axis() const -> QVector3D;

  auto compute_outward_dir(bool is_left) const -> QVector3D;
};

} // namespace Render::GL
