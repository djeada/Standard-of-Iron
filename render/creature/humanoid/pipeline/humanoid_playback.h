#pragma once

#include <cstdint>
#include <optional>

#include "animation/bpat/bpat_playback.h"
#include "render/creature/pose_intent_enum.h"
#include "render/creature/render_request.h"

namespace Render::GL {
struct HumanoidAnimationContext;
struct HumanoidPose;
} // namespace Render::GL

// Which clip, phase and variant a humanoid plays for its animation context,
// and where its feet meet the ground in that frame.

namespace Render::Creature::Pipeline {

[[nodiscard]] auto
humanoid_state_for_anim(const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> Render::Creature::AnimationStateId;

[[nodiscard]] auto humanoid_state_for_intent(
    Render::Creature::PoseIntent intent) noexcept -> Render::Creature::AnimationStateId;

[[nodiscard]] auto
humanoid_phase_for_state(const Render::GL::HumanoidAnimationContext& anim,
                         Render::Creature::AnimationStateId state) noexcept -> float;

[[nodiscard]] auto humanoid_phase_for_anim(
    const Render::GL::HumanoidAnimationContext& anim) noexcept -> float;

[[nodiscard]] auto
humanoid_idle_breath_phase_for_lod(float sample_time,
                                   std::uint32_t inst_seed,
                                   Render::Creature::CreatureLOD lod,
                                   bool template_prewarm) noexcept -> float;

[[nodiscard]] auto humanoid_clip_variant_for_state(
    Render::Creature::ArchetypeId archetype_id,
    const Render::GL::HumanoidAnimationContext& anim,
    Render::Creature::AnimationStateId state) noexcept -> std::uint8_t;

[[nodiscard]] auto humanoid_clip_variant_for_anim(
    Render::Creature::ArchetypeId archetype_id,
    const Render::GL::HumanoidAnimationContext& anim) noexcept -> std::uint8_t;

[[nodiscard]] auto humanoid_bpat_playback_for_anim(
    Render::Creature::ArchetypeId archetype_id,
    std::uint32_t species_id,
    const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> std::optional<BpatPlayback>;

[[nodiscard]] auto humanoid_clip_contact_y(
    Render::Creature::ArchetypeId archetype_id,
    std::uint32_t species_id,
    const Render::GL::HumanoidAnimationContext& anim) noexcept -> std::optional<float>;

[[nodiscard]] auto grounded_humanoid_contact_y(
    Render::Creature::ArchetypeId archetype_id,
    std::uint32_t species_id,
    const Render::GL::HumanoidPose& pose,
    const Render::GL::HumanoidAnimationContext& anim) noexcept -> float;

} // namespace Render::Creature::Pipeline
