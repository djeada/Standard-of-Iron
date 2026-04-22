#include "carthage_saddle_renderer.h"
#include "../horse_attachment_archetype.h"
#include "../../equipment_submit.h"

#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

auto carthage_saddle_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("carthage_horse_saddle");
    builder.add_palette_box(QVector3D(0.0F, 0.010F, 0.0F),
                            QVector3D(0.22F, 0.10F, 0.58F) * 0.22F, 0, nullptr,
                            1.0F, 4);
    builder.add_palette_box(QVector3D(0.0F, 0.020F, 0.13F),
                            QVector3D(0.08F, 0.32F, 0.18F) * 0.17F, 0, nullptr,
                            1.0F, 4);
    builder.add_palette_box(QVector3D(0.0F, 0.026F, -0.13F),
                            QVector3D(0.12F, 0.46F, 0.24F) * 0.19F, 0, nullptr,
                            1.0F, 4);
    return std::move(builder).build();
  }();
  return archetype;
}

} // namespace

void CarthageSaddleRenderer::submit(const DrawContext &ctx,
                                    const HorseBodyFrames &frames,
                                    const HorseVariant &variant,
                                    const HorseAnimationContext &,
                                    EquipmentBatch &batch) {
  std::array<QVector3D, 1> const palette{variant.saddle_color};
  append_horse_attachment_archetype(batch, ctx, frames.back_center,
                                    carthage_saddle_archetype(), palette);
}

} // namespace Render::GL
