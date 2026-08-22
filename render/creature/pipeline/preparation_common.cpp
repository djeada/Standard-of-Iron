#include "preparation_common.h"

#include <QVector4D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include "animation/bpat/bpat_registry.h"
#include "animation/clip_manifest.h"
#include "animation/locomotion_manifest.h"
#include "animation/playback_manifest.h"
#include "animation/rig/humanoid_proportions.h"
#include "game/core/component.h"
#include "game/core/entity.h"
#include "game/map/terrain_service.h"
#include "render/anim_key.h"
#include "render/creature/archetype_registry.h"
#include "render/creature/humanoid_clip_ids.h"
#include "render/creature/pose_intent.h"
#include "render/elephant/elephant_spec.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/horse/horse_spec.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/schema/skeleton_schema.h"
#include "render/wildlife/sheep_spec.h"
#include "render/wildlife/wildlife_rig.h"
#include "render/wildlife/wolf_spec.h"

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
      .idle_breath_phase = anim.idle_breath_phase,
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

auto humanoid_idle_breath_phase_for_lod(float sample_time,
                                        std::uint32_t inst_seed,
                                        Render::Creature::CreatureLOD lod,
                                        bool template_prewarm) noexcept -> float {

  float const phase = template_prewarm
                          ? sample_time
                          : sample_time / Animation::k_humanoid_idle_breath_cycle_time +
                                Animation::humanoid_idle_breath_offset(inst_seed);

  if (lod == Render::Creature::CreatureLOD::Full) {
    return Animation::wrap_locomotion_phase(phase);
  }

  constexpr auto k_steps = static_cast<float>(Render::GL::k_anim_frame_count - 1U);
  float const wrapped = Animation::wrap_locomotion_phase(phase);
  return Animation::wrap_locomotion_phase(std::round(wrapped * k_steps) / k_steps);
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

} // namespace

