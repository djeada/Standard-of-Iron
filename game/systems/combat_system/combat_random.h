#pragma once

#include <cstdint>

namespace Game::Systems::Combat {

[[nodiscard]] inline auto hash_to_unit(std::uint32_t value) noexcept -> float {
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  value ^= value >> 16U;
  return static_cast<float>(value & 0x00FFFFFFU) / static_cast<float>(0x00FFFFFFU);
}

[[nodiscard]] inline auto
deterministic_unit_roll(std::uint32_t seed, std::uint32_t salt) noexcept -> float {
  return hash_to_unit(seed ^ (salt * 0x9E3779B9U));
}

[[nodiscard]] inline auto deterministic_range(std::uint32_t seed,
                                              std::uint32_t salt,
                                              float min_value,
                                              float max_value) noexcept -> float {
  return min_value + deterministic_unit_roll(seed, salt) * (max_value - min_value);
}

} // namespace Game::Systems::Combat
