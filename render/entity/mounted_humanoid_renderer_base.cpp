#include "mounted_humanoid_renderer_base.h"

#include "../creature/anatomy_bake.h"
#include "../creature/animation_state_components.h"
#include "../creature/archetype_registry.h"
#include "../creature/bpat/bpat_format.h"
#include "../creature/bpat/bpat_registry.h"
#include "../creature/pipeline/creature_render_graph.h"
#include "../creature/pipeline/lod_decision.h"
#include "../creature/pipeline/preparation_common.h"
#include "../gl/camera.h"
#include "../graphics_settings.h"
#include "../horse/horse_motion.h"
#include "../horse/lod.h"
#include "../horse/prepare.h"
#include "../humanoid/cache_control.h"
#include "../humanoid/humanoid_full_builder.h"
#include "../humanoid/skeleton.h"
#include "../palette.h"
#include "../visibility_budget.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"

#include "mounted_knight_pose.h"

#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Render::GL {

namespace {

auto horse_clip_for_gait(Render::GL::GaitType gait) noexcept -> std::uint16_t {
  using Render::GL::GaitType;
  switch (gait) {
  case GaitType::IDLE:
    return 0U;
  case GaitType::WALK:
    return 1U;
  case GaitType::TROT:
    return 2U;
  case GaitType::CANTER:
    return 3U;
  case GaitType::GALLOP:
    return 4U;
  }
  return 0U;
}

auto grounded_horse_world_from_mount(
    const DrawContext &ctx, const HorseProfile &profile,
    const HorseMotionSample &motion) noexcept -> QMatrix4x4 {
  QMatrix4x4 world = ctx.model;
  Render::Horse::HorseSpecPose pose{};
  Render::Horse::make_horse_spec_pose_reduced(
      profile.dims, profile.gait,
      Render::Horse::HorseReducedMotion{motion.phase, motion.bob,
                                        motion.is_moving},
      pose);
  float const y_scale = world.mapVector(QVector3D(0.0F, 1.0F, 0.0F)).length();
  Render::Creature::Pipeline::ground_model_contact_to_surface(
      world,
      Render::Creature::Pipeline::grounded_horse_contact_y(
          pose, horse_clip_for_gait(motion.gait_type), motion.phase),
      y_scale);
  return world;
}

auto rider_mount_frame(const MountedAttachmentFrame &mount) noexcept
    -> QMatrix4x4 {
  QMatrix4x4 local;
  local.setColumn(0, QVector4D(mount.seat_right, 0.0F));
  local.setColumn(1, QVector4D(mount.seat_up, 0.0F));
  local.setColumn(2, QVector4D(mount.seat_forward, 0.0F));
  local.setColumn(3, QVector4D(mount.seat_position, 1.0F));
  return local;
}

auto rider_state_for_anim(const HumanoidAnimationContext &anim) noexcept
    -> Render::Creature::AnimationStateId {
  return Render::Creature::Pipeline::humanoid_state_for_anim(anim);
}

auto rider_phase_for_anim(const HumanoidAnimationContext &anim) noexcept
    -> float {
  return Render::Creature::Pipeline::humanoid_phase_for_anim(anim);
}

auto rider_clip_variant_for_anim(const HumanoidAnimationContext &anim) noexcept
    -> std::uint8_t {
  return Render::Creature::Pipeline::humanoid_clip_variant_for_anim(anim);
}

auto rider_root_transform(Render::Creature::ArchetypeId archetype_id,
                          const HumanoidAnimationContext &anim) noexcept
    -> std::optional<QMatrix4x4> {
  using Render::Creature::ArchetypeDescriptor;
  auto const state = rider_state_for_anim(anim);
  auto const base_clip =
      Render::Creature::ArchetypeRegistry::instance().bpat_clip(archetype_id,
                                                                state);
  if (base_clip == ArchetypeDescriptor::kUnmappedClip) {
    return std::nullopt;
  }

  auto const clip_id =
      static_cast<std::uint16_t>(base_clip + rider_clip_variant_for_anim(anim));
  auto const *blob = Render::Creature::Bpat::BpatRegistry::instance().blob(
      Render::Creature::Bpat::kSpeciesHumanoid);
  if (blob == nullptr || clip_id >= blob->clip_count()) {
    return std::nullopt;
  }

  auto const clip = blob->clip(clip_id);
  if (clip.frame_count == 0U) {
    return std::nullopt;
  }

  float phase = rider_phase_for_anim(anim);
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
  auto const root_index =
      static_cast<std::size_t>(Render::Humanoid::HumanoidBone::Root);
  if (palette.size() <= root_index) {
    return std::nullopt;
  }
  return palette[root_index];
}

auto rider_local_world_from_mount(const MountedAttachmentFrame &mount,
                                  Render::Creature::ArchetypeId archetype_id,
                                  const HumanoidAnimationContext &anim) noexcept
    -> QMatrix4x4 {
  QMatrix4x4 const seat_frame = rider_mount_frame(mount);
  auto const root = rider_root_transform(archetype_id, anim);
  if (!root.has_value()) {
    return seat_frame;
  }
  bool invertible = false;
  QMatrix4x4 const root_inverse = root->inverted(&invertible);
  return invertible ? seat_frame * root_inverse : seat_frame;
}

} // namespace

