#include "preparation_common.h"

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

#include <QVector4D>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Render::Creature::Pipeline {

auto pass_intent_from_ctx(const Render::GL::DrawContext &ctx) noexcept
    -> RenderPassIntent {
  return ctx.template_prewarm ? RenderPassIntent::Shadow
                              : RenderPassIntent::Main;
}

auto derive_unit_seed(const Render::GL::DrawContext &ctx,
                      const Engine::Core::UnitComponent *unit) noexcept
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

auto humanoid_state_for_anim(
    const Render::GL::HumanoidAnimationContext &anim) noexcept
    -> Render::Creature::AnimationStateId {
  if (anim.inputs.is_in_hold_mode || anim.inputs.is_exiting_hold) {
    return Render::Creature::AnimationStateId::Hold;
  }
  if (anim.is_attacking()) {
    if (anim.inputs.is_melee) {
      return Render::Creature::AnimationStateId::AttackSword;
    }
    return Render::Creature::AnimationStateId::AttackBow;
  }
  switch (anim.motion_state) {
  case Render::GL::HumanoidMotionState::Idle:
    return Render::Creature::AnimationStateId::Idle;
  case Render::GL::HumanoidMotionState::Walk:
    return Render::Creature::AnimationStateId::Walk;
  case Render::GL::HumanoidMotionState::Run:
    return Render::Creature::AnimationStateId::Run;
  case Render::GL::HumanoidMotionState::Hold:
    return Render::Creature::AnimationStateId::Hold;
  default:
    return Render::Creature::AnimationStateId::Idle;
  }
}

auto humanoid_phase_for_anim(
    const Render::GL::HumanoidAnimationContext &anim) noexcept -> float {
  auto const state = humanoid_state_for_anim(anim);
  bool const is_attack_state =
      (state == Render::Creature::AnimationStateId::AttackSword ||
       state == Render::Creature::AnimationStateId::AttackSpear ||
       state == Render::Creature::AnimationStateId::AttackBow);
  return is_attack_state ? anim.attack_phase : anim.gait.cycle_phase;
}

auto humanoid_clip_variant_for_anim(
    const Render::GL::HumanoidAnimationContext &anim) noexcept -> std::uint8_t {
  auto const state = humanoid_state_for_anim(anim);
  bool const is_attack_state =
      (state == Render::Creature::AnimationStateId::AttackSword ||
       state == Render::Creature::AnimationStateId::AttackSpear ||
       state == Render::Creature::AnimationStateId::AttackBow);
  return (is_attack_state &&
          state != Render::Creature::AnimationStateId::AttackBow)
             ? std::min<std::uint8_t>(anim.inputs.attack_variant, 2U)
             : 0U;
}

auto humanoid_clip_contact_y(Render::Creature::ArchetypeId archetype_id,
                             const Render::GL::HumanoidAnimationContext
                                 &anim) noexcept -> std::optional<float> {
  using Render::Creature::ArchetypeDescriptor;

  if (archetype_id == Render::Creature::kInvalidArchetype) {
    archetype_id = Render::Creature::ArchetypeRegistry::kHumanoidBase;
  }

  auto const state = humanoid_state_for_anim(anim);
  auto const base_clip =
      Render::Creature::ArchetypeRegistry::instance().bpat_clip(archetype_id,
                                                                state);
  if (base_clip == ArchetypeDescriptor::kUnmappedClip) {
    return std::nullopt;
  }

  auto const clip_id = static_cast<std::uint16_t>(
      base_clip + humanoid_clip_variant_for_anim(anim));
  auto const *blob = Render::Creature::Bpat::BpatRegistry::instance().blob(
      Render::Creature::Bpat::kSpeciesHumanoid);
  if (blob == nullptr || clip_id >= blob->clip_count()) {
    return std::nullopt;
  }

  auto const clip = blob->clip(clip_id);
  if (clip.frame_count == 0U) {
    return std::nullopt;
  }

  float phase = humanoid_phase_for_anim(anim);
  if (!clip.loops && phase >= 1.0F) {
    phase = 1.0F;
  } else {
    phase -= std::floor(phase);
    if (phase < 0.0F) {
      phase += 1.0F;
    }
  }

  auto const frame_count = static_cast<float>(clip.frame_count);
  int frame_idx = (!clip.loops && phase >= 1.0F)
                      ? static_cast<int>(clip.frame_count - 1U)
                      : static_cast<int>(phase * frame_count);
  frame_idx = std::clamp(frame_idx, 0, static_cast<int>(clip.frame_count) - 1);

  auto const palette = blob->frame_palette_view(
      clip.frame_offset + static_cast<std::uint32_t>(frame_idx));
  if (palette.size() < Render::Humanoid::k_bone_count) {
    return std::nullopt;
  }
  auto const foot_l_idx =
      static_cast<std::size_t>(Render::Humanoid::HumanoidBone::FootL);
  auto const foot_r_idx =
      static_cast<std::size_t>(Render::Humanoid::HumanoidBone::FootR);
  return std::min(palette[foot_l_idx].column(3).y(),
                  palette[foot_r_idx].column(3).y());
}

