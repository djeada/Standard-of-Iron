#include "roman_heavy_helmet.h"

#include "../generated_equipment.h"
#include "../humanoid_attachment_archetype.h"

#include "../../humanoid/style_palette.h"

#include <array>

namespace Render::GL {

using Render::GL::Humanoid::saturate_color;

namespace {

enum RomanHeavyPaletteSlot : std::uint8_t {
  k_steel_slot = 0U,
  k_steel_light_slot = 1U,
  k_brass_slot = 2U,
  k_crest_slot = 3U,
};

auto roman_heavy_helmet_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 7> const primitives{{
        generated_cylinder(QVector3D(0.0F, -0.38F, 0.0F),
                           QVector3D(0.0F, 1.70F, 0.0F), 1.52F, k_steel_slot,
                           1.0F, 2),
        generated_cylinder(QVector3D(0.0F, 1.70F, 0.0F),
                           QVector3D(0.0F, 1.92F, 0.0F), 1.43F,
                           k_steel_light_slot, 1.0F, 2),
        generated_cylinder(QVector3D(0.0F, 0.068F, 0.0F),
                           QVector3D(0.0F, 0.145F, 0.0F), 1.61F, k_brass_slot,
                           1.0F, 2),
        generated_cylinder(QVector3D(0.0F, -0.35F, -1.02F),
                           QVector3D(0.0F, -0.12F, -1.08F), 1.37F, k_steel_slot,
                           1.0F, 2),
        generated_cylinder(QVector3D(0.0F, 1.92F, 0.0F),
                           QVector3D(0.0F, 2.07F, 0.0F), 0.032F, k_brass_slot,
                           1.0F, 2),
        generated_cone(QVector3D(0.0F, 2.07F, 0.0F),
                       QVector3D(0.0F, 2.35F, 0.0F), 0.075F, k_crest_slot, 1.0F,
                       0),
        generated_sphere(QVector3D(0.0F, 2.35F, 0.0F), 0.034F, k_brass_slot,
                         1.0F, 2),
    }};
    return build_generated_equipment_archetype("roman_heavy_helmet",
                                               primitives);
  }();
  return archetype;
}

auto roman_heavy_palette(const HumanoidPalette &palette)
    -> std::array<QVector3D, 4> {
  QVector3D const steel =
      saturate_color(palette.metal * QVector3D(0.88F, 0.92F, 1.08F));
  QVector3D const steel_light = saturate_color(steel * 1.06F);
  QVector3D const brass =
      saturate_color(palette.metal * QVector3D(1.40F, 1.15F, 0.65F));
  return {steel, steel_light, brass, QVector3D(0.96F, 0.12F, 0.12F)};
}

} // namespace

void RomanHeavyHelmetRenderer::render(const DrawContext &ctx,
                                      const BodyFrames &frames,
                                      const HumanoidPalette &palette,
                                      const HumanoidAnimationContext &anim,
                                      EquipmentBatch &batch) {
  submit({}, ctx, frames, palette, anim, batch);
}

void RomanHeavyHelmetRenderer::submit(const RomanHeavyHelmetConfig &,
                                      const DrawContext &ctx,
                                      const BodyFrames &frames,
                                      const HumanoidPalette &palette,
                                      const HumanoidAnimationContext &anim,
                                      EquipmentBatch &batch) {
  (void)anim;

  if (frames.head.radius <= 0.0F) {
    return;
  }

  auto const equipment_palette = roman_heavy_palette(palette);
  append_humanoid_attachment_archetype(
      batch, ctx, frames.head, roman_heavy_helmet_archetype(),
      equipment_palette, QVector3D(0.0F, 0.05F, 0.0F));
}

} // namespace Render::GL
