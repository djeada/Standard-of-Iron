#pragma once

#include <array>
#include <cstdint>

namespace Render::Creature::Quadruped {

enum class LegId : std::uint8_t {
  FrontLeft = 0,
  FrontRight = 1,
  BackLeft = 2,
  BackRight = 3,
};

inline constexpr std::array<LegId, 4> kAllLegs{
    LegId::FrontLeft, LegId::FrontRight, LegId::BackLeft, LegId::BackRight};

struct Dimensions {
  float body_width{1.0F};
  float body_height{1.0F};
  float body_length{1.0F};
  float barrel_center_y{0.0F};

  float leg_length{1.0F};
  float upper_leg_length{0.5F};
  float lower_leg_length{0.5F};
  float leg_radius{0.1F};

  float neck_length{0.2F};
  float head_width{0.3F};
  float head_height{0.3F};
  float head_length{0.3F};

  float appendage_length{0.0F};
  float appendage_base_radius{0.0F};
  float appendage_tip_radius{0.0F};

  float idle_bob_amplitude{0.01F};
  float move_bob_amplitude{0.03F};
};

[[nodiscard]] constexpr auto leg_is_front(LegId leg) noexcept -> bool {
  return leg == LegId::FrontLeft || leg == LegId::FrontRight;
}

[[nodiscard]] constexpr auto leg_is_left(LegId leg) noexcept -> bool {
  return leg == LegId::FrontLeft || leg == LegId::BackLeft;
}

[[nodiscard]] constexpr auto leg_lateral_sign(LegId leg) noexcept -> float {
  return leg_is_left(leg) ? 1.0F : -1.0F;
}

[[nodiscard]] constexpr auto leg_forward_sign(LegId leg) noexcept -> float {
  return leg_is_front(leg) ? 1.0F : -1.0F;
}

} // namespace Render::Creature::Quadruped
