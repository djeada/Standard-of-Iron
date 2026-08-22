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
    state.formation_yaw_degrees = wrap_degrees(inputs.formation_yaw_degrees);
    state.wheel_direction = 0.0F;
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

  float const formation_yaw_delta =
      wrap_degrees(inputs.formation_yaw_degrees - state.formation_yaw_degrees);
  if (std::abs(formation_yaw_delta) > k_turn_direction_epsilon_degrees) {
    state.wheel_direction = formation_yaw_delta < 0.0F ? -1.0F : 1.0F;
  }
  state.formation_yaw_degrees = wrap_degrees(inputs.formation_yaw_degrees);

  float const max_step = inputs.max_speed * inputs.dt;
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
  if (state.relocating) {
    state.relocating = remaining > inputs.settle_distance;
  } else {
    state.relocating = remaining > inputs.relocate_distance;
  }
  if (!state.relocating) {
    state.wheel_direction = 0.0F;
  }
  result.relocating = state.relocating;
  result.travel_speed = inputs.dt > 0.0F ? step / inputs.dt : 0.0F;
  result.travel_yaw_degrees = wrap_degrees(travel_yaw);

  bool const face_travel =
      inputs.allow_travel_yaw && result.relocating && result.travel_speed > 0.3F;
  float const travel_facing =
      blend_degrees(travel_yaw, inputs.formation_yaw_degrees, wheel_amount);
  float const yaw_target = face_travel ? travel_facing : inputs.formation_yaw_degrees;
  state.body_yaw_degrees = turn_toward(
      state.body_yaw_degrees, yaw_target, inputs.turn_rate_degrees * inputs.dt);

  result.x = state.world_x;
  result.z = state.world_z;
  result.yaw_degrees = state.body_yaw_degrees;
  return result;
}

} // namespace Render::Humanoid
