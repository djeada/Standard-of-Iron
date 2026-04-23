#pragma once

#include "../creature/pipeline/unit_visual_spec.h"
#include "../humanoid/humanoid_renderer_base.h"
#include "../humanoid/mounted_pose_controller.h"
#include "horse_renderer.h"

namespace Render::GL {

class MountedHumanoidRendererBase : public HumanoidRendererBase {
public:
  MountedHumanoidRendererBase();
  ~MountedHumanoidRendererBase() override = default;

  virtual auto mounted_visual_spec() const
      -> const Render::Creature::Pipeline::MountedSpec &;

  void customize_pose(const DrawContext &ctx,
                      const HumanoidAnimationContext &anim_ctx, uint32_t seed,
                      HumanoidPose &pose) const override;

  virtual auto get_mount_scale() const -> float = 0;

protected:
  mutable Render::Creature::Pipeline::MountedSpec m_mounted_visual_spec_cache{};
  mutable bool m_mounted_visual_spec_baked{false};

  virtual void apply_riding_animation(MountedPoseController &controller,
                                      MountedAttachmentFrame &mount,
                                      const HumanoidAnimationContext &anim_ctx,
                                      HumanoidPose &pose,
                                      const HorseDimensions &dims,
                                      const ReinState &reins) const = 0;

  auto resolve_entity_ground_offset(
      const DrawContext &ctx, Engine::Core::UnitComponent *unit_comp,
      Engine::Core::TransformComponent *transform_comp) const -> float override;

  auto get_scaled_horse_dimensions(uint32_t seed) const -> HorseDimensions;

  HorseRenderer m_horseRenderer;

  void append_companion_preparation(
      const DrawContext &ctx, const HumanoidVariant &variant,
      const HumanoidPose &pose, const HumanoidAnimationContext &anim_ctx,
      std::uint32_t seed, Render::Creature::CreatureLOD lod,
      Render::Creature::Pipeline::CreaturePreparationResult &out)
      const override;

private:
  void resolve_mount_render_state(const DrawContext &ctx, std::uint32_t seed,
                                  const HumanoidVariant &variant,
                                  const HumanoidAnimationContext &anim_ctx,
                                  bool use_cached_profile,
                                  HorseProfile &profile, HorseDimensions &dims,
                                  MountedAttachmentFrame &mount,
                                  HorseMotionSample &motion,
                                  ReinState &reins) const;

  [[nodiscard]] auto
  resolve_mount_lod(const DrawContext &ctx) const -> HorseLOD;
};

} // namespace Render::GL