auto grounded_humanoid_contact_y(
    Render::Creature::ArchetypeId archetype_id,
    const Render::GL::HumanoidPose &pose,
    const Render::GL::HumanoidAnimationContext &anim) noexcept -> float {
  if (auto const clip_contact = humanoid_clip_contact_y(archetype_id, anim);
      clip_contact.has_value()) {
    return *clip_contact;
  }
  return std::min(pose.foot_l.y(), pose.foot_r.y());
}

auto horse_clip_contact_y(std::uint16_t clip_id,
                          float phase) noexcept -> std::optional<float> {
  auto const *blob = Render::Creature::Bpat::BpatRegistry::instance().blob(
      Render::Creature::Bpat::kSpeciesHorse);
  if (blob == nullptr || clip_id >= blob->clip_count()) {
    return std::nullopt;
  }

  auto const clip = blob->clip(clip_id);
  if (clip.frame_count == 0U) {
    return std::nullopt;
  }

  if (!clip.loops && phase >= 1.0F) {
    phase = 1.0F;
  } else {
    phase -= std::floor(phase);
    if (phase < 0.0F) {
      phase += 1.0F;
    }
  }

  auto const frame_count = static_cast<float>(clip.frame_count);
  int frame_idx = (!clip.loops && phase >= 1.0F)
                      ? static_cast<int>(clip.frame_count - 1U)
                      : static_cast<int>(phase * frame_count);
  frame_idx = std::clamp(frame_idx, 0, static_cast<int>(clip.frame_count) - 1);

  auto const palette = blob->frame_palette_view(
      clip.frame_offset + static_cast<std::uint32_t>(frame_idx));
  if (palette.size() < Render::Horse::k_horse_bone_count) {
    return std::nullopt;
  }

  auto const foot_fl_idx =
      static_cast<std::size_t>(Render::Horse::HorseBone::FootFL);
  auto const foot_fr_idx =
      static_cast<std::size_t>(Render::Horse::HorseBone::FootFR);
  auto const foot_bl_idx =
      static_cast<std::size_t>(Render::Horse::HorseBone::FootBL);
  auto const foot_br_idx =
      static_cast<std::size_t>(Render::Horse::HorseBone::FootBR);
  return std::min(std::min(palette[foot_fl_idx].column(3).y(),
                           palette[foot_fr_idx].column(3).y()),
                  std::min(palette[foot_bl_idx].column(3).y(),
                           palette[foot_br_idx].column(3).y()));
}

