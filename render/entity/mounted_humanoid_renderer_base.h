#pragma once

#include "../humanoid/mounted_pose_controller.h"
#include "../humanoid/rig.h"
#include "horse_renderer.h"

#include <mutex>
#include <unordered_map>

namespace Render::GL {

class MountedHumanoidRendererBase : public HumanoidRendererBase {
public:
  MountedHumanoidRendererBase();
  ~MountedHumanoidRendererBase() override = default;

  void customize_pose(const DrawContext &ctx,
                      const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                      HumanoidPose &pose) const override;

  void add_attachments(const DrawContext &ctx, const HumanoidVariant &v,
                       const HumanoidPose &pose,
                       const HumanoidAnimationContext &anim_ctx,
                       ISubmitter &out) const override;

  virtual auto get_mount_scale() const -> float = 0;

protected:
  virtual void apply_riding_animation(MountedPoseController &controller,
                                      MountedAttachmentFrame &mount,
                                      const HumanoidAnimationContext &anim_ctx,
                                      HumanoidPose &pose,
                                      const HorseDimensions &dims,
                                      const ReinState &reins) const = 0;

  virtual void draw_equipment(const DrawContext &ctx, const HumanoidVariant &v,
                              const HumanoidPose &pose,
                              const HumanoidAnimationContext &anim_ctx,
                              ISubmitter &out) const {}

  auto resolve_entity_ground_offset(
      const DrawContext &ctx, Engine::Core::UnitComponent *unit_comp,
      Engine::Core::TransformComponent *transform_comp) const -> float override;

  auto get_scaled_horse_dimensions(uint32_t seed) const -> HorseDimensions;
  auto get_cached_horse_profile(uint32_t seed,
                                const HumanoidVariant &v) const -> HorseProfile;

  HorseRenderer m_horseRenderer;

private:
  mutable std::mutex m_profile_cache_mutex;
  mutable std::unordered_map<uint32_t, HorseProfile> m_profile_cache;
  static constexpr size_t MAX_PROFILE_CACHE_SIZE = 100;

  mutable const HumanoidPose *m_last_pose = nullptr;
  mutable MountedAttachmentFrame m_last_mount{};
  mutable HorseMotionSample m_last_motion{};
  mutable ReinState m_last_rein_state{};
  mutable bool m_has_last_reins = false;
};

} // namespace Render::GL
