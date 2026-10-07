#include "siege_wreck_manifest.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Animation {

namespace {

struct WreckShape {
  float roll_degrees;
  float pitch_degrees;
  float drop;
};

constexpr float k_tip_end = 0.72F;
constexpr float k_settle_bounce = 0.07F;
constexpr float k_full_char = 0.55F;

auto shape_for(SiegeWreckKind kind) noexcept -> WreckShape {
  switch (kind) {
  case SiegeWreckKind::Ballista:
    return {28.0F, 9.0F, 0.08F};
  case SiegeWreckKind::Ram:
    return {15.0F, 4.0F, 0.14F};
  case SiegeWreckKind::Tower:
    return {74.0F, 8.0F, 0.0F};
  case SiegeWreckKind::Catapult:
    break;
  }
  return {21.0F, 6.0F, 0.10F};
}

auto smoothstep01(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

auto tip_curve(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  if (t < k_tip_end) {
    float const falling = t / k_tip_end;
    return falling * falling;
  }
  float const settle = (t - k_tip_end) / (1.0F - k_tip_end);
  return 1.0F - (k_settle_bounce * std::sin(std::numbers::pi_v<float> * settle) *
                 (1.0F - settle));
}

auto hash_bits(std::uint32_t value) noexcept -> std::uint32_t {
  value ^= value >> 16U;
  value *= 0x7FEB352DU;
  value ^= value >> 15U;
  value *= 0x846CA68BU;
  value ^= value >> 16U;
  return value;
}

} // namespace

auto resolve_siege_wreck_pose(SiegeWreckKind kind,
                              float collapse,
                              std::uint32_t seed) noexcept -> SiegeWreckPose {
  WreckShape const shape = shape_for(kind);
  std::uint32_t const bits = hash_bits(seed);
  float const side = (bits & 1U) != 0U ? 1.0F : -1.0F;
  float const pitch_sign = (bits & 2U) != 0U ? 1.0F : -1.0F;
  float const t = std::clamp(collapse, 0.0F, 1.0F);
  float const settled = smoothstep01(t);
  return {.roll_degrees = side * shape.roll_degrees * tip_curve(t),
          .pitch_degrees = pitch_sign * shape.pitch_degrees * settled,
          .drop = shape.drop * settled,
          .side = side,
          .char_amount = k_full_char * settled};
}

auto siege_crew_fall_delay(std::uint32_t member_index) noexcept -> float {
  return 0.12F * static_cast<float>(member_index) +
         0.08F * static_cast<float>(hash_bits(member_index + 17U) % 5U);
}

} // namespace Animation
