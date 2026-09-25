#pragma once

#include <cmath>
#include <cstdint>

namespace Game::Map {

inline constexpr float k_forest_outline_reach = 1.2F;

[[nodiscard]] inline auto forest_outline_seed(float x, float z) -> float {
  auto const ix =
      static_cast<std::uint32_t>(static_cast<std::int32_t>(std::lround(x * 4.0F)));
  auto const iz =
      static_cast<std::uint32_t>(static_cast<std::int32_t>(std::lround(z * 4.0F)));
  std::uint32_t h = (ix * 73856093U) ^ (iz * 19349663U);
  h ^= h >> 13U;
  h *= 0x5bd1e995U;
  h ^= h >> 15U;
  return static_cast<float>(h & 0xFFFFU) / 65536.0F;
}

[[nodiscard]] inline auto
forest_outline_scale(float seed, float dx, float dz) -> float {
  constexpr float k_tau = 6.2831853F;
  float const angle = std::atan2(dz, dx);
  return 1.0F + (0.10F * std::sin((2.0F * angle) + (seed * k_tau))) +
         (0.06F * std::sin((3.0F * angle) + (seed * 2.0F * k_tau) + 1.3F)) +
         (0.04F * std::sin((5.0F * angle) + (seed * 3.0F * k_tau) + 2.1F));
}

} // namespace Game::Map
