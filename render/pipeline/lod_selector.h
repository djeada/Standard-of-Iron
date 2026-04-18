// Stage 6 — LOD tiers as the frame-budget instrument.
//
// A single enum names the four possible outcomes of the pipeline's decision
// tree for each collected unit entry. Downstream code branches on this
// tier, never on raw distances / visible counts.
//
//   Culled:     entry is off-screen or fog-hidden -> emit nothing.
//   Full:       full detail (imperative rig, full LOD mask).
//   Simplified: mid-range: use batching path / reduced detail.
//   Minimal:    far / budget-starved: emit a capsule or skip shadow.
//
// Keeping this in a standalone header lets both unit tests and future pass
// / culling code reason about budget decisions without pulling in any of
// the scene_renderer's heavy ECS / Qt dependencies.

#pragma once

#include <cstdint>

namespace Render::Pipeline {

enum class LodTier : std::uint8_t {
  Culled = 0,
  Minimal = 1,
  Simplified = 2,
  Full = 3,
};

// The inputs the LOD selector needs. Kept as a POD so tests can construct
// it without the full ECS stack.
struct LodInputs {
  float distance_sq{0.0F};
  int visible_unit_count{0};
  float full_detail_max_distance_sq{900.0F}; // 30 units
  bool selected{false};
  bool hovered{false};
  bool in_frustum{true};
  bool fog_visible{true};
  bool force_batching{false};
  bool never_batch{false};
};

// Returns the LOD tier a single unit should render at this frame. Pure:
// depends only on the inputs, no side effects. Selected / hovered units
// never drop below Full, matching the old inline policy in
// scene_renderer::render_world.
[[nodiscard]] constexpr auto select_lod(const LodInputs &in) noexcept
    -> LodTier {
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
    // Far units: drop to Minimal once we're well past the batching ring
    // and the visible-unit pressure is high. Matches the heuristic that
    // used to live inline in render_world.
    if (in.visible_unit_count > 420 ||
        in.distance_sq > in.full_detail_max_distance_sq * 4.0F) {
      return LodTier::Minimal;
    }
    return LodTier::Simplified;
  }
  return LodTier::Full;
}

// Helper: given the current batching ratio and batching config, compute
// the squared distance threshold above which a unit switches from the full
// shader path to the batched path.
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
