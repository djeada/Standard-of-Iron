#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "animation/rig/humanoid_proportions.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/humanoid_types.h"
#include "render/humanoid/asset/humanoid_capabilities.h"

namespace Engine::Core {
class Entity;
class World;
class MovementComponent;
class TransformComponent;
class UnitComponent;
} // namespace Engine::Core

namespace Render::Creature::Pipeline {
struct CreaturePreparationResult;
}

namespace Render::GL {
struct DrawContext;
struct AnimationInputs;
class HumanoidRendererBase;
} // namespace Render::GL

namespace Render::Humanoid {
using HumanoidPreparation = Render::Creature::Pipeline::CreaturePreparationResult;
struct HumanoidRuntimeContext;
void prepare_humanoid_instances(const ::Render::GL::HumanoidRendererBase& owner,
                                const ::Render::GL::DrawContext& ctx,
                                const ::Render::GL::AnimationInputs& anim,
                                HumanoidRuntimeContext& runtime,
                                HumanoidPreparation& out);
} // namespace Render::Humanoid

namespace Render::GL {

class HumanoidRendererBase : public IParallelPreparer {
public:
  HumanoidRendererBase() = default;

  explicit HumanoidRendererBase(Render::Creature::Pipeline::UnitVisualSpec spec) {
    set_visual_spec(std::move(spec));
  }

  HumanoidRendererBase(const HumanoidRendererBase&) = delete;
  auto operator=(const HumanoidRendererBase&) -> HumanoidRendererBase& = delete;
  HumanoidRendererBase(HumanoidRendererBase&&) = delete;
  auto operator=(HumanoidRendererBase&&) -> HumanoidRendererBase& = delete;

  virtual ~HumanoidRendererBase() = default;

  [[nodiscard]] auto get_proportion_scaling() const noexcept -> QVector3D {
    return m_visual_spec.scaling.as_vector();
  }

  virtual auto get_torso_scale() const -> float { return get_proportion_scaling().x(); }

  virtual auto get_mount_scale() const -> float { return 1.0F; }

  virtual auto get_hold_kneel_depth() const -> float { return 1.0F; }

  [[nodiscard]] auto
  visual_spec() const noexcept -> const Render::Creature::Pipeline::UnitVisualSpec& {
    return m_visual_spec;
  }

  virtual auto uses_mounted_pipeline() const noexcept -> bool { return false; }

  virtual void adjust_variation(const DrawContext&, uint32_t, VariationParams&) const {}

  virtual void
  get_variant(const DrawContext& ctx, uint32_t seed, HumanoidVariant& v) const;

  void render(const DrawContext& ctx, ISubmitter& out) const;

  void ensure_prepare_components(Engine::Core::Entity& entity) const override;

  void
  prepare(const DrawContext& ctx,
          Render::Creature::Pipeline::CreaturePreparationResult& out) const override;

  virtual auto resolve_entity_ground_offset(
      const DrawContext& ctx,
      Engine::Core::UnitComponent* unit_comp,
      Engine::Core::TransformComponent* transform_comp) const -> float;

  static auto frame_local_position(const AttachmentFrame& frame,
                                   const QVector3D& local) -> QVector3D;

  static auto make_frame_local_transform(const QMatrix4x4& parent,
                                         const AttachmentFrame& frame,
                                         const QVector3D& local_offset,
                                         float uniform_scale) -> QMatrix4x4;

  static auto head_local_position(const HeadFrame& frame,
                                  const QVector3D& local) -> QVector3D;

  static auto make_head_local_transform(const QMatrix4x4& parent,
                                        const HeadFrame& frame,
                                        const QVector3D& local_offset,
                                        float uniform_scale) -> QMatrix4x4;

  static void compute_locomotion_pose(uint32_t seed,
                                      float time,
                                      const HumanoidGaitDescriptor& gait,
                                      const VariationParams& variation,
                                      HumanoidPose& io_pose);

  static void compute_locomotion_pose(uint32_t seed,
                                      float time,
                                      bool is_moving,
                                      const VariationParams& variation,
                                      HumanoidPose& io_pose);

  static auto resolve_formation(const HumanoidRendererBase& owner,
                                const DrawContext& ctx) -> FormationParams;

  virtual void append_companion_preparation(
      const DrawContext& ctx,
      const HumanoidVariant& variant,
      const HumanoidPose& pose,
      const HumanoidAnimationContext& anim_ctx,
      std::uint32_t seed,
      Render::Creature::CreatureLOD lod,
      Render::Creature::Pipeline::CreaturePreparationResult& out) const;

protected:
  void set_visual_spec(Render::Creature::Pipeline::UnitVisualSpec spec) {
    m_visual_spec = std::move(spec);
    m_visual_spec.capabilities =
        Render::Humanoid::resolve_humanoid_capabilities(m_visual_spec.archetype_id);
  }

  static auto resolve_team_tint(const DrawContext& ctx) -> QVector3D;

private:
  Render::Creature::Pipeline::UnitVisualSpec m_visual_spec{
      .debug_name = "humanoid/default",
      .kind = Render::Creature::Pipeline::CreatureKind::Humanoid};
};

void register_humanoid_renderer(EntityRendererRegistry& registry,
                                std::string key,
                                std::shared_ptr<const HumanoidRendererBase> renderer);

} // namespace Render::GL
