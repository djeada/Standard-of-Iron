#pragma once

#include <cstdint>

namespace Game::Wildlife {

[[nodiscard]] inline auto next_random(std::uint32_t& state) -> float {
  state = (state * 1664525U) + 1013904223U;
  return static_cast<float>((state >> 8U) & 0xFFFFFFU) / 16777216.0F;
}

[[nodiscard]] inline auto
random_range(std::uint32_t& state, float low, float high) -> float {
  return low + ((high - low) * next_random(state));
}

} // namespace Game::Wildlife
