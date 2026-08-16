#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace App::Core::GameSpeed {

inline constexpr std::array<float, 5> k_options{0.5F, 1.0F, 2.0F, 3.0F, 4.0F};

inline constexpr float k_default = 1.0F;
inline constexpr float k_min = k_options.front();
inline constexpr float k_max = k_options.back();

inline auto is_supported(float speed) -> bool {
  return std::any_of(k_options.begin(), k_options.end(), [speed](float option) {
    return std::abs(option - speed) < 0.001F;
  });
}

inline auto nearest_index(float speed) -> std::size_t {
  std::size_t best = 0;
  float best_distance = std::abs(k_options.front() - speed);
  for (std::size_t index = 1; index < k_options.size(); ++index) {
    const float distance = std::abs(k_options.at(index) - speed);
    if (distance < best_distance) {
      best_distance = distance;
      best = index;
    }
  }
  return best;
}

inline auto sanitize(float speed) -> float {
  if (!std::isfinite(speed)) {
    return k_default;
  }
  if (is_supported(speed)) {
    return speed;
  }
  return k_options.at(nearest_index(std::clamp(speed, k_min, k_max)));
}

inline auto stepped(float speed, int direction) -> float {
  const auto index = static_cast<long long>(nearest_index(sanitize(speed)));
  const auto count = static_cast<long long>(k_options.size());
  const long long next = std::clamp(index + direction, 0LL, count - 1);
  return k_options.at(static_cast<std::size_t>(next));
}

} // namespace App::Core::GameSpeed
