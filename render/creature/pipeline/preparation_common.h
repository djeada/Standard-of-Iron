#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <optional>

#include "../pose_intent_enum.h"
#include "../render_request.h"
#include "creature_render_state.h"

namespace Engine::Core {
class UnitComponent;
}

namespace Render::GL {
struct DrawContext;
struct HumanoidAnimationContext;
struct HumanoidPose;
} // namespace Render::GL

namespace Render::Creature::Pipeline {

[[nodiscard]] auto
pass_intent_from_ctx(const Render::GL::DrawContext& ctx) noexcept -> RenderPassIntent;

[[nodiscard]] auto
derive_unit_seed(const Render::GL::DrawContext& ctx,
                 const Engine::Core::UnitComponent* unit) noexcept -> std::uint32_t;

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

[[nodiscard]] auto horse_clip_contact_y(std::uint16_t clip_id,
                                        float phase) noexcept -> std::optional<float>;

[[nodiscard]] auto
palette_contact_y(CreatureKind kind,
                  std::span<const QMatrix4x4> palette) noexcept -> float;

[[nodiscard]] auto sample_terrain_height_or_fallback(
    float world_x, float world_z, float fallback_y) noexcept -> float;

[[nodiscard]] auto model_world_origin(const QMatrix4x4& model) noexcept -> QVector3D;

[[nodiscard]] auto make_runtime_prewarm_ctx(const Render::GL::DrawContext& ctx) noexcept
    -> Render::GL::DrawContext;

void ground_model_contact_to_surface(QMatrix4x4& model,
                                     float local_contact_y,
                                     float y_scale = 1.0F,
                                     float entity_ground_offset = 0.0F) noexcept;

void ground_model_to_terrain(QMatrix4x4& model, float world_y_offset = 0.0F) noexcept;

void set_model_world_y(QMatrix4x4& model, float world_y) noexcept;

[[nodiscard]] inline auto fast_random(std::uint32_t& state) noexcept -> float {
  state = state * 1664525U + 1013904223U;
  return static_cast<float>(state & 0x7FFFFFU) / static_cast<float>(0x7FFFFFU);
}

[[nodiscard]] inline auto instance_seed(std::uint32_t base_seed,
                                        int index) noexcept -> std::uint32_t {
  return base_seed ^ static_cast<std::uint32_t>(index * 9176);
}

[[nodiscard]] inline auto
seeded_variant_index(std::uint32_t seed, std::uint8_t stride) noexcept -> std::uint8_t {
  if (stride == 0U) {
    return 0U;
  }

  seed ^= seed >> 16U;
  seed *= 0x7FEB352DU;
  seed ^= seed >> 15U;
  seed *= 0x846CA68BU;
  seed ^= seed >> 16U;
  return static_cast<std::uint8_t>(seed % stride);
}

} // namespace Render::Creature::Pipeline
