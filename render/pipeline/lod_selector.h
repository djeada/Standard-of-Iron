

#pragma once

#include <cstdint>

namespace Render::Pipeline {

enum class LodTier : std::uint8_t {
  Culled = 0,
  Minimal = 1,
  Simplified = 2,
  Full = 3,
};

struct LodInputs {
  float distance_sq{0.0F};
  int visible_unit_count{0};
  float full_detail_max_distance_sq{900.0F};
  bool selected{false};
  bool hovered{false};
  bool in_frustum{true};
  bool fog_visible{true};
  bool force_batching{false};
  bool never_batch{false};
};

[[nodiscard]] constexpr auto
select_lod(const LodInputs &in) noexcept -> LodTier {
  if (!in.in_frustum || !in.fog_visible) {
    return LodTier::Culled;
  }
  if (in.selected || in.hovered) {
    return LodTier::Full;
  }
  if (in.never_batch) {
    return LodTier::Full;
  }
  if (in.force_batching) {
    return LodTier::Simplified;
  }
  if (in.distance_sq > in.full_detail_max_distance_sq) {

    if (in.visible_unit_count > 420 ||
        in.distance_sq > in.full_detail_max_distance_sq * 4.0F) {
      return LodTier::Minimal;
    }
    return LodTier::Simplified;
  }
  return LodTier::Full;
}

[[nodiscard]] constexpr auto
compute_full_detail_max_distance_sq(float batching_ratio,
                                    bool force_batching) noexcept -> float {
  if (force_batching) {
    return 0.0F;
  }
  const float d = 30.0F * (1.0F - batching_ratio * 0.7F);
  return d * d;
}

} // namespace Render::Pipeline
