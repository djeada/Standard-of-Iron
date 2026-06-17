#pragma once

#include <cstdint>

#include "pose_manifest.h"

namespace Animation {

enum class CombatAttackFamily : std::uint8_t {
  None,
  Sword,
  Spear,
  Bow,
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
  CombatAttackFamily attack_family{CombatAttackFamily::None};
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

enum class CombatPhase : std::uint8_t {
  Idle,
  Advance,
  WindUp,
  Strike,
  Impact,
  Recover,
  Reposition,
};

struct CombatPhaseWindow {
  float start{0.0F};
  float end{0.0F};
  float offset_weight{0.0F};
};

struct ScrubbedCombatPhase {
  CombatPhase phase{CombatPhase::Idle};
  float progress{0.0F};
};

enum class CombatTransactionPhase : std::uint8_t {
  None = 0,
  Enter,
  Anticipation,
  Strike,
  FollowThrough,
  Recover,
  ExitBlend,
};

enum class CombatExitPolicy : std::uint8_t {
  None = 0,
  NormalComplete,
  TargetDied,
  ChaseResumes,
  Retarget,
};

enum class CombatInterruptReason : std::uint8_t {
  None = 0,
  NormalComplete,
  TargetDied,
  ChaseResumes,
  Retarget,
  HitReaction,
  Death,
  Invalidated,
};

struct CombatTimingConfig {
  float minimum_hold_duration{0.10F};
  float recover_duration{0.30F};
  float exit_blend_duration{0.16F};
  float new_transaction_reset_threshold{0.18F};
};

struct CombatQueuedAttack {
  bool pending{false};
  std::uint32_t target_id{0U};
  CombatAttackFamily family{CombatAttackFamily::None};
  SoldierCombatLane lane{SoldierCombatLane::None};
};

struct CombatPersistentState {
  bool active{false};
  bool is_melee{false};
  bool is_mounted{false};
  bool is_casting{false};
  bool finisher_attack{false};
  bool amplified_attack{false};
  SoldierCombatLane locked_lane{SoldierCombatLane::None};
  CombatQueuedAttack queued_next{};
  CombatTransactionPhase phase{CombatTransactionPhase::None};
  CombatExitPolicy exit_policy{CombatExitPolicy::None};
  CombatInterruptReason interruption_reason{CombatInterruptReason::None};
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
  CombatAttackFamily locked_family{CombatAttackFamily::None};
};

struct CombatResolvedState {
  bool authoritative{false};
  bool active{false};
  bool prioritize_action_over_locomotion{false};
  bool is_melee{false};
  bool is_mounted{false};
  bool is_casting{false};
  bool finisher_attack{false};
  bool amplified_attack{false};
  CombatTransactionPhase phase{CombatTransactionPhase::None};
  CombatExitPolicy exit_policy{CombatExitPolicy::None};
  CombatInterruptReason interruption_reason{CombatInterruptReason::None};
  SoldierCombatLane lane{SoldierCombatLane::None};
  float attack_phase{0.0F};
  float phase_progress{0.0F};
  float exit_blend_progress{0.0F};
  float attack_emphasis{1.0F};
  std::uint8_t attack_variant{0U};
  std::uint32_t transaction_id{0U};
  std::uint32_t source_target_id{0U};
  CombatAttackFamily attack_family{CombatAttackFamily::None};
};

struct CombatRawInputs {
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
  MovementState locomotion{MovementState::Idle};
  CombatPhase combat_phase{CombatPhase::Idle};
  float combat_phase_progress{0.0F};
  std::uint8_t attack_variant{0U};
  std::uint32_t attack_target_id{0U};
  bool attack_target_alive{false};
  CombatAttackFamily attack_family{CombatAttackFamily::None};
};

struct CombatStateResolution {
  CombatPersistentState persistent{};
  CombatResolvedState resolved{};
};

inline constexpr std::uint8_t k_attack_variant_seed_slots = 8U;

[[nodiscard]] constexpr auto
combat_pressure_bucket(float local_enemy_pressure) noexcept -> std::uint8_t {
  float const clamped =
      local_enemy_pressure < 0.0F
          ? 0.0F
          : (local_enemy_pressure > 1.0F ? 1.0F : local_enemy_pressure);
  return static_cast<std::uint8_t>(clamped * 2.99F);
}

[[nodiscard]] constexpr auto
combat_mix_hash(std::uint32_t value, std::uint32_t seed) noexcept -> std::uint32_t {
  std::uint32_t v = value ^ seed;
  v ^= v >> 16U;
  v *= 0x7feb352dU;
  v ^= v >> 15U;
  v *= 0x846ca68bU;
  v ^= v >> 16U;
  return v;
}

[[nodiscard]] constexpr auto
combat_combine_hash(std::uint32_t seed, std::uint32_t value) noexcept -> std::uint32_t {
  return combat_mix_hash(seed ^ (value + 0x9e3779b9U + (seed << 6U) + (seed >> 2U)),
                         0x85ebca6bU);
}

[[nodiscard]] constexpr auto
lane_uses_attack_pose(SoldierCombatLane lane) noexcept -> bool {
  return lane != SoldierCombatLane::ShieldBrace &&
         lane != SoldierCombatLane::IdleReady && lane != SoldierCombatLane::StepOut &&
         lane != SoldierCombatLane::RangedReload && lane != SoldierCombatLane::None;
}

[[nodiscard]] constexpr auto
lane_blocks_attack_request(SoldierCombatLane lane) noexcept -> bool {
  return lane == SoldierCombatLane::ShieldBrace ||
         lane == SoldierCombatLane::IdleReady || lane == SoldierCombatLane::StepOut ||
         lane == SoldierCombatLane::RangedReload;
}

[[nodiscard]] constexpr auto default_attack_lane_for_request(
    SoldierCombatLane requested_lane,
    bool is_mounted,
    CombatAttackFamily family) noexcept -> SoldierCombatLane {
  if (lane_uses_attack_pose(requested_lane) ||
      requested_lane == SoldierCombatLane::None) {
    return requested_lane;
  }

  if (is_mounted) {
    return SoldierCombatLane::StepIn;
  }

  if (family == CombatAttackFamily::Bow) {
    return SoldierCombatLane::SupportStrike;
  }

  switch (requested_lane) {
  case SoldierCombatLane::StepOut:
    return SoldierCombatLane::StepIn;
  case SoldierCombatLane::ShieldBrace:
  case SoldierCombatLane::IdleReady:
  case SoldierCombatLane::RangedReload:
    return SoldierCombatLane::SupportStrike;
  case SoldierCombatLane::LeadStrike:
  case SoldierCombatLane::SupportStrike:
  case SoldierCombatLane::StepIn:
  case SoldierCombatLane::None:
    break;
  }
  return requested_lane;
}

[[nodiscard]] constexpr auto
lane_profile_for_lane(SoldierCombatLane lane) noexcept -> CombatLaneProfile {
  CombatLaneProfile profile{};
  profile.lane = lane;
  switch (lane) {
  case SoldierCombatLane::LeadStrike:
    profile.phase_bias = 0.075F;
    profile.recover_scale = 0.92F;
    profile.emphasis_scale = 1.10F;
    break;
  case SoldierCombatLane::SupportStrike:
    profile.phase_bias = 0.030F;
    profile.recover_scale = 0.98F;
    profile.emphasis_scale = 1.04F;
    break;
  case SoldierCombatLane::ShieldBrace:
    profile.phase_bias = -0.060F;
    profile.recover_scale = 1.08F;
    profile.emphasis_scale = 0.94F;
    break;
  case SoldierCombatLane::StepIn:
    profile.phase_bias = -0.015F;
    profile.recover_scale = 1.00F;
    profile.emphasis_scale = 1.00F;
    break;
  case SoldierCombatLane::StepOut:
    profile.phase_bias = -0.105F;
    profile.recover_scale = 1.08F;
    profile.emphasis_scale = 0.92F;
    break;
  case SoldierCombatLane::IdleReady:
    profile.phase_bias = -0.145F;
    profile.recover_scale = 1.15F;
    profile.emphasis_scale = 0.88F;
    break;
  case SoldierCombatLane::RangedReload:
    profile.phase_bias = -0.180F;
    profile.recover_scale = 1.18F;
    profile.emphasis_scale = 0.86F;
    break;
  case SoldierCombatLane::None:
    break;
  }
  return profile;
}

[[nodiscard]] constexpr auto lane_profile_for_state(
    const SoldierCombatLaneState& state) noexcept -> CombatLaneProfile {
  CombatLaneProfile profile = lane_profile_for_lane(state.lane);
  profile.local_enemy_pressure = static_cast<float>(state.pressure_bucket) / 2.0F;
  profile.variant_bias = state.variant_bias;
  profile.lane_seed = state.lane_seed;
  return profile;
}

[[nodiscard]] constexpr auto
profile_with_lane(const CombatLaneProfile& base,
                  SoldierCombatLane lane) noexcept -> CombatLaneProfile {
  CombatLaneProfile profile = base;
  CombatLaneProfile const lane_profile = lane_profile_for_lane(lane);
  profile.lane = lane;
  profile.phase_bias = lane_profile.phase_bias;
  profile.recover_scale = lane_profile.recover_scale;
  profile.emphasis_scale = lane_profile.emphasis_scale;
  return profile;
}

[[nodiscard]] constexpr auto
wounded_bucket(float health_ratio) noexcept -> std::uint32_t {
  return (health_ratio <= 0.35F) ? 1U : 0U;
}

[[nodiscard]] constexpr auto
rank_band_for_inputs(const CombatLaneInputs& inputs) noexcept -> int {
  if (inputs.force_single_soldier || inputs.rows <= 1) {
    return 0;
  }
  if (inputs.row <= 0) {
    return 0;
  }
  if (inputs.row + 1 >= inputs.rows) {
    return 2;
  }
  return 1;
}

[[nodiscard]] constexpr auto
choose_lane(const CombatLaneInputs& inputs,
            std::uint32_t selection_seed) noexcept -> SoldierCombatLane {
  if (inputs.force_single_soldier || (inputs.rows * inputs.cols) <= 1) {
    return inputs.is_melee ? SoldierCombatLane::LeadStrike : SoldierCombatLane::StepOut;
  }

  bool const wounded = wounded_bucket(inputs.health_ratio) != 0U;
  bool const ranged =
      !inputs.is_melee || inputs.attack_family == CombatAttackFamily::Bow;
  bool const high_pressure = combat_pressure_bucket(inputs.local_enemy_pressure) >= 2U;
  bool const medium_pressure =
      combat_pressure_bucket(inputs.local_enemy_pressure) >= 1U;
  int const rank_band = rank_band_for_inputs(inputs);
  std::uint32_t const choice = selection_seed % 3U;

  if (ranged) {
    if (rank_band == 0) {
      return (high_pressure || choice == 0U) ? SoldierCombatLane::StepOut
                                             : SoldierCombatLane::LeadStrike;
    }
    if (rank_band == 1) {
      return (medium_pressure && choice == 0U) ? SoldierCombatLane::SupportStrike
                                               : SoldierCombatLane::RangedReload;
    }
    return wounded ? SoldierCombatLane::IdleReady : SoldierCombatLane::RangedReload;
  }

  if (inputs.is_mounted) {
    switch (choice) {
    case 0U:
      return SoldierCombatLane::LeadStrike;
    case 1U:
      return SoldierCombatLane::StepIn;
    default:
      return SoldierCombatLane::StepOut;
    }
  }

  if (wounded) {
    return (rank_band == 0) ? SoldierCombatLane::ShieldBrace
                            : SoldierCombatLane::IdleReady;
  }

  if (rank_band == 0) {
    if (high_pressure) {
      return (choice == 0U) ? SoldierCombatLane::ShieldBrace
                            : SoldierCombatLane::LeadStrike;
    }
    switch (choice) {
    case 0U:
      return SoldierCombatLane::LeadStrike;
    case 1U:
      return SoldierCombatLane::ShieldBrace;
    default:
      return SoldierCombatLane::StepIn;
    }
  }
  if (rank_band == 1) {
    if (!medium_pressure && choice == 2U) {
      return SoldierCombatLane::StepOut;
    }
    switch (choice) {
    case 0U:
      return SoldierCombatLane::SupportStrike;
    case 1U:
      return SoldierCombatLane::StepIn;
    default:
      return SoldierCombatLane::ShieldBrace;
    }
  }
  return (choice == 0U) ? SoldierCombatLane::StepOut : SoldierCombatLane::IdleReady;
}

[[nodiscard]] constexpr auto resolve_soldier_combat_lane(
    const SoldierCombatLaneState& previous,
    const CombatLaneInputs& inputs) noexcept -> SoldierCombatLaneResolution {
  constexpr std::uint32_t k_combat_lane_schema_version = 2U;
  SoldierCombatLaneResolution resolution{};

  std::uint32_t signature = k_combat_lane_schema_version;
  signature = combat_combine_hash(signature, inputs.unit_seed);
  signature = combat_combine_hash(signature, inputs.soldier_seed);
  signature = combat_combine_hash(signature, static_cast<std::uint32_t>(inputs.row));
  signature = combat_combine_hash(signature, static_cast<std::uint32_t>(inputs.col));
  signature = combat_combine_hash(signature, static_cast<std::uint32_t>(inputs.rows));
  signature = combat_combine_hash(signature, static_cast<std::uint32_t>(inputs.cols));
  signature = combat_combine_hash(signature, wounded_bucket(inputs.health_ratio));
  signature = combat_combine_hash(signature,
                                  combat_pressure_bucket(inputs.local_enemy_pressure));
  signature =
      combat_combine_hash(signature, static_cast<std::uint32_t>(inputs.is_melee));
  signature =
      combat_combine_hash(signature, static_cast<std::uint32_t>(inputs.is_mounted));
  signature =
      combat_combine_hash(signature, static_cast<std::uint32_t>(inputs.attack_family));

  if (previous.initialized && previous.signature == signature) {
    resolution.state = previous;
    resolution.profile = lane_profile_for_state(previous);
    return resolution;
  }

  resolution.state.initialized = true;
  resolution.state.signature = signature;
  resolution.state.pressure_bucket =
      combat_pressure_bucket(inputs.local_enemy_pressure);
  resolution.state.lane_seed =
      combat_combine_hash(signature, inputs.soldier_seed ^ 0x6b43a9b5U);
  resolution.state.variant_bias = static_cast<std::uint8_t>(
      combat_combine_hash(resolution.state.lane_seed, 0x51ed2705U) % 3U);
  resolution.state.lane = choose_lane(inputs, resolution.state.lane_seed);
  resolution.profile = lane_profile_for_state(resolution.state);
  return resolution;
}

[[nodiscard]] constexpr auto
next_attack_variant_for_lane(std::uint8_t base_variant,
                             CombatAttackFamily family,
                             SoldierCombatLane lane,
                             float local_enemy_pressure,
                             std::uint8_t variant_bias,
                             std::uint32_t lane_seed,
                             std::uint32_t transaction_id) noexcept -> std::uint8_t {
  std::uint32_t const variant_seed = combat_combine_hash(
      lane_seed, transaction_id ^ static_cast<std::uint32_t>(base_variant));
  std::uint32_t family_bias = 0U;
  switch (family) {
  case CombatAttackFamily::Bow:
    family_bias = 1U;
    break;
  case CombatAttackFamily::Spear:
    family_bias = 2U;
    break;
  case CombatAttackFamily::Sword:
  case CombatAttackFamily::None:
    break;
  }
  std::uint32_t lane_bias = 0U;
  switch (lane) {
  case SoldierCombatLane::LeadStrike:
    lane_bias = 0U;
    break;
  case SoldierCombatLane::SupportStrike:
    lane_bias = 1U;
    break;
  case SoldierCombatLane::ShieldBrace:
    lane_bias = 2U;
    break;
  case SoldierCombatLane::StepIn:
    lane_bias = 1U;
    break;
  case SoldierCombatLane::StepOut:
    lane_bias = 2U;
    break;
  case SoldierCombatLane::IdleReady:
    lane_bias = 0U;
    break;
  case SoldierCombatLane::RangedReload:
    lane_bias = 1U;
    break;
  case SoldierCombatLane::None:
    break;
  }
  auto const pressure_bias = combat_pressure_bucket(local_enemy_pressure);
  auto const jitter = static_cast<std::uint8_t>(variant_seed % 3U);
  return static_cast<std::uint8_t>(
      (base_variant + variant_bias + jitter + family_bias + lane_bias + pressure_bias) %
      k_attack_variant_seed_slots);
}

[[nodiscard]] auto default_combat_timing_config() noexcept -> const CombatTimingConfig&;

[[nodiscard]] auto
attack_phase_window(CombatPhase phase,
                    bool amplified_attack,
                    bool finisher_attack) noexcept -> CombatPhaseWindow;

[[nodiscard]] auto scrubbed_combat_phase_from_attack_phase(
    float attack_phase,
    bool amplified_attack,
    bool finisher_attack) noexcept -> ScrubbedCombatPhase;

[[nodiscard]] auto resolve_combat_transaction_state(
    const CombatPersistentState& previous,
    const CombatRawInputs& raw,
    const CombatLaneProfile& lane,
    const CombatTimingConfig& config = default_combat_timing_config()) noexcept
    -> CombatStateResolution;

} // namespace Animation
