#include "roman_saddle_renderer.h"
#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

auto roman_saddle_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("roman_horse_saddle");
    builder.add_palette_box(QVector3D(0.0F, 0.012F, 0.0F),
                            QVector3D(0.20F, 0.11F, 0.54F) * 0.22F, 0, nullptr,
                            1.0F, 4);
    builder.add_palette_box(QVector3D(0.0F, 0.025F, 0.12F),
                            QVector3D(0.10F, 0.38F, 0.18F) * 0.18F, 0, nullptr,
                            1.0F, 4);
    builder.add_palette_box(QVector3D(0.0F, 0.03F, -0.11F),
                            QVector3D(0.12F, 0.44F, 0.22F) * 0.20F, 0, nullptr,
                            1.0F, 4);
    builder.add_palette_box(QVector3D(0.0F, 0.04F, 0.06F),
                            QVector3D(0.08F, 0.28F, 0.08F) * 0.14F, 0, nullptr,
                            1.0F, 4);
    builder.add_palette_box(QVector3D(0.0F, 0.04F, -0.06F),
                            QVector3D(0.08F, 0.28F, 0.08F) * 0.14F, 0, nullptr,
                            1.0F, 4);
    return std::move(builder).build();
  }();
  return archetype;
}

} // namespace

void RomanSaddleRenderer::submit(const DrawContext &ctx,
                                 const HorseBodyFrames &frames,
                                 const HorseVariant &variant,
                                 const HorseAnimationContext &,
                                 EquipmentBatch &batch) {
  std::array<QVector3D, 1> const palette{variant.saddle_color};
  append_horse_attachment_archetype(batch, ctx, frames.back_center,
                                    roman_saddle_archetype(), palette);
}

} // namespace Render::GL
