#include "roman_light_helmet.h"

#include "../generated_equipment.h"
#include "../humanoid_attachment_archetype.h"

#include "../../humanoid/style_palette.h"

#include <array>

namespace Render::GL {

using Render::GL::Humanoid::saturate_color;

namespace {

enum RomanLightPaletteSlot : std::uint8_t {
  k_metal_slot = 0U,
  k_metal_accent_slot = 1U,
  k_crest_slot = 2U,
};

auto roman_light_helmet_archetype() -> const RenderArchetype & {
  static const RenderArchetype archetype = [] {
    std::array<GeneratedEquipmentPrimitive, 8> const primitives{{
        generated_cylinder(QVector3D(0.0F, -0.12F, 0.0F),
                           QVector3D(0.0F, 1.58F, 0.0F), 1.42F, k_metal_slot,
                           1.0F, 2),
        generated_cone(QVector3D(0.0F, 1.58F, 0.0F),
                       QVector3D(0.0F, 1.96F, 0.0F), 1.35F, k_metal_accent_slot,
                       1.0F, 2),
        generated_cylinder(QVector3D(0.0F, 0.207F, 0.0F),
                           QVector3D(0.0F, 0.233F, 0.0F), 1.48F,
                           k_metal_accent_slot, 1.0F, 2),
        generated_cylinder(QVector3D(0.0F, 1.087F, 0.0F),
                           QVector3D(0.0F, 1.113F, 0.0F), 1.42F, k_metal_slot,
                           1.0F, 2),
        generated_cylinder(QVector3D(0.0F, -0.40F, -1.00F),
                           QVector3D(0.0F, -0.02F, -0.92F), 1.16F, k_metal_slot,
                           1.0F, 2),
        generated_cylinder(QVector3D(0.0F, 1.96F, 0.0F),
                           QVector3D(0.0F, 2.10F, 0.0F), 0.026F,
                           k_metal_accent_slot, 1.0F, 2),
        generated_cone(QVector3D(0.0F, 2.10F, 0.0F),
                       QVector3D(0.0F, 2.30F, 0.0F), 0.060F, k_crest_slot, 1.0F,
                       0),
        generated_sphere(QVector3D(0.0F, 2.30F, 0.0F), 0.028F,
                         k_metal_accent_slot, 1.0F, 2),
    }};
    return build_generated_equipment_archetype("roman_light_helmet",
                                               primitives);
  }();
  return archetype;
}

auto roman_light_palette(const HumanoidPalette &palette)
    -> std::array<QVector3D, 3> {
  QVector3D const metal =
      saturate_color(palette.metal * QVector3D(1.15F, 0.92F, 0.68F));
  return {metal, saturate_color(metal * 1.14F), QVector3D(0.88F, 0.18F, 0.18F)};
}

} // namespace

void RomanLightHelmetRenderer::render(const DrawContext &ctx,
                                      const BodyFrames &frames,
                                      const HumanoidPalette &palette,
                                      const HumanoidAnimationContext &anim,
                                      EquipmentBatch &batch) {
  submit({}, ctx, frames, palette, anim, batch);
}

void RomanLightHelmetRenderer::submit(const RomanLightHelmetConfig &,
                                      const DrawContext &ctx,
                                      const BodyFrames &frames,
                                      const HumanoidPalette &palette,
                                      const HumanoidAnimationContext &anim,
                                      EquipmentBatch &batch) {
  (void)anim;

  if (frames.head.radius <= 0.0F) {
    return;
  }

  auto const equipment_palette = roman_light_palette(palette);
  append_humanoid_attachment_archetype(
      batch, ctx, frames.head, roman_light_helmet_archetype(),
      equipment_palette, QVector3D(0.0F, 0.05F, 0.0F));
}

} // namespace Render::GL
