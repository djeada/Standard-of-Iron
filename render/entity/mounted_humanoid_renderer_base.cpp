#include "mounted_humanoid_renderer_base.h"

#include "../creature/pipeline/creature_render_graph.h"
#include "../creature/pipeline/lod_decision.h"
#include "../gl/camera.h"
#include "../graphics_settings.h"
#include "../horse/lod.h"
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

void MountedHumanoidRendererBase::customize_pose(
    const DrawContext &ctx, const HumanoidAnimationContext &anim_ctx,
    uint32_t seed, HumanoidPose &pose) const {
  const AnimationInputs &anim = anim_ctx.inputs;

  uint32_t horse_seed = seed;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx.entity) &
                                       0xFFFFFFFFU);
  }

  HorseDimensions dims = get_scaled_horse_dimensions(horse_seed);
  HorseProfile mount_profile{};
  mount_profile.dims = dims;
  MountedAttachmentFrame mount = compute_mount_frame(mount_profile);
  tune_mounted_knight_frame(dims, mount);
  HorseMotionSample const motion =
      evaluate_horse_motion(mount_profile, anim, anim_ctx);
  apply_mount_vertical_offset(mount, motion.bob);

  ReinState const reins = compute_rein_state(horse_seed, anim_ctx);
  if (!ctx.template_prewarm) {
    m_last_pose = &pose;
    m_last_mount = mount;
    m_last_motion = motion;
    m_last_rein_state = reins;
    m_has_last_reins = true;
  }

  MountedPoseController mounted_controller(pose, anim_ctx);

  mounted_controller.mount_on_horse(mount);

  apply_riding_animation(mounted_controller, mount, anim_ctx, pose, dims,
                         reins);

  applyMountedKnightLowerBody(dims, mount, anim_ctx, pose);

  mounted_controller.finalize_head_sync(mount, "customize_pose_final_sync");
}

void MountedHumanoidRendererBase::add_attachments(
    const DrawContext &ctx, const HumanoidVariant &v, const HumanoidPose &pose,
    const HumanoidAnimationContext &anim_ctx, ISubmitter &out) const {
  uint32_t horse_seed = 0U;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx.entity) &
                                       0xFFFFFFFFU);
  }

  HorseProfile profile = get_cached_horse_profile(horse_seed, v);
  const bool is_current_pose = !ctx.template_prewarm && (m_last_pose == &pose);
  const MountedAttachmentFrame *mount_ptr =
      (is_current_pose) ? &m_last_mount : nullptr;
  const ReinState *rein_ptr =
      (is_current_pose && m_has_last_reins) ? &m_last_rein_state : nullptr;
  const HorseMotionSample *motion_ptr =
      (is_current_pose) ? &m_last_motion : nullptr;
  const AnimationInputs &anim = anim_ctx.inputs;

  HorseLOD horse_lod = HorseLOD::Full;
  {
    namespace RCP = Render::Creature::Pipeline;
    const auto lod_config = RCP::horse_lod_config_from_settings();
    
    RCP::CreatureLodDecisionInputs in{};
    if (ctx.force_horse_lod) {
      in.forced_lod = static_cast<Render::Creature::CreatureLOD>(
          ctx.forced_horse_lod);
    }
    in.has_camera = (ctx.camera != nullptr);
    if (ctx.camera != nullptr) {
      QVector3D const horse_world_pos =
          ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));
      in.distance =
          (horse_world_pos - ctx.camera->get_position()).length();
    }
    in.thresholds = lod_config.thresholds;
    in.apply_visibility_budget = lod_config.apply_visibility_budget;
    in.temporal = lod_config.temporal;
    in.budget_grant_full = true;
    if (in.apply_visibility_budget && !ctx.force_horse_lod &&
        ctx.camera != nullptr) {
      const auto distance_lod = RCP::select_distance_lod(in.distance, in.thresholds);
      if (distance_lod == Render::Creature::CreatureLOD::Full) {
        const auto granted =
            Render::VisibilityBudgetTracker::instance().request_horse_lod(
                HorseLOD::Full);
        in.budget_grant_full = (granted == HorseLOD::Full);
      }
    }
    const auto decision = RCP::decide_creature_lod(in);
    horse_lod = decision.culled
                    ? HorseLOD::Billboard
                    : static_cast<HorseLOD>(decision.lod);
  }

  m_horseRenderer.render(ctx, anim, anim_ctx, profile, mount_ptr, rein_ptr,
                         motion_ptr, out, horse_lod);

  if (!ctx.template_prewarm) {
    m_last_pose = nullptr;
    m_has_last_reins = false;
  }
}

} // namespace Render::GL
