#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <utility>

namespace Game::Systems {

[[nodiscard]] inline auto planar_length(float x, float z) noexcept -> float {
  return std::sqrt((x * x) + (z * z));
}

[[nodiscard]] inline auto yaw_degrees_from_direction(float dx,
                                                     float dz) noexcept -> float {
  return std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
}

[[nodiscard]] inline auto signed_yaw_delta(float from_degrees,
                                           float to_degrees) noexcept -> float {

  return std::remainder(to_degrees - from_degrees, 360.0F);
}

[[nodiscard]] inline auto turn_yaw_toward(float current_degrees,
                                          float target_degrees,
                                          float max_step_degrees) noexcept -> float {
  float const delta = signed_yaw_delta(current_degrees, target_degrees);
  float const step = std::max(0.0F, max_step_degrees);
  return current_degrees + std::clamp(delta, -step, step);
}

struct PlanarPoint {
  float x{0.0F};
  float z{0.0F};
};

struct SegmentSpan {
  float enter{0.0F};
  float exit{1.0F};
};

enum class BoxEdges : std::uint8_t {
  Solid,
  Open
};

[[nodiscard]] inline auto segment_box_span(float start_x,
                                           float start_z,
                                           float delta_x,
                                           float delta_z,
                                           float half_x,
                                           float half_z,
                                           BoxEdges edges = BoxEdges::Solid) noexcept
    -> std::optional<SegmentSpan> {
  constexpr float k_parallel = 1.0e-5F;
  bool const solid = edges == BoxEdges::Solid;
  SegmentSpan span;
  auto const slab = [&span, solid](float start, float delta, float half) {
    if (std::abs(delta) <= k_parallel) {
      return solid ? std::abs(start) <= half : std::abs(start) < half;
    }
    float t0 = (-half - start) / delta;
    float t1 = (half - start) / delta;
    if (t0 > t1) {
      std::swap(t0, t1);
    }
    span.enter = std::max(span.enter, t0);
    span.exit = std::min(span.exit, t1);
    return solid ? span.enter <= span.exit : span.enter < span.exit;
  };
  if (!slab(start_x, delta_x, half_x) || !slab(start_z, delta_z, half_z)) {
    return std::nullopt;
  }
  return span;
}

[[nodiscard]] inline auto
segment_box_entry(PlanarPoint from, PlanarPoint to, float half_x, float half_z) noexcept
    -> std::optional<float> {
  auto const span = segment_box_span(
      from.x, from.z, to.x - from.x, to.z - from.z, half_x, half_z, BoxEdges::Open);
  if (!span.has_value()) {
    return std::nullopt;
  }
  return span->enter;
}

template <typename Usable>
[[nodiscard]] auto box_detour_corner(PlanarPoint from,
                                     PlanarPoint to,
                                     float half_x,
                                     float half_z,
                                     float margin,
                                     Usable&& usable) -> std::optional<PlanarPoint> {
  constexpr float k_skin = 0.02F;
  constexpr float k_reached = 0.09F;
  float const skin_x = half_x - k_skin;
  float const skin_z = half_z - k_skin;
  const PlanarPoint corners[4] = {{-half_x - margin, -half_z - margin},
                                  {half_x + margin, -half_z - margin},
                                  {half_x + margin, half_z + margin},
                                  {-half_x - margin, half_z + margin}};
  auto const blocked = [skin_x, skin_z](PlanarPoint a, PlanarPoint b) {
    return segment_box_entry(a, b, skin_x, skin_z).has_value();
  };
  auto const span = [](PlanarPoint a, PlanarPoint b) {
    return planar_length(b.x - a.x, b.z - a.z);
  };
  std::optional<PlanarPoint> best;
  float best_cost = 0.0F;
  for (int index = 0; index < 4; ++index) {
    PlanarPoint const corner = corners[index];
    float const reach = span(from, corner);
    if (reach < k_reached || blocked(from, corner) || !usable(corner)) {
      continue;
    }
    float onward = span(corner, to);
    if (blocked(corner, to)) {
      PlanarPoint const next = corners[(index + 1) % 4];
      PlanarPoint const prior = corners[(index + 3) % 4];
      onward = std::min(span(corner, next) + span(next, to),
                        span(corner, prior) + span(prior, to));
    }
    if (!best.has_value() || reach + onward < best_cost) {
      best = corner;
      best_cost = reach + onward;
    }
  }
  return best;
}

} // namespace Game::Systems
