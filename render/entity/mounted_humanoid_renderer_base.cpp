#include "mounted_humanoid_renderer_base.h"

#include "../creature/pipeline/creature_render_graph.h"
#include "../creature/pipeline/lod_decision.h"
#include "../gl/camera.h"
#include "../graphics_settings.h"
#include "../horse/horse_motion.h"
#include "../horse/lod.h"
#include "../horse/prepare.h"
#include "../humanoid/humanoid_math.h"
#include "../humanoid/humanoid_specs.h"
#include "../palette.h"
#include "../visibility_budget.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"

#include "mounted_knight_pose.h"

#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Render::GL {

MountedHumanoidRendererBase::MountedHumanoidRendererBase() = default;

auto MountedHumanoidRendererBase::mounted_visual_spec() const
    -> const Render::Creature::Pipeline::MountedSpec & {

  static thread_local Render::Creature::Pipeline::MountedSpec spec;
  spec.rider = HumanoidRendererBase::visual_spec();
  spec.rider.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.mount = m_horseRenderer.visual_spec();
  spec.mount.kind = Render::Creature::Pipeline::CreatureKind::Horse;
  spec.mount_socket = Render::Creature::kInvalidSocket;
  return spec;
}

auto MountedHumanoidRendererBase::get_scaled_horse_dimensions(
    uint32_t seed) const -> HorseDimensions {
  HorseDimensions dims = make_horse_dimensions(seed);
  scale_horse_dimensions(dims, get_mount_scale());
  return dims;
}

auto MountedHumanoidRendererBase::get_cached_horse_profile(
    uint32_t seed, const HumanoidVariant &v) const -> HorseProfile {
  std::lock_guard<std::mutex> lock(m_profile_cache_mutex);
  auto it = m_profile_cache.find(seed);
  if (it != m_profile_cache.end()) {
    return it->second;
  }

  HorseDimensions dims = get_scaled_horse_dimensions(seed);
  HorseProfile profile =
      make_horse_profile(seed, v.palette.leather, v.palette.cloth);
  profile.dims = dims;

  m_profile_cache[seed] = std::move(profile);
  if (m_profile_cache.size() > MAX_PROFILE_CACHE_SIZE) {
    m_profile_cache.clear();
  }
  return m_profile_cache[seed];
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
    HorseMotionSample &motion, ReinState &reins) const {
  const AnimationInputs &anim = anim_ctx.inputs;

  std::uint32_t horse_seed = seed;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(ctx.entity) & 0xFFFFFFFFU);
  }

  dims = get_scaled_horse_dimensions(horse_seed);
  if (use_cached_profile) {
    profile = get_cached_horse_profile(horse_seed, variant);
  } else {
    profile = make_horse_profile(horse_seed, variant.palette.leather,
                                 variant.palette.cloth);
    profile.dims = dims;
  }
  dims = profile.dims;
  mount = compute_mount_frame(profile);
  tune_mounted_knight_frame(dims, mount);
  motion = evaluate_horse_motion(profile, anim, anim_ctx);
  apply_mount_vertical_offset(mount, motion.bob);
  reins = compute_rein_state(horse_seed, anim_ctx);
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

void MountedHumanoidRendererBase::customize_pose(
    const DrawContext &ctx, const HumanoidAnimationContext &anim_ctx,
    uint32_t seed, HumanoidPose &pose) const {
  HorseProfile mount_profile{};
  HorseDimensions dims{};
  MountedAttachmentFrame mount{};
  HorseMotionSample motion{};
  ReinState reins{};
  resolve_mount_render_state(ctx, seed, HumanoidVariant{}, anim_ctx, false,
                             mount_profile, dims, mount, motion, reins);

  MountedPoseController mounted_controller(pose, anim_ctx);

  mounted_controller.mount_on_horse(mount);

  apply_riding_animation(mounted_controller, mount, anim_ctx, pose, dims,
                         reins);

  applyMountedKnightLowerBody(dims, mount, anim_ctx, pose);

  mounted_controller.finalize_head_sync(mount, "customize_pose_final_sync");
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
  ReinState reins{};
  resolve_mount_render_state(ctx, seed, variant, anim_ctx, true, profile, dims,
                             mount, motion, reins);

  Render::Horse::prepare_horse_render(
      m_horseRenderer, ctx, anim_ctx.inputs, anim_ctx, profile, &mount, &reins,
      &motion, resolve_mount_lod(ctx), {}, nullptr, out);
}

} // namespace Render::GL
