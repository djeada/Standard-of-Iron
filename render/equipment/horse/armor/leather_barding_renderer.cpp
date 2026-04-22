#include "leather_barding_renderer.h"
#include "../horse_attachment_archetype.h"
#include "../../equipment_submit.h"

#include "../../../gl/primitives.h"
#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

auto leather_chest_barding_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_leather_barding_chest");
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.0F, -0.03F, 0.0F),
                                             QVector3D(0.38F, 0.28F, 0.32F)),
                             0, nullptr, 1.0F, 1);
    return std::move(builder).build();
  }();
  return archetype;
}

auto leather_barrel_barding_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_leather_barding_barrel");
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.32F, -0.08F, 0.0F),
                                             QVector3D(0.10F, 0.25F, 0.45F)),
                             0, nullptr, 1.0F, 1);
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(-0.32F, -0.08F, 0.0F),
                                             QVector3D(0.10F, 0.25F, 0.45F)),
                             0, nullptr, 1.0F, 1);
    return std::move(builder).build();
  }();
  return archetype;
}

} // namespace

void LeatherBardingRenderer::submit(const DrawContext &ctx,
                                    const HorseBodyFrames &frames,
                                    const HorseVariant &variant,
                                    const HorseAnimationContext &,
                                    EquipmentBatch &batch) {
  std::array<QVector3D, 1> const palette{variant.saddle_color * 0.90F};
  append_horse_attachment_archetype(batch, ctx, frames.chest,
                                    leather_chest_barding_archetype(), palette);
  append_horse_attachment_archetype(batch, ctx, frames.barrel,
                                    leather_barrel_barding_archetype(), palette);
}

} // namespace Render::GL
