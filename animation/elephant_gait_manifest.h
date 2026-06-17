#pragma once

#include <array>

#include "pose_manifest.h"
#include "quadruped_gait_manifest.h"

namespace Animation {

struct ElephantLegGaitState {
  PoseVec3 planted_foot{};
  PoseVec3 swing_start{};
  PoseVec3 swing_target{};
  float swing_progress{0.0F};
  bool in_swing{false};
};

struct ElephantGaitState {
  std::array<ElephantLegGaitState, 4> legs{};
  float cycle_phase{0.0F};
  float weight_shift_x{0.0F};
  float weight_shift_z{0.0F};
  float shoulder_lag{0.0F};
  float hip_lag{0.0F};
  bool initialized{false};
};

struct ElephantGaitUpdateInputs {
  ElephantGaitState previous{};
  QuadrupedDimensions dimensions{};
  QuadrupedGait gait{};
  float sample_time{0.0F};
  float body_world_x{0.0F};
  float body_world_z{0.0F};
  float body_forward_z{1.0F};
  bool is_moving{false};
  bool is_running{false};
};

[[nodiscard]] auto elephant_leg_phase_offset(int leg_index) noexcept -> float;
[[nodiscard]] auto elephant_leg_is_in_swing(float cycle_phase,
                                            int leg_index) noexcept -> bool;
[[nodiscard]] auto elephant_leg_swing_progress(float cycle_phase,
                                               int leg_index) noexcept -> float;

[[nodiscard]] auto resolve_elephant_gait_state(
    const ElephantGaitUpdateInputs& inputs) noexcept -> ElephantGaitState;

} // namespace Animation
