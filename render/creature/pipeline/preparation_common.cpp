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

namespace {

constexpr float k_terminal_non_looping_phase = std::nextafter(1.0F, 0.0F);

auto looping_phase(float phase) noexcept -> float {
  float wrapped = std::fmod(phase, 1.0F);
  if (wrapped < 0.0F) {
    wrapped += 1.0F;
  }
  return wrapped;
}

auto hold_phase_for_anim(
    const Render::GL::HumanoidAnimationContext &anim) noexcept -> float {
  float const hold_phase = Render::GL::hold_transition_amount(anim.inputs);
  if (hold_phase > 0.0F) {
    return std::clamp(hold_phase, 0.0F, k_terminal_non_looping_phase);
  }
  return k_terminal_non_looping_phase;
}

} // namespace

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
  if (anim.inputs.is_dying) {
    return Render::Creature::AnimationStateId::Die;
  }
  if (anim.inputs.is_dead) {
    return Render::Creature::AnimationStateId::Dead;
  }
  if (anim.inputs.is_in_hold_mode || anim.inputs.is_exiting_hold) {
    return Render::Creature::AnimationStateId::Hold;
  }
  if (anim.is_attacking()) {
    switch (anim.inputs.attack_family) {
    case Engine::Core::CombatAttackFamily::Spear:
      return Render::Creature::AnimationStateId::AttackSpear;
    case Engine::Core::CombatAttackFamily::Bow:
      return Render::Creature::AnimationStateId::AttackBow;
    case Engine::Core::CombatAttackFamily::Sword:
      return Render::Creature::AnimationStateId::AttackSword;
    case Engine::Core::CombatAttackFamily::None:
    default:
      if (anim.inputs.is_melee) {
        return Render::Creature::AnimationStateId::AttackSword;
      }
      return Render::Creature::AnimationStateId::AttackBow;
    }
  }
  if (anim.inputs.is_constructing) {
    return Render::Creature::AnimationStateId::AttackSword;
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
  if (state == Render::Creature::AnimationStateId::Die) {
    return std::clamp(anim.inputs.death_progress, 0.0F, 1.0F);
  }
  if (state == Render::Creature::AnimationStateId::Dead) {
    return 0.0F;
  }
  if (state == Render::Creature::AnimationStateId::Hold) {
    return hold_phase_for_anim(anim);
  }
  if (anim.inputs.is_constructing) {
    return looping_phase(anim.inputs.construction_progress + anim.jitter_seed);
  }
  bool const is_attack_state =
      (state == Render::Creature::AnimationStateId::AttackSword ||
       state == Render::Creature::AnimationStateId::AttackSpear ||
       state == Render::Creature::AnimationStateId::AttackBow);
  return is_attack_state ? anim.attack_phase : anim.gait.cycle_phase;
}

namespace {

auto default_humanoid_archetype(
    Render::Creature::ArchetypeId archetype_id) noexcept
    -> Render::Creature::ArchetypeId {
  return (archetype_id != Render::Creature::kInvalidArchetype)
             ? archetype_id
             : Render::Creature::ArchetypeRegistry::kHumanoidBase;
}

auto humanoid_requested_clip_variant_for_anim(
    const Render::GL::HumanoidAnimationContext &anim) noexcept -> std::uint8_t {
  auto const state = humanoid_state_for_anim(anim);
  if (state == Render::Creature::AnimationStateId::Die ||
      state == Render::Creature::AnimationStateId::Dead) {
    return anim.inputs.death_variant;
  }
  if (anim.inputs.is_constructing &&
      state == Render::Creature::AnimationStateId::AttackSword) {
    auto const bucket =
        static_cast<std::uint32_t>(std::floor(anim.jitter_seed * 64.0F));
    return static_cast<std::uint8_t>(bucket % 3U);
  }
  bool const is_attack_state =
      (state == Render::Creature::AnimationStateId::AttackSword ||
       state == Render::Creature::AnimationStateId::AttackSpear ||
       state == Render::Creature::AnimationStateId::AttackBow);
  return is_attack_state ? anim.inputs.attack_variant : 0U;
}

} // namespace

auto humanoid_clip_variant_for_anim(
    Render::Creature::ArchetypeId archetype_id,
    const Render::GL::HumanoidAnimationContext &anim) noexcept -> std::uint8_t {
  auto const resolved_archetype = default_humanoid_archetype(archetype_id);
  auto const state = humanoid_state_for_anim(anim);
  auto const variant_count =
      Render::Creature::ArchetypeRegistry::instance().clip_variant_count(
          resolved_archetype, state);
  if (variant_count <= 1U) {
    return 0U;
  }
  return std::min<std::uint8_t>(humanoid_requested_clip_variant_for_anim(anim),
                                variant_count - 1U);
}

auto humanoid_bpat_playback_for_anim(
    Render::Creature::ArchetypeId archetype_id,
    const Render::GL::HumanoidAnimationContext &anim) noexcept
    -> std::optional<BpatPlayback> {
  using Render::Creature::ArchetypeDescriptor;

  archetype_id = default_humanoid_archetype(archetype_id);

  auto const state = humanoid_state_for_anim(anim);
  auto const clip_id =
      Render::Creature::ArchetypeRegistry::instance().resolve_bpat_clip(
          archetype_id, state,
          humanoid_clip_variant_for_anim(archetype_id, anim));
  if (clip_id == ArchetypeDescriptor::kUnmappedClip) {
    return std::nullopt;
  }
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

  return BpatPlayback{clip_id, static_cast<std::uint16_t>(frame_idx)};
}

auto humanoid_clip_contact_y(Render::Creature::ArchetypeId archetype_id,
                             const Render::GL::HumanoidAnimationContext
                                 &anim) noexcept -> std::optional<float> {
  auto const playback = humanoid_bpat_playback_for_anim(archetype_id, anim);
  if (!playback.has_value()) {
    return std::nullopt;
  }

  auto const *blob = Render::Creature::Bpat::BpatRegistry::instance().blob(
      Render::Creature::Bpat::kSpeciesHumanoid);
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
