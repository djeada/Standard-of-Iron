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

  float apparent_size_scale{1.0F};

  float projected_radius_px{-1.0F};

  float min_projected_radius_px{0.0F};

  bool selected{false};
  bool hovered{false};
  bool in_frustum{true};
  bool fog_visible{true};
  bool force_batching{false};
  bool never_batch{false};
};

inline constexpr float k_min_unit_projected_radius_px = 0.75F;

[[nodiscard]] constexpr auto select_lod(const LodInputs& in) noexcept -> LodTier {
  if (!in.in_frustum || !in.fog_visible) {
    return LodTier::Culled;
  }
  if (in.selected || in.hovered) {
    return LodTier::Full;
  }
  if (in.projected_radius_px >= 0.0F &&
      in.projected_radius_px < in.min_projected_radius_px) {
    return LodTier::Culled;
  }
  if (in.never_batch) {
    return LodTier::Full;
  }
  if (in.force_batching) {
    return LodTier::Simplified;
  }

  const float scale = in.apparent_size_scale > 0.0F ? in.apparent_size_scale : 1.0F;
  const float full_detail_budget_sq = in.full_detail_max_distance_sq * scale * scale;

  if (in.distance_sq > full_detail_budget_sq) {

    if (in.visible_unit_count > 420 || in.distance_sq > full_detail_budget_sq * 4.0F) {
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

inline constexpr int k_default_minimal_tier_individuals = 8;

[[nodiscard]] constexpr auto
representative_individual_count(float projected_radius_px) noexcept -> int {
  if (projected_radius_px < 0.0F || projected_radius_px >= 128.0F) {
    return k_default_minimal_tier_individuals;
  }
  if (projected_radius_px >= 56.0F) {
    return 6;
  }
  if (projected_radius_px >= 28.0F) {
    return 4;
  }
  if (projected_radius_px >= 14.0F) {
    return 2;
  }
  return 1;
}

} // namespace Render::Pipeline
