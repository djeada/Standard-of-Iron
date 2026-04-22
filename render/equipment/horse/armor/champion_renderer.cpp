#include "champion_renderer.h"
#include "../horse_attachment_archetype.h"
#include "../../equipment_submit.h"

#include "../../../gl/primitives.h"
#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

auto champion_barding_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_champion_barding");
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.0F, 0.0F, 0.0F),
                                             QVector3D(0.42F, 0.35F, 0.38F)),
                             0, nullptr, 1.0F, 1);
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.0F, 0.12F, 0.05F),
                                             QVector3D(0.38F, 0.18F, 0.32F)),
                             1, nullptr, 1.0F, 1);
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.0F, -0.12F, 0.05F),
                                             QVector3D(0.38F, 0.18F, 0.32F)),
                             2, nullptr, 1.0F, 1);
    return std::move(builder).build();
  }();
  return archetype;
}

} // namespace

void ChampionRenderer::submit(const DrawContext &ctx,
                              const HorseBodyFrames &frames,
                              const HorseVariant &variant,
                              const HorseAnimationContext &,
                              EquipmentBatch &batch) {
  QVector3D const armor_color = variant.tack_color * 0.82F;
  std::array<QVector3D, 3> const palette{
      armor_color, armor_color * 1.05F, armor_color * 0.95F};
  append_horse_attachment_archetype(batch, ctx, frames.chest,
                                    champion_barding_archetype(), palette);
}

} // namespace Render::GL