MountedHumanoidRendererBase::MountedHumanoidRendererBase() = default;

auto MountedHumanoidRendererBase::mounted_visual_spec() const
    -> const Render::Creature::Pipeline::MountedSpec & {
  if (!m_mounted_visual_spec_baked) {
    m_mounted_visual_spec_cache.rider = HumanoidRendererBase::visual_spec();
    m_mounted_visual_spec_cache.rider.kind =
        Render::Creature::Pipeline::CreatureKind::Humanoid;
    if (m_mounted_visual_spec_cache.rider.archetype_id ==
        Render::Creature::kInvalidArchetype) {
      m_mounted_visual_spec_cache.rider.archetype_id =
          Render::Creature::ArchetypeRegistry::kRiderBase;
    }
    m_mounted_visual_spec_cache.mount = m_horseRenderer.visual_spec();
    m_mounted_visual_spec_cache.mount.kind =
        Render::Creature::Pipeline::CreatureKind::Horse;
    m_mounted_visual_spec_cache.mount.archetype_id = m_mount_archetype_id;
    m_mounted_visual_spec_cache.mount_socket = Render::Creature::kInvalidSocket;
    m_mounted_visual_spec_baked = true;
  }
  return m_mounted_visual_spec_cache;
}

auto MountedHumanoidRendererBase::get_scaled_horse_dimensions(
    uint32_t seed) const -> HorseDimensions {
  HorseDimensions dims = make_horse_dimensions(seed);
  scale_horse_dimensions(dims, get_mount_scale());
  return dims;
}

auto MountedHumanoidRendererBase::resolve_entity_ground_offset(
    const DrawContext &ctx, Engine::Core::UnitComponent *unit_comp,
    Engine::Core::TransformComponent *transform_comp) const -> float {
  (void)unit_comp;

  uint32_t horse_seed = 0U;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx.entity) &
                                       0xFFFFFFFFU);
  }

  HorseDimensions dims = get_scaled_horse_dimensions(horse_seed);
  float offset = -dims.barrel_center_y;
  if (transform_comp != nullptr) {
    offset *= transform_comp->scale.y;
  }
  return offset;
}

void MountedHumanoidRendererBase::resolve_mount_render_state(
    const DrawContext &ctx, std::uint32_t seed, const HumanoidVariant &variant,
    const HumanoidAnimationContext &anim_ctx, bool use_cached_profile,
    HorseProfile &profile, HorseDimensions &dims, MountedAttachmentFrame &mount,
    HorseMotionSample &motion) const {
  const AnimationInputs &anim = anim_ctx.inputs;

  std::uint32_t horse_seed = seed;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
  }

  dims = get_scaled_horse_dimensions(horse_seed);
  if (use_cached_profile) {
    profile = Render::Creature::get_or_bake_horse_anatomy(
                  ctx.entity, horse_seed, variant.palette.leather,
                  variant.palette.cloth, get_mount_scale())
                  .profile;
  } else {
    profile = make_horse_profile(horse_seed, variant.palette.leather,
                                 variant.palette.cloth);
    profile.dims = dims;
  }
  dims = profile.dims;
  mount = compute_mount_frame(profile);
  tune_mounted_knight_frame(dims, mount);
  motion = evaluate_horse_motion(
      profile, anim, anim_ctx,
      Engine::Core::get_or_add_component<
          Render::Creature::HorseAnimationStateComponent>(ctx.entity));
}

