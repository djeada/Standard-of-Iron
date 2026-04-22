#include "stirrup_renderer.h"
#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

#include "../../../geom/transforms.h"
#include "../../../gl/primitives.h"
#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

auto stirrup_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_stirrups");
    for (float side : {1.0F, -1.0F}) {
      QVector3D const stirrup_attach(side * 0.45F, -0.02F, 0.0F);
      QVector3D const stirrup_bottom =
          stirrup_attach - QVector3D(0.0F, 0.30F, 0.0F);
      builder.add_palette_mesh(get_unit_cylinder(),
                               Render::Geom::cylinder_between(
                                   stirrup_attach, stirrup_bottom, 0.008F),
                               0, nullptr, 1.0F, 4);
      builder.add_palette_mesh(
          get_unit_sphere(),
          box_local_model(stirrup_bottom, QVector3D(0.10F, 0.015F, 0.12F)), 0,
          nullptr, 1.0F, 4);
    }
    return std::move(builder).build();
  }();
  return archetype;
}

} // namespace

void StirrupRenderer::submit(const DrawContext &ctx,
                             const HorseBodyFrames &frames,
                             const HorseVariant &variant,
                             const HorseAnimationContext &,
                             EquipmentBatch &batch) {
  std::array<QVector3D, 1> const palette{variant.tack_color};
  append_horse_attachment_archetype(batch, ctx, frames.back_center,
                                    stirrup_archetype(), palette);
}

} // namespace Render::GL
