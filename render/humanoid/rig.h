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

void advance_pose_cache_frame();

inline auto calculate_humanoid_lod(float distance) -> HumanoidLOD {
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
                              uint32_t seed, HumanoidPose &io_pose) const;

  virtual void add_attachments(const DrawContext &ctx, const HumanoidVariant &v,
                              const HumanoidPose &pose,
                              const HumanoidAnimationContext &anim_ctx,
                              ISubmitter &out) const;

  virtual void draw_helmet(const DrawContext &ctx, const HumanoidVariant &v,
                           const HumanoidPose &pose, ISubmitter &out) const;

  virtual void draw_armor(const DrawContext &ctx, const HumanoidVariant &v,
                          const HumanoidPose &pose,
                          const HumanoidAnimationContext &anim,
                          ISubmitter &out) const;

  virtual void draw_armor_overlay(const DrawContext &ctx,
                                 const HumanoidVariant &v,
                                 const HumanoidPose &pose, float y_top_cover,
                                 float torso_r, float shoulder_half_span,
                                 float upper_arm_r, const QVector3D &right_axis,
                                 ISubmitter &out) const;

  virtual void draw_shoulder_decorations(const DrawContext &ctx,
                                       const HumanoidVariant &v,
                                       const HumanoidPose &pose,
                                       float y_top_cover, float y_neck,
                                       const QVector3D &right_axis,
                                       ISubmitter &out) const;

  virtual void draw_facial_hair(const DrawContext &ctx, const HumanoidVariant &v,
                              const HumanoidPose &pose, ISubmitter &out) const;

  void render(const DrawContext &ctx, ISubmitter &out) const;

  virtual auto resolve_entity_ground_offset(
      const DrawContext &ctx, Engine::Core::UnitComponent *unit_comp,
      Engine::Core::TransformComponent *transform_comp) const -> float;

  void draw_simplified_body(const DrawContext &ctx, const HumanoidVariant &v,
                          HumanoidPose &pose, ISubmitter &out) const;

  void draw_minimal_body(const DrawContext &ctx, const HumanoidVariant &v,
                       const HumanoidPose &pose, ISubmitter &out) const;

  static auto frame_local_position(const AttachmentFrame &frame,
                                 const QVector3D &local) -> QVector3D;

  static auto make_frame_local_transform(const QMatrix4x4 &parent,
                                      const AttachmentFrame &frame,
                                      const QVector3D &local_offset,
                                      float uniform_scale) -> QMatrix4x4;

  static auto head_local_position(const HeadFrame &frame,
                                const QVector3D &local) -> QVector3D;

  static auto make_head_local_transform(const QMatrix4x4 &parent,
                                     const HeadFrame &frame,
                                     const QVector3D &local_offset,
                                     float uniform_scale) -> QMatrix4x4;

protected:
  mutable QVector3D m_cached_proportion_scale;
  mutable bool m_proportion_scale_cached = false;

  static auto resolve_formation(const DrawContext &ctx) -> FormationParams;

  static void compute_locomotion_pose(uint32_t seed, float time, bool is_moving,
                                    const VariationParams &variation,
                                    HumanoidPose &io_pose);

  static auto resolve_team_tint(const DrawContext &ctx) -> QVector3D;

  void draw_common_body(const DrawContext &ctx, const HumanoidVariant &v,
                      HumanoidPose &pose, ISubmitter &out) const;
};

struct HumanoidRenderStats {
  uint32_t soldiers_total{0};
  uint32_t soldiers_rendered{0};
  uint32_t soldiers_skipped_frustum{0};
  uint32_t soldiers_skipped_lod{0};
  uint32_t poses_computed{0};
  uint32_t poses_cached{0};
  uint32_t lod_full{0};
  uint32_t lod_reduced{0};
  uint32_t lod_minimal{0};

  void reset() {
    soldiers_total = 0;
    soldiers_rendered = 0;
    soldiers_skipped_frustum = 0;
    soldiers_skipped_lod = 0;
    poses_computed = 0;
    poses_cached = 0;
    lod_full = 0;
    lod_reduced = 0;
    lod_minimal = 0;
  }
};

void advance_pose_cache_frame();

auto get_humanoid_render_stats() -> const HumanoidRenderStats &;

void reset_humanoid_render_stats();

} // namespace Render::GL
