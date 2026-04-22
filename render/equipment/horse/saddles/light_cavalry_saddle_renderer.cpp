#include "light_cavalry_saddle_renderer.h"
#include "../horse_attachment_archetype.h"
#include "../../equipment_submit.h"

#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

auto light_cavalry_saddle_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("light_cavalry_horse_saddle");
    builder.add_palette_box(QVector3D(0.0F, 0.008F, 0.0F),
                            QVector3D(0.18F, 0.09F, 0.48F) * 0.21F, 0, nullptr,
                            1.0F, 4);
    builder.add_palette_box(QVector3D(0.0F, 0.016F, 0.11F),
                            QVector3D(0.07F, 0.28F, 0.16F) * 0.15F, 0, nullptr,
                            1.0F, 4);
    builder.add_palette_box(QVector3D(0.0F, 0.020F, -0.12F),
                            QVector3D(0.08F, 0.30F, 0.18F) * 0.16F, 0, nullptr,
                            1.0F, 4);
    return std::move(builder).build();
  }();
  return archetype;
}

} // namespace

void LightCavalrySaddleRenderer::submit(const DrawContext &ctx,
                                        const HorseBodyFrames &frames,
                                        const HorseVariant &variant,
                                        const HorseAnimationContext &,
                                        EquipmentBatch &batch) {
  std::array<QVector3D, 1> const palette{variant.saddle_color};
  append_horse_attachment_archetype(batch, ctx, frames.back_center,
                                    light_cavalry_saddle_archetype(), palette);
}

} // namespace Render::GL
