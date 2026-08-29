#pragma once

#include <algorithm>
#include <cmath>

#include "resource_types.h"

namespace Game::Systems {

inline constexpr float k_stockpile_center_x = 5.20F;
inline constexpr float k_stockpile_center_z = 0.0F;
inline constexpr float k_stockpile_half_width = 1.45F;
inline constexpr float k_stockpile_half_depth = 2.10F;

inline constexpr float k_stockpile_drop_x = 5.95F;
inline constexpr float k_stockpile_drop_z = 0.0F;

inline constexpr float k_stockpile_drop_radius = 2.20F;

inline constexpr float k_stockpile_haul_patience_seconds = 30.0F;

inline constexpr float k_stockpile_haul_abandon_seconds = 75.0F;
inline constexpr float k_stockpile_depot_arrival_radius = 4.50F;

inline constexpr int k_stockpile_wood_display_cap = 640;
inline constexpr int k_stockpile_stone_display_cap = 480;
inline constexpr int k_stockpile_iron_display_cap = 480;
inline constexpr int k_stockpile_food_display_cap = 400;

inline constexpr float k_stockpile_deposit_flash_seconds = 1.1F;

struct StockpilePoint {
  float x{0.0F};
  float z{0.0F};
};

[[nodiscard]] inline auto stockpile_local_to_world(float center_x,
                                                   float center_z,
                                                   float yaw_degrees,
                                                   float local_x,
                                                   float local_z) -> StockpilePoint {
  constexpr float k_deg_to_rad = 3.14159265358979323846F / 180.0F;
  float const yaw = yaw_degrees * k_deg_to_rad;
  float const cos_yaw = std::cos(yaw);
  float const sin_yaw = std::sin(yaw);
  return {center_x + (local_x * cos_yaw) + (local_z * sin_yaw),
          center_z - (local_x * sin_yaw) + (local_z * cos_yaw)};
}

[[nodiscard]] inline auto stockpile_drop_point(float center_x,
                                               float center_z,
                                               float yaw_degrees) -> StockpilePoint {
  return stockpile_local_to_world(
      center_x, center_z, yaw_degrees, k_stockpile_drop_x, k_stockpile_drop_z);
}

[[nodiscard]] inline auto stockpile_display_cap(ResourceType type) -> int {
  switch (type) {
  case ResourceType::Wood:
    return k_stockpile_wood_display_cap;
  case ResourceType::Stone:
    return k_stockpile_stone_display_cap;
  case ResourceType::Iron:
    return k_stockpile_iron_display_cap;
  case ResourceType::Food:
    return k_stockpile_food_display_cap;
  default:
    return k_stockpile_wood_display_cap;
  }
}

[[nodiscard]] inline auto stockpile_fill_ratio(int stored_amount,
                                               ResourceType type) -> float {
  auto const cap = static_cast<float>(std::max(1, stockpile_display_cap(type)));
  return std::clamp(static_cast<float>(stored_amount) / cap, 0.0F, 1.0F);
}

} // namespace Game::Systems
