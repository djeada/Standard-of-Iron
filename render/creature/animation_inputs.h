#pragma once

#include <QVector3D>

#include <cstdint>

#include "animation/action_manifest.h"
#include "animation/clip_manifest.h"
#include "animation/guard_manifest.h"
#include "animation/individuality_manifest.h"
#include "animation/locomotion_manifest.h"
#include "animation/playback_manifest.h"
#include "game/core/component_combat.h"
#include "render/creature/combat_visual_state.h"
#include "render/creature/movement_state.h"

namespace Render::GL {

using AmbientIdleType = Animation::HumanoidAmbientIdle;

[[nodiscard]] inline auto
ambient_idle_clip_variant(AmbientIdleType t) noexcept -> std::uint8_t {
  return Animation::humanoid_ambient_idle_clip_variant(t);
}

using CastVisualKind = Animation::CastVisualKind;

using CombatAnimPhase = Engine::Core::CombatAnimationState;

using ConstructionRole = Animation::HumanoidConstructionRole;

using ShieldFormationPose = Animation::ShieldFormationPose;

struct VisualMovementState {
  bool is_authoritative{false};
  Render::Creature::MovementAnimationState movement_state{
      Render::Creature::MovementAnimationState::Idle};
  bool has_velocity{false};
  bool has_navigation_intent{false};
  bool has_chase_intent{false};
  bool forced_displacement{false};

  bool measured_from_ground{false};

  bool facing_independent_of_travel{false};
  bool attack_target_in_range{false};
  bool has_movement_target{false};
  QVector3D locomotion_direction{0.0F, 0.0F, 1.0F};
  QVector3D movement_target{0.0F, 0.0F, 0.0F};
  float speed_hint{0.0F};
};

struct AnimationInputs {
  float time{0.0F};
  Render::Creature::MovementAnimationState movement_state{
      Render::Creature::MovementAnimationState::Idle};
  VisualMovementState visual_movement{};

  float action_link_weight{0.0F};
  bool is_mounted{false};
  bool is_attacking{false};
  bool is_melee{false};
  bool is_in_hold_mode{false};

  bool hold_attack_preserves_pose{false};
  bool is_exiting_hold{false};
  float hold_exit_progress{0.0F};
  float hold_entry_progress{0.0F};
  CombatAnimPhase combat_phase{CombatAnimPhase::Idle};
  float combat_phase_progress{0.0F};

  Animation::MeleeIntent melee_intent{};
  bool melee_intent_valid{false};
  Animation::MeleeRestDirection melee_rest{};
  bool melee_rest_valid{false};
  Engine::Core::CombatAttackFamily attack_family{
      Engine::Core::CombatAttackFamily::None};
  std::uint8_t attack_variant{0};
  bool has_sword_attack_animation{false};
  Animation::SwordAttackAnimation sword_attack_animation{
      Animation::SwordAttackAnimation::InfantrySlashA};
  bool has_showcase_clip{false};
  std::uint16_t showcase_clip{Animation::k_unmapped_clip};
  float showcase_phase{0.0F};
  bool has_authored_action_clip{false};
  std::uint16_t authored_action_clip{Animation::k_unmapped_clip};
  float authored_action_phase{0.0F};
  bool has_authored_action_phase{false};
  bool simulation_owns_root_motion{false};
  bool finisher_attack{false};
  float attack_offset{0.0F};
  bool has_attack_offset{false};
  bool is_in_melee_lock{false};
  bool attack_from_melee_lock{false};
  std::uint8_t attack_exchange_outcome{0};
  bool is_casting{false};
  CastVisualKind cast_kind{CastVisualKind::None};
  bool is_hit_reacting{false};
  float hit_reaction_intensity{0.0F};
  float hit_reaction_progress{0.0F};
  Engine::Core::HitReactionKind hit_reaction_kind{
      Engine::Core::HitReactionKind::Flinch};

  float hit_recoil_x{0.0F};
  float hit_recoil_z{0.0F};
  bool is_healing{false};
  bool is_routing{false};

  bool is_wading{false};
  float wade_amount{0.0F};
  float healing_target_dx{0.0F};
  float healing_target_dz{0.0F};
  bool is_constructing{false};
  bool is_carrying_load{false};
  float construction_progress{0.0F};
  std::uint8_t construction_job{0};
  bool is_dying{false};
  bool is_dead{false};
  float death_progress{0.0F};
  std::uint8_t death_variant{0};
  float death_sink_progress{0.0F};
  float idle_duration{0.0F};
  bool is_guarding{false};
  bool is_defensive_layout_locked{false};
  bool is_exiting_guard{false};
  float guard_pose_progress{0.0F};
  ShieldFormationPose shield_formation_pose{ShieldFormationPose::None};
  Render::Creature::CombatVisualResolvedState combat_visual{};
};

inline auto hold_transition_amount(const AnimationInputs& inputs) -> float {
  return Animation::humanoid_hold_transition_amount({
      .is_in_hold_mode = inputs.is_in_hold_mode,
      .is_exiting_hold = inputs.is_exiting_hold,
      .hold_entry_progress = inputs.hold_entry_progress,
      .hold_exit_progress = inputs.hold_exit_progress,
  });
}

inline auto guard_pose_amount(const AnimationInputs& inputs) -> float {
  return Animation::humanoid_guard_pose_amount({
      .is_guarding = inputs.is_guarding,
      .is_exiting_guard = inputs.is_exiting_guard,
      .guard_pose_progress = inputs.guard_pose_progress,
  });
}

using HumanoidMotionState = Animation::HumanoidMotionState;

} // namespace Render::GL
