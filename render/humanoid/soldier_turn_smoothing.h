#pragma once

namespace Render::Humanoid {

struct SoldierTurnSmoothingState {
  float world_x{0.0F};
  float world_z{0.0F};
  float body_yaw_degrees{0.0F};
  bool valid{false};
  bool relocating{false};
};

struct SoldierTurnSmoothingInputs {

  float target_x{0.0F};
  float target_z{0.0F};

  float formation_yaw_degrees{0.0F};

  float dt{0.0F};

  float max_speed{2.5F};

  float turn_rate_degrees{540.0F};

  float snap_distance{7.0F};

  float settle_distance{0.06F};

  float relocate_distance{0.30F};

  bool allow_travel_yaw{true};
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
