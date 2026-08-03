#pragma once

#include <cstdint>

namespace Game::Systems {

enum class ArrowVisualStyle : std::uint8_t {
  Focused,
  Volley,
  Marker,
  Javelin,
};

struct ArrowVisualProfile {
  float initial_progress{0.0F};
  float scale{1.0F};
  float length_scale{1.0F};
  float arc_scale{1.0F};
  float roll_deg{0.0F};
  float spin_rate_deg{0.0F};
  float trail_alpha{0.0F};
  float trail_length{0.0F};
  float brightness{1.0F};
};

[[nodiscard]] auto
make_arrow_visual_profile(ArrowVisualStyle style,
                          std::uint32_t sequence) -> ArrowVisualProfile;

} // namespace Game::Systems
