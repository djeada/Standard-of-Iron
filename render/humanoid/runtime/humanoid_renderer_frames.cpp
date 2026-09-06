#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

#include <memory>

#include "game/core/component_core.h"
#include "game/core/entity.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"
#include "game/visuals/team_colors.h"
#include "render/creature/pipeline/prepared_submit.h"
#include "render/creature/pipeline/unit_visual_spec.h"
#include "render/entity/registry.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/humanoid/asset/mesh_helpers.h"
#include "render/humanoid/runtime/frame_control.h"
#include "render/humanoid/runtime/humanoid_renderer.h"
#include "render/humanoid/runtime/prepare.h"
#include "render/palette.h"

namespace Render::GL {

void align_torso_mesh_forward(QMatrix4x4& model) noexcept {
  model.rotate(90.0F, 0.0F, 1.0F, 0.0F);
}

auto HumanoidRendererBase::frame_local_position(const AttachmentFrame& frame,
                                                const QVector3D& local) -> QVector3D {
  float const lx = local.x() * frame.radius;
  float const ly = local.y() * frame.radius;
  float const lz = local.z() * frame.radius;
  return frame.origin + frame.right * lx + frame.up * ly + frame.forward * lz;
}

auto HumanoidRendererBase::make_frame_local_transform(const QMatrix4x4& parent,
                                                      const AttachmentFrame& frame,
                                                      const QVector3D& local_offset,
                                                      float uniform_scale)
    -> QMatrix4x4 {
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

auto HumanoidRendererBase::head_local_position(const HeadFrame& frame,
                                               const QVector3D& local) -> QVector3D {
  return frame_local_position(frame, local);
}

auto HumanoidRendererBase::make_head_local_transform(const QMatrix4x4& parent,
                                                     const HeadFrame& frame,
                                                     const QVector3D& local_offset,
                                                     float uniform_scale)
    -> QMatrix4x4 {
  return make_frame_local_transform(parent, frame, local_offset, uniform_scale);
}

void HumanoidRendererBase::get_variant(const DrawContext& ctx,
                                       uint32_t seed,
                                       HumanoidVariant& v) const {
  QVector3D const team_tint = resolve_team_tint(ctx);
  v.palette = make_humanoid_palette(team_tint, seed);
  seed_missing_humanoid_wear(v, seed);
}

void HumanoidRendererBase::append_companion_preparation(
    const DrawContext&,
    const HumanoidVariant&,
    const HumanoidPose&,
    const HumanoidAnimationContext&,
    uint32_t,
    Render::Creature::CreatureLOD,
    Render::Creature::Pipeline::CreaturePreparationResult&) const {
}

auto HumanoidRendererBase::resolve_entity_ground_offset(
    const DrawContext&,
    Engine::Core::UnitComponent* unit_comp,
    Engine::Core::TransformComponent* transform_comp) const -> float {
  (void)unit_comp;
  (void)transform_comp;

  return 0.0F;
}

} // namespace Render::GL
