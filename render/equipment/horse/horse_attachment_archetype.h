#pragma once

#include "../equipment_submit.h"

#include "../../entity/registry.h"
#include "../../horse/attachment_frames.h"

namespace Render::GL {

inline void append_horse_attachment_archetype(
    EquipmentBatch &batch, const DrawContext &ctx,
    const HorseAttachmentFrame &frame, const RenderArchetype &archetype,
    std::span<const QVector3D> palette = {}, Texture *default_texture = nullptr,
    float alpha_multiplier = 1.0F,
    RenderArchetypeLod lod = RenderArchetypeLod::Full) {
  append_equipment_archetype(
      batch, archetype,
      frame.make_local_transform(ctx.model, QVector3D(0.0F, 0.0F, 0.0F), 1.0F),
      palette, default_texture, alpha_multiplier, lod);
}

} // namespace Render::GL
