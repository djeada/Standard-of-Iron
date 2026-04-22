#include "scale_barding_renderer.h"
#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

#include "../../../gl/primitives.h"
#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

auto scale_chest_barding_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_scale_barding_chest");
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.0F, -0.05F, 0.0F),
                                             QVector3D(0.40F, 0.32F, 0.35F)),
                             0, nullptr, 1.0F, 1);
    return std::move(builder).build();
  }();
  return archetype;
}

auto scale_barrel_barding_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_scale_barding_barrel");
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.35F, -0.10F, 0.0F),
                                             QVector3D(0.12F, 0.28F, 0.48F)),
                             0, nullptr, 1.0F, 1);
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(-0.35F, -0.10F, 0.0F),
                                             QVector3D(0.12F, 0.28F, 0.48F)),
                             0, nullptr, 1.0F, 1);
    return std::move(builder).build();
  }();
  return archetype;
}

auto scale_neck_barding_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_scale_barding_neck");
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.0F, 0.0F, 0.15F),
                                             QVector3D(0.36F, 0.30F, 0.38F)),
                             0, nullptr, 1.0F, 1);
    return std::move(builder).build();
  }();
  return archetype;
}

} // namespace

void ScaleBardingRenderer::submit(const DrawContext &ctx,
                                  const HorseBodyFrames &frames,
                                  const HorseVariant &variant,
                                  const HorseAnimationContext &,
                                  EquipmentBatch &batch) {
  std::array<QVector3D, 1> const palette{variant.tack_color * 0.85F};
  append_horse_attachment_archetype(batch, ctx, frames.chest,
                                    scale_chest_barding_archetype(), palette);
  append_horse_attachment_archetype(batch, ctx, frames.barrel,
                                    scale_barrel_barding_archetype(), palette);
  append_horse_attachment_archetype(batch, ctx, frames.neck_base,
                                    scale_neck_barding_archetype(), palette);
}

} // namespace Render::GL
