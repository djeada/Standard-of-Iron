#pragma once

#include "../entity/registry.h"
#include "../gl/humanoid/humanoid_types.h"
#include "../gl/mesh.h"
#include "humanoid_specs.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cstdint>

namespace Engine::Core {
class Entity;
class World;
class MovementComponent;
class TransformComponent;
class UnitComponent;
} // namespace Engine::Core

namespace Render::GL {

auto torso_mesh_without_bottom_cap() -> Mesh *;

class HumanoidRendererBase {
public:
  virtual ~HumanoidRendererBase() = default;

  virtual auto get_proportion_scaling() const -> QVector3D {
    return {1.0F, 1.0F, 1.0F};
  }

  virtual auto get_mount_scale() const -> float { return 1.0F; }

  virtual void adjust_variation(const DrawContext &, uint32_t,
                                VariationParams &) const {}

  virtual void get_variant(const DrawContext &ctx, uint32_t seed,
                           HumanoidVariant &v) const;

  virtual void customize_pose(const DrawContext &ctx,
                              const HumanoidAnimationContext &anim_ctx,
                              uint32_t seed, HumanoidPose &ioPose) const;

  virtual void addAttachments(const DrawContext &ctx, const HumanoidVariant &v,
                              const HumanoidPose &pose,
                              const HumanoidAnimationContext &anim_ctx,
                              ISubmitter &out) const;

  virtual void draw_helmet(const DrawContext &ctx, const HumanoidVariant &v,
                           const HumanoidPose &pose, ISubmitter &out) const;

  virtual void draw_armor(const DrawContext &ctx, const HumanoidVariant &v,
                          const HumanoidPose &pose,
                          const HumanoidAnimationContext &anim,
                          ISubmitter &out) const;

  virtual void draw_armorOverlay(const DrawContext &ctx,
                                 const HumanoidVariant &v,
                                 const HumanoidPose &pose, float y_top_cover,
                                 float torso_r, float shoulder_half_span,
                                 float upper_arm_r, const QVector3D &right_axis,
                                 ISubmitter &out) const;

  virtual void drawShoulderDecorations(const DrawContext &ctx,
                                       const HumanoidVariant &v,
                                       const HumanoidPose &pose,
                                       float y_top_cover, float y_neck,
                                       const QVector3D &right_axis,
                                       ISubmitter &out) const;

  virtual void drawFacialHair(const DrawContext &ctx, const HumanoidVariant &v,
                              const HumanoidPose &pose, ISubmitter &out) const;

  void render(const DrawContext &ctx, ISubmitter &out) const;

  virtual auto resolve_entity_ground_offset(
      const DrawContext &ctx, Engine::Core::UnitComponent *unit_comp,
      Engine::Core::TransformComponent *transform_comp) const -> float;

  static auto frameLocalPosition(const AttachmentFrame &frame,
                                 const QVector3D &local) -> QVector3D;

  static auto makeFrameLocalTransform(const QMatrix4x4 &parent,
                                      const AttachmentFrame &frame,
                                      const QVector3D &local_offset,
                                      float uniform_scale) -> QMatrix4x4;

  static auto headLocalPosition(const HeadFrame &frame,
                                const QVector3D &local) -> QVector3D;

  static auto makeHeadLocalTransform(const QMatrix4x4 &parent,
                                     const HeadFrame &frame,
                                     const QVector3D &local_offset,
                                     float uniform_scale) -> QMatrix4x4;

protected:
  mutable QVector3D m_cachedProportionScale;
  mutable bool m_proportionScaleCached = false;

  static auto resolveFormation(const DrawContext &ctx) -> FormationParams;

  static void computeLocomotionPose(uint32_t seed, float time, bool isMoving,
                                    const VariationParams &variation,
                                    HumanoidPose &ioPose);

  static auto resolveTeamTint(const DrawContext &ctx) -> QVector3D;

  void drawCommonBody(const DrawContext &ctx, const HumanoidVariant &v,
                      HumanoidPose &pose, ISubmitter &out) const;
};

} // namespace Render::GL
