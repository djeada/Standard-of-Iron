#pragma once

#include "clip_manifest.h"
#include "pose_manifest.h"

namespace Animation {

struct HumanoidHoldPhaseInputs {
  bool is_constructing{false};
  HumanoidConstructionRole construction_role{HumanoidConstructionRole::None};
  bool is_in_hold_mode{false};
  bool is_exiting_hold{false};
  float hold_entry_progress{0.0F};
  float hold_exit_progress{0.0F};
  bool is_guarding{false};
  bool is_exiting_guard{false};
  float guard_pose_progress{0.0F};
};

struct HumanoidPlaybackPhaseInputs {
  StateId state{StateId::Idle};
  bool is_mounted{false};
  bool is_attacking{false};
  bool is_melee{false};
  MovementState movement_state{MovementState::Idle};
  bool is_constructing{false};
  HumanoidConstructionRole construction_role{HumanoidConstructionRole::None};
  float construction_progress{0.0F};
  float construction_jitter_seed{0.0F};
  bool is_in_hold_mode{false};
  bool is_exiting_hold{false};
  float hold_entry_progress{0.0F};
  float hold_exit_progress{0.0F};
  bool is_guarding{false};
  bool is_exiting_guard{false};
  float guard_pose_progress{0.0F};
  float death_progress{0.0F};
  float attack_phase{0.0F};
  HumanoidAmbientIdle ambient_idle{HumanoidAmbientIdle::None};
  float ambient_idle_phase{0.0F};
  float gait_cycle_phase{0.0F};
};

[[nodiscard]] auto normalize_clip_phase(float phase, bool loops) noexcept -> float;

[[nodiscard]] auto humanoid_hold_transition_amount(
    const HumanoidHoldPhaseInputs& inputs) noexcept -> float;

[[nodiscard]] auto
humanoid_guard_pose_amount(const HumanoidHoldPhaseInputs& inputs) noexcept -> float;

[[nodiscard]] auto
humanoid_hold_phase(const HumanoidHoldPhaseInputs& inputs) noexcept -> float;

[[nodiscard]] auto resolve_humanoid_playback_phase(
    const HumanoidPlaybackPhaseInputs& inputs) noexcept -> float;

} // namespace Animation
