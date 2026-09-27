#pragma once

namespace Render::GL {

enum class BuildingState {
  Normal,
  Damaged,
  Destroyed
};

inline constexpr float HEALTH_THRESHOLD_NORMAL = 0.70F;
inline constexpr float HEALTH_THRESHOLD_DAMAGED = 0.30F;

inline constexpr float HEALTH_STATE_HYSTERESIS = 0.04F;

inline auto get_building_state(float health_ratio) -> BuildingState {
  if (health_ratio >= HEALTH_THRESHOLD_NORMAL) {
    return BuildingState::Normal;
  } else if (health_ratio >= HEALTH_THRESHOLD_DAMAGED) {
    return BuildingState::Damaged;
  } else {
    return BuildingState::Destroyed;
  }
}

inline auto get_building_state(float health_ratio,
                               BuildingState shown) -> BuildingState {
  BuildingState const raw = get_building_state(health_ratio);
  if (raw == shown || health_ratio <= 0.0F) {
    return raw;
  }

  float const margin = raw > shown ? -HEALTH_STATE_HYSTERESIS : HEALTH_STATE_HYSTERESIS;
  return get_building_state(health_ratio - margin);
}

} // namespace Render::GL
