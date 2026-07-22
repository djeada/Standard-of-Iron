#include "preparation_common.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../../../game/core/component.h"
#include "../../../game/core/entity.h"
#include "../../../game/map/terrain_service.h"
#include "../../elephant/elephant_spec.h"
#include "../../entity/registry.h"
#include "../../gl/humanoid/humanoid_types.h"
#include "../../horse/horse_spec.h"
#include "../../humanoid/humanoid_spec.h"
#include "../../humanoid/skeleton.h"
#include "../archetype_registry.h"
#include "../bpat/bpat_registry.h"
#include "../pose_intent.h"
#include "animation/clip_manifest.h"
#include "animation/playback_manifest.h"

namespace Render::Creature::Pipeline {

auto pass_intent_from_ctx(const Render::GL::DrawContext& ctx) noexcept
    -> RenderPassIntent {
  return ctx.template_prewarm ? RenderPassIntent::Shadow : RenderPassIntent::Main;
}

auto derive_unit_seed(const Render::GL::DrawContext& ctx,
                      const Engine::Core::UnitComponent* unit) noexcept
    -> std::uint32_t {
  if (ctx.has_seed_override) {
    return ctx.seed_override;
  }
  std::uint32_t seed = 0U;
  if (unit != nullptr) {
    seed ^= static_cast<std::uint32_t>(unit->owner_id * 2654435761U);
  }
  if (ctx.entity != nullptr) {
    seed ^= ctx.entity->get_id() * 2246822519U;
  }
  return seed;
}

auto resolved_humanoid_pose(const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> Render::Creature::ResolvedPose {
  return Render::Creature::resolve_pose(anim.inputs);
}

auto humanoid_state_for_intent(Render::Creature::PoseIntent intent) noexcept
    -> Render::Creature::AnimationStateId {
  return Render::Creature::animation_state_for_intent(intent);
}

auto humanoid_state_for_anim(const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> Render::Creature::AnimationStateId {
  return resolved_humanoid_pose(anim).animation_state;
}

namespace {

auto humanoid_playback_phase_inputs_for_state(
    const Render::GL::HumanoidAnimationContext& anim,
    Render::Creature::AnimationStateId state) noexcept
    -> Animation::HumanoidPlaybackPhaseInputs {
  return {
      .state = state,
      .is_mounted = anim.inputs.is_mounted,
      .is_attacking = anim.inputs.is_attacking,
      .is_melee = anim.inputs.is_melee,
      .movement_state = anim.inputs.movement_state,
      .is_constructing = anim.inputs.is_constructing,
      .construction_role = anim.construction_role,
      .construction_progress = anim.inputs.construction_progress,
      .construction_jitter_seed = anim.jitter_seed,
      .is_in_hold_mode = anim.inputs.is_in_hold_mode,
      .is_exiting_hold = anim.inputs.is_exiting_hold,
      .hold_entry_progress = anim.inputs.hold_entry_progress,
      .hold_exit_progress = anim.inputs.hold_exit_progress,
      .is_guarding = anim.inputs.is_guarding,
      .is_exiting_guard = anim.inputs.is_exiting_guard,
      .guard_pose_progress = anim.inputs.guard_pose_progress,
      .death_progress = anim.inputs.death_progress,
      .attack_phase = anim.attack_phase,
      .ambient_idle = anim.ambient_idle_type,
      .ambient_idle_phase = anim.ambient_idle_phase,
      .gait_cycle_phase = anim.gait.cycle_phase,
  };
}

} // namespace

auto humanoid_phase_for_state(const Render::GL::HumanoidAnimationContext& anim,
                              Render::Creature::AnimationStateId state) noexcept
    -> float {
  return Animation::resolve_humanoid_playback_phase(
      humanoid_playback_phase_inputs_for_state(anim, state));
}

namespace {

auto default_humanoid_archetype(Render::Creature::ArchetypeId archetype_id) noexcept
    -> Render::Creature::ArchetypeId {
  return (archetype_id != Render::Creature::k_invalid_archetype)
             ? archetype_id
             : Render::Creature::ArchetypeRegistry::k_humanoid_base;
}

auto humanoid_variant_inputs_for_state(const Render::GL::HumanoidAnimationContext& anim,
                                       Render::Creature::AnimationStateId state,
                                       std::uint8_t available_variant_count) noexcept
    -> Animation::HumanoidClipVariantInputs {
  return {
      .state = state,
      .is_constructing = anim.inputs.is_constructing,
      .construction_role = anim.construction_role,
      .construction_jitter_seed = anim.jitter_seed,
      .death_variant = anim.inputs.death_variant,
      .attack_variant = anim.inputs.attack_variant,
      .ambient_idle = anim.ambient_idle_type,
      .available_variant_count = available_variant_count,
  };
}

auto humanoid_clip_matches_requested_idle_variant(
    const Render::Creature::Bpat::BpatBlob& blob,
    std::uint16_t clip_id,
    Render::Creature::AnimationStateId state,
    std::uint8_t clip_variant) noexcept -> bool {
  if (state != Render::Creature::AnimationStateId::Idle || clip_variant == 0U) {
    return true;
  }
  if (clip_id >= blob.clip_count()) {
    return false;
  }
  return blob.clip(clip_id).name ==
         Animation::humanoid_idle_variant_clip_name(clip_variant);
}

} // namespace

auto humanoid_clip_variant_for_state(Render::Creature::ArchetypeId archetype_id,
                                     const Render::GL::HumanoidAnimationContext& anim,
                                     Render::Creature::AnimationStateId state) noexcept
    -> std::uint8_t {
  auto const resolved_archetype = default_humanoid_archetype(archetype_id);
  auto const variant_count =
      Render::Creature::ArchetypeRegistry::instance().clip_variant_count(
          resolved_archetype, state);
  if (variant_count <= 1U) {
    return 0U;
  }
  return Animation::resolve_humanoid_clip_variant(
      humanoid_variant_inputs_for_state(anim, state, variant_count));
}

auto humanoid_phase_for_anim(const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> float {
  return humanoid_phase_for_state(anim, humanoid_state_for_anim(anim));
}

auto humanoid_clip_variant_for_anim(
    Render::Creature::ArchetypeId archetype_id,
    const Render::GL::HumanoidAnimationContext& anim) noexcept -> std::uint8_t {
  return humanoid_clip_variant_for_state(
      archetype_id, anim, humanoid_state_for_anim(anim));
}

auto humanoid_bpat_playback_for_anim(Render::Creature::ArchetypeId archetype_id,
                                     std::uint32_t species_id,
                                     const Render::GL::HumanoidAnimationContext&
                                         anim) noexcept -> std::optional<BpatPlayback> {
  using Render::Creature::ArchetypeDescriptor;

  archetype_id = default_humanoid_archetype(archetype_id);

  auto const resolved_pose = resolved_humanoid_pose(anim);
  auto const state = resolved_pose.animation_state;
  auto clip_variant = humanoid_clip_variant_for_state(archetype_id, anim, state);
  auto clip_id = Render::Creature::ArchetypeRegistry::instance().resolve_bpat_clip(
      archetype_id, state, clip_variant);
  if (clip_id == ArchetypeDescriptor::k_unmapped_clip) {
    return std::nullopt;
  }
  auto const* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);
  if (blob == nullptr) {
    return std::nullopt;
  }

  if (clip_id >= blob->clip_count() || !humanoid_clip_matches_requested_idle_variant(
                                           *blob, clip_id, state, clip_variant)) {
    clip_variant = 0U;
    clip_id = Render::Creature::ArchetypeRegistry::instance().resolve_bpat_clip(
        archetype_id, state, clip_variant);
    if (clip_id == ArchetypeDescriptor::k_unmapped_clip ||
        clip_id >= blob->clip_count()) {
      return std::nullopt;
    }
  }

  auto const playback =
      resolve_bpat_playback(blob, clip_id, humanoid_phase_for_state(anim, state));
  if (!playback.valid()) {
    return std::nullopt;
  }

  return BpatPlayback{playback.clip_id,
                      static_cast<std::uint16_t>(playback.frame_in_clip)};
}

auto humanoid_clip_contact_y(Render::Creature::ArchetypeId archetype_id,
                             std::uint32_t species_id,
                             const Render::GL::HumanoidAnimationContext& anim) noexcept
    -> std::optional<float> {
  auto const playback = humanoid_bpat_playback_for_anim(archetype_id, species_id, anim);
  if (!playback.has_value()) {
    return std::nullopt;
  }

  auto const* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);
  if (blob == nullptr || playback->clip_id >= blob->clip_count()) {
    return std::nullopt;
  }

  auto const clip = blob->clip(playback->clip_id);
  if (clip.frame_count == 0U) {
    return std::nullopt;
  }

  auto const palette =
      blob->frame_palette_view(clip.frame_offset + playback->frame_in_clip);
  if (palette.size() < Render::Humanoid::k_bone_count) {
    return std::nullopt;
  }
  auto const foot_l_idx =
      static_cast<std::size_t>(Render::Humanoid::HumanoidBone::FootL);
  auto const foot_r_idx =
      static_cast<std::size_t>(Render::Humanoid::HumanoidBone::FootR);
  return std::min(palette[foot_l_idx].column(3).y(), palette[foot_r_idx].column(3).y());
}

auto grounded_humanoid_contact_y(
    Render::Creature::ArchetypeId archetype_id,
    std::uint32_t species_id,
    const Render::GL::HumanoidPose& pose,
    const Render::GL::HumanoidAnimationContext& anim) noexcept -> float {
  if (auto const clip_contact = humanoid_clip_contact_y(archetype_id, species_id, anim);
      clip_contact.has_value()) {
    return *clip_contact;
  }
  return std::min(pose.foot_l.y(), pose.foot_r.y());
}

auto horse_clip_contact_y(std::uint16_t clip_id,
                          float phase) noexcept -> std::optional<float> {
  auto const* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(
      Render::Creature::Bpat::k_species_horse);
  if (blob == nullptr || clip_id >= blob->clip_count()) {
    return std::nullopt;
  }

  auto const playback = resolve_bpat_playback(blob, clip_id, phase);
  if (!playback.valid()) {
    return std::nullopt;
  }

  auto const palette = blob->frame_palette_view(playback.global_frame);
  if (palette.size() < Render::Horse::k_horse_bone_count) {
    return std::nullopt;
  }

  auto const foot_fl_idx = static_cast<std::size_t>(Render::Horse::HorseBone::FootFL);
  auto const foot_fr_idx = static_cast<std::size_t>(Render::Horse::HorseBone::FootFR);
  auto const foot_bl_idx = static_cast<std::size_t>(Render::Horse::HorseBone::FootBL);
  auto const foot_br_idx = static_cast<std::size_t>(Render::Horse::HorseBone::FootBR);
  return std::min(
      std::min(palette[foot_fl_idx].column(3).y(), palette[foot_fr_idx].column(3).y()),
      std::min(palette[foot_bl_idx].column(3).y(), palette[foot_br_idx].column(3).y()));
}

auto palette_contact_y(CreatureKind kind,
                       std::span<const QMatrix4x4> palette) noexcept -> float {
  auto const bind_adjusted_y = [&](std::size_t index,
                                   std::span<const QMatrix4x4> bind_palette) -> float {
    if (index >= palette.size() || index >= bind_palette.size()) {
      return 0.0F;
    }
    return (palette[index] * bind_palette[index].inverted()).column(3).y();
  };

  switch (kind) {
  case CreatureKind::Humanoid: {
    auto const bind_palette = Render::Humanoid::humanoid_bind_palette();
    return std::min(
        bind_adjusted_y(static_cast<std::size_t>(Render::Humanoid::HumanoidBone::FootL),
                        bind_palette),
        bind_adjusted_y(static_cast<std::size_t>(Render::Humanoid::HumanoidBone::FootR),
                        bind_palette));
  }
  case CreatureKind::Horse: {
    auto const bind_palette = Render::Horse::horse_bind_palette();
    return std::min(
        std::min(
            bind_adjusted_y(static_cast<std::size_t>(Render::Horse::HorseBone::FootFL),
                            bind_palette),
            bind_adjusted_y(static_cast<std::size_t>(Render::Horse::HorseBone::FootFR),
                            bind_palette)),
        std::min(
            bind_adjusted_y(static_cast<std::size_t>(Render::Horse::HorseBone::FootBL),
                            bind_palette),
            bind_adjusted_y(static_cast<std::size_t>(Render::Horse::HorseBone::FootBR),
                            bind_palette)));
  }
  case CreatureKind::Elephant: {
    auto const bind_palette = Render::Elephant::elephant_bind_palette();
    return std::min(
        std::min(bind_adjusted_y(
                     static_cast<std::size_t>(Render::Elephant::ElephantBone::FootFL),
                     bind_palette),
                 bind_adjusted_y(
                     static_cast<std::size_t>(Render::Elephant::ElephantBone::FootFR),
                     bind_palette)),
        std::min(bind_adjusted_y(
                     static_cast<std::size_t>(Render::Elephant::ElephantBone::FootBL),
                     bind_palette),
                 bind_adjusted_y(
                     static_cast<std::size_t>(Render::Elephant::ElephantBone::FootBR),
                     bind_palette)));
  }
  case CreatureKind::Mounted:
    return 0.0F;
  }
  return 0.0F;
}

auto sample_terrain_height_or_fallback(float world_x,
                                       float world_z,
                                       float fallback_y) noexcept -> float {
  auto& terrain_service = Game::Map::TerrainService::instance();
  return terrain_service
      .resolve_surface_world_position(world_x, world_z, 0.0F, fallback_y)
      .y();
}

auto model_world_origin(const QMatrix4x4& model) noexcept -> QVector3D {
  return model.column(3).toVector3D();
}

auto make_runtime_prewarm_ctx(const Render::GL::DrawContext& ctx) noexcept
    -> Render::GL::DrawContext {
  Render::GL::DrawContext runtime_ctx = ctx;
  runtime_ctx.template_prewarm = false;
  runtime_ctx.allow_template_cache = false;
  runtime_ctx.suppress_animation_state_persistence = true;
  return runtime_ctx;
}

auto ground_model_contact_to_surface(QMatrix4x4& model,
                                     float local_contact_y,
                                     float y_scale,
                                     float entity_ground_offset) noexcept -> float {
  float const world_y_offset = (entity_ground_offset + local_contact_y) * y_scale;
  return ground_model_to_terrain(model, world_y_offset);
}

auto ground_model_to_terrain(QMatrix4x4& model, float world_y_offset) noexcept
    -> float {
  QVector3D const origin = model_world_origin(model);
  auto& terrain_service = Game::Map::TerrainService::instance();
  QVector3D const grounded_origin = terrain_service.resolve_surface_world_position(
      origin.x(), origin.z(), -world_y_offset, origin.y());
  set_model_world_y(model, grounded_origin.y());
  return grounded_origin.y() + world_y_offset;
}

void set_model_world_y(QMatrix4x4& model, float world_y) noexcept {
  QVector3D origin = model_world_origin(model);
  origin.setY(world_y);
  model.setColumn(3, QVector4D(origin, 1.0F));
}

} // namespace Render::Creature::Pipeline
