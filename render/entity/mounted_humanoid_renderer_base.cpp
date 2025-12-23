#include "mounted_humanoid_renderer_base.h"

#include "../gl/camera.h"
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

auto MountedHumanoidRendererBase::get_scaled_horse_dimensions(
    uint32_t seed) const -> HorseDimensions {
  HorseDimensions dims = make_horse_dimensions(seed);
  scale_horse_dimensions(dims, get_mount_scale());
  return dims;
}

auto MountedHumanoidRendererBase::get_cached_horse_profile(
    uint32_t seed, const HumanoidVariant &v) const -> const HorseProfile & {
  auto it = m_profile_cache.find(seed);
  if (it != m_profile_cache.end()) {
    return it->second;
  }

  HorseDimensions dims = get_scaled_horse_dimensions(seed);
  HorseProfile profile =
      make_horse_profile(seed, v.palette.leather, v.palette.cloth);
  profile.dims = dims;

  m_profile_cache[seed] = profile;
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

  m_last_pose = &pose;
  m_last_mount = mount;
  m_last_motion = motion;

  ReinState const reins = compute_rein_state(horse_seed, anim_ctx);
  m_last_rein_state = reins;
  m_has_last_reins = true;

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
  static uint64_t s_mounted_frame_counter = 0;
  ++s_mounted_frame_counter;
  uint64_t frame_id = s_mounted_frame_counter;

  uint32_t horse_seed = 0U;
  if (ctx.entity != nullptr) {
    horse_seed = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx.entity) &
                                       0xFFFFFFFFU);
  }

  const HorseProfile &profile_const = get_cached_horse_profile(horse_seed, v);
  HorseProfile &profile = const_cast<HorseProfile &>(profile_const);

  const bool is_current_pose = (m_last_pose == &pose);
  const MountedAttachmentFrame *mount_ptr =
      (is_current_pose) ? &m_last_mount : nullptr;
  const ReinState *rein_ptr =
      (is_current_pose && m_has_last_reins) ? &m_last_rein_state : nullptr;
  const HorseMotionSample *motion_ptr =
      (is_current_pose) ? &m_last_motion : nullptr;
  const AnimationInputs &anim = anim_ctx.inputs;

  HorseLOD horse_lod = HorseLOD::Full;
  if (ctx.camera != nullptr) {
    QVector3D const horse_world_pos =
        ctx.model.map(QVector3D(0.0F, 0.0F, 0.0F));
    float const distance =
        (horse_world_pos - ctx.camera->get_position()).length();
    horse_lod = calculate_horse_lod(distance);
    horse_lod =
        Render::VisibilityBudgetTracker::instance().request_horse_lod(horse_lod);
  }

  m_horseRenderer.render(ctx, anim, anim_ctx, profile, mount_ptr, rein_ptr,
                         motion_ptr, out, horse_lod);

  m_last_pose = nullptr;
  m_has_last_reins = false;

  draw_equipment(ctx, v, pose, anim_ctx, out);
}

} // namespace Render::GL