auto MountedHumanoidRendererBase::resolve_mount_lod(
    const DrawContext &ctx) const -> HorseLOD {
  namespace RCP = Render::Creature::Pipeline;

  const auto lod_config = RCP::horse_lod_config_from_settings();
  RCP::CreatureGraphInputs inputs{};
  inputs.ctx = &ctx;
  inputs.entity = ctx.entity;
  inputs.has_camera = (ctx.camera != nullptr);
  if (ctx.force_horse_lod) {
    inputs.forced_lod =
        static_cast<Render::Creature::CreatureLOD>(ctx.forced_horse_lod);
  }
  if (ctx.camera != nullptr) {
    const QVector3D horse_world_pos =
        ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));
    inputs.camera_distance =
        (horse_world_pos - ctx.camera->get_position()).length();
  }

  if (lod_config.apply_visibility_budget && !ctx.force_horse_lod &&
      ctx.camera != nullptr) {
    const auto distance_lod =
        RCP::select_distance_lod(inputs.camera_distance, lod_config.thresholds);
    if (distance_lod == Render::Creature::CreatureLOD::Full) {
      const auto granted =
          Render::VisibilityBudgetTracker::instance().request_horse_lod(
              HorseLOD::Full);
      inputs.budget_grant_full = (granted == HorseLOD::Full);
    }
  }

  const auto decision = RCP::evaluate_creature_lod(inputs, lod_config);
  return decision.culled ? HorseLOD::Billboard
                         : static_cast<HorseLOD>(decision.lod);
}

void MountedHumanoidRendererBase::append_companion_preparation(
    const DrawContext &ctx, const HumanoidVariant &variant,
    const HumanoidPose &pose, const HumanoidAnimationContext &anim_ctx,
    std::uint32_t seed, Render::Creature::CreatureLOD lod,
    Render::Creature::Pipeline::CreaturePreparationResult &out) const {
  (void)pose;
  if (lod == Render::Creature::CreatureLOD::Minimal ||
      lod == Render::Creature::CreatureLOD::Billboard) {
    return;
  }

  HorseProfile profile{};
  HorseDimensions dims{};
  MountedAttachmentFrame mount{};
  HorseMotionSample motion{};
  resolve_mount_render_state(ctx, seed, variant, anim_ctx, true, profile, dims,
                             mount, motion);

  Render::Horse::prepare_horse_render(m_horseRenderer, ctx, anim_ctx.inputs,
                                      anim_ctx, profile, &mount, &motion,
                                      resolve_mount_lod(ctx), out, seed);

  DrawContext rider_ctx = ctx;
  rider_ctx.model = grounded_horse_world_from_mount(ctx, profile, motion);

  namespace RCP = Render::Creature::Pipeline;
  RCP::CreatureGraphInputs rider_inputs{};
  rider_inputs.ctx = &rider_ctx;
  rider_inputs.anim = &anim_ctx.inputs;
  rider_inputs.entity = ctx.entity;
  RCP::CreatureLodDecision rider_lod{};
  rider_lod.lod = lod;
  auto rider_output = RCP::build_base_graph_output(rider_inputs, rider_lod);
  rider_output.spec = mounted_visual_spec().rider;
  rider_output.seed = seed;
  rider_output.world_matrix =
      rider_ctx.model * rider_local_world_from_mount(
                            mount, rider_output.spec.archetype_id, anim_ctx);
  out.bodies.add_humanoid(rider_output, pose, variant, anim_ctx);
}

} // namespace Render::GL
