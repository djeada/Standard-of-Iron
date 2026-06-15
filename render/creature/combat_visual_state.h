#pragma once

#include <cstdint>

#include "../../game/core/component.h"
#include "animation/combat_manifest.h"
#include "movement_state.h"

namespace Render::Creature {

using SoldierCombatLane = Animation::SoldierCombatLane;
using CombatVisualTransactionPhase = Animation::CombatTransactionPhase;
using CombatVisualExitPolicy = Animation::CombatExitPolicy;
using CombatVisualInterruptReason = Animation::CombatInterruptReason;
using CombatVisualTimingConfig = Animation::CombatTimingConfig;
using CombatVisualQueuedAttack = Animation::CombatQueuedAttack;
using CombatVisualPersistentState = Animation::CombatPersistentState;
using CombatVisualResolvedState = Animation::CombatResolvedState;

struct CombatVisualRawInputs {
  float sample_time{0.0F};
  float attack_offset{0.0F};
  bool has_attack_offset{false};
  bool attack_requested{false};
  bool is_melee{false};
  bool is_mounted{false};
  bool is_casting{false};
  bool finisher_attack{false};
  bool amplified_attack{false};
  bool is_hit_reacting{false};
  bool is_healing{false};
  bool is_constructing{false};
  bool is_dying{false};
  bool is_dead{false};
  MovementAnimationState locomotion{MovementAnimationState::Idle};
  Engine::Core::CombatAnimationState combat_phase{
      Engine::Core::CombatAnimationState::Idle};
  float combat_phase_progress{0.0F};
  std::uint8_t attack_variant{0U};
  std::uint32_t attack_target_id{0U};
  bool attack_target_alive{false};
  Engine::Core::CombatAttackFamily attack_family{
      Engine::Core::CombatAttackFamily::None};
};

using SoldierCombatLaneState = Animation::SoldierCombatLaneState;

struct CombatLaneInputs {
  std::uint32_t unit_seed{0U};
  std::uint32_t soldier_seed{0U};
  int row{0};
  int col{0};
  int rows{1};
  int cols{1};
  float health_ratio{1.0F};
  float local_enemy_pressure{0.0F};
  bool force_single_soldier{false};
  bool is_melee{false};
  bool is_mounted{false};
  Engine::Core::CombatAttackFamily attack_family{
      Engine::Core::CombatAttackFamily::None};
};

using CombatLaneProfile = Animation::CombatLaneProfile;

struct SoldierCombatLaneResolution {
  SoldierCombatLaneState state{};
  CombatLaneProfile profile{};
};

using CombatVisualStateResolution = Animation::CombatStateResolution;

[[nodiscard]] auto
default_combat_visual_timing_config() noexcept -> const CombatVisualTimingConfig&;

[[nodiscard]] auto resolve_soldier_combat_lane(const SoldierCombatLaneState& previous,
                                               const CombatLaneInputs& inputs) noexcept
    -> SoldierCombatLaneResolution;

[[nodiscard]] auto
next_attack_variant_for_lane(std::uint8_t base_variant,
                             Engine::Core::CombatAttackFamily family,
                             SoldierCombatLane lane,
                             float local_enemy_pressure,
                             std::uint8_t variant_bias,
                             std::uint32_t lane_seed,
                             std::uint32_t transaction_id) noexcept -> std::uint8_t;

[[nodiscard]] auto resolve_combat_visual_state(
    const CombatVisualPersistentState& previous,
    const CombatVisualRawInputs& raw,
    const CombatLaneProfile& lane,
    const CombatVisualTimingConfig& config =
        default_combat_visual_timing_config()) noexcept -> CombatVisualStateResolution;

} // namespace Render::Creature