auto humanoid_clip_variant_for_state(Render::Creature::ArchetypeId archetype_id,
                                     const Render::GL::HumanoidAnimationContext& anim,
                                     Render::Creature::AnimationStateId state) noexcept
    -> std::uint8_t {
  auto const resolved_archetype = default_humanoid_archetype(archetype_id);
  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto const variant_count = registry.clip_variant_count(resolved_archetype, state);
  if (variant_count <= 1U) {
    return 0U;
  }

  if (state == Render::Creature::AnimationStateId::Idle &&
      registry.bpat_clip(resolved_archetype, state) !=
          Render::Creature::k_humanoid_idle_clip) {
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

  auto& registry = Render::Creature::ArchetypeRegistry::instance();
  auto clip_variant = anim.inputs.is_in_hold_mode
                          ? std::uint8_t{0U}
                          : humanoid_clip_variant_for_state(archetype_id, anim, state);
  auto const base_clip_id = registry.resolve_bpat_clip(archetype_id, state, 0U);
  auto clip_id = registry.resolve_bpat_clip(archetype_id, state, clip_variant);
  if (clip_id == ArchetypeDescriptor::k_unmapped_clip) {
    return std::nullopt;
  }
  auto const* blob = Render::Creature::Bpat::BpatRegistry::instance().blob(species_id);
  if (blob == nullptr) {
    return std::nullopt;
  }

  if (clip_id != base_clip_id &&
      !blob->clip_is_variant_of(base_clip_id, clip_id, clip_variant)) {
    clip_variant = 0U;
    clip_id = base_clip_id;
  }
  if (clip_id >= blob->clip_count()) {
    return std::nullopt;
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

  auto const contacts = blob->frame_contacts();
  std::uint32_t const global_frame = clip.frame_offset + playback->frame_in_clip;
  if (global_frame >= contacts.size()) {
    return std::nullopt;
  }
  return contacts[global_frame].foot_y;
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

  auto const contacts = blob->frame_contacts();
  if (playback.global_frame >= contacts.size()) {
    return std::nullopt;
  }
  return contacts[playback.global_frame].foot_y;
}

auto bind_palette_for_kind(CreatureKind kind) noexcept -> std::span<const QMatrix4x4> {
  switch (kind) {
  case CreatureKind::Humanoid:
    return Render::Humanoid::humanoid_bind_palette();
  case CreatureKind::Horse:
    return Render::Horse::horse_bind_palette();
  case CreatureKind::Elephant:
    return Render::Elephant::elephant_bind_palette();
  case CreatureKind::Sheep:
    return Render::Wildlife::sheep_bind_palette();
  case CreatureKind::Wolf:
    return Render::Wildlife::wolf_bind_palette();
  case CreatureKind::Mounted:
    return {};
  }
  return {};
}

namespace {

auto lowest_of(std::span<const std::size_t> bones,
               std::span<const QMatrix4x4> palette,
               auto&& sample) -> float {
  float lowest = std::numeric_limits<float>::max();
  for (auto const bone : bones) {
    if (bone < palette.size()) {
      lowest = std::min(lowest, sample(bone));
    }
  }
  return lowest == std::numeric_limits<float>::max() ? 0.0F : lowest;
}

auto foot_bones(CreatureKind kind) noexcept -> std::span<const std::size_t> {
  static constexpr std::array<std::size_t, 2> k_humanoid{
      static_cast<std::size_t>(Render::Humanoid::HumanoidBone::FootL),
      static_cast<std::size_t>(Render::Humanoid::HumanoidBone::FootR)};
  static constexpr std::array<std::size_t, 4> k_horse{
      static_cast<std::size_t>(Render::Horse::HorseBone::FootFL),
      static_cast<std::size_t>(Render::Horse::HorseBone::FootFR),
      static_cast<std::size_t>(Render::Horse::HorseBone::FootBL),
      static_cast<std::size_t>(Render::Horse::HorseBone::FootBR)};
  static constexpr std::array<std::size_t, 4> k_elephant{
      static_cast<std::size_t>(Render::Elephant::ElephantBone::FootFL),
      static_cast<std::size_t>(Render::Elephant::ElephantBone::FootFR),
      static_cast<std::size_t>(Render::Elephant::ElephantBone::FootBL),
      static_cast<std::size_t>(Render::Elephant::ElephantBone::FootBR)};
  static constexpr std::array<std::size_t, 4> k_wildlife{
      static_cast<std::size_t>(Render::Wildlife::Bone::FootFL),
      static_cast<std::size_t>(Render::Wildlife::Bone::FootFR),
      static_cast<std::size_t>(Render::Wildlife::Bone::FootBL),
      static_cast<std::size_t>(Render::Wildlife::Bone::FootBR)};
  switch (kind) {
  case CreatureKind::Humanoid:
    return k_humanoid;
  case CreatureKind::Horse:
    return k_horse;
  case CreatureKind::Elephant:
    return k_elephant;
  case CreatureKind::Sheep:
  case CreatureKind::Wolf:
    return k_wildlife;
  case CreatureKind::Mounted:
    return {};
  }
  return {};
}

} // namespace

auto palette_contact_y(CreatureKind kind,
                       std::span<const QMatrix4x4> skin_palette) noexcept -> float {
  if (kind == CreatureKind::Humanoid) {
    auto const bind = bind_palette_for_kind(kind);
    constexpr std::array<QVector3D, 2> k_sole_points{
        QVector3D{0.0F, -Render::GL::HumanProportions::FOOT_Y_OFFSET_DEFAULT, -0.060F},
        QVector3D{0.0F, -Render::GL::HumanProportions::FOOT_Y_OFFSET_DEFAULT, 0.165F}};
    return lowest_of(foot_bones(kind), skin_palette, [&](std::size_t bone) {
      if (bone >= bind.size()) {
        return 0.0F;
      }
      float lowest = std::numeric_limits<float>::max();
      for (auto const& local : k_sole_points) {
        QVector4D const bound = bind[bone] * QVector4D(local, 1.0F);
        float const posed = (skin_palette[bone] * bound).y();
        lowest = std::min(lowest, posed - bound.y());
      }
      return lowest;
    });
  }
  return lowest_of(foot_bones(kind), skin_palette, [&](std::size_t bone) {
    return skin_palette[bone].column(3).y();
  });
}

auto palette_foot_contact_y(
    CreatureKind kind, std::span<const QMatrix4x4> skin_palette) noexcept -> float {
  auto const bind = bind_palette_for_kind(kind);
  return lowest_of(foot_bones(kind), skin_palette, [&](std::size_t bone) {
    return bone < bind.size() ? (skin_palette[bone] * bind[bone]).column(3).y() : 0.0F;
  });
}

auto creature_kind_for_bpat_species(std::uint32_t species_id) noexcept -> CreatureKind {
  switch (species_id) {
  case Render::Creature::Bpat::k_species_horse:
    return CreatureKind::Horse;
  case Render::Creature::Bpat::k_species_elephant:
    return CreatureKind::Elephant;
  case Render::Creature::Bpat::k_species_sheep:
    return CreatureKind::Sheep;
  case Render::Creature::Bpat::k_species_wolf:
    return CreatureKind::Wolf;
  default:
    return CreatureKind::Humanoid;
  }
}

auto sample_terrain_height_or_fallback(const Game::Map::TerrainService& terrain_service,
                                       float world_x,
                                       float world_z,
                                       float fallback_y) noexcept -> float {
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
  runtime_ctx.prewarming_via_runtime_path = true;
  runtime_ctx.allow_template_cache = false;
  runtime_ctx.suppress_animation_state_persistence = true;
  return runtime_ctx;
}

auto ground_model_contact_to_surface(const Game::Map::TerrainService& terrain,
                                     QMatrix4x4& model,
                                     float local_contact_y,
                                     float y_scale,
                                     float entity_ground_offset) noexcept -> float {
  float const world_y_offset = (entity_ground_offset + local_contact_y) * y_scale;
  return ground_model_to_terrain(terrain, model, world_y_offset);
}

auto ground_model_to_terrain(const Game::Map::TerrainService& terrain_service,
                             QMatrix4x4& model,
                             float world_y_offset) noexcept -> float {
  QVector3D const origin = model_world_origin(model);
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
