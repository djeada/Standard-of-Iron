#pragma once

#include "../creature/pipeline/unit_visual_spec.h"
#include "../humanoid/humanoid_renderer_base.h"
#include "horse_renderer.h"

namespace Render::GL {

class MountedHumanoidRendererBase : public HumanoidRendererBase {
public:
  MountedHumanoidRendererBase();
  ~MountedHumanoidRendererBase() override = default;

  virtual auto mounted_visual_spec() const
      -> const Render::Creature::Pipeline::MountedSpec &;

  auto uses_mounted_pipeline() const noexcept -> bool override { return true; }

  virtual auto get_mount_scale() const -> float = 0;

  void set_mount_archetype_id(Render::Creature::ArchetypeId id) {
    m_mount_archetype_id = id;
    m_mounted_visual_spec_baked = false;
  }
  [[nodiscard]] auto
  mount_archetype_id() const noexcept -> Render::Creature::ArchetypeId {
    return m_mount_archetype_id;
  }

protected:
  mutable Render::Creature::Pipeline::MountedSpec m_mounted_visual_spec_cache{};
  mutable bool m_mounted_visual_spec_baked{false};
  Render::Creature::ArchetypeId m_mount_archetype_id{
      Render::Creature::kInvalidArchetype};

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
                                  HorseMotionSample &motion) const;

  [[nodiscard]] auto
  resolve_mount_lod(const DrawContext &ctx) const -> HorseLOD;
};

} // namespace Render::GL
