#pragma once

#include "equipment_submit.h"

#include "../gl/humanoid/humanoid_types.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

namespace Render::GL {

inline auto make_humanoid_attachment_transform_scaled(
    const QMatrix4x4 &parent, const AttachmentFrame &frame,
    const QVector3D &local_offset, const QVector3D &axis_scale) -> QMatrix4x4 {
  QMatrix4x4 m = parent;
  QVector3D const world_pos =
      frame.origin + frame.right * (local_offset.x() * axis_scale.x()) +
      frame.up * (local_offset.y() * axis_scale.y()) +
      frame.forward * (local_offset.z() * axis_scale.z());
  m.translate(world_pos);

  QMatrix4x4 basis;
  basis.setColumn(0, QVector4D(frame.right * axis_scale.x(), 0.0F));
  basis.setColumn(1, QVector4D(frame.up * axis_scale.y(), 0.0F));
  basis.setColumn(2, QVector4D(frame.forward * axis_scale.z(), 0.0F));
  basis.setColumn(3, QVector4D(0.0F, 0.0F, 0.0F, 1.0F));
  return m * basis;
}

inline auto make_humanoid_attachment_transform(
    const QMatrix4x4 &parent, const AttachmentFrame &frame,
    const QVector3D &local_offset, float uniform_scale) -> QMatrix4x4 {
  return make_humanoid_attachment_transform_scaled(
      parent, frame, local_offset,
      QVector3D(frame.radius * uniform_scale, frame.radius * uniform_scale,
                frame.radius * uniform_scale));
}

inline void append_humanoid_attachment_archetype(
    EquipmentBatch &batch, const DrawContext &ctx, const AttachmentFrame &frame,
    const RenderArchetype &archetype, std::span<const QVector3D> palette = {},
    const QVector3D &local_offset = QVector3D(0.0F, 0.0F, 0.0F),
    float uniform_scale = 1.0F, Texture *default_texture = nullptr,
    float alpha_multiplier = 1.0F,
    RenderArchetypeLod lod = RenderArchetypeLod::Full) {
  append_equipment_archetype(batch, archetype,
                             make_humanoid_attachment_transform(
                                 ctx.model, frame, local_offset, uniform_scale),
                             palette, default_texture, alpha_multiplier, lod);
}

inline void append_humanoid_attachment_archetype_scaled(
    EquipmentBatch &batch, const DrawContext &ctx, const AttachmentFrame &frame,
    const RenderArchetype &archetype, const QVector3D &axis_scale,
    std::span<const QVector3D> palette = {},
    const QVector3D &local_offset = QVector3D(0.0F, 0.0F, 0.0F),
    Texture *default_texture = nullptr, float alpha_multiplier = 1.0F,
    RenderArchetypeLod lod = RenderArchetypeLod::Full) {
  append_equipment_archetype(batch, archetype,
                             make_humanoid_attachment_transform_scaled(
                                 ctx.model, frame, local_offset, axis_scale),
                             palette, default_texture, alpha_multiplier, lod);
}

} // namespace Render::GL
