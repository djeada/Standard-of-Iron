#pragma once

#include <algorithm>
#include <cstdint>

namespace Render::Humanoid {

constexpr float k_soldier_catch_up_margin = 1.25F;
constexpr float k_soldier_catch_up_floor = 1.6F;

[[nodiscard]] constexpr auto soldier_catch_up_speed(float travel_speed) -> float {
  return std::max(k_soldier_catch_up_floor, travel_speed * k_soldier_catch_up_margin);
}

struct SoldierTurnSmoothingState {
  float world_x{0.0F};
  float world_z{0.0F};
  float body_yaw_degrees{0.0F};
  float formation_yaw_degrees{0.0F};
  float facing_yaw_degrees{0.0F};
  float wheel_direction{0.0F};
  float turn_delay_remaining{0.0F};
  float formation_stable_seconds{0.0F};

  std::uint32_t updated_frame{0U};
  bool valid{false};
  bool relocating{false};
  bool turn_pending{false};
};

struct SoldierTurnVariation {
  float catch_up_speed_scale{1.0F};
  float turn_rate_scale{1.0F};
  float response_delay_seconds{0.0F};
};

[[nodiscard]] auto soldier_turn_variation(std::uint32_t seed,
                                          int row,
                                          int rows,
                                          bool mounted) -> SoldierTurnVariation;

struct SoldierTurnSmoothingInputs {

  float target_x{0.0F};
  float target_z{0.0F};

  float formation_yaw_degrees{0.0F};

  float formation_center_x{0.0F};
  float formation_center_z{0.0F};

  float dt{0.0F};

  float max_speed{2.5F};

  float turn_rate_degrees{540.0F};

  float response_delay_seconds{0.0F};

  float snap_distance{7.0F};

  float settle_distance{0.06F};

  float relocate_distance{0.30F};

  bool allow_travel_yaw{true};

  bool allow_wheel_path{true};

  bool position_is_authoritative{false};

  std::uint32_t frame_index{0U};
};

struct SoldierTurnSmoothingResult {
  float x{0.0F};
  float z{0.0F};
  float yaw_degrees{0.0F};

  float travel_speed{0.0F};

  float travel_yaw_degrees{0.0F};

  bool relocating{false};
};

[[nodiscard]] auto resolve_soldier_turn_smoothing(
    SoldierTurnSmoothingState& state,
    const SoldierTurnSmoothingInputs& inputs) -> SoldierTurnSmoothingResult;

} // namespace Render::Humanoid
