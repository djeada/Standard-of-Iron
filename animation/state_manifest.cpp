#include "state_manifest.h"

#include <algorithm>

namespace Animation {

namespace {

[[nodiscard]] auto positive_duration(float duration) noexcept -> float {
  return std::max(duration, 1.0e-4F);
}

} // namespace

auto humanoid_action_blocks_hold_pose(const HumanoidActionBlockInputs& action) noexcept
    -> bool {
  return (action.is_attacking && action.is_melee) || action.is_hit_reacting ||
         action.is_constructing || action.is_healing || action.is_dying ||
         action.is_dead;
}

auto humanoid_action_allows_guard_pose(const HumanoidActionBlockInputs& action) noexcept
    -> bool {
  return !action.is_attacking && !action.is_hit_reacting && !action.is_constructing &&
         !action.is_healing && !action.is_dying && !action.is_dead;
}

auto resolve_humanoid_stance_targets(const HumanoidStanceTargetInputs& inputs) noexcept
    -> HumanoidStanceTargets {
  HumanoidStanceTargets targets{};
  bool const hold_blocked = humanoid_action_blocks_hold_pose(inputs.action);
  targets.hold = inputs.hold_requested && !hold_blocked;
  targets.exiting_hold = hold_blocked ? inputs.previous_hold_pose_progress > 1.0e-4F
                                      : inputs.hold_exit_requested;
  if (targets.hold) {
    targets.hold_entry_progress = std::clamp(inputs.hold_entry_progress, 0.0F, 1.0F);
  }
  if (targets.exiting_hold && !hold_blocked) {
    targets.hold_exit_progress = std::clamp(inputs.hold_exit_progress, 0.0F, 1.0F);
  }

  targets.guard =
      inputs.raw_guard_requested && humanoid_action_allows_guard_pose(inputs.action);
  return targets;
}

auto resolve_humanoid_persistent_stance_state(
    const HumanoidPersistentStanceInputs& inputs) noexcept
    -> HumanoidPersistentStanceState {
  bool const time_regressed =
      inputs.previous_initialized && inputs.sample_time < inputs.previous_sample_time;
  bool const initialized = inputs.previous_initialized && !time_regressed;
  float const delta_time =
      initialized ? std::max(0.0F, inputs.sample_time - inputs.previous_sample_time)
                  : 0.0F;

  HumanoidPersistentStanceState state{};
  state.initialized = true;
  state.last_sample_time = inputs.sample_time;
  state.idle_duration = initialized ? inputs.previous_idle_duration : 0.0F;
  state.is_guarding = inputs.stance.guard;
  state.guard_pose_progress = inputs.stance.guard ? 1.0F : 0.0F;
  state.is_in_hold_mode = inputs.stance.hold;
  state.is_exiting_hold = inputs.stance.exiting_hold;
  state.hold_pose_progress =
      inputs.stance.hold ? std::clamp(inputs.stance.hold_entry_progress, 0.0F, 1.0F)
                         : 0.0F;

  if (!initialized) {
    state.guard_pose_progress = 0.0F;
    state.hold_pose_progress = 0.0F;
    state.is_exiting_hold = false;
    state.hold_entry_progress = 0.0F;
    state.hold_exit_progress = 0.0F;
    return state;
  }

  float const previous_guard_pose =
      std::clamp(inputs.previous_guard_pose_progress, 0.0F, 1.0F);
  float const previous_hold_pose =
      std::clamp(inputs.previous_hold_pose_progress, 0.0F, 1.0F);

  if (inputs.stance.guard) {
    state.guard_pose_progress =
        std::min(1.0F,
                 previous_guard_pose +
                     delta_time / positive_duration(inputs.guard_enter_duration));
  } else if (previous_guard_pose > 0.0F) {
    state.guard_pose_progress =
        std::max(0.0F,
                 previous_guard_pose -
                     delta_time / positive_duration(inputs.guard_exit_duration));
    state.is_exiting_guard = state.guard_pose_progress > 0.0F;
  } else {
    state.guard_pose_progress = 0.0F;
  }

  if (inputs.stance.hold) {
    float const gameplay_progress =
        std::clamp(inputs.stance.hold_entry_progress, 0.0F, 1.0F);
    float const max_visual_progress =
        std::min(1.0F,
                 previous_hold_pose +
                     delta_time / positive_duration(inputs.hold_enter_duration));
    state.hold_pose_progress =
        std::min(std::max(previous_hold_pose, gameplay_progress), max_visual_progress);
  } else if (previous_hold_pose > 0.0F) {
    state.hold_pose_progress = std::max(
        0.0F,
        previous_hold_pose - delta_time / positive_duration(inputs.hold_exit_duration));
    state.is_exiting_hold = state.hold_pose_progress > 0.0F;
  } else {
    state.hold_pose_progress = 0.0F;
    state.is_exiting_hold = false;
  }

  if (inputs.is_active) {
    state.idle_duration = 0.0F;
  } else {
    state.idle_duration += delta_time;
  }

  if (inputs.stance.hold) {
    state.hold_entry_progress = state.hold_pose_progress;
    state.hold_exit_progress = 0.0F;
  } else if (state.is_exiting_hold) {
    state.hold_entry_progress = 0.0F;
    state.hold_exit_progress = 1.0F - state.hold_pose_progress;
  } else {
    state.hold_entry_progress = 0.0F;
    state.hold_exit_progress = 0.0F;
  }

  return state;
}

} // namespace Animation
