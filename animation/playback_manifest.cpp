#include "playback_manifest.h"

#include <algorithm>
#include <cmath>

namespace Animation {

namespace {

[[nodiscard]] auto terminal_non_looping_phase() noexcept -> float {
  return std::nextafter(1.0F, 0.0F);
}

[[nodiscard]] auto smoothstep01(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] auto hold_phase_inputs(const HumanoidPlaybackPhaseInputs& inputs) noexcept
    -> HumanoidHoldPhaseInputs {
  return {
      .is_constructing = inputs.is_constructing,
      .construction_role = inputs.construction_role,
      .is_in_hold_mode = inputs.is_in_hold_mode,
      .is_exiting_hold = inputs.is_exiting_hold,
      .hold_entry_progress = inputs.hold_entry_progress,
      .hold_exit_progress = inputs.hold_exit_progress,
      .is_guarding = inputs.is_guarding,
      .is_exiting_guard = inputs.is_exiting_guard,
      .guard_pose_progress = inputs.guard_pose_progress,
  };
}

} // namespace

auto normalize_clip_phase(float phase, bool loops) noexcept -> float {
  if (!loops) {
    return std::clamp(phase, 0.0F, terminal_non_looping_phase());
  }

  float wrapped = std::fmod(phase, 1.0F);
  if (wrapped < 0.0F) {
    wrapped += 1.0F;
  }
  return wrapped;
}

auto humanoid_hold_transition_amount(const HumanoidHoldPhaseInputs& inputs) noexcept
    -> float {
  if (inputs.is_in_hold_mode) {
    return smoothstep01(inputs.hold_entry_progress);
  }
  if (inputs.is_exiting_hold) {
    return smoothstep01(1.0F - inputs.hold_exit_progress);
  }
  return 0.0F;
}

auto humanoid_guard_pose_amount(const HumanoidHoldPhaseInputs& inputs) noexcept
    -> float {
  if (!inputs.is_guarding && !inputs.is_exiting_guard) {
    return 0.0F;
  }
  return smoothstep01(inputs.guard_pose_progress);
}

auto humanoid_hold_phase(const HumanoidHoldPhaseInputs& inputs) noexcept -> float {
  if (inputs.is_constructing &&
      inputs.construction_role == HumanoidConstructionRole::KneelingChisel) {
    return terminal_non_looping_phase();
  }

  float const hold_phase = humanoid_hold_transition_amount(inputs);
  if (hold_phase > 0.0F) {
    return std::clamp(hold_phase, 0.0F, terminal_non_looping_phase());
  }

  float const guard_phase = humanoid_guard_pose_amount(inputs);
  if (guard_phase > 0.0F) {
    return std::clamp(guard_phase, 0.0F, terminal_non_looping_phase());
  }

  return 0.0F;
}

auto resolve_humanoid_playback_phase(const HumanoidPlaybackPhaseInputs& inputs) noexcept
    -> float {
  if (inputs.state == StateId::Die) {
    return std::clamp(inputs.death_progress, 0.0F, 1.0F);
  }
  if (inputs.state == StateId::Dead) {
    return 0.0F;
  }
  if (inputs.state == StateId::Hold) {
    return humanoid_hold_phase(hold_phase_inputs(inputs));
  }
  if (inputs.state == StateId::RidingCharge && inputs.is_mounted &&
      inputs.is_attacking && inputs.is_melee && !is_moving(inputs.movement_state)) {
    return 0.12F + 0.66F * std::clamp(inputs.attack_phase, 0.0F, 1.0F);
  }
  if (inputs.is_constructing) {
    return normalize_clip_phase(
        inputs.construction_progress + inputs.construction_jitter_seed, true);
  }
  if (is_attack_state(inputs.state)) {
    return inputs.attack_phase;
  }
  if (inputs.state == StateId::Idle &&
      inputs.ambient_idle != HumanoidAmbientIdle::None) {
    return inputs.ambient_idle_phase;
  }
  return inputs.gait_cycle_phase;
}

} // namespace Animation
