#include "blanket_renderer.h"
#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

auto blanket_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_blanket");
    builder.add_palette_box(QVector3D(0.0F, -0.005F, 0.0F),
                            QVector3D(0.20F, 0.015F, 0.34F) * 0.86F, 0, nullptr,
                            1.0F, 4);
    builder.add_palette_box(QVector3D(0.18F, -0.055F, 0.0F),
                            QVector3D(0.06F, 0.11F, 0.25F) * 0.70F, 0, nullptr,
                            1.0F, 4);
    builder.add_palette_box(QVector3D(-0.18F, -0.055F, 0.0F),
                            QVector3D(0.06F, 0.11F, 0.25F) * 0.70F, 0, nullptr,
                            1.0F, 4);
    return std::move(builder).build();
  }();
  return archetype;
}

} // namespace

void BlanketRenderer::submit(const DrawContext &ctx,
                             const HorseBodyFrames &frames,
                             const HorseVariant &variant,
                             const HorseAnimationContext &,
                             EquipmentBatch &batch) {
  std::array<QVector3D, 1> const palette{variant.blanket_color};
  append_horse_attachment_archetype(batch, ctx, frames.back_center,
                                    blanket_archetype(), palette);
}

} // namespace Render::GL
