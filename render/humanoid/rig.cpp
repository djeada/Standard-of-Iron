#include "cache_control.h"
#include "humanoid_renderer_base.h"
#include "mesh_helpers.h"
#include "prepare.h"

#include "../../game/core/component.h"
#include "../../game/core/entity.h"
#include "../../game/units/spawn_type.h"
#include "../../game/units/troop_config.h"
#include "../../game/visuals/team_colors.h"
#include "../creature/pipeline/prepared_submit.h"
#include "../creature/pipeline/unit_visual_spec.h"
#include "../entity/registry.h"
#include "../geom/parts.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "../palette.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <algorithm>
#include <memory>

namespace Render::GL {

auto torso_mesh_without_bottom_cap() -> Mesh * {
  static std::unique_ptr<Mesh> s_mesh;
  if (s_mesh != nullptr) {
    return s_mesh.get();
  }

  Mesh *base = get_unit_torso();
  if (base == nullptr) {
    return nullptr;
  }

  auto filtered = base->clone_with_filtered_indices(
      [](unsigned int a, unsigned int b, unsigned int c,
         const std::vector<Vertex> &verts) -> bool {
        auto sample = [&](unsigned int idx) -> QVector3D {
          return {verts[idx].position[0], verts[idx].position[1],
                  verts[idx].position[2]};
        };
        QVector3D pa = sample(a);
        QVector3D pb = sample(b);
        QVector3D pc = sample(c);
        float min_y = std::min({pa.y(), pb.y(), pc.y()});
        float max_y = std::max({pa.y(), pb.y(), pc.y()});

        QVector3D n(
            verts[a].normal[0] + verts[b].normal[0] + verts[c].normal[0],
            verts[a].normal[1] + verts[b].normal[1] + verts[c].normal[1],
            verts[a].normal[2] + verts[b].normal[2] + verts[c].normal[2]);
        if (n.lengthSquared() > 0.0F) {
          n.normalize();
        }

        constexpr float k_band_height = 0.02F;
        constexpr float k_bottom_threshold = 0.45F;
        bool is_flat = (max_y - min_y) < k_band_height;
        bool is_at_bottom = min_y > k_bottom_threshold;
        bool facing_down = (n.y() > 0.35F);
        return is_flat && is_at_bottom && facing_down;
      });

  s_mesh =
      (filtered != nullptr) ? std::move(filtered) : std::unique_ptr<Mesh>(base);
  return s_mesh.get();
}

void align_torso_mesh_forward(QMatrix4x4 &model) noexcept {
  model.rotate(90.0F, 0.0F, 1.0F, 0.0F);
}

auto HumanoidRendererBase::frame_local_position(
    const AttachmentFrame &frame, const QVector3D &local) -> QVector3D {
  float const lx = local.x() * frame.radius;
  float const ly = local.y() * frame.radius;
  float const lz = local.z() * frame.radius;
  return frame.origin + frame.right * lx + frame.up * ly + frame.forward * lz;
}

auto HumanoidRendererBase::make_frame_local_transform(
    const QMatrix4x4 &parent, const AttachmentFrame &frame,
    const QVector3D &local_offset, float uniform_scale) -> QMatrix4x4 {
  float scale = frame.radius * uniform_scale;
  if (scale == 0.0F) {
    scale = uniform_scale;
  }

  QVector3D const origin = frame_local_position(frame, local_offset);

  QMatrix4x4 local;
  local.setColumn(0, QVector4D(frame.right * scale, 0.0F));
  local.setColumn(1, QVector4D(frame.up * scale, 0.0F));
  local.setColumn(2, QVector4D(frame.forward * scale, 0.0F));
  local.setColumn(3, QVector4D(origin, 1.0F));
  return parent * local;
}

auto HumanoidRendererBase::head_local_position(
    const HeadFrame &frame, const QVector3D &local) -> QVector3D {
  return frame_local_position(frame, local);
}

auto HumanoidRendererBase::make_head_local_transform(
    const QMatrix4x4 &parent, const HeadFrame &frame,
    const QVector3D &local_offset, float uniform_scale) -> QMatrix4x4 {
  return make_frame_local_transform(parent, frame, local_offset, uniform_scale);
}

void HumanoidRendererBase::get_variant(const DrawContext &ctx, uint32_t seed,
                                       HumanoidVariant &v) const {
  QVector3D const team_tint = resolve_team_tint(ctx);
  v.palette = make_humanoid_palette(team_tint, seed);
}

void HumanoidRendererBase::customize_pose(const DrawContext &,
                                          const HumanoidAnimationContext &,
                                          uint32_t, HumanoidPose &) const {}

void HumanoidRendererBase::add_attachments(const DrawContext &,
                                           const HumanoidVariant &,
                                           const HumanoidPose &,
                                           const HumanoidAnimationContext &,
                                           ISubmitter &) const {}

void HumanoidRendererBase::append_companion_preparation(
    const DrawContext &, const HumanoidVariant &, const HumanoidPose &,
    const HumanoidAnimationContext &, uint32_t, Render::Creature::CreatureLOD,
    Render::Creature::Pipeline::CreaturePreparationResult &) const {}

auto HumanoidRendererBase::visual_spec() const
    -> const Render::Creature::Pipeline::UnitVisualSpec & {

  static thread_local Render::Creature::Pipeline::UnitVisualSpec spec;
  spec.kind = Render::Creature::Pipeline::CreatureKind::Humanoid;
  spec.debug_name = "humanoid/default";
  spec.equipment = {};
  spec.pose_hook = nullptr;
  spec.variant_hook = nullptr;
  const QVector3D ps = get_proportion_scaling();
  spec.scaling =
      Render::Creature::Pipeline::ProportionScaling{ps.x(), ps.y(), ps.z()};
  return spec;
}

auto HumanoidRendererBase::resolve_entity_ground_offset(
    const DrawContext &, Engine::Core::UnitComponent *unit_comp,
    Engine::Core::TransformComponent *transform_comp) const -> float {
  (void)unit_comp;
  (void)transform_comp;

  return 0.0F;
}

} // namespace Render::GL
