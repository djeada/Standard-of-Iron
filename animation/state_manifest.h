#pragma once

namespace Animation {

struct HumanoidActionBlockInputs {
  bool is_attacking{false};
  bool is_melee{false};
  bool is_hit_reacting{false};
  bool is_constructing{false};
  bool is_healing{false};
  bool is_dying{false};
  bool is_dead{false};
};

struct HumanoidStanceTargetInputs {
  bool hold_requested{false};
  bool hold_exit_requested{false};
  bool raw_guard_requested{false};
  float hold_entry_progress{0.0F};
  float hold_exit_progress{0.0F};
  float previous_hold_pose_progress{0.0F};
  HumanoidActionBlockInputs action{};
};

struct HumanoidStanceTargets {
  bool hold{false};
  bool exiting_hold{false};
  float hold_entry_progress{0.0F};
  float hold_exit_progress{0.0F};
  bool guard{false};
};

struct HumanoidPersistentStanceInputs {
  float sample_time{0.0F};
  bool previous_initialized{false};
  float previous_sample_time{0.0F};
  float previous_idle_duration{0.0F};
  float previous_guard_pose_progress{0.0F};
  float previous_hold_pose_progress{0.0F};
  bool is_active{false};
  HumanoidStanceTargets stance{};
  float guard_enter_duration{0.0F};
  float guard_exit_duration{0.0F};
  float hold_enter_duration{0.0F};
  float hold_exit_duration{0.0F};
};

struct HumanoidPersistentStanceState {
  bool initialized{false};
  float last_sample_time{0.0F};
  float idle_duration{0.0F};
  bool is_guarding{false};
  bool is_exiting_guard{false};
  float guard_pose_progress{0.0F};
  bool is_in_hold_mode{false};
  bool is_exiting_hold{false};
  float hold_pose_progress{0.0F};
  float hold_entry_progress{0.0F};
  float hold_exit_progress{0.0F};
};

[[nodiscard]] auto humanoid_action_blocks_hold_pose(
    const HumanoidActionBlockInputs& action) noexcept -> bool;

[[nodiscard]] auto humanoid_action_allows_guard_pose(
    const HumanoidActionBlockInputs& action) noexcept -> bool;

[[nodiscard]] auto resolve_humanoid_stance_targets(
    const HumanoidStanceTargetInputs& inputs) noexcept -> HumanoidStanceTargets;

[[nodiscard]] auto resolve_humanoid_persistent_stance_state(
    const HumanoidPersistentStanceInputs& inputs) noexcept
    -> HumanoidPersistentStanceState;

} // namespace Animation
