#pragma once

#include "pose_intent.h"
#include "render_request.h"

#include <array>
#include <cstdint>

namespace Render::Creature {

// Maximum number of visual variants (covers FacialHairStyle enum width).
inline constexpr std::size_t k_max_visual_variants = 8;

// Data-driven replacement for the legacy runtime hook pattern.
//
// Encodes two override paths:
//   1. Pose-indexed:  look up by resolve_pose_intent() result.
//   2. Variant-keyed: look up by seed % stride (seed-based tools) or
//                     facial_hair.style (appearance-based variants).
//
// Sentinels:
//   archetype entry == k_invalid_archetype → no override
//   state entry     == AnimationStateId::Count → no override
struct ArchetypeVariantTable {
  // Per-pose archetype and state overrides.
  std::array<ArchetypeId, pose_intent_count()> archetype_for_pose;
  std::array<AnimationStateId, pose_intent_count()> state_for_pose;

  // Variant-keyed selection parameters.
  //
  // variant_trigger_pose:
  //   PoseIntent::Count — always-on (apply for every pose)
  //   otherwise         — only apply when resolved pose == trigger_pose
  //
  // variant_stride == 0: variant lookup is disabled.
  //
  // variant_is_seed_based:
  //   true  — index = seed % variant_stride
  //   false — index = static_cast<size_t>(facial_hair.style),
  //           clamped to [0, stride - 1]
  PoseIntent variant_trigger_pose{PoseIntent::Count};
  std::uint8_t variant_stride{0};
  bool variant_is_seed_based{false};

  std::array<ArchetypeId, k_max_visual_variants> archetype_for_variant;
  std::array<AnimationStateId, k_max_visual_variants> state_for_variant;

  ArchetypeVariantTable() noexcept {
    archetype_for_pose.fill(k_invalid_archetype);
    state_for_pose.fill(AnimationStateId::Count);
    archetype_for_variant.fill(k_invalid_archetype);
    state_for_variant.fill(AnimationStateId::Count);
  }
};

} // namespace Render::Creature
