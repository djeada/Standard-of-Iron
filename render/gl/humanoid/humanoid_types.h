#pragma once

#include <QVector3D>

#include <algorithm>
#include <cstdint>

#include "../../../game/core/component.h"
#include "../../creature/movement_state.h"
#include "../../palette.h"

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

[[nodiscard]] inline constexpr auto
ambient_idle_clip_variant(AmbientIdleType t) noexcept -> std::uint8_t {
  switch (t) {
  case AmbientIdleType::SitDown:
    return 1U;
  case AmbientIdleType::Jump:
    return 2U;
  case AmbientIdleType::RaiseWeapon:
    return 3U;
  case AmbientIdleType::ShiftWeight:
    return 4U;
  default:
    return 0U;
  }
}

enum class CombatAnimPhase : std::uint8_t {
  Idle,
  Advance,
  WindUp,
  Strike,
  Impact,
  Recover,
  Reposition
};

struct VisualMovementState {
  bool is_authoritative{false};
  Render::Creature::MovementAnimationState movement_state{
      Render::Creature::MovementAnimationState::Idle};
  bool has_velocity{false};
  bool has_navigation_intent{false};
  bool has_chase_intent{false};
  bool attack_target_in_range{false};
  bool has_movement_target{false};
  QVector3D locomotion_direction{0.0F, 0.0F, 1.0F};
  QVector3D movement_target{0.0F, 0.0F, 0.0F};
  float speed_hint{0.0F};
};

struct AnimationInputs {
  float time;
  Render::Creature::MovementAnimationState movement_state{
      Render::Creature::MovementAnimationState::Idle};
  VisualMovementState visual_movement{};
  bool is_attacking;
  bool is_melee;
  bool is_in_hold_mode;
  bool is_exiting_hold;
  float hold_exit_progress;
  float hold_entry_progress;
  CombatAnimPhase combat_phase{CombatAnimPhase::Idle};
  float combat_phase_progress{0.0F};
  Engine::Core::CombatAttackFamily attack_family{
      Engine::Core::CombatAttackFamily::None};
  std::uint8_t attack_variant{0};
  bool finisher_attack{false};
  float attack_offset{0.0F};
  bool has_attack_offset{false};
  bool is_in_melee_lock{false};
  bool is_hit_reacting{false};
  float hit_reaction_intensity{0.0F};
  bool is_healing{false};
  float healing_target_dx{0.0F};
  float healing_target_dz{0.0F};
  bool is_constructing{false};
  float construction_progress{0.0F};
  bool is_dying{false};
  bool is_dead{false};
  float death_progress{0.0F};
  std::uint8_t death_variant{0};
  float idle_duration{0.0F};
  bool is_guarding{false};
  float guard_pose_progress{0.0F};
};

inline auto hold_transition_amount(const AnimationInputs& inputs) -> float {
  if (inputs.is_in_hold_mode) {
    return std::clamp(inputs.hold_entry_progress, 0.0F, 1.0F);
  }
  if (inputs.is_exiting_hold) {
    return std::clamp(1.0F - inputs.hold_exit_progress, 0.0F, 1.0F);
  }
  return 0.0F;
}

inline auto guard_pose_amount(const AnimationInputs& inputs) -> float {
  if (!inputs.is_guarding) {
    return 0.0F;
  }
  return std::clamp(inputs.guard_pose_progress, 0.0F, 1.0F);
}

struct FormationParams {
  int individuals_per_unit;
  int max_per_row;
  float spacing;
};

struct AttachmentFrame {
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 1.0F, 0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};
  float radius{0.0F};
  float depth{0.0F};
};

using HeadFrame = AttachmentFrame;

struct BodyFrames {
  AttachmentFrame head{};
  AttachmentFrame torso{};
  AttachmentFrame back{};
  AttachmentFrame waist{};
  AttachmentFrame shoulder_l{};
  AttachmentFrame shoulder_r{};
  AttachmentFrame hand_l{};
  AttachmentFrame hand_r{};
  AttachmentFrame grip_l{};
  AttachmentFrame grip_r{};
  AttachmentFrame foot_l{};
  AttachmentFrame foot_r{};

  AttachmentFrame shin_l{};
  AttachmentFrame shin_r{};
};

struct HumanoidPose {
  QVector3D head_pos;
  float head_r{};
  QVector3D neck_base;

  HeadFrame head_frame{};

  BodyFrames body_frames{};

  QVector3D shoulder_l, shoulder_r;
  QVector3D elbow_l, elbow_r;
  QVector3D hand_l, hand_r;

  QVector3D pelvis_pos;
  QVector3D knee_l, knee_r;

  float foot_y_offset{};
  QVector3D foot_l, foot_r;
};

struct VariationParams {
  float height_scale;
  float bulk_scale;
  float stance_width;
  float arm_swing_amp;
  float walk_speed_mult;
  float posture_slump;
  float shoulder_tilt;

  static auto from_seed(uint32_t seed) -> VariationParams {
    VariationParams v{};

    auto next_rand = [](uint32_t& s) -> float {
      s = s * 1664525U + 1013904223U;
      return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
    };

    uint32_t rng = seed;
    v.height_scale = 0.95F + next_rand(rng) * 0.10F;
    v.bulk_scale = 0.92F + next_rand(rng) * 0.16F;
    v.stance_width = 0.88F + next_rand(rng) * 0.24F;
    v.arm_swing_amp = 0.85F + next_rand(rng) * 0.30F;
    v.walk_speed_mult = 0.90F + next_rand(rng) * 0.20F;
    v.posture_slump = next_rand(rng) * 0.08F;
    v.shoulder_tilt = (next_rand(rng) - 0.5F) * 0.06F;

    return v;
  }
};

