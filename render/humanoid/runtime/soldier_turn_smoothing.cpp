#include "render/humanoid/runtime/soldier_turn_smoothing.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::Humanoid {

namespace {

constexpr float k_wheel_path_min_radius = 0.35F;
constexpr float k_wheel_path_blend_start_degrees = 40.0F;
constexpr float k_wheel_path_blend_end_degrees = 65.0F;
constexpr float k_turn_direction_epsilon_degrees = 0.05F;
constexpr float k_turn_settle_heading_degrees = 1.0F;
constexpr float k_turn_stable_reset_seconds = 0.08F;
constexpr float k_pivot_yaw_rate_degrees = 10.0F;
constexpr float k_pivot_center_speed = 0.35F;
constexpr float k_sweep_excess_speed = 0.5F;
constexpr float k_wheel_catch_up_scale = 1.5F;
constexpr float k_pivot_snap_distance_scale = 2.0F;
constexpr float k_relocate_travel_speed = 0.3F;

[[nodiscard]] auto hash_u32(std::uint32_t value) -> std::uint32_t {
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return value;
}

[[nodiscard]] auto hash_unit_float(std::uint32_t seed, std::uint32_t salt) -> float {
  return static_cast<float>(hash_u32(seed ^ salt) & 0xFFFFU) / 65535.0F;
}

[[nodiscard]] auto wrap_degrees(float degrees) -> float {
  float wrapped = std::fmod(degrees + 180.0F, 360.0F);
  if (wrapped < 0.0F) {
    wrapped += 360.0F;
  }
  return wrapped - 180.0F;
}

[[nodiscard]] auto turn_toward(float current_degrees,
                               float target_degrees,
                               float max_step_degrees) -> float {
  float const diff = wrap_degrees(target_degrees - current_degrees);
  float const step = std::clamp(diff, -max_step_degrees, max_step_degrees);
  return wrap_degrees(current_degrees + step);
}

[[nodiscard]] auto smoothstep(float edge0, float edge1, float value) -> float {
  float const t = std::clamp((value - edge0) / (edge1 - edge0), 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] auto
blend_degrees(float from_degrees, float to_degrees, float amount) -> float {
  return wrap_degrees(from_degrees + wrap_degrees(to_degrees - from_degrees) * amount);
}

} // namespace

auto soldier_turn_variation(std::uint32_t seed,
                            int row,
                            int rows,
                            bool mounted) -> SoldierTurnVariation {
  int const safe_rows = std::max(1, rows);
  int const safe_row = std::clamp(row, 0, safe_rows - 1);
  float const rear_rank = safe_rows > 1 ? static_cast<float>(safe_rows - 1 - safe_row) /
                                              static_cast<float>(safe_rows - 1)
                                        : 0.0F;
  float const response_jitter = hash_unit_float(seed, 0x9e3779b9U);
  float const speed_jitter = hash_unit_float(seed, 0x85ebca6bU);
  float const turn_jitter = hash_unit_float(seed, 0xc2b2ae35U);

  SoldierTurnVariation result;
  result.catch_up_speed_scale = 0.90F + speed_jitter * 0.18F;
  result.turn_rate_scale = 0.78F + turn_jitter * 0.40F;
  result.response_delay_seconds = 0.015F + response_jitter * 0.055F + rear_rank * 0.08F;
  if (mounted) {
    result.response_delay_seconds *= 1.2F;
  }
  return result;
}

auto resolve_soldier_turn_smoothing(SoldierTurnSmoothingState& state,
                                    const SoldierTurnSmoothingInputs& inputs)
    -> SoldierTurnSmoothingResult {
  SoldierTurnSmoothingResult result{};
  state.updated_frame = inputs.frame_index;

  float const formation_yaw_delta =
      state.valid
          ? wrap_degrees(inputs.formation_yaw_degrees - state.formation_yaw_degrees)
          : 0.0F;
  bool pivoting = false;
  if (inputs.allow_pivot_wheel && state.valid && inputs.dt > 0.0F) {
    float const center_step =
        std::hypot(inputs.formation_center_x - state.formation_center_x,
                   inputs.formation_center_z - state.formation_center_z);
    float const center_speed = center_step / inputs.dt;
    float const slot_speed =
        std::hypot(inputs.target_x - state.slot_x, inputs.target_z - state.slot_z) /
        inputs.dt;
    float const yaw_rate = std::abs(formation_yaw_delta) / inputs.dt;
    bool const rotating = yaw_rate > k_pivot_yaw_rate_degrees;
    bool const turning_on_the_spot = center_speed < k_pivot_center_speed;
    bool const slot_outruns_the_unit = slot_speed > center_speed + k_sweep_excess_speed;
    pivoting = rotating && (turning_on_the_spot || slot_outruns_the_unit);
  }
  result.pivoting = pivoting;

  bool const slot_owns_position =
      inputs.position_is_authoritative && !pivoting && !state.relocating;
  if (slot_owns_position) {
    state.world_x = inputs.target_x;
    state.world_z = inputs.target_z;
  }

  float const to_target_x = inputs.target_x - state.world_x;
  float const to_target_z = inputs.target_z - state.world_z;
  float const distance =
      std::sqrt(to_target_x * to_target_x + to_target_z * to_target_z);

  bool const wheeling_from_authoritative_slot =
      pivoting || (inputs.position_is_authoritative && state.relocating);
  float const snap_distance = wheeling_from_authoritative_slot
                                  ? inputs.snap_distance * k_pivot_snap_distance_scale
                                  : inputs.snap_distance;
  bool const must_snap = !state.valid || distance > snap_distance;
  if (must_snap) {
    state.world_x = inputs.target_x;
    state.world_z = inputs.target_z;
    state.body_yaw_degrees = wrap_degrees(inputs.formation_yaw_degrees);
    state.formation_yaw_degrees = wrap_degrees(inputs.formation_yaw_degrees);
    state.facing_yaw_degrees = wrap_degrees(inputs.formation_yaw_degrees);
    state.formation_center_x = inputs.formation_center_x;
    state.formation_center_z = inputs.formation_center_z;
    state.slot_x = inputs.target_x;
    state.slot_z = inputs.target_z;
    state.wheel_direction = 0.0F;
    state.turn_delay_remaining = 0.0F;
    state.formation_stable_seconds = 0.0F;
    state.valid = true;
    state.relocating = false;
    state.wheeling = false;
    state.turn_pending = false;
    result.x = state.world_x;
    result.z = state.world_z;
    result.yaw_degrees = state.body_yaw_degrees;
    result.travel_yaw_degrees = state.body_yaw_degrees;
    return result;
  }

  if (inputs.dt <= 0.0F) {
    result.x = state.world_x;
    result.z = state.world_z;
    result.yaw_degrees = state.body_yaw_degrees;
    result.travel_yaw_degrees = state.body_yaw_degrees;
    result.relocating = state.relocating;
    return result;
  }

  state.formation_center_x = inputs.formation_center_x;
  state.formation_center_z = inputs.formation_center_z;
  state.slot_x = inputs.target_x;
  state.slot_z = inputs.target_z;
  if (pivoting && state.relocating) {
    state.wheeling = true;
  }
  if (std::abs(formation_yaw_delta) > k_turn_direction_epsilon_degrees) {
    state.wheel_direction = formation_yaw_delta < 0.0F ? -1.0F : 1.0F;
    state.formation_stable_seconds = 0.0F;
    if (!state.turn_pending) {
      state.turn_pending = true;
      state.turn_delay_remaining = std::max(0.0F, inputs.response_delay_seconds);
    }
  } else {
    state.formation_stable_seconds += inputs.dt;
  }
  state.formation_yaw_degrees = wrap_degrees(inputs.formation_yaw_degrees);
  if (inputs.response_delay_seconds <= 0.0F) {
    state.turn_delay_remaining = 0.0F;
  }
  if (state.turn_pending && state.turn_delay_remaining > 0.0F) {
    state.turn_delay_remaining = std::max(0.0F, state.turn_delay_remaining - inputs.dt);
  }
  if (!state.turn_pending || state.turn_delay_remaining <= 0.0F) {
    state.facing_yaw_degrees = state.formation_yaw_degrees;
  }

  float const max_speed =
      state.wheeling ? inputs.max_speed * k_wheel_catch_up_scale : inputs.max_speed;
  float const max_step = max_speed * inputs.dt;
  float step = std::min(distance, max_step);
  float travel_yaw = state.body_yaw_degrees;
  float wheel_amount = 0.0F;
  if (distance > 1e-5F && step > 0.0F) {
    float const inv_distance = 1.0F / distance;
    float const direct_x = to_target_x * inv_distance;
    float const direct_z = to_target_z * inv_distance;
    float step_x = direct_x;
    float step_z = direct_z;

    float const current_x = state.world_x - inputs.formation_center_x;
    float const current_z = state.world_z - inputs.formation_center_z;
    float const target_x = inputs.target_x - inputs.formation_center_x;
    float const target_z = inputs.target_z - inputs.formation_center_z;
    float const current_radius = std::hypot(current_x, current_z);
    float const target_radius = std::hypot(target_x, target_z);
    float const dot = current_x * target_x + current_z * target_z;
    float const cross = current_z * target_x - current_x * target_z;
    float const angle = std::atan2(cross, dot);
    float const angle_degrees = std::abs(angle) * (180.0F / std::numbers::pi_v<float>);

    bool const wheel_path = inputs.allow_wheel_path &&
                            current_radius >= k_wheel_path_min_radius &&
                            target_radius >= k_wheel_path_min_radius;
    if (wheel_path) {
      wheel_amount = smoothstep(k_wheel_path_blend_start_degrees,
                                k_wheel_path_blend_end_degrees,
                                angle_degrees);
      float const geometric_direction = angle < 0.0F ? -1.0F : 1.0F;
      float const turn_sign =
          state.wheel_direction != 0.0F ? state.wheel_direction : geometric_direction;
      float const tangent_x = turn_sign * current_z / current_radius;
      float const tangent_z = -turn_sign * current_x / current_radius;
      float const radial_error = target_radius - current_radius;
      float const radial_x = current_x / current_radius;
      float const radial_z = current_z / current_radius;
      float const radial_weight =
          std::clamp(radial_error / current_radius, -0.35F, 0.35F);
      float const wheel_x = tangent_x + radial_x * radial_weight;
      float const wheel_z = tangent_z + radial_z * radial_weight;

      step_x = direct_x + (wheel_x - direct_x) * wheel_amount;
      step_z = direct_z + (wheel_z - direct_z) * wheel_amount;
      float const direction_length = std::hypot(step_x, step_z);
      if (direction_length > 1.0e-5F) {
        step_x /= direction_length;
        step_z /= direction_length;
      } else {
        step_x = tangent_x;
        step_z = tangent_z;
      }
      step += (max_step - step) * wheel_amount;
    }

    state.world_x += step_x * step;
    state.world_z += step_z * step;

    travel_yaw = std::atan2(step_x, step_z) * (180.0F / std::numbers::pi_v<float>);
  }

  float const remaining_x = inputs.target_x - state.world_x;
  float const remaining_z = inputs.target_z - state.world_z;
  float const remaining = std::hypot(remaining_x, remaining_z);
  result.travel_speed = inputs.dt > 0.0F ? step / inputs.dt : 0.0F;
  bool const swept_by_pivot = pivoting && result.travel_speed > k_relocate_travel_speed;
  if (state.relocating) {
    state.relocating = remaining > inputs.settle_distance || swept_by_pivot;
  } else {
    state.relocating = remaining > inputs.relocate_distance || swept_by_pivot;
  }
  if (!state.relocating) {
    state.wheel_direction = 0.0F;
    state.wheeling = false;
  }
  result.relocating = state.relocating;
  result.travel_yaw_degrees = wrap_degrees(travel_yaw);

  bool const awaiting_turn_response =
      state.turn_pending && state.turn_delay_remaining > 0.0F;
  bool const face_travel = inputs.allow_travel_yaw && !awaiting_turn_response &&
                           result.relocating && result.travel_speed > 0.3F;
  float const travel_facing =
      blend_degrees(travel_yaw, state.facing_yaw_degrees, wheel_amount);
  float const yaw_target = face_travel ? travel_facing : state.facing_yaw_degrees;
  state.body_yaw_degrees = turn_toward(
      state.body_yaw_degrees, yaw_target, inputs.turn_rate_degrees * inputs.dt);
  if (state.turn_pending && state.turn_delay_remaining <= 0.0F &&
      state.formation_stable_seconds >= k_turn_stable_reset_seconds &&
      std::abs(wrap_degrees(state.formation_yaw_degrees - state.body_yaw_degrees)) <=
          k_turn_settle_heading_degrees) {
    state.turn_pending = false;
  }

  result.x = state.world_x;
  result.z = state.world_z;
  result.yaw_degrees = state.body_yaw_degrees;
  return result;
}

} // namespace Render::Humanoid
