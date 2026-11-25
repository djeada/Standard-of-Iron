#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Game::Visuals {

struct HedgePlant {

  float x{0.0F};
  float z{0.0F};

  float scale{0.0F};

  auto operator==(const HedgePlant&) const -> bool = default;
};

inline constexpr int k_hedge_first_plants = 2;
inline constexpr int k_hedge_max_plants = 9;
inline constexpr int k_hedge_harvests_to_full = 7;

[[nodiscard]] constexpr auto farm_hedge_plant_count(int harvests) noexcept -> int {
  return std::clamp(
      k_hedge_first_plants + harvests, k_hedge_first_plants, k_hedge_max_plants);
}

[[nodiscard]] inline auto
farm_hedge_plants(int harvests, std::uint32_t seed) -> std::vector<HedgePlant> {

  constexpr float k_edge = 7.0F;

  constexpr std::array<std::array<float, 2>, k_hedge_max_plants> k_slots{{
      {-6.2F, -k_edge},
      {-k_edge, -4.6F},
      {-3.7F, -k_edge},
      {-1.2F, -k_edge},
      {-k_edge, -2.1F},
      {1.3F, -k_edge},
      {3.8F, -k_edge},
      {-k_edge, 0.4F},
      {6.3F, -k_edge},
  }};
  auto const roll = [seed](std::uint32_t salt) {
    std::uint32_t value = seed ^ (salt * 0x9E3779B9U);
    value ^= value >> 16U;
    value *= 0x7FEB352DU;
    value ^= value >> 15U;
    value *= 0x846CA68BU;
    value ^= value >> 16U;
    return static_cast<float>(value & 0xFFFFU) / 65535.0F;
  };

  std::vector<HedgePlant> plants;
  const int count = farm_hedge_plant_count(harvests);
  plants.reserve(static_cast<std::size_t>(count));
  for (int slot = 0; slot < count; ++slot) {
    const auto index = static_cast<std::uint32_t>(slot);
    const auto& at = k_slots[static_cast<std::size_t>(slot)];
    plants.push_back({.x = at[0] + (roll(index * 3U + 1U) - 0.5F) * 0.7F,
                      .z = at[1] + (roll(index * 3U + 2U) - 0.5F) * 0.7F,
                      .scale = 0.34F + roll(index * 3U + 3U) * 0.14F});
  }
  return plants;
}

} // namespace Game::Visuals