enum class FacialHairStyle {
  None,
  Stubble,
  ShortBeard,
  FullBeard,
  LongBeard,
  Goatee,
  Mustache,
  MustacheAndBeard
};

struct FacialHairParams {
  FacialHairStyle style = FacialHairStyle::None;
  QVector3D color{0.15F, 0.12F, 0.10F};
  float length = 1.0F;
  float thickness = 1.0F;
  float coverage = 1.0F;
  float greyness = 0.0F;
};

struct HumanoidVariant {
  HumanoidPalette palette;
  FacialHairParams facial_hair;
  float muscularity = 1.0F;
  float scarring = 0.0F;
  float weathering = 0.0F;
  float grime = 0.0F;
  float bloodiness = 0.0F;
  float pattern_seed = 0.0F;
};

[[nodiscard]] inline auto next_seeded_unit_float(std::uint32_t& s) noexcept -> float {
  s = s * 1664525U + 1013904223U;
  return float(s & 0x7FFFFFU) / float(0x7FFFFFU);
}

inline void seed_missing_humanoid_wear(HumanoidVariant& variant,
                                       std::uint32_t seed) noexcept {
  std::uint32_t rng = seed ^ 0xA341316CU;
  float const generated_scarring =
      next_seeded_unit_float(rng) * next_seeded_unit_float(rng) * 0.42F;
  float const generated_weathering = 0.30F + next_seeded_unit_float(rng) * 0.55F;
  float const generated_grime = 0.18F + next_seeded_unit_float(rng) * 0.52F;
  float const generated_blood = next_seeded_unit_float(rng) *
                                next_seeded_unit_float(rng) *
                                next_seeded_unit_float(rng) * 0.34F;
  float const generated_pattern = next_seeded_unit_float(rng);

  if (variant.scarring <= 0.0F) {
    variant.scarring = generated_scarring;
  }
  if (variant.weathering <= 0.0F) {
    variant.weathering = generated_weathering;
  }
  if (variant.grime <= 0.0F) {
    variant.grime = generated_grime;
  }
  if (variant.bloodiness <= 0.0F) {
    variant.bloodiness = generated_blood;
  }
  if (variant.pattern_seed <= 0.0F) {
    variant.pattern_seed = generated_pattern;
  }
}

enum class HumanoidMotionState {
  Idle,
  Walk,
  Run,
  Hold,
  Attacking
};

struct HumanoidGaitDescriptor {
  HumanoidMotionState state{HumanoidMotionState::Idle};
  float speed{0.0F};
  float normalized_speed{0.0F};
  float cycle_time{0.0F};
  float cycle_phase{0.0F};
  float stride_distance{0.0F};
  QVector3D velocity{0.0F, 0.0F, 0.0F};
  bool has_target{false};
  bool is_airborne{false};
  float locomotion_blend{0.0F};
  float run_blend{0.0F};
  float turn_amount{0.0F};
  float acceleration{0.0F};

  auto is_stationary() const -> bool { return speed <= 0.01F; }
  auto is_walking() const -> bool { return state == HumanoidMotionState::Walk; }
  auto is_running() const -> bool { return state == HumanoidMotionState::Run; }
  auto is_holding() const -> bool { return state == HumanoidMotionState::Hold; }
  auto is_attacking() const -> bool { return state == HumanoidMotionState::Attacking; }
};

struct HumanoidAnimationContext {
  AnimationInputs inputs;
  VariationParams variation;
  FormationParams formation;
  HumanoidGaitDescriptor gait{};
  float locomotion_cycle_time{0.0F};
  float locomotion_phase{0.0F};
  float attack_phase{0.0F};
  float attack_emphasis{1.0F};
  bool amplified_attack{false};
  bool finisher_attack{false};
  float jitter_seed{0.0F};
  QVector3D entity_forward{0.0F, 0.0F, 1.0F};
  QVector3D entity_right{1.0F, 0.0F, 0.0F};
  QVector3D entity_up{0.0F, 1.0F, 0.0F};
  QVector3D locomotion_direction{0.0F, 0.0F, 1.0F};
  QVector3D locomotion_velocity{0.0F, 0.0F, 0.0F};
  QVector3D movement_target{0.0F, 0.0F, 0.0F};
  QVector3D instance_position{0.0F, 0.0F, 0.0F};
  float move_speed{0.0F};
  bool has_movement_target{false};
  float yaw_radians{0.0F};
  float yaw_degrees{0.0F};
  AmbientIdleType ambient_idle_type{AmbientIdleType::None};
  float ambient_idle_phase{0.0F};

  auto locomotion_speed() const -> float { return gait.speed; }
  auto locomotion_normalized_speed() const -> float { return gait.normalized_speed; }
  auto locomotion_forward() const -> QVector3D { return locomotion_direction; }
  auto locomotion_velocity_flat() const -> QVector3D { return gait.velocity; }
  auto heading_forward() const -> QVector3D { return entity_forward; }
  auto heading_right() const -> QVector3D { return entity_right; }
  auto heading_up() const -> QVector3D { return entity_up; }
  auto is_stationary() const -> bool { return gait.is_stationary(); }
  auto is_walking() const -> bool { return gait.is_walking(); }
  auto is_running() const -> bool { return gait.is_running(); }
  auto is_holding() const -> bool { return gait.is_holding(); }
  auto is_attacking() const -> bool { return gait.is_attacking(); }
};

} // namespace Render::GL
