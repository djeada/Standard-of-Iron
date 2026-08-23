#include "lod_decision.h"

namespace Render::Creature::Pipeline {

auto lod_reference_distance(float distance,
                            float apparent_size_scale) noexcept -> float {
  return apparent_size_scale > 0.0F ? distance / apparent_size_scale : distance;
}

auto select_distance_lod(float distance,
                         const LodDistanceThresholds& t) noexcept -> CreatureLOD {
  return select_distance_lod(distance, t, 1.0F);
}

auto select_distance_lod(float distance,
                         const LodDistanceThresholds& t,
                         float apparent_size_scale) noexcept -> CreatureLOD {
  if (lod_reference_distance(distance, apparent_size_scale) < t.full) {
    return CreatureLOD::Full;
  }
  if (distance < t.cull) {
    return CreatureLOD::Minimal;
  }
  return CreatureLOD::Culled;
}

auto decide_creature_lod(const CreatureLodDecisionInputs& in) noexcept
    -> CreatureLodDecision {
  CreatureLodDecision out{};

  if (in.forced_lod.has_value()) {
    out.lod = *in.forced_lod;
    return out;
  }

  if (!in.has_camera) {
    out.lod = CreatureLOD::Full;
    return out;
  }

  out.lod = select_distance_lod(in.distance, in.thresholds, in.apparent_size_scale);

  if (out.lod == CreatureLOD::Culled) {
    out.culled = true;
    out.reason = CullReason::Distance;
    return out;
  }

  if (in.apply_visibility_budget && out.lod == CreatureLOD::Full &&
      !in.budget_grant_full) {
    out.lod = CreatureLOD::Minimal;
  }

  return out;
}

} // namespace Render::Creature::Pipeline
