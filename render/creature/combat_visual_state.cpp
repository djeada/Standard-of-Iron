#include "combat_visual_state.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "../profiling/combat_animation_diagnostics.h"

namespace Render::Creature {

namespace {

constexpr std::uint32_t k_combat_lane_schema_version = 2U;

auto pressure_bucket(float local_enemy_pressure) noexcept -> std::uint8_t {
  return static_cast<std::uint8_t>(std::clamp(local_enemy_pressure, 0.0F, 1.0F) *
                                   2.99F);
}

auto mix_hash(std::uint32_t value, std::uint32_t seed) noexcept -> std::uint32_t {
  std::uint32_t v = value ^ seed;
  v ^= v >> 16U;
  v *= 0x7feb352dU;
  v ^= v >> 15U;
  v *= 0x846ca68bU;
  v ^= v >> 16U;
  return v;
}

auto combine_hash(std::uint32_t seed, std::uint32_t value) noexcept -> std::uint32_t {
  return mix_hash(seed ^ (value + 0x9e3779b9U + (seed << 6U) + (seed >> 2U)),
                  0x85ebca6bU);
}

auto wrap_phase(float phase) noexcept -> float {
  float wrapped = std::fmod(phase, 1.0F);
  if (wrapped < 0.0F) {
    wrapped += 1.0F;
  }
  return wrapped;
}

auto lane_profile_for_state(const SoldierCombatLaneState& state) noexcept
    -> CombatLaneProfile {
  CombatLaneProfile profile{};
  profile.lane = state.lane;
  profile.local_enemy_pressure = static_cast<float>(state.pressure_bucket) / 2.0F;
  profile.variant_bias = state.variant_bias;
  profile.lane_seed = state.lane_seed;
  switch (state.lane) {
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

auto wounded_bucket(float health_ratio) noexcept -> std::uint32_t {
  return (health_ratio <= 0.35F) ? 1U : 0U;
}

auto rank_band_for_inputs(const CombatLaneInputs& inputs) noexcept -> int {
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

auto choose_lane(const CombatLaneInputs& inputs,
                 std::uint32_t selection_seed) noexcept -> SoldierCombatLane {
  if (inputs.force_single_soldier || (inputs.rows * inputs.cols) <= 1) {
    return inputs.is_melee ? SoldierCombatLane::LeadStrike : SoldierCombatLane::StepOut;
  }

  bool const wounded = wounded_bucket(inputs.health_ratio) != 0U;
  bool const ranged =
      !inputs.is_melee || inputs.attack_family == Engine::Core::CombatAttackFamily::Bow;
  bool const high_pressure = pressure_bucket(inputs.local_enemy_pressure) >= 2U;
  bool const medium_pressure = pressure_bucket(inputs.local_enemy_pressure) >= 1U;
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

auto phase_from_attack_phase(float attack_phase) noexcept
    -> CombatVisualTransactionPhase {
  if (attack_phase <= 0.14F) {
    return CombatVisualTransactionPhase::Enter;
  }
  if (attack_phase <= 0.34F) {
    return CombatVisualTransactionPhase::Anticipation;
  }
  if (attack_phase <= 0.52F) {
    return CombatVisualTransactionPhase::Strike;
  }
  if (attack_phase <= 0.74F) {
    return CombatVisualTransactionPhase::FollowThrough;
  }
  return CombatVisualTransactionPhase::Recover;
}

auto phase_progress_from_attack_phase(
    float attack_phase, CombatVisualTransactionPhase phase) noexcept -> float {
  switch (phase) {
  case CombatVisualTransactionPhase::Enter:
    return std::clamp(attack_phase / 0.14F, 0.0F, 1.0F);
  case CombatVisualTransactionPhase::Anticipation:
    return std::clamp((attack_phase - 0.14F) / 0.20F, 0.0F, 1.0F);
  case CombatVisualTransactionPhase::Strike:
    return std::clamp((attack_phase - 0.34F) / 0.18F, 0.0F, 1.0F);
  case CombatVisualTransactionPhase::FollowThrough:
    return std::clamp((attack_phase - 0.52F) / 0.22F, 0.0F, 1.0F);
  case CombatVisualTransactionPhase::Recover:
    return std::clamp((attack_phase - 0.74F) / 0.255F, 0.0F, 1.0F);
  case CombatVisualTransactionPhase::ExitBlend:
  case CombatVisualTransactionPhase::None:
    break;
  }
  return 0.0F;
}

auto raw_attack_phase_hint(const CombatVisualRawInputs& raw,
                           const CombatLaneProfile& lane) noexcept -> float {
  if (!raw.attack_requested) {
    return 0.0F;
  }

  if (raw.combat_phase != Engine::Core::CombatAnimationState::Idle) {
    auto const window = Render::Profiling::attack_phase_window(
        raw.combat_phase, raw.amplified_attack, raw.finisher_attack);
    float const progress = std::clamp(
        raw.combat_phase_progress + lane.phase_bias * window.offset_weight, 0.0F, 1.0F);
    return window.start + (window.end - window.start) * progress;
  }

  float const attack_offset = raw.has_attack_offset ? raw.attack_offset : 0.0F;
  return wrap_phase(raw.sample_time + attack_offset + lane.phase_bias);
}

auto interrupt_reason_for_exit_policy(CombatVisualExitPolicy policy) noexcept
    -> CombatVisualInterruptReason {
  switch (policy) {
  case CombatVisualExitPolicy::NormalComplete:
    return CombatVisualInterruptReason::NormalComplete;
  case CombatVisualExitPolicy::TargetDied:
    return CombatVisualInterruptReason::TargetDied;
  case CombatVisualExitPolicy::ChaseResumes:
    return CombatVisualInterruptReason::ChaseResumes;
  case CombatVisualExitPolicy::Retarget:
    return CombatVisualInterruptReason::Retarget;
  case CombatVisualExitPolicy::None:
    break;
  }
  return CombatVisualInterruptReason::None;
}

void begin_transaction(CombatVisualPersistentState& state,
                       const CombatVisualRawInputs& raw,
                       const CombatLaneProfile& lane,
                       const CombatVisualTimingConfig& config,
                       std::uint32_t transaction_id,
                       float raw_phase_hint) noexcept {
  state.active = true;
  state.is_melee = raw.is_melee;
  state.is_mounted = raw.is_mounted;
  state.is_casting = raw.is_casting;
  state.finisher_attack = raw.finisher_attack;
  state.amplified_attack = raw.amplified_attack;
  state.phase = phase_from_attack_phase(raw_phase_hint);
  state.exit_policy = CombatVisualExitPolicy::None;
  state.interruption_reason = CombatVisualInterruptReason::None;
  state.attack_phase = std::clamp(raw_phase_hint, 0.0F, 0.995F);
  state.phase_progress =
      phase_progress_from_attack_phase(state.attack_phase, state.phase);
  state.last_sample_time = raw.sample_time;
  state.phase_start_time = raw.sample_time;
  state.transaction_start_time = raw.sample_time;
  state.minimum_hold_until = raw.sample_time + config.minimum_hold_duration;
  state.exit_blend_progress = 0.0F;
  state.attack_emphasis =
      lane.emphasis_scale *
      (raw.finisher_attack ? 1.30F : (raw.amplified_attack ? 1.12F : 1.0F));
  state.locked_variant = next_attack_variant_for_lane(raw.attack_variant,
                                                      raw.attack_family,
                                                      lane.lane,
                                                      lane.local_enemy_pressure,
                                                      lane.variant_bias,
                                                      lane.lane_seed,
                                                      transaction_id);
  state.transaction_id = transaction_id;
  state.source_target_id = raw.attack_target_id;
  state.locked_family = raw.attack_family;
}

auto build_resolved_state(const CombatVisualPersistentState& persistent,
                          const CombatVisualRawInputs& raw,
                          const CombatLaneProfile& lane) noexcept
    -> CombatVisualResolvedState {
  CombatVisualResolvedState resolved{};
  bool const lane_uses_attack_pose = lane.lane != SoldierCombatLane::ShieldBrace &&
                                     lane.lane != SoldierCombatLane::IdleReady &&
                                     lane.lane != SoldierCombatLane::StepOut &&
                                     lane.lane != SoldierCombatLane::RangedReload;
  resolved.authoritative = persistent.active || raw.attack_requested ||
                           raw.is_hit_reacting || raw.is_dying || raw.is_dead;
  resolved.active = persistent.active && lane_uses_attack_pose;
  resolved.prioritize_action_over_locomotion =
      resolved.active && persistent.phase != CombatVisualTransactionPhase::ExitBlend;
  resolved.is_melee = persistent.active ? persistent.is_melee : raw.is_melee;
  resolved.is_mounted = persistent.active ? persistent.is_mounted : raw.is_mounted;
  resolved.is_casting = persistent.active ? persistent.is_casting : raw.is_casting;
  resolved.finisher_attack =
      persistent.active ? persistent.finisher_attack : raw.finisher_attack;
  resolved.amplified_attack =
      persistent.active ? persistent.amplified_attack : raw.amplified_attack;
  resolved.phase =
      persistent.active ? persistent.phase : CombatVisualTransactionPhase::None;
  resolved.exit_policy =
      persistent.active ? persistent.exit_policy : CombatVisualExitPolicy::None;
  resolved.interruption_reason = persistent.interruption_reason;
  resolved.lane = lane.lane;
  resolved.attack_phase = persistent.active ? persistent.attack_phase : 0.0F;
  resolved.phase_progress = persistent.active ? persistent.phase_progress : 0.0F;
  resolved.exit_blend_progress =
      persistent.active ? persistent.exit_blend_progress : 0.0F;
  resolved.attack_emphasis =
      persistent.active ? persistent.attack_emphasis : lane.emphasis_scale;
  resolved.attack_variant =
      persistent.active ? persistent.locked_variant : raw.attack_variant;
  resolved.transaction_id = persistent.transaction_id;
  resolved.source_target_id =
      persistent.active ? persistent.source_target_id : raw.attack_target_id;
  resolved.attack_family =
      persistent.active ? persistent.locked_family : raw.attack_family;
  return resolved;
}

} // namespace

auto default_combat_visual_timing_config() noexcept -> const CombatVisualTimingConfig& {
  static const CombatVisualTimingConfig config{};
  return config;
}

auto resolve_soldier_combat_lane(const SoldierCombatLaneState& previous,
                                 const CombatLaneInputs& inputs) noexcept
    -> SoldierCombatLaneResolution {
  SoldierCombatLaneResolution resolution{};

  std::uint32_t signature = k_combat_lane_schema_version;
  signature = combine_hash(signature, inputs.unit_seed);
  signature = combine_hash(signature, inputs.soldier_seed);
  signature = combine_hash(signature, static_cast<std::uint32_t>(inputs.row));
  signature = combine_hash(signature, static_cast<std::uint32_t>(inputs.col));
  signature = combine_hash(signature, static_cast<std::uint32_t>(inputs.rows));
  signature = combine_hash(signature, static_cast<std::uint32_t>(inputs.cols));
  signature = combine_hash(signature, wounded_bucket(inputs.health_ratio));
  signature = combine_hash(signature, pressure_bucket(inputs.local_enemy_pressure));
  signature = combine_hash(signature, static_cast<std::uint32_t>(inputs.is_melee));
  signature = combine_hash(signature, static_cast<std::uint32_t>(inputs.is_mounted));
  signature = combine_hash(signature, static_cast<std::uint32_t>(inputs.attack_family));

  if (previous.initialized && previous.signature == signature) {
    resolution.state = previous;
    resolution.profile = lane_profile_for_state(previous);
    return resolution;
  }

  resolution.state.initialized = true;
  resolution.state.signature = signature;
  resolution.state.pressure_bucket = pressure_bucket(inputs.local_enemy_pressure);
  resolution.state.lane_seed =
      combine_hash(signature, inputs.soldier_seed ^ 0x6b43a9b5U);
  resolution.state.variant_bias = static_cast<std::uint8_t>(
      combine_hash(resolution.state.lane_seed, 0x51ed2705U) % 3U);
  resolution.state.lane = choose_lane(inputs, resolution.state.lane_seed);
  resolution.profile = lane_profile_for_state(resolution.state);
  return resolution;
}

auto next_attack_variant_for_lane(std::uint8_t base_variant,
                                  Engine::Core::CombatAttackFamily family,
                                  SoldierCombatLane lane,
                                  float local_enemy_pressure,
                                  std::uint8_t variant_bias,
                                  std::uint32_t lane_seed,
                                  std::uint32_t transaction_id) noexcept
    -> std::uint8_t {
  constexpr std::uint8_t k_variant_slots =
      Engine::Core::CombatStateComponent::k_attack_variant_seed_slots;
  std::uint32_t const variant_seed = combine_hash(
      lane_seed, transaction_id ^ static_cast<std::uint32_t>(base_variant));
  std::uint32_t family_bias = 0U;
  switch (family) {
  case Engine::Core::CombatAttackFamily::Bow:
    family_bias = 1U;
    break;
  case Engine::Core::CombatAttackFamily::Spear:
    family_bias = 2U;
    break;
  case Engine::Core::CombatAttackFamily::Sword:
  case Engine::Core::CombatAttackFamily::None:
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
  auto const pressure_bias = pressure_bucket(local_enemy_pressure);
  auto const jitter = static_cast<std::uint8_t>(variant_seed % 3U);
  return static_cast<std::uint8_t>(
      (base_variant + variant_bias + jitter + family_bias + lane_bias + pressure_bias) %
      k_variant_slots);
}

auto resolve_combat_visual_state(const CombatVisualPersistentState& previous,
                                 const CombatVisualRawInputs& raw,
                                 const CombatLaneProfile& lane,
                                 const CombatVisualTimingConfig& config) noexcept
    -> CombatVisualStateResolution {
  CombatVisualStateResolution resolution{};
  resolution.persistent = previous;
  std::optional<CombatVisualPersistentState> resolved_override;

  if (raw.sample_time < previous.last_sample_time) {
    resolution.persistent = {};
    resolution.persistent.interruption_reason =
        CombatVisualInterruptReason::Invalidated;
  }

  CombatVisualPersistentState& next = resolution.persistent;

  if (raw.is_dying || raw.is_dead) {
    next = {};
    next.interruption_reason = CombatVisualInterruptReason::Death;
    resolution.resolved = build_resolved_state(next, raw, lane);
    resolution.resolved.authoritative = true;
    return resolution;
  }
  if (raw.is_hit_reacting) {
    next = {};
    next.interruption_reason = CombatVisualInterruptReason::HitReaction;
    resolution.resolved = build_resolved_state(next, raw, lane);
    resolution.resolved.authoritative = true;
    return resolution;
  }

  bool const request_attack =
      raw.attack_requested && !raw.is_healing && !raw.is_constructing;
  float const raw_phase_hint = raw_attack_phase_hint(raw, lane);

  if (!next.active) {
    if (request_attack) {
      begin_transaction(
          next, raw, lane, config, previous.transaction_id + 1U, raw_phase_hint);
    }
    resolution.resolved = build_resolved_state(next, raw, lane);
    return resolution;
  }

  float const delta_time = std::max(0.0F, raw.sample_time - next.last_sample_time);
  bool const same_target = next.source_target_id == 0U || raw.attack_target_id == 0U ||
                           next.source_target_id == raw.attack_target_id;
  bool const target_changed = next.source_target_id > 0U && raw.attack_target_id > 0U &&
                              next.source_target_id != raw.attack_target_id;
  bool const target_died =
      next.source_target_id > 0U && !raw.attack_target_alive &&
      (raw.attack_target_id == 0U || raw.attack_target_id == next.source_target_id);
  bool const family_changed =
      next.locked_family != Engine::Core::CombatAttackFamily::None &&
      raw.attack_family != Engine::Core::CombatAttackFamily::None &&
      next.locked_family != raw.attack_family;
  bool const cycle_restart =
      request_attack && next.attack_phase >= 0.92F &&
      raw_phase_hint + config.new_transaction_reset_threshold < next.attack_phase &&
      same_target && !family_changed;

  if (cycle_restart) {
    begin_transaction(
        next, raw, lane, config, previous.transaction_id + 1U, raw_phase_hint);
    resolution.resolved = build_resolved_state(next, raw, lane);
    return resolution;
  }

  next.last_sample_time = raw.sample_time;

  bool const hold_attack_lifetime = raw.sample_time < next.minimum_hold_until;
  if (request_attack && same_target && !family_changed &&
      next.exit_policy == CombatVisualExitPolicy::None) {
    next.attack_phase = std::clamp(
        std::max(next.attack_phase, raw_phase_hint), next.attack_phase, 0.995F);
    next.phase = phase_from_attack_phase(next.attack_phase);
    next.phase_progress =
        phase_progress_from_attack_phase(next.attack_phase, next.phase);
    next.exit_blend_progress = 0.0F;
  } else {
    if (next.exit_policy == CombatVisualExitPolicy::None && !hold_attack_lifetime) {
      if (target_changed) {
        next.exit_policy = CombatVisualExitPolicy::Retarget;
      } else if (target_died) {
        next.exit_policy = CombatVisualExitPolicy::TargetDied;
      } else if (!request_attack &&
                 raw.locomotion != Render::Creature::MovementAnimationState::Idle) {
        next.exit_policy = CombatVisualExitPolicy::ChaseResumes;
      } else {
        next.exit_policy = CombatVisualExitPolicy::NormalComplete;
      }
    }

    if (next.attack_phase < 0.995F) {
      float const recover_duration =
          std::max(0.05F, config.recover_duration * lane.recover_scale);
      float const phase_step = (delta_time / recover_duration) * 0.40F;
      next.attack_phase = std::clamp(
          std::max(next.attack_phase, 0.60F) + phase_step, next.attack_phase, 0.995F);
      next.phase = CombatVisualTransactionPhase::Recover;
      next.phase_progress =
          std::clamp((next.attack_phase - 0.60F) / 0.395F, 0.0F, 1.0F);
      if (next.attack_phase >= 0.995F) {
        next.phase = CombatVisualTransactionPhase::ExitBlend;
        next.phase_progress = 0.0F;
        next.exit_blend_progress = 0.0F;
      }
    } else {
      next.phase = CombatVisualTransactionPhase::ExitBlend;
      next.phase_progress = next.exit_blend_progress;
      next.exit_blend_progress =
          std::clamp(next.exit_blend_progress +
                         (delta_time / std::max(0.05F, config.exit_blend_duration)),
                     0.0F,
                     1.0F);
      next.phase_progress = next.exit_blend_progress;
      if (next.exit_blend_progress >= 0.999F) {
        resolved_override = next;
        CombatVisualInterruptReason const reason =
            interrupt_reason_for_exit_policy(next.exit_policy);
        std::uint32_t const transaction_id = next.transaction_id;
        next = {};
        next.transaction_id = transaction_id;
        next.interruption_reason = reason;
      }
    }
  }

  resolution.resolved = build_resolved_state(
      resolved_override.has_value() ? *resolved_override : next, raw, lane);
  return resolution;
}

} // namespace Render::Creature
