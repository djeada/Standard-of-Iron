#include "headwrap.h"

#include "../generated_equipment.h"
#include "../humanoid_attachment_archetype.h"

#include "../../humanoid/style_palette.h"

#include <array>

namespace Render::GL {

using Render::GL::Humanoid::saturate_color;

namespace {

enum HeadwrapPaletteSlot : std::uint8_t {
  k_band_slot = 0U,
  k_knot_slot = 1U,
  k_tail_slot = 2U,
};

auto headwrap_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 3> const primitives{{
        generated_cylinder(QVector3D(0.0F, 0.30F, 0.0F),
                           QVector3D(0.0F, 0.70F, 0.0F), 1.08F, k_band_slot),
        generated_sphere(QVector3D(0.10F, 0.60F, 0.72F), 0.32F, k_knot_slot),
        generated_cylinder(QVector3D(0.02F, 0.55F, 0.66F),
                           QVector3D(0.04F, 0.27F, 0.58F), 0.28F, k_tail_slot),
    }};
    return build_generated_equipment_archetype("headwrap", primitives);
  }();
  return archetype;
}

auto headwrap_palette(const HumanoidPalette &palette)
    -> std::array<QVector3D, 3> {
  QVector3D const cloth =
      saturate_color(palette.cloth * QVector3D(0.9F, 1.05F, 1.05F));
  return {cloth, cloth * 1.05F, cloth * QVector3D(0.92F, 0.98F, 1.05F)};
}

} // namespace

void HeadwrapRenderer::render(const DrawContext &ctx, const BodyFrames &frames,
                              const HumanoidPalette &palette,
                              const HumanoidAnimationContext &anim,
                              EquipmentBatch &batch) {
  submit({}, ctx, frames, palette, anim, batch);
}

void HeadwrapRenderer::submit(const HeadwrapConfig &, const DrawContext &ctx,
                              const BodyFrames &frames,
                              const HumanoidPalette &palette,
                              const HumanoidAnimationContext &anim,
                              EquipmentBatch &batch) {
  (void)anim;

  if (frames.head.radius <= 0.0F) {
    return;
  }

  auto const equipment_palette = headwrap_palette(palette);
  append_humanoid_attachment_archetype(batch, ctx, frames.head,
                                       headwrap_archetype(), equipment_palette);
}

} // namespace Render::GL
