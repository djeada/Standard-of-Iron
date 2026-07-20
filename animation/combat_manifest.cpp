#include "combat_manifest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace Animation {

namespace {

auto wrap_phase(float phase) noexcept -> float {
  float wrapped = std::fmod(phase, 1.0F);
  if (wrapped < 0.0F) {
    wrapped += 1.0F;
  }
  return wrapped;
}

auto smooth01(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

auto all_attack_phase_windows(bool amplified_attack, bool finisher_attack)
    -> std::array<std::pair<CombatPhase, CombatPhaseWindow>, 6> {
  return {{
      {CombatPhase::Advance,
       attack_phase_window(CombatPhase::Advance, amplified_attack, finisher_attack)},
      {CombatPhase::WindUp,
       attack_phase_window(CombatPhase::WindUp, amplified_attack, finisher_attack)},
      {CombatPhase::Strike,
       attack_phase_window(CombatPhase::Strike, amplified_attack, finisher_attack)},
      {CombatPhase::Impact,
       attack_phase_window(CombatPhase::Impact, amplified_attack, finisher_attack)},
      {CombatPhase::Recover,
       attack_phase_window(CombatPhase::Recover, amplified_attack, finisher_attack)},
      {CombatPhase::Reposition,
       attack_phase_window(CombatPhase::Reposition, amplified_attack, finisher_attack)},
  }};
}

auto eased_combat_phase_progress(CombatPhase phase, float progress) noexcept -> float {
  float const t = std::clamp(progress, 0.0F, 1.0F);
  switch (phase) {
  case CombatPhase::Advance:
    return smooth01(t);
  case CombatPhase::WindUp:
    return t * t * (2.4F - 1.4F * t);
  case CombatPhase::Strike:
    return 1.0F - ((1.0F - t) * (1.0F - t));
  case CombatPhase::Impact:
    return smooth01(t);
  case CombatPhase::Recover:
  case CombatPhase::Reposition:
    return t * t * (3.0F - 2.0F * t);
  case CombatPhase::Idle:
    break;
  }
  return t;
}

auto attack_lane_profile_for_request(const CombatLaneProfile& requested_lane,
                                     SoldierCombatLane fallback_lane,
                                     const CombatRawInputs& raw) noexcept
    -> std::optional<CombatLaneProfile> {
  if (!lane_blocks_attack_request(requested_lane.lane)) {
    return requested_lane;
  }
  if (lane_uses_attack_pose(fallback_lane)) {
    return profile_with_lane(requested_lane, fallback_lane);
  }
  return profile_with_lane(requested_lane,
                           default_attack_lane_for_request(
                               requested_lane.lane, raw.is_mounted, raw.attack_family));
}

auto transaction_phase_from_attack_phase(float attack_phase) noexcept
    -> CombatTransactionPhase {
  if (attack_phase <= 0.14F) {
    return CombatTransactionPhase::Enter;
  }
  if (attack_phase <= 0.34F) {
    return CombatTransactionPhase::Anticipation;
  }
  if (attack_phase <= 0.52F) {
    return CombatTransactionPhase::Strike;
  }
  if (attack_phase <= 0.74F) {
    return CombatTransactionPhase::FollowThrough;
  }
  return CombatTransactionPhase::Recover;
}

auto transaction_phase_progress_from_attack_phase(
    float attack_phase, CombatTransactionPhase phase) noexcept -> float {
  switch (phase) {
  case CombatTransactionPhase::Enter:
    return std::clamp(attack_phase / 0.14F, 0.0F, 1.0F);
  case CombatTransactionPhase::Anticipation:
    return std::clamp((attack_phase - 0.14F) / 0.20F, 0.0F, 1.0F);
  case CombatTransactionPhase::Strike:
    return std::clamp((attack_phase - 0.34F) / 0.18F, 0.0F, 1.0F);
  case CombatTransactionPhase::FollowThrough:
    return std::clamp((attack_phase - 0.52F) / 0.22F, 0.0F, 1.0F);
  case CombatTransactionPhase::Recover:
    return std::clamp((attack_phase - 0.74F) / 0.255F, 0.0F, 1.0F);
  case CombatTransactionPhase::ExitBlend:
  case CombatTransactionPhase::None:
    break;
  }
  return 0.0F;
}

auto raw_attack_phase_hint(const CombatRawInputs& raw,
                           const CombatLaneProfile& lane) noexcept -> float {
  if (!raw.attack_requested) {
    return 0.0F;
  }

  if (raw.combat_phase != CombatPhase::Idle) {
    auto const window = attack_phase_window(
        raw.combat_phase, raw.amplified_attack, raw.finisher_attack);
    float const progress =
        eased_combat_phase_progress(raw.combat_phase, raw.combat_phase_progress);
    float const authored_phase = window.start + (window.end - window.start) * progress;
    // The gameplay phase owns the broad action window, but it must not force
    // every rendered body onto effectively the same frame. Apply cadence in
    // normalized clip time so its visual separation survives narrow Strike
    // and Impact windows; applying it to window progress reduced a meaningful
    // 0.24 soldier offset to less than one rendered frame.
    constexpr float k_visual_cadence_scale = 0.65F;
    return std::clamp(
        authored_phase + lane.phase_bias * k_visual_cadence_scale, 0.0F, 0.995F);
  }

  float const attack_offset = raw.has_attack_offset ? raw.attack_offset : 0.0F;
  return wrap_phase(raw.sample_time + attack_offset + lane.phase_bias);
}

auto attack_cycle_entry_phase(const CombatLaneProfile& lane) noexcept -> float {
  // Follow-up presentation transactions always re-enter through anticipation,
  // even when the gameplay attack state remains in Recover during cooldown.
  // Keep a small amount of the personal cadence so the restart itself does
  // not synchronize the formation.
  return std::clamp(0.045F + lane.phase_bias * 0.30F, 0.0F, 0.13F);
}

auto interrupt_reason_for_exit_policy(CombatExitPolicy policy) noexcept
    -> CombatInterruptReason {
  switch (policy) {
  case CombatExitPolicy::NormalComplete:
    return CombatInterruptReason::NormalComplete;
  case CombatExitPolicy::TargetDied:
    return CombatInterruptReason::TargetDied;
  case CombatExitPolicy::ChaseResumes:
    return CombatInterruptReason::ChaseResumes;
  case CombatExitPolicy::Retarget:
    return CombatInterruptReason::Retarget;
  case CombatExitPolicy::None:
    break;
  }
  return CombatInterruptReason::None;
}

void begin_transaction(CombatPersistentState& state,
                       const CombatRawInputs& raw,
                       const CombatLaneProfile& lane,
                       const CombatTimingConfig& config,
                       std::uint32_t transaction_id,
                       float raw_phase_hint) noexcept {
  state.active = true;
  state.is_melee = raw.is_melee;
  state.is_mounted = raw.is_mounted;
  state.is_casting = raw.is_casting;
  state.finisher_attack = raw.finisher_attack;
  state.amplified_attack = raw.amplified_attack;
  state.locked_lane = lane.lane;
  state.queued_next = {};
  state.phase = transaction_phase_from_attack_phase(raw_phase_hint);
  state.exit_policy = CombatExitPolicy::None;
  state.interruption_reason = CombatInterruptReason::None;
  state.attack_phase = std::clamp(raw_phase_hint, 0.0F, 0.995F);
  state.phase_progress =
      transaction_phase_progress_from_attack_phase(state.attack_phase, state.phase);
  state.last_sample_time = raw.sample_time;
  state.phase_start_time = raw.sample_time;
  state.transaction_start_time = raw.sample_time;
  state.terminal_pose_start_time = -1.0F;
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

void queue_followup_attack(CombatPersistentState& state,
                           const CombatRawInputs& raw,
                           const CombatLaneProfile& lane) noexcept {
  std::optional<CombatLaneProfile> queued_lane;
  if (lane_uses_attack_pose(lane.lane)) {
    queued_lane = lane;
  } else if (lane_uses_attack_pose(state.locked_lane)) {
    queued_lane = profile_with_lane(lane, state.locked_lane);
  } else if (lane_blocks_attack_request(lane.lane)) {
    queued_lane = profile_with_lane(
        lane,
        default_attack_lane_for_request(lane.lane, raw.is_mounted, raw.attack_family));
  }
  if (!queued_lane.has_value()) {
    return;
  }

  state.queued_next.pending = true;
  state.queued_next.target_id = raw.attack_target_id;
  state.queued_next.family = raw.attack_family;
  state.queued_next.lane = queued_lane->lane;
}

auto queued_followup_valid(const CombatQueuedAttack& queued,
                           const CombatRawInputs& raw) noexcept -> bool {
  if (!queued.pending || !raw.attack_requested) {
    return false;
  }

  bool const family_matches = queued.family == CombatAttackFamily::None ||
                              raw.attack_family == CombatAttackFamily::None ||
                              queued.family == raw.attack_family;
  if (!family_matches) {
    return false;
  }

  if (queued.target_id == 0U) {
    return true;
  }

  return raw.attack_target_id == queued.target_id && raw.attack_target_alive;
}

auto build_resolved_state(const CombatPersistentState& persistent,
                          const CombatRawInputs& raw,
                          const CombatLaneProfile& lane) noexcept
    -> CombatResolvedState {
  CombatResolvedState resolved{};
  SoldierCombatLane const resolved_lane =
      persistent.active ? persistent.locked_lane : lane.lane;
  resolved.authoritative = persistent.active || raw.attack_requested ||
                           raw.is_hit_reacting || raw.is_dying || raw.is_dead;
  resolved.active = persistent.active && lane_uses_attack_pose(resolved_lane);
  resolved.prioritize_action_over_locomotion =
      resolved.active && persistent.phase != CombatTransactionPhase::ExitBlend;
  resolved.is_melee = persistent.active ? persistent.is_melee : raw.is_melee;
  resolved.is_mounted = persistent.active ? persistent.is_mounted : raw.is_mounted;
  resolved.is_casting = persistent.active ? persistent.is_casting : raw.is_casting;
  resolved.finisher_attack =
      persistent.active ? persistent.finisher_attack : raw.finisher_attack;
  resolved.amplified_attack =
      persistent.active ? persistent.amplified_attack : raw.amplified_attack;
  resolved.phase = persistent.active ? persistent.phase : CombatTransactionPhase::None;
  resolved.exit_policy =
      persistent.active ? persistent.exit_policy : CombatExitPolicy::None;
  resolved.interruption_reason = persistent.interruption_reason;
  resolved.lane = resolved_lane;
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

auto default_combat_timing_config() noexcept -> const CombatTimingConfig& {
  static const CombatTimingConfig config{};
  return config;
}

auto attack_phase_window(CombatPhase phase,
                         bool amplified_attack,
                         bool finisher_attack) noexcept -> CombatPhaseWindow {
  if (amplified_attack) {
    switch (phase) {
    case CombatPhase::Advance:
      return finisher_attack ? CombatPhaseWindow{0.00F, 0.14F, 0.95F}
                             : CombatPhaseWindow{0.00F, 0.10F, 0.95F};
    case CombatPhase::WindUp:
      return finisher_attack ? CombatPhaseWindow{0.14F, 0.46F, 0.82F}
                             : CombatPhaseWindow{0.10F, 0.38F, 0.80F};
    case CombatPhase::Strike:
      return finisher_attack ? CombatPhaseWindow{0.46F, 0.68F, 0.22F}
                             : CombatPhaseWindow{0.38F, 0.58F, 0.26F};
    case CombatPhase::Impact:
      return finisher_attack ? CombatPhaseWindow{0.68F, 0.78F, 0.16F}
                             : CombatPhaseWindow{0.58F, 0.68F, 0.16F};
    case CombatPhase::Recover:
      return finisher_attack ? CombatPhaseWindow{0.78F, 0.94F, 0.88F}
                             : CombatPhaseWindow{0.68F, 0.90F, 0.85F};
    case CombatPhase::Reposition:
      return finisher_attack ? CombatPhaseWindow{0.94F, 0.995F, 0.70F}
                             : CombatPhaseWindow{0.90F, 0.995F, 0.70F};
    case CombatPhase::Idle:
    default:
      break;
    }
  }

  switch (phase) {
  case CombatPhase::Advance:
    return {0.00F, 0.14F, 0.90F};
  case CombatPhase::WindUp:
    return {0.14F, 0.34F, 0.75F};
  case CombatPhase::Strike:
    return {0.34F, 0.52F, 0.25F};
  case CombatPhase::Impact:
    return {0.52F, 0.60F, 0.15F};
  case CombatPhase::Recover:
    return {0.60F, 0.86F, 0.80F};
  case CombatPhase::Reposition:
    return {0.86F, 0.995F, 0.70F};
  case CombatPhase::Idle:
  default:
    break;
  }

  return {};
}

auto scrubbed_combat_phase_from_attack_phase(float attack_phase,
                                             bool amplified_attack,
                                             bool finisher_attack) noexcept
    -> ScrubbedCombatPhase {
  float const clamped_phase = std::clamp(attack_phase, 0.0F, 0.995F);
  for (const auto& [phase, window] :
       all_attack_phase_windows(amplified_attack, finisher_attack)) {
    if (clamped_phase <= window.end || phase == CombatPhase::Reposition) {
      float const span = std::max(window.end - window.start, 1.0e-4F);
      float const progress =
          std::clamp((clamped_phase - window.start) / span, 0.0F, 1.0F);
      return {phase, progress};
    }
  }
  return {};
}

auto resolve_combat_transaction_state(const CombatPersistentState& previous,
                                      const CombatRawInputs& raw,
                                      const CombatLaneProfile& lane,
                                      const CombatTimingConfig& config) noexcept
    -> CombatStateResolution {
  CombatStateResolution resolution{};
  resolution.persistent = previous;
  std::optional<CombatPersistentState> resolved_override;

  if (raw.sample_time < previous.last_sample_time) {
    resolution.persistent = {};
    resolution.persistent.interruption_reason = CombatInterruptReason::Invalidated;
  }

  CombatPersistentState& next = resolution.persistent;

  if (raw.is_dying || raw.is_dead) {
    next = {};
    next.interruption_reason = CombatInterruptReason::Death;
    resolution.resolved = build_resolved_state(next, raw, lane);
    resolution.resolved.authoritative = true;
    return resolution;
  }
  if (raw.is_hit_reacting) {
    next = {};
    next.interruption_reason = CombatInterruptReason::HitReaction;
    resolution.resolved = build_resolved_state(next, raw, lane);
    resolution.resolved.authoritative = true;
    return resolution;
  }

  bool const request_attack =
      raw.attack_requested && !raw.is_healing && !raw.is_constructing;
  bool const request_attack_viable =
      request_attack && (raw.attack_target_id == 0U || raw.attack_target_alive);
  // Keep the soldier's stable personal cadence while the transaction locks
  // its semantic lane. Reconstructing only from the lane enum erased the
  // per-soldier timing and synchronized whole formations after the first
  // frame of combat.
  CombatLaneProfile const active_lane =
      next.active ? profile_with_lane(lane, next.locked_lane) : lane;
  auto const startup_attack_lane =
      attack_lane_profile_for_request(lane, previous.locked_lane, raw);
  float const current_lane_phase_hint = raw_attack_phase_hint(raw, lane);
  float const active_lane_phase_hint =
      next.active && next.presentation_driven_followup
          ? std::clamp(attack_cycle_entry_phase(active_lane) +
                           (raw.sample_time - next.transaction_start_time) / 0.95F,
                       0.0F,
                       0.995F)
          : (next.active ? raw_attack_phase_hint(raw, active_lane)
                         : current_lane_phase_hint);

  if (!request_attack_viable) {
    next.queued_next = {};
  }

  if (!next.active) {
    if (request_attack_viable && startup_attack_lane.has_value()) {
      begin_transaction(next,
                        raw,
                        *startup_attack_lane,
                        config,
                        previous.transaction_id + 1U,
                        raw_attack_phase_hint(raw, *startup_attack_lane));
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
  bool const family_changed = next.locked_family != CombatAttackFamily::None &&
                              raw.attack_family != CombatAttackFamily::None &&
                              next.locked_family != raw.attack_family;
  constexpr float k_max_continuous_visual_cycle_seconds = 0.95F;
  constexpr float k_max_terminal_pose_seconds = 0.10F;
  bool const terminal_pose_expired =
      next.attack_phase >= 0.92F && next.terminal_pose_start_time >= 0.0F &&
      raw.sample_time - next.terminal_pose_start_time >= k_max_terminal_pose_seconds;
  bool const visual_cycle_expired =
      next.attack_phase >= 0.92F && raw.sample_time - next.transaction_start_time >=
                                        k_max_continuous_visual_cycle_seconds;
  bool const cycle_restart =
      request_attack_viable && next.phase != CombatTransactionPhase::ExitBlend &&
      next.attack_phase >= 0.92F &&
      (active_lane_phase_hint + config.new_transaction_reset_threshold <
           next.attack_phase ||
       visual_cycle_expired || terminal_pose_expired) &&
      same_target && !family_changed;
  bool const queued_followup_requested =
      request_attack_viable && (cycle_restart || target_changed || family_changed);
  bool const continue_current_transaction = request_attack_viable && same_target &&
                                            !target_changed && !family_changed &&
                                            !cycle_restart && !next.queued_next.pending;

  if (queued_followup_requested) {
    queue_followup_attack(next, raw, lane);
  }

  next.last_sample_time = raw.sample_time;

  bool const hold_attack_lifetime = raw.sample_time < next.minimum_hold_until;
  if (continue_current_transaction && next.exit_policy == CombatExitPolicy::None) {
    next.attack_phase = std::clamp(
        std::max(next.attack_phase, active_lane_phase_hint), next.attack_phase, 0.995F);
    next.phase = transaction_phase_from_attack_phase(next.attack_phase);
    next.phase_progress =
        transaction_phase_progress_from_attack_phase(next.attack_phase, next.phase);
    if (next.attack_phase >= 0.92F && next.terminal_pose_start_time < 0.0F) {
      next.terminal_pose_start_time = raw.sample_time;
    } else if (next.attack_phase < 0.92F) {
      next.terminal_pose_start_time = -1.0F;
    }
    next.exit_blend_progress = 0.0F;
  } else {
    if (next.exit_policy == CombatExitPolicy::None && !hold_attack_lifetime) {
      if (target_changed) {
        next.exit_policy = CombatExitPolicy::Retarget;
      } else if (family_changed) {
        next.exit_policy = CombatExitPolicy::Retarget;
      } else if (target_died) {
        next.exit_policy = CombatExitPolicy::TargetDied;
      } else if (!request_attack_viable && raw.locomotion != MovementState::Idle) {
        next.exit_policy = CombatExitPolicy::ChaseResumes;
      } else {
        next.exit_policy = CombatExitPolicy::NormalComplete;
      }
    }

    if (next.attack_phase < 0.995F) {
      float const recover_duration =
          std::max(0.05F, config.recover_duration * active_lane.recover_scale);
      float const phase_step = (delta_time / recover_duration) * 0.40F;
      next.attack_phase = std::clamp(
          std::max(next.attack_phase, 0.60F) + phase_step, next.attack_phase, 0.995F);
      next.phase = CombatTransactionPhase::Recover;
      next.phase_progress =
          std::clamp((next.attack_phase - 0.60F) / 0.395F, 0.0F, 1.0F);
      if (next.attack_phase >= 0.995F) {
        next.phase = CombatTransactionPhase::ExitBlend;
        next.phase_progress = 0.0F;
        next.exit_blend_progress = 0.0F;
      }
    } else {
      next.phase = CombatTransactionPhase::ExitBlend;
      next.phase_progress = next.exit_blend_progress;
      next.exit_blend_progress =
          std::clamp(next.exit_blend_progress +
                         (delta_time / std::max(0.05F, config.exit_blend_duration)),
                     0.0F,
                     1.0F);
      next.phase_progress = next.exit_blend_progress;
      if (next.exit_blend_progress >= 0.999F) {
        resolved_override = next;
        CombatInterruptReason const reason =
            interrupt_reason_for_exit_policy(next.exit_policy);
        std::uint32_t const transaction_id = next.transaction_id;
        CombatQueuedAttack const queued_next = next.queued_next;
        SoldierCombatLane const last_attack_lane = next.locked_lane;
        next = {};
        next.transaction_id = transaction_id;
        next.interruption_reason = reason;
        next.locked_lane = last_attack_lane;
        if (queued_followup_valid(queued_next, raw)) {
          CombatLaneProfile const queued_lane =
              profile_with_lane(lane, queued_next.lane);
          begin_transaction(next,
                            raw,
                            queued_lane,
                            config,
                            transaction_id + 1U,
                            attack_cycle_entry_phase(queued_lane));
          next.presentation_driven_followup = true;
        } else if (request_attack_viable && !target_died &&
                   startup_attack_lane.has_value()) {
          begin_transaction(next,
                            raw,
                            *startup_attack_lane,
                            config,
                            transaction_id + 1U,
                            raw_attack_phase_hint(raw, *startup_attack_lane));
        }
      }
    }
  }

  resolution.resolved = build_resolved_state(
      resolved_override.has_value() ? *resolved_override : next, raw, lane);
  return resolution;
}

} // namespace Animation
