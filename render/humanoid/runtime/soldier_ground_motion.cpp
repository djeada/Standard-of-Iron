#include "render/humanoid/runtime/soldier_ground_motion.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "animation/locomotion_manifest.h"

namespace Render::Humanoid {

namespace {

constexpr float k_velocity_tau = 0.12F;
constexpr float k_turn_tau = 0.12F;
constexpr float k_step_enter_speed = 0.12F;
constexpr float k_step_exit_speed = 0.06F;
constexpr float k_step_confirm_distance = 0.04F;
constexpr float k_step_debt_distance = 0.09F;
constexpr float k_foot_turn_radius = 0.155F;
constexpr float k_min_step_seconds = 0.30F;
constexpr float k_min_rest_seconds = 0.20F;
constexpr float k_urgent_speed = 0.45F;
constexpr float k_run_exit_ratio = 0.85F;
constexpr float k_run_entry_seconds = 0.15F;
constexpr float k_stale_seconds = 0.25F;
constexpr float k_max_dt = 0.1F;
constexpr float k_teleport_floor = 0.75F;
constexpr float k_teleport_speed = 12.0F;
constexpr float k_travel_direction_speed = 0.03F;

[[nodiscard]] auto wrap_degrees(float degrees) -> float {
  float wrapped = std::fmod(degrees + 180.0F, 360.0F);
  if (wrapped < 0.0F) {
    wrapped += 360.0F;
  }
  return wrapped - 180.0F;
}

[[nodiscard]] auto radians(float degrees) -> float {
  return degrees * std::numbers::pi_v<float> / 180.0F;
}

void seed(SoldierGroundMotionState& state,
          const SoldierGroundMotionInputs& inputs,
          float run_speed) {
  float const speed = inputs.carried
                          ? 0.0F
                          : std::hypot(inputs.seed_velocity_x, inputs.seed_velocity_z);
  state = {};
  state.x = inputs.x;
  state.z = inputs.z;
  state.yaw_degrees = inputs.yaw_degrees;
  state.rest_x = inputs.x;
  state.rest_z = inputs.z;
  state.rest_yaw_degrees = inputs.yaw_degrees;
  state.time = inputs.time;
  state.valid = true;
  if (!inputs.carried) {
    state.velocity_x = inputs.seed_velocity_x;
    state.velocity_z = inputs.seed_velocity_z;
  }
  state.stepping = speed > k_step_enter_speed;
  state.running = state.stepping && speed > run_speed;
  state.held_seconds = k_min_step_seconds;
}

[[nodiscard]] auto
report(const SoldierGroundMotionState& state) -> SoldierGroundMotion {
  SoldierGroundMotion motion{};
  motion.speed = std::hypot(state.velocity_x, state.velocity_z);
  motion.turn_rate = state.turn_rate;
  motion.stride_speed =
      std::max(motion.speed, radians(state.turn_rate) * k_foot_turn_radius);
  motion.travelling = motion.speed > k_travel_direction_speed;
  if (motion.travelling) {
    motion.travel_x = state.velocity_x / motion.speed;
    motion.travel_z = state.velocity_z / motion.speed;
  }
  motion.stepping = state.stepping;
  motion.running = state.stepping && state.running;
  motion.stride_presence =
      state.stepping
          ? ground_motion_stride_presence(motion.stride_speed, motion.running)
          : 0.0F;
  return motion;
}

} // namespace

auto ground_motion_run_speed(float march_speed) -> float {
  return std::max(k_ground_motion_min_run_speed,
                  march_speed * k_ground_motion_run_speed_ratio);
}

auto ground_motion_stride_presence(float stride_speed, bool running) -> float {
  if (running) {
    return 1.0F;
  }
  float const speed = std::max(0.0F, stride_speed);
  float const cycle_time = Animation::humanoid_walk_cycle_time_for_speed(speed);
  float const covered = speed * cycle_time / Animation::k_humanoid_walk_cycle_distance;
  return std::clamp(covered * k_ground_motion_stride_presence_gain,
                    k_ground_motion_min_stride_presence,
                    1.0F);
}

auto observe_soldier_ground_motion(SoldierGroundMotionState& state,
                                   const SoldierGroundMotionInputs& inputs)
    -> SoldierGroundMotion {
  float const run_speed = ground_motion_run_speed(inputs.march_speed);
  float const elapsed = inputs.time - state.time;
  if (!state.valid || elapsed < 0.0F || elapsed > k_stale_seconds) {
    seed(state, inputs, run_speed);
    return report(state);
  }
  if (elapsed <= 1.0e-5F) {
    return report(state);
  }
  float const dt = std::min(elapsed, k_max_dt);
  float const dx = inputs.x - state.x;
  float const dz = inputs.z - state.z;
  float const step = std::hypot(dx, dz);
  float const turned = std::abs(wrap_degrees(inputs.yaw_degrees - state.yaw_degrees));
  bool const teleported = step > std::max(k_teleport_floor, k_teleport_speed * dt);

  state.x = inputs.x;
  state.z = inputs.z;
  state.yaw_degrees = inputs.yaw_degrees;
  state.time = inputs.time;
  state.held_seconds += dt;

  if (inputs.carried) {
    state.velocity_x = 0.0F;
    state.velocity_z = 0.0F;
    state.turn_rate = 0.0F;
    state.rest_x = inputs.x;
    state.rest_z = inputs.z;
    state.rest_yaw_degrees = inputs.yaw_degrees;
    if (state.stepping) {
      state.held_seconds = 0.0F;
    }
    state.stepping = false;
    state.running = false;
    state.run_pending_seconds = 0.0F;
    return report(state);
  }

  if (teleported) {
    state.rest_x = inputs.x;
    state.rest_z = inputs.z;
    state.rest_yaw_degrees = inputs.yaw_degrees;
  } else {
    float const follow = 1.0F - std::exp(-dt / k_velocity_tau);
    state.velocity_x += (dx / dt - state.velocity_x) * follow;
    state.velocity_z += (dz / dt - state.velocity_z) * follow;
    float const turn_follow = 1.0F - std::exp(-dt / k_turn_tau);
    state.turn_rate += (turned / dt - state.turn_rate) * turn_follow;
  }

  float const speed = std::hypot(state.velocity_x, state.velocity_z);
  float const stride_speed =
      std::max(speed, radians(state.turn_rate) * k_foot_turn_radius);

  if (state.stepping) {
    state.rest_x = inputs.x;
    state.rest_z = inputs.z;
    state.rest_yaw_degrees = inputs.yaw_degrees;
    if (stride_speed < k_step_exit_speed && state.held_seconds >= k_min_step_seconds) {
      state.stepping = false;
      state.held_seconds = 0.0F;
    }
  } else {
    float const strayed =
        std::hypot(inputs.x - state.rest_x, inputs.z - state.rest_z) +
        radians(std::abs(wrap_degrees(inputs.yaw_degrees - state.rest_yaw_degrees))) *
            k_foot_turn_radius;
    bool const moving_off =
        stride_speed > k_step_enter_speed && strayed > k_step_confirm_distance;
    bool const wants_step = moving_off || strayed > k_step_debt_distance;
    bool const rested =
        state.held_seconds >= k_min_rest_seconds || stride_speed > k_urgent_speed;
    if (wants_step && rested) {
      state.stepping = true;
      state.held_seconds = 0.0F;
      state.rest_x = inputs.x;
      state.rest_z = inputs.z;
      state.rest_yaw_degrees = inputs.yaw_degrees;
    }
  }

  if (!state.stepping) {
    state.running = false;
    state.run_pending_seconds = 0.0F;
  } else if (state.running) {
    if (speed < run_speed * k_run_exit_ratio) {
      state.running = false;
      state.run_pending_seconds = 0.0F;
    }
  } else if (speed > run_speed) {
    state.run_pending_seconds += dt;
    if (state.run_pending_seconds >= k_run_entry_seconds) {
      state.running = true;
      state.run_pending_seconds = 0.0F;
    }
  } else {
    state.run_pending_seconds = 0.0F;
  }
  return report(state);
}

auto peek_soldier_ground_motion(const SoldierGroundMotionState& state,
                                const SoldierGroundMotionInputs& inputs)
    -> SoldierGroundMotion {
  SoldierGroundMotionState scratch = state;
  return observe_soldier_ground_motion(scratch, inputs);
}

} // namespace Render::Humanoid
