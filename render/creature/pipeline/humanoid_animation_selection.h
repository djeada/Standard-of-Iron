#pragma once

#include <cstdint>
#include <optional>

#include "../pose_intent.h"
#include "unit_visual_spec.h"

namespace Render::GL {
struct AnimationInputs;
struct HumanoidAnimationContext;
struct HumanoidVariant;
} // namespace Render::GL

namespace Render::Creature::Pipeline {

struct HumanoidPlaybackLayerSelection {
  Render::Creature::ArchetypeId archetype{Render::Creature::k_invalid_archetype};
  Render::Creature::AnimationStateId state{Render::Creature::AnimationStateId::Idle};
  float phase{0.0F};
  float weight{0.0F};
  std::uint8_t clip_variant{0U};
  Render::Creature::PlaybackLayerMode mode{Render::Creature::PlaybackLayerMode::None};

  [[nodiscard]] auto active() const noexcept -> bool {
    return mode != Render::Creature::PlaybackLayerMode::None &&
           archetype != Render::Creature::k_invalid_archetype && weight > 0.0F;
  }
};

struct HumanoidAnimationSelection {
  Render::Creature::ResolvedPose pose{};
  Render::Creature::ArchetypeId requested_archetype{
      Render::Creature::k_invalid_archetype};
  Render::Creature::ArchetypeId resolved_archetype{
      Render::Creature::k_invalid_archetype};
  Render::Creature::AnimationStateId state{Render::Creature::AnimationStateId::Idle};
  float phase{0.0F};
  std::uint8_t clip_variant{0U};
  std::optional<std::uint16_t> clip_id{};
  HumanoidPlaybackLayerSelection full_body_blend{};
  HumanoidPlaybackLayerSelection upper_body_overlay{};
  bool variant_table_changed{false};
};

[[nodiscard]] auto resolve_humanoid_animation_selection(
    const UnitVisualSpec& spec,
    const Render::GL::HumanoidAnimationContext& anim,
    std::uint32_t seed,
    const Render::GL::HumanoidVariant* variant = nullptr) noexcept
    -> HumanoidAnimationSelection;

[[nodiscard]] auto
finalize_visible_humanoid_spec(UnitVisualSpec spec,
                               const Render::GL::HumanoidVariant& variant,
                               const Render::GL::AnimationInputs& anim,
                               bool has_locomotion) -> UnitVisualSpec;

} // namespace Render::Creature::Pipeline
