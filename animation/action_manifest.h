#pragma once

#include <cstdint>

#include "combat_manifest.h"

namespace Animation {

enum class CastVisualKind : std::uint8_t {
  None = 0,
  Fireball,
};

struct HumanoidDeathActionInputs {
  bool active{false};
  bool dying{false};
  float state_time{0.0F};
  float state_duration{0.0F};
  std::uint8_t variant{0U};
};

struct HumanoidConstructionActionInputs {
  bool active{false};
  float build_time{0.0F};
  float time_remaining{0.0F};
  float cycles_per_second{1.75F};
};

struct HumanoidCombatActionInputs {
  bool has_state{false};
  CombatPhase phase{CombatPhase::Idle};
  float phase_time{0.0F};
  float phase_duration{0.0F};
  CombatAttackFamily attack_family{CombatAttackFamily::None};
  std::uint8_t attack_variant{0U};
  bool finisher_attack{false};
  float attack_offset{0.0F};
  bool fallback_mode_is_melee{false};
};

struct HumanoidMeleeLockActionInputs {
  bool in_lock{false};
  bool participates{false};
  CombatAttackFamily fallback_attack_family{CombatAttackFamily::None};
};

struct HumanoidHitReactionActionInputs {
  bool active{false};
  float reaction_time{0.0F};
  float reaction_duration{0.0F};
  float intensity{0.0F};
  float knockback_x{0.0F};
  float knockback_z{0.0F};
};

struct HumanoidCastActionInputs {
  bool has_projectile_cast{false};
  bool projectile_is_fireball{false};
};

struct HumanoidCommanderJumpPoseInputs {
  bool has_commander{false};
  bool active{false};
  float phase{0.0F};
  float height_offset{0.0F};
};

struct HumanoidCommanderJumpPose {
  bool active{false};
  float phase{0.0F};
  float height_offset{0.0F};
};

struct HumanoidCommanderAttackPoseInputs {
  bool has_commander{false};
  bool fpv_controlled{false};
  bool is_melee{false};
  bool has_style{false};
  std::uint8_t style{0U};
};

struct HumanoidCommanderAttackPose {
  bool amplified{false};
  bool has_style{false};
  std::uint8_t style{0U};
};

struct HumanoidCommanderFlagRallyPoseInputs {
  bool has_commander{false};
  bool planting{false};
  float animation_timer{0.0F};
  float cost{0.0F};
};

struct HumanoidCommanderFlagRallyPose {
  bool active{false};
  float phase{0.0F};
};

struct HumanoidActionSampleInputs {
  HumanoidDeathActionInputs death{};
  HumanoidConstructionActionInputs construction{};
  HumanoidCombatActionInputs combat{};
  HumanoidMeleeLockActionInputs melee_lock{};
  HumanoidHitReactionActionInputs hit_reaction{};
  HumanoidCastActionInputs cast{};
};

struct HumanoidActionSample {
  bool is_attacking{false};
  bool is_melee{false};
  bool is_in_melee_lock{false};
  CombatPhase combat_phase{CombatPhase::Idle};
  float combat_phase_progress{0.0F};
  CombatAttackFamily attack_family{CombatAttackFamily::None};
  std::uint8_t attack_variant{0U};
  bool finisher_attack{false};
  float attack_offset{0.0F};
  bool has_attack_offset{false};

  bool is_casting{false};
  CastVisualKind cast_kind{CastVisualKind::None};

  bool is_hit_reacting{false};
  float hit_reaction_intensity{0.0F};
  float hit_recoil_x{0.0F};
  float hit_recoil_z{0.0F};

  bool is_constructing{false};
  float construction_progress{0.0F};

  bool is_dying{false};
  bool is_dead{false};
  float death_progress{0.0F};
  std::uint8_t death_variant{0U};

  bool attack_from_combat_state{false};
  bool attack_from_melee_lock{false};
};

struct HumanoidAttackPhaseInputs {
  bool is_attacking{false};
  CombatPhase combat_phase{CombatPhase::Idle};
  float combat_phase_progress{0.0F};
  bool finisher_attack{false};
  float sample_time{0.0F};
  float attack_offset{0.0F};
  bool has_attack_offset{false};
};

[[nodiscard]] auto wrap_action_phase(float phase) noexcept -> float;

[[nodiscard]] auto resolve_humanoid_action_sample(
    const HumanoidActionSampleInputs& inputs) noexcept -> HumanoidActionSample;

[[nodiscard]] auto resolve_humanoid_commander_jump_pose(
    const HumanoidCommanderJumpPoseInputs& inputs) noexcept
    -> HumanoidCommanderJumpPose;

[[nodiscard]] auto resolve_humanoid_commander_attack_pose(
    const HumanoidCommanderAttackPoseInputs& inputs) noexcept
    -> HumanoidCommanderAttackPose;

[[nodiscard]] auto resolve_humanoid_commander_flag_rally_pose(
    const HumanoidCommanderFlagRallyPoseInputs& inputs) noexcept
    -> HumanoidCommanderFlagRallyPose;

[[nodiscard]] auto approximate_humanoid_attack_phase(
    const HumanoidAttackPhaseInputs& inputs) noexcept -> float;

} // namespace Animation
