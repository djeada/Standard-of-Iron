#pragma once

#include <cmath>

namespace Game::Systems {

[[nodiscard]] inline auto planar_length(float x, float z) noexcept -> float {
  return std::sqrt((x * x) + (z * z));
}

} // namespace Game::Systems
