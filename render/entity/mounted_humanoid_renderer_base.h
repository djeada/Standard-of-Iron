#pragma once

#include <string>
#include <string_view>

#include "horse_renderer.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/humanoid/runtime/humanoid_renderer.h"

namespace Render::GL {

class MountedHumanoidRendererBase : public HumanoidRendererBase {
public:
  MountedHumanoidRendererBase();
  ~MountedHumanoidRendererBase() override = default;

  [[nodiscard]] auto mounted_visual_spec() const noexcept
      -> const Render::Creature::Pipeline::MountedSpec& {
    return m_mounted_visual_spec;
  }

  auto uses_mounted_pipeline() const noexcept -> bool override { return true; }

  void ensure_prepare_components(Engine::Core::Entity& entity) const override;

  auto get_mount_scale() const -> float override = 0;

  void set_mount_visual(Render::Creature::ArchetypeId id, std::string_view debug_name) {
    m_mount_archetype_id = id;
    m_mount_debug_name = debug_name;
    rebuild_mounted_visual_spec();
  }
  [[nodiscard]] auto
  mount_archetype_id() const noexcept -> Render::Creature::ArchetypeId {
    return m_mount_archetype_id;
  }

protected:
  Render::Creature::Pipeline::MountedSpec m_mounted_visual_spec{};
  Render::Creature::ArchetypeId m_mount_archetype_id{
      Render::Creature::k_invalid_archetype};
  std::string m_mount_debug_name{"troops/mounted/horse"};

  auto resolve_entity_ground_offset(
      const DrawContext& ctx,
      Engine::Core::UnitComponent* unit_comp,
      Engine::Core::TransformComponent* transform_comp) const -> float override;

  auto get_scaled_horse_dimensions(uint32_t seed) const -> HorseDimensions;

  HorseRenderer m_horse_renderer;

  void append_companion_preparation(
      const DrawContext& ctx,
      const HumanoidVariant& variant,
      const HumanoidPose& pose,
      const HumanoidAnimationContext& anim_ctx,
      std::uint32_t seed,
      Render::Creature::CreatureLOD lod,
      Render::Creature::Pipeline::CreaturePreparationResult& out) const override;

  void resolve_mount_render_state(const DrawContext& ctx,
                                  std::uint32_t seed,
                                  const HumanoidVariant& variant,
                                  const HumanoidAnimationContext& anim_ctx,
                                  bool use_cached_profile,
                                  HorseProfile& profile,
                                  HorseDimensions& dims,
                                  MountedAttachmentFrame& mount,
                                  HorseMotionSample& motion) const;

private:
  void rebuild_mounted_visual_spec();

  [[nodiscard]] auto
  resolve_mount_lod(const DrawContext& ctx) const -> Render::Creature::CreatureLOD;
};

} // namespace Render::GL
