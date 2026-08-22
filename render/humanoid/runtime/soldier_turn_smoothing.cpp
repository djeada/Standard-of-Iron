#include "render/humanoid/runtime/soldier_turn_smoothing.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::Humanoid {

namespace {

constexpr float k_wheel_path_min_radius = 0.35F;
constexpr float k_wheel_path_min_angle_degrees = 50.0F;
constexpr float k_wheel_path_arrival_angle_degrees = 8.0F;

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

} // namespace

auto resolve_soldier_turn_smoothing(SoldierTurnSmoothingState& state,
                                    const SoldierTurnSmoothingInputs& inputs)
    -> SoldierTurnSmoothingResult {
  SoldierTurnSmoothingResult result{};
  state.updated_frame = inputs.frame_index;

  float const to_target_x = inputs.target_x - state.world_x;
  float const to_target_z = inputs.target_z - state.world_z;
  float const distance =
      std::sqrt(to_target_x * to_target_x + to_target_z * to_target_z);

  bool const must_snap = !state.valid || distance > inputs.snap_distance;
  if (must_snap) {
    state.world_x = inputs.target_x;
    state.world_z = inputs.target_z;
    state.body_yaw_degrees = wrap_degrees(inputs.formation_yaw_degrees);
    state.valid = true;
    state.relocating = false;
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

  float const max_step = inputs.max_speed * inputs.dt;
  float step = std::min(distance, max_step);
  float travel_yaw = state.body_yaw_degrees;
  if (distance > 1e-5F && step > 0.0F) {
    float step_x = to_target_x;
    float step_z = to_target_z;

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
                            target_radius >= k_wheel_path_min_radius &&
                            angle_degrees >= k_wheel_path_min_angle_degrees;
    if (wheel_path) {

      float const turn_sign = angle < 0.0F ? -1.0F : 1.0F;
      float const tangent_x = turn_sign * current_z / current_radius;
      float const tangent_z = -turn_sign * current_x / current_radius;
      float const radial_error = target_radius - current_radius;
      float const radial_x = current_x / current_radius;
      float const radial_z = current_z / current_radius;
      float const radial_weight =
          std::clamp(radial_error / current_radius, -0.35F, 0.35F);
      step_x = tangent_x + radial_x * radial_weight;
      step_z = tangent_z + radial_z * radial_weight;

      float const direction_length = std::hypot(step_x, step_z);
      step_x /= direction_length;
      step_z /= direction_length;
      step = max_step;
    } else {
      float const inv_distance = 1.0F / distance;
      step_x *= inv_distance;
      step_z *= inv_distance;
    }

    if (angle_degrees < k_wheel_path_arrival_angle_degrees) {
      step = std::min(distance, max_step);
    }
    state.world_x += step_x * step;
    state.world_z += step_z * step;

    travel_yaw = std::atan2(step_x, step_z) * (180.0F / std::numbers::pi_v<float>);
  }

  float const remaining_x = inputs.target_x - state.world_x;
  float const remaining_z = inputs.target_z - state.world_z;
  float const remaining = std::hypot(remaining_x, remaining_z);
  if (state.relocating) {
    state.relocating = remaining > inputs.settle_distance;
  } else {
    state.relocating = remaining > inputs.relocate_distance;
  }
  result.relocating = state.relocating;
  result.travel_speed = inputs.dt > 0.0F ? step / inputs.dt : 0.0F;
  result.travel_yaw_degrees = wrap_degrees(travel_yaw);

  bool const face_travel =
      inputs.allow_travel_yaw && result.relocating && result.travel_speed > 0.3F;
  float const yaw_target = face_travel ? travel_yaw : inputs.formation_yaw_degrees;
  state.body_yaw_degrees = turn_toward(
      state.body_yaw_degrees, yaw_target, inputs.turn_rate_degrees * inputs.dt);

  result.x = state.world_x;
  result.z = state.world_z;
  result.yaw_degrees = state.body_yaw_degrees;
  return result;
}

} // namespace Render::Humanoid
