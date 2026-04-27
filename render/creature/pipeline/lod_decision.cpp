#include "lod_decision.h"

namespace Render::Creature::Pipeline {

auto select_distance_lod(
    float distance, const LodDistanceThresholds &t) noexcept -> CreatureLOD {
  if (distance < t.full) {
    return CreatureLOD::Full;
  }
  if (distance < t.minimal) {
    return CreatureLOD::Minimal;
  }
  return CreatureLOD::Billboard;
}

auto should_render_temporal(std::uint32_t frame, std::uint32_t seed,
                            std::uint32_t period) noexcept -> bool {
  if (period <= 1) {
    return true;
  }
  return ((frame + seed) % period) == 0U;
}

auto decide_creature_lod(const CreatureLodDecisionInputs &in) noexcept
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

  out.lod = select_distance_lod(in.distance, in.thresholds);

  if (out.lod == CreatureLOD::Billboard) {
    out.culled = true;
    out.reason = CullReason::Billboard;
    return out;
  }

  if (in.apply_visibility_budget && out.lod == CreatureLOD::Full &&
      !in.budget_grant_full) {
    out.lod = CreatureLOD::Minimal;
  }

  if (out.lod == CreatureLOD::Minimal &&
      in.distance > in.temporal.distance_minimal) {
    if (!should_render_temporal(in.frame_index, in.instance_seed,
                                in.temporal.period_minimal)) {
      out.culled = true;
      out.reason = CullReason::Temporal;
    }
  }

  return out;
}

} // namespace Render::Creature::Pipeline