auto palette_contact_y(CreatureKind kind,
                       std::span<const QMatrix4x4> palette) noexcept -> float {
  auto const bind_adjusted_y =
      [&](std::size_t index,
          std::span<const QMatrix4x4> bind_palette) -> float {
    if (index >= palette.size() || index >= bind_palette.size()) {
      return 0.0F;
    }
    return (palette[index] * bind_palette[index].inverted()).column(3).y();
  };

  switch (kind) {
  case CreatureKind::Humanoid: {
    auto const bind_palette = Render::Humanoid::humanoid_bind_palette();
    return std::min(bind_adjusted_y(static_cast<std::size_t>(
                                        Render::Humanoid::HumanoidBone::FootL),
                                    bind_palette),
                    bind_adjusted_y(static_cast<std::size_t>(
                                        Render::Humanoid::HumanoidBone::FootR),
                                    bind_palette));
  }
  case CreatureKind::Horse: {
    auto const bind_palette = Render::Horse::horse_bind_palette();
    return std::min(
        std::min(bind_adjusted_y(
                     static_cast<std::size_t>(Render::Horse::HorseBone::FootFL),
                     bind_palette),
                 bind_adjusted_y(
                     static_cast<std::size_t>(Render::Horse::HorseBone::FootFR),
                     bind_palette)),
        std::min(bind_adjusted_y(
                     static_cast<std::size_t>(Render::Horse::HorseBone::FootBL),
                     bind_palette),
                 bind_adjusted_y(
                     static_cast<std::size_t>(Render::Horse::HorseBone::FootBR),
                     bind_palette)));
  }
  case CreatureKind::Elephant: {
    auto const bind_palette = Render::Elephant::elephant_bind_palette();
    return std::min(
        std::min(bind_adjusted_y(static_cast<std::size_t>(
                                     Render::Elephant::ElephantBone::FootFL),
                                 bind_palette),
                 bind_adjusted_y(static_cast<std::size_t>(
                                     Render::Elephant::ElephantBone::FootFR),
                                 bind_palette)),
        std::min(bind_adjusted_y(static_cast<std::size_t>(
                                     Render::Elephant::ElephantBone::FootBL),
                                 bind_palette),
                 bind_adjusted_y(static_cast<std::size_t>(
                                     Render::Elephant::ElephantBone::FootBR),
                                 bind_palette)));
  }
  case CreatureKind::Mounted:
    return 0.0F;
  }
  return 0.0F;
}

auto sample_terrain_height_or_fallback(float world_x, float world_z,
                                       float fallback_y) noexcept -> float {
  auto &terrain_service = Game::Map::TerrainService::instance();
  return terrain_service
      .resolve_surface_world_position(world_x, world_z, 0.0F, fallback_y)
      .y();
}

auto model_world_origin(const QMatrix4x4 &model) noexcept -> QVector3D {
  return model.column(3).toVector3D();
}

auto make_runtime_prewarm_ctx(const Render::GL::DrawContext &ctx) noexcept
    -> Render::GL::DrawContext {
  Render::GL::DrawContext runtime_ctx = ctx;
  runtime_ctx.template_prewarm = false;
  runtime_ctx.allow_template_cache = false;
  return runtime_ctx;
}

void ground_model_contact_to_surface(QMatrix4x4 &model, float local_contact_y,
                                     float y_scale,
                                     float entity_ground_offset) noexcept {
  float const world_y_offset =
      (entity_ground_offset + local_contact_y) * y_scale;
  ground_model_to_terrain(model, world_y_offset);
}

void ground_model_to_terrain(QMatrix4x4 &model, float world_y_offset) noexcept {
  QVector3D const origin = model_world_origin(model);
  auto &terrain_service = Game::Map::TerrainService::instance();
  QVector3D const grounded_origin =
      terrain_service.resolve_surface_world_position(
          origin.x(), origin.z(), -world_y_offset, origin.y());
  set_model_world_y(model, grounded_origin.y());
}

void set_model_world_y(QMatrix4x4 &model, float world_y) noexcept {
  QVector3D origin = model_world_origin(model);
  origin.setY(world_y);
  model.setColumn(3, QVector4D(origin, 1.0F));
}

} // namespace Render::Creature::Pipeline
