#pragma once

#include <cstdint>

#include "../../game/core/component.h"
#include "movement_state.h"

namespace Render::Creature {

enum class CombatVisualTransactionPhase : std::uint8_t {
  None = 0,
  Enter,
  Anticipation,
  Strike,
  FollowThrough,
  Recover,
  ExitBlend,
};

enum class CombatVisualExitPolicy : std::uint8_t {
  None = 0,
  NormalComplete,
  TargetDied,
  ChaseResumes,
  Retarget,
};

enum class CombatVisualInterruptReason : std::uint8_t {
  None = 0,
  NormalComplete,
  TargetDied,
  ChaseResumes,
  Retarget,
  HitReaction,
  Death,
  Invalidated,
};

enum class SoldierCombatLane : std::uint8_t {
  None = 0,
  LeadStrike,
  SupportStrike,
  ShieldBrace,
  StepIn,
  StepOut,
  IdleReady,
  RangedReload,
};

struct CombatVisualTimingConfig {
  float minimum_hold_duration{0.10F};
  float recover_duration{0.30F};
  float exit_blend_duration{0.16F};
  float new_transaction_reset_threshold{0.18F};
};

struct CombatVisualQueuedAttack {
  bool pending{false};
  std::uint32_t target_id{0U};
  Engine::Core::CombatAttackFamily family{Engine::Core::CombatAttackFamily::None};
  SoldierCombatLane lane{SoldierCombatLane::None};
};

struct CombatVisualPersistentState {
  bool active{false};
  bool is_melee{false};
  bool is_mounted{false};
  bool is_casting{false};
  bool finisher_attack{false};
  bool amplified_attack{false};
  SoldierCombatLane locked_lane{SoldierCombatLane::None};
  CombatVisualQueuedAttack queued_next{};
  CombatVisualTransactionPhase phase{CombatVisualTransactionPhase::None};
  CombatVisualExitPolicy exit_policy{CombatVisualExitPolicy::None};
  CombatVisualInterruptReason interruption_reason{CombatVisualInterruptReason::None};
  float attack_phase{0.0F};
  float phase_progress{0.0F};
  float last_sample_time{0.0F};
  float phase_start_time{0.0F};
  float transaction_start_time{0.0F};
  float minimum_hold_until{0.0F};
  float exit_blend_progress{0.0F};
  float attack_emphasis{1.0F};
  std::uint8_t locked_variant{0U};
  std::uint32_t transaction_id{0U};
  std::uint32_t source_target_id{0U};
  Engine::Core::CombatAttackFamily locked_family{
      Engine::Core::CombatAttackFamily::None};
};

struct CombatVisualResolvedState {
  bool authoritative{false};
  bool active{false};
  bool prioritize_action_over_locomotion{false};
  bool is_melee{false};
  bool is_mounted{false};
  bool is_casting{false};
  bool finisher_attack{false};
  bool amplified_attack{false};
  CombatVisualTransactionPhase phase{CombatVisualTransactionPhase::None};
  CombatVisualExitPolicy exit_policy{CombatVisualExitPolicy::None};
  CombatVisualInterruptReason interruption_reason{CombatVisualInterruptReason::None};
  SoldierCombatLane lane{SoldierCombatLane::None};
  float attack_phase{0.0F};
  float phase_progress{0.0F};
  float exit_blend_progress{0.0F};
  float attack_emphasis{1.0F};
  std::uint8_t attack_variant{0U};
  std::uint32_t transaction_id{0U};
  std::uint32_t source_target_id{0U};
  Engine::Core::CombatAttackFamily attack_family{
      Engine::Core::CombatAttackFamily::None};
};

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

struct SoldierCombatLaneState {
  bool initialized{false};
  SoldierCombatLane lane{SoldierCombatLane::None};
  std::uint8_t pressure_bucket{0U};
  std::uint8_t variant_bias{0U};
  std::uint32_t signature{0U};
  std::uint32_t lane_seed{0U};
};

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

struct CombatLaneProfile {
  SoldierCombatLane lane{SoldierCombatLane::None};
  float phase_bias{0.0F};
  float recover_scale{1.0F};
  float emphasis_scale{1.0F};
  float local_enemy_pressure{0.0F};
  std::uint8_t variant_bias{0U};
  std::uint32_t lane_seed{0U};
};

struct SoldierCombatLaneResolution {
  SoldierCombatLaneState state{};
  CombatLaneProfile profile{};
};

struct CombatVisualStateResolution {
  CombatVisualPersistentState persistent{};
  CombatVisualResolvedState resolved{};
};

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
