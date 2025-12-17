#pragma once

namespace Render::GL {

enum class BuildingState { Normal, Damaged, Destroyed };

inline auto get_building_state(float health_ratio) -> BuildingState {
  if (health_ratio >= 0.70F) {
    return BuildingState::Normal;
  } else if (health_ratio >= 0.30F) {
    return BuildingState::Damaged;
  } else {
    return BuildingState::Destroyed;
  }
}

} // namespace Render::GL
