#include "saddle_bag_renderer.h"
#include "../horse_attachment_archetype.h"
#include "../../equipment_submit.h"

#include "../../../gl/primitives.h"
#include "../../../render_archetype.h"

#include <array>

namespace Render::GL {

namespace {

auto saddle_bag_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    RenderArchetypeBuilder builder("horse_saddle_bags");
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(0.28F, -0.12F, -0.15F),
                                             QVector3D(0.18F, 0.22F, 0.30F)),
                             0, nullptr, 1.0F, 4);
    builder.add_palette_mesh(get_unit_sphere(),
                             box_local_model(QVector3D(-0.28F, -0.12F, -0.15F),
                                             QVector3D(0.18F, 0.22F, 0.30F)),
                             0, nullptr, 1.0F, 4);
    return std::move(builder).build();
  }();
  return archetype;
}

} // namespace

void SaddleBagRenderer::submit(const DrawContext &ctx,
                               const HorseBodyFrames &frames,
                               const HorseVariant &variant,
                               const HorseAnimationContext &,
                               EquipmentBatch &batch) {
  QVector3D const bag_color = variant.saddle_color * 0.85F;
  std::array<QVector3D, 1> const palette{bag_color};
  append_horse_attachment_archetype(batch, ctx, frames.back_center,
                                    saddle_bag_archetype(), palette);

  const HorseAttachmentFrame &back = frames.back_center;
  for (float side : {1.0F, -1.0F}) {
    QVector3D const strap_top = back.origin + back.right * side * 0.28F +
                                back.up * 0.02F - back.forward * 0.10F;
    QVector3D const strap_bottom = back.origin + back.right * side * 0.28F -
                                   back.up * 0.12F - back.forward * 0.15F;
    batch.cylinders.push_back(
        {strap_top, strap_bottom, 0.012F, variant.tack_color, 1.0F});
  }
}

} // namespace Render::GL
