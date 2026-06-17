#include "elephant_gait_manifest.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Animation {

namespace {

constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;
constexpr float k_weight_shift_smoothing_x = 0.22F;
constexpr float k_weight_shift_smoothing_z = 0.20F;
constexpr float k_lag_smoothing = 0.28F;
constexpr float k_idle_lag_damping = 0.86F;
constexpr float k_position_phase_desync_x = 0.173F;
constexpr float k_position_phase_desync_z = 0.127F;

constexpr float k_leg_phase_fl = 0.00F;
constexpr float k_leg_phase_fr = 0.50F;
constexpr float k_leg_phase_rl = 0.75F;
constexpr float k_leg_phase_rr = 0.25F;
constexpr float k_swing_duration = 0.25F;
constexpr float k_weight_shift_lateral = 0.025F;
constexpr float k_weight_shift_fore_aft = 0.015F;
constexpr float k_shoulder_lag_factor = 0.08F;
constexpr float k_hip_lag_factor = 0.06F;

[[nodiscard]] constexpr auto leg_for_index(int leg_index) noexcept -> QuadrupedLegId {
  return static_cast<QuadrupedLegId>(leg_index);
}

} // namespace

auto elephant_leg_phase_offset(int leg_index) noexcept -> float {
  switch (leg_index) {
  case 0:
    return k_leg_phase_fl;
  case 1:
    return k_leg_phase_fr;
  case 2:
    return k_leg_phase_rl;
  case 3:
    return k_leg_phase_rr;
  default:
    return 0.0F;
  }
}

auto elephant_leg_is_in_swing(float cycle_phase, int leg_index) noexcept -> bool {
  float const leg_phase =
      wrap_quadruped_phase(cycle_phase - elephant_leg_phase_offset(leg_index));
  return leg_phase < k_swing_duration;
}

auto elephant_leg_swing_progress(float cycle_phase, int leg_index) noexcept -> float {
  float const leg_phase =
      wrap_quadruped_phase(cycle_phase - elephant_leg_phase_offset(leg_index));
  if (leg_phase < k_swing_duration) {
    return leg_phase / k_swing_duration;
  }
  return -1.0F;
}

auto resolve_elephant_gait_state(const ElephantGaitUpdateInputs& inputs) noexcept
    -> ElephantGaitState {
  ElephantGaitState state = inputs.previous;
  float const cycle_time = std::max(inputs.gait.cycle_time, 0.001F);
  float const locomotion_scale = inputs.is_running ? 1.18F : 1.0F;
  float const position_phase_offset =
      wrap_quadruped_phase(inputs.body_world_x * k_position_phase_desync_x +
                           inputs.body_world_z * k_position_phase_desync_z);
  float const forward_alignment =
      std::clamp(std::abs(inputs.body_forward_z), 0.2F, 1.0F);

  PoseVec3 const barrel_center{
      .x = 0.0F, .y = inputs.dimensions.barrel_center_y, .z = 0.0F};
  if (!state.initialized) {
    for (int i = 0; i < 4; ++i) {
      state.legs[static_cast<std::size_t>(i)].planted_foot =
          quadruped_default_foot_position(
              inputs.dimensions, leg_for_index(i), barrel_center);
      state.legs[static_cast<std::size_t>(i)].swing_start =
          state.legs[static_cast<std::size_t>(i)].planted_foot;
      state.legs[static_cast<std::size_t>(i)].swing_target =
          state.legs[static_cast<std::size_t>(i)].planted_foot;
      state.legs[static_cast<std::size_t>(i)].in_swing = false;
      state.legs[static_cast<std::size_t>(i)].swing_progress = 0.0F;
    }
    state.initialized = true;
  }

  if (inputs.is_moving) {
    state.cycle_phase =
        wrap_quadruped_phase(inputs.sample_time / cycle_time + position_phase_offset);
  } else {
    state.cycle_phase = 0.0F;
    for (auto& leg : state.legs) {
      leg.in_swing = false;
    }
  }

  float const stride_length =
      inputs.dimensions.body_length *
      (0.18F + std::clamp(inputs.gait.stride_swing, 0.0F, 1.0F) * 0.28F) *
      locomotion_scale;

  for (int i = 0; i < 4; ++i) {
    auto& leg = state.legs[static_cast<std::size_t>(i)];
    float const swing_progress = elephant_leg_swing_progress(state.cycle_phase, i);

    if (swing_progress >= 0.0F && inputs.is_moving) {
      if (!leg.in_swing) {
        leg.swing_start = leg.planted_foot;
        leg.swing_target = quadruped_swing_target(inputs.dimensions,
                                                  leg_for_index(i),
                                                  barrel_center,
                                                  stride_length,
                                                  0.52F,
                                                  0.42F,
                                                  0.48F,
                                                  false);
        leg.in_swing = true;
      }
      leg.swing_progress = swing_progress;
    } else {
      if (leg.in_swing) {
        leg.planted_foot = leg.swing_target;
        leg.in_swing = false;
      }
      leg.swing_progress = 0.0F;
    }
  }

  float total_x = 0.0F;
  float total_z = 0.0F;
  int planted_count = 0;
  for (auto& leg : state.legs) {
    if (!leg.in_swing) {
      total_x += leg.planted_foot.x;
      total_z += leg.planted_foot.z;
      ++planted_count;
    }
  }

  if (planted_count > 0) {
    float const center_x = total_x / static_cast<float>(planted_count);
    float const center_z = total_z / static_cast<float>(planted_count);
    float const target_shift_x = -center_x * k_weight_shift_lateral;
    float const target_shift_z =
        -center_z * k_weight_shift_fore_aft * 0.5F * forward_alignment;
    state.weight_shift_x +=
        (target_shift_x - state.weight_shift_x) * k_weight_shift_smoothing_x;
    state.weight_shift_z +=
        (target_shift_z - state.weight_shift_z) * k_weight_shift_smoothing_z;
  }

  if (inputs.is_moving) {
    float const cycle_sin = std::sin(state.cycle_phase * k_two_pi);
    float const shoulder_target =
        cycle_sin * k_shoulder_lag_factor * (0.90F + 0.15F * locomotion_scale);
    float const hip_target =
        -cycle_sin * k_hip_lag_factor * (0.90F + 0.10F * locomotion_scale);
    state.shoulder_lag += (shoulder_target - state.shoulder_lag) * k_lag_smoothing;
    state.hip_lag += (hip_target - state.hip_lag) * k_lag_smoothing;
  } else {
    state.shoulder_lag *= k_idle_lag_damping;
    state.hip_lag *= k_idle_lag_damping;
  }

  return state;
}

} // namespace Animation
