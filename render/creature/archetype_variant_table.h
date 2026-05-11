#pragma once

#include "pose_intent.h"
#include "render_request.h"

#include <array>
#include <cstdint>

namespace Render::Creature {

inline constexpr std::size_t k_max_visual_variants = 8;

struct ArchetypeVariantTable {

  std::array<ArchetypeId, pose_intent_count()> archetype_for_pose;
  std::array<AnimationStateId, pose_intent_count()> state_for_pose;

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
