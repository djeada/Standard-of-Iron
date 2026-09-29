#pragma once

#include <algorithm>
#include <cmath>

namespace Game::Audio::Mastering {

inline constexpr float PI = 3.14159265358979323846F;
inline constexpr float SILENCE = 1e-12F;

inline auto to_db(float linear) -> float {
  return 20.0F * std::log10(std::max(linear, SILENCE));
}

} // namespace Game::Audio::Mastering
