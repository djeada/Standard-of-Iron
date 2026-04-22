#include "crupper_renderer.h"
#include "../../equipment_submit.h"
#include "../horse_attachment_archetype.h"

#include "../../../gl/primitives.h"
#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

auto crupper_barding_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_crupper_barding");
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.0F, 0.02F, -0.15F),
                                             QVector3D(0.48F, 0.32F, 0.28F)),
                             0, nullptr, 1.0F, 1);
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.28F, -0.05F, -0.20F),
                                             QVector3D(0.20F, 0.25F, 0.22F)),
                             1, nullptr, 1.0F, 1);
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(-0.28F, -0.05F, -0.20F),
                                             QVector3D(0.20F, 0.25F, 0.22F)),
                             1, nullptr, 1.0F, 1);
    return std::move(builder).build();
  }();
  return archetype;
}

} // namespace

void CrupperRenderer::submit(const DrawContext &ctx,
                             const HorseBodyFrames &frames,
                             const HorseVariant &variant,
                             const HorseAnimationContext &,
                             EquipmentBatch &batch) {
  QVector3D const armor_color = variant.tack_color * 0.88F;
  std::array<QVector3D, 2> const palette{armor_color, armor_color * 0.95F};
  append_horse_attachment_archetype(batch, ctx, frames.rump,
                                    crupper_barding_archetype(), palette);
}

} // namespace Render::GL
