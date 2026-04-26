#include "mounted_humanoid_renderer_base.h"

#include "../creature/anatomy_bake.h"
#include "../creature/animation_state_components.h"
#include "../creature/archetype_registry.h"
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

auto grounded_horse_world_from_mount(
    const DrawContext &ctx,
    const MountedAttachmentFrame &mount) noexcept -> QMatrix4x4 {
  QMatrix4x4 world = ctx.model;
  world.translate(mount.ground_offset);
  const QVector3D origin =
      Render::Creature::Pipeline::model_world_origin(world);
  const float ground_y =
      Render::Creature::Pipeline::sample_terrain_height_or_fallback(
          origin.x(), origin.z(), origin.y());
  Render::Creature::Pipeline::set_model_world_y(world, ground_y);
  return world;
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
    m_mounted_visual_spec_cache.rider.inherits_parent_world = true;
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
                                      resolve_mount_lod(ctx), out);

  DrawContext rider_ctx = ctx;
  rider_ctx.model = grounded_horse_world_from_mount(ctx, mount);

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
  out.bodies.add_humanoid(rider_output, pose, variant, anim_ctx);
}

} // namespace Render::GL
