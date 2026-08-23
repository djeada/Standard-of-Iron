#pragma once

namespace Render::GL {

enum class BuildingState {
  Normal,
  Damaged,
  Destroyed
};

inline constexpr float HEALTH_THRESHOLD_NORMAL = 0.70F;
inline constexpr float HEALTH_THRESHOLD_DAMAGED = 0.30F;

inline auto get_building_state(float health_ratio) -> BuildingState {
  if (health_ratio >= HEALTH_THRESHOLD_NORMAL) {
    return BuildingState::Normal;
  } else if (health_ratio >= HEALTH_THRESHOLD_DAMAGED) {
    return BuildingState::Damaged;
  } else {
    return BuildingState::Destroyed;
  }
}

} // namespace Render::GL
