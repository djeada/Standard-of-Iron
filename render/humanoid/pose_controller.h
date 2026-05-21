#pragma once

#include <QVector3D>

#include <cstdint>

#include "humanoid_renderer_base.h"
#include "humanoid_specs.h"

namespace Render::GL {

class HumanoidPoseController {
public:
  HumanoidPoseController(HumanoidPose& pose, const HumanoidAnimationContext& anim_ctx);

  void stand_idle();

  void apply_micro_idle(float time, std::uint32_t seed);

  void apply_ambient_idle(float time, std::uint32_t seed, float idle_duration);

  void apply_ambient_idle_explicit(AmbientIdleType idle_type, float phase);

  static auto get_ambient_idle_type(float time,
                                    std::uint32_t seed,
                                    float idle_duration) -> AmbientIdleType;

  static auto compute_ambient_idle_phase(float idle_duration,
                                         std::uint32_t seed) -> float;

  void kneel(float depth);
  void kneel_transition(float progress, bool standing_up);

  void lean(const QVector3D& direction, float amount);

  void place_hand_at(Side side, const QVector3D& target_position);

  void aim_bow(float draw_phase);
  void melee_strike(float strike_phase);
  void grasp_two_handed(const QVector3D& grip_center, float hand_separation);
  void spear_thrust(float attack_phase);
  void spear_thrust_from_hold(float attack_phase, float hold_depth);
  void construction_saw(float work_phase);
  void construction_chisel(float work_phase, bool kneeling);
  void sword_slash(float attack_phase);
  void combat_sword_slash_variant(float attack_phase,
                                  std::uint8_t variant,
                                  float reach_scale = 1.0F);
  void sword_slash_variant(float attack_phase,
                           std::uint8_t variant,
                           float reach_scale = 1.0F);
  void spear_thrust_variant(float attack_phase, std::uint8_t variant);
  void mount_on_horse(float saddle_height);
  void hold_spear_idle();
  void brace_spear_for_hold();
  void hold_bow_ready();
  void brace_sword_and_shield_for_hold();
  void hold_sword_and_shield();
  void guard_sword_and_shield_formation(ShieldFormationPose pose, float amount);
  void look_at(const QVector3D& target);
  void hit_flinch(float intensity);
  void tilt_torso(float side_tilt, float forward_tilt);

  auto solve_elbow_ik(Side side,
                      const QVector3D& shoulder,
                      const QVector3D& hand,
                      const QVector3D& outward_dir,
                      float along_frac,
                      float lateral_offset,
                      float y_bias,
                      float outward_sign) const -> QVector3D;

  auto solve_knee_ik(Side side,
                     const QVector3D& hip,
                     const QVector3D& foot,
                     float height_scale) const -> QVector3D;

  auto get_shoulder_y(Side side) const -> float;
  auto get_pelvis_y() const -> float;

private:
  HumanoidPose& m_pose;
  const HumanoidAnimationContext& m_anim_ctx;

  auto get_shoulder(Side side) const -> const QVector3D&;

  auto get_hand(Side side) -> QVector3D&;
  auto get_hand(Side side) const -> const QVector3D&;

  auto get_elbow(Side side) -> QVector3D&;

  auto compute_right_axis() const -> QVector3D;

  auto compute_outward_dir(Side side) const -> QVector3D;
};

} // namespace Render::GL
