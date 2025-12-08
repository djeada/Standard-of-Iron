#pragma once

#include "../entity/registry.h"
#include "../gl/humanoid/humanoid_types.h"
#include "../gl/mesh.h"
#include "../graphics_settings.h"
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

void advancePoseCacheFrame();

inline auto calculateHumanoidLOD(float distance) -> HumanoidLOD {
  const auto &settings = Render::GraphicsSettings::instance();
  if (distance < settings.humanoidFullDetailDistance()) {
    return HumanoidLOD::Full;
  }
  if (distance < settings.humanoidReducedDetailDistance()) {
    return HumanoidLOD::Reduced;
  }
  if (distance < settings.humanoidMinimalDetailDistance()) {
    return HumanoidLOD::Minimal;
  }
  return HumanoidLOD::Billboard;
}

class HumanoidRendererBase {
public:
  virtual ~HumanoidRendererBase() = default;

  virtual auto get_proportion_scaling() const -> QVector3D {
    return {1.0F, 1.0F, 1.0F};
  }

  virtual auto get_torso_scale() const -> float {
    return get_proportion_scaling().x();
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

  void drawSimplifiedBody(const DrawContext &ctx, const HumanoidVariant &v,
                          HumanoidPose &pose, ISubmitter &out) const;

  void drawMinimalBody(const DrawContext &ctx, const HumanoidVariant &v,
                       const HumanoidPose &pose, ISubmitter &out) const;

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

struct HumanoidRenderStats {
  uint32_t soldiersTotal{0};
  uint32_t soldiersRendered{0};
  uint32_t soldiersSkippedFrustum{0};
  uint32_t soldiersSkippedLOD{0};
  uint32_t posesComputed{0};
  uint32_t posesCached{0};
  uint32_t lodFull{0};
  uint32_t lodReduced{0};
  uint32_t lodMinimal{0};

  void reset() {
    soldiersTotal = 0;
    soldiersRendered = 0;
    soldiersSkippedFrustum = 0;
    soldiersSkippedLOD = 0;
    posesComputed = 0;
    posesCached = 0;
    lodFull = 0;
    lodReduced = 0;
    lodMinimal = 0;
  }
};

void advancePoseCacheFrame();

auto getHumanoidRenderStats() -> const HumanoidRenderStats &;

void resetHumanoidRenderStats();

} // namespace Render::GL
