#pragma once

namespace Render::GL {

inline constexpr float k_health_bar_recent_damage_seconds = 3.0F;

inline constexpr float k_health_bar_max_camera_distance = 55.0F;

struct HealthBarVisibilityInputs {
  bool alive = true;
  bool selected = false;
  bool hovered = false;
  bool recently_damaged = false;
  bool commander_target = false;
  bool full_health = true;
  float camera_distance = 0.0F;
};

[[nodiscard]] auto health_bar_visible(const HealthBarVisibilityInputs& inputs) -> bool;

} // namespace Render::GL
