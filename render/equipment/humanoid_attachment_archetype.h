#pragma once

#include "equipment_submit.h"

#include "../gl/humanoid/humanoid_types.h"
#include "../instance_transform.h"
#include "../static_attachment_spec.h"

#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

inline auto make_humanoid_attachment_transform_scaled(
    const QMatrix4x4 &parent, const AttachmentFrame &frame,
    const QVector3D &local_offset, const QVector3D &axis_scale) -> QMatrix4x4 {
  return make_basis_attachment_transform_scaled(
      parent, frame.origin, frame.right, frame.up, frame.forward, local_offset,
      axis_scale);
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
