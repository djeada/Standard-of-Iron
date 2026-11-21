#pragma once

namespace Render::Humanoid {

constexpr float k_ground_motion_min_run_speed = 2.6F;
constexpr float k_ground_motion_run_speed_ratio = 1.45F;
constexpr float k_ground_motion_min_stride_presence = 0.30F;
constexpr float k_ground_motion_stride_presence_gain = 1.35F;

struct SoldierGroundMotionState {
  float x{0.0F};
  float z{0.0F};
  float yaw_degrees{0.0F};
  float velocity_x{0.0F};
  float velocity_z{0.0F};
  float turn_rate{0.0F};
  float rest_x{0.0F};
  float rest_z{0.0F};
  float rest_yaw_degrees{0.0F};
  float time{0.0F};
  float held_seconds{0.0F};
  float run_pending_seconds{0.0F};
  bool valid{false};
  bool stepping{false};
  bool running{false};
};

struct SoldierGroundMotionInputs {
  float x{0.0F};
  float z{0.0F};
  float yaw_degrees{0.0F};
  float time{0.0F};
  float march_speed{2.0F};
  float seed_velocity_x{0.0F};
  float seed_velocity_z{0.0F};
  bool carried{false};
};

struct SoldierGroundMotion {
  float speed{0.0F};
  float stride_speed{0.0F};
  float travel_x{0.0F};
  float travel_z{0.0F};
  float turn_rate{0.0F};
  float stride_presence{0.0F};
  bool travelling{false};
  bool stepping{false};
  bool running{false};
};

[[nodiscard]] auto ground_motion_run_speed(float march_speed) -> float;

[[nodiscard]] auto ground_motion_stride_presence(float stride_speed,
                                                 bool running) -> float;

[[nodiscard]] auto observe_soldier_ground_motion(
    SoldierGroundMotionState& state,
    const SoldierGroundMotionInputs& inputs) -> SoldierGroundMotion;

[[nodiscard]] auto peek_soldier_ground_motion(const SoldierGroundMotionState& state,
                                              const SoldierGroundMotionInputs& inputs)
    -> SoldierGroundMotion;

} // namespace Render::Humanoid
