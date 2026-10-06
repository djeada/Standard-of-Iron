#pragma once

#include <cstdint>

namespace Animation {

inline constexpr float k_siege_wreck_collapse_seconds = 1.6F;
inline constexpr float k_siege_wreck_hold_seconds = 9.0F;
inline constexpr float k_siege_wreck_sink_seconds = 2.6F;

enum class SiegeWreckKind : std::uint8_t {
  Catapult,
  Ballista,
  Ram,
  Tower,
};

struct SiegeWreckPose {
  float roll_degrees{0.0F};
  float pitch_degrees{0.0F};
  float drop{0.0F};
  float side{1.0F};
  float char_amount{0.0F};
};

[[nodiscard]] auto resolve_siege_wreck_pose(
    SiegeWreckKind kind, float collapse, std::uint32_t seed) noexcept -> SiegeWreckPose;

[[nodiscard]] auto siege_crew_fall_delay(std::uint32_t member_index) noexcept -> float;

inline constexpr float k_siege_crew_fall_seconds = 1.1F;

} // namespace Animation
