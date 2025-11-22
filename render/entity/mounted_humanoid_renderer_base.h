#pragma once

#include "../humanoid/rig.h"
#include "horse_renderer.h"
#include "../humanoid/mounted_pose_controller.h"

#include <unordered_map>

namespace Render::GL {

class MountedHumanoidRendererBase : public HumanoidRendererBase {
public:
  MountedHumanoidRendererBase();
  ~MountedHumanoidRendererBase() override = default;

  // Common HumanoidRendererBase overrides
  void customize_pose(const DrawContext &ctx,
                      const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                      HumanoidPose &pose) const override;

  void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                      const HumanoidPose &pose,
                      const HumanoidAnimationContext &anim_ctx,
                      ISubmitter &out) const override;

  // Pure virtuals to be implemented by specific mounted units
  virtual auto get_mount_scale() const -> float = 0;

protected:
  virtual void apply_riding_animation(MountedPoseController &controller,
                                      MountedAttachmentFrame &mount,
                                      const HumanoidAnimationContext &anim_ctx,
                                      HumanoidPose &pose,
                                      const HorseDimensions &dims,
                                      const ReinState &reins) const = 0;
  
  // Optional hook for drawing specific equipment (bow, spear, shield, etc.)
  virtual void draw_equipment(const DrawContext &ctx, const HumanoidVariant &v,
                              const HumanoidPose &pose,
                              const HumanoidAnimationContext &anim_ctx,
                              ISubmitter &out) const {}

  // Helper methods
  auto get_scaled_horse_dimensions(uint32_t seed) const -> HorseDimensions;
  auto get_cached_horse_profile(uint32_t seed, const HumanoidVariant &v) const -> const HorseProfile&;

  // Members
  HorseRenderer m_horseRenderer;

private:
  mutable std::unordered_map<uint32_t, HorseProfile> m_profile_cache;
  static constexpr size_t MAX_PROFILE_CACHE_SIZE = 100;

  // State for synchronization (Jitter Fix)
  mutable const HumanoidPose *m_last_pose = nullptr;
  mutable MountedAttachmentFrame m_last_mount{};
  mutable HorseMotionSample m_last_motion{};
  mutable ReinState m_last_rein_state{};
  mutable bool m_has_last_reins = false;
};

} // namespace Render::GL
