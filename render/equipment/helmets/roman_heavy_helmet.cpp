#include "roman_heavy_helmet.h"

#include "../attachment_builder.h"
#include "../generated_equipment.h"

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

auto roman_heavy_palette(const HumanoidPalette &palette)
    -> std::array<QVector3D, 4> {
  QVector3D const steel =
      saturate_color(palette.metal * QVector3D(0.88F, 0.92F, 1.08F));
  QVector3D const steel_light = saturate_color(steel * 1.06F);
  QVector3D const brass =
      saturate_color(palette.metal * QVector3D(1.40F, 1.15F, 0.65F));
  return {steel, steel_light, brass, QVector3D(0.96F, 0.12F, 0.12F)};
}

constexpr QVector3D k_authored_local_offset(0.0F, 0.05F, 0.0F);

} // namespace

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

auto roman_heavy_helmet_fill_role_colors(const HumanoidPalette &palette,
                                         QVector3D *out,
                                         std::size_t max) -> std::uint32_t {
  if (max < kRomanHeavyHelmetRoleCount) {
    return 0;
  }
  auto const colors = roman_heavy_palette(palette);
  out[0] = colors[0];
  out[1] = colors[1];
  out[2] = colors[2];
  out[3] = colors[3];
  return kRomanHeavyHelmetRoleCount;
}

auto roman_heavy_helmet_make_static_attachment(
    std::uint16_t socket_bone_index, std::uint8_t base_role_byte,
    const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  constexpr float kHeadSocketRadius = 0.16F;
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &roman_heavy_helmet_archetype(),
      .socket_bone_index = socket_bone_index,
      .authored_local_offset = k_authored_local_offset,
      .bind_radius = kHeadSocketRadius,
      .bind_socket_transform = bind_palette_socket_bone,
  });
  spec.palette_role_remap[k_steel_slot] = base_role_byte;
  spec.palette_role_remap[k_steel_light_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_brass_slot] =
      static_cast<std::uint8_t>(base_role_byte + 2U);
  spec.palette_role_remap[k_crest_slot] =
      static_cast<std::uint8_t>(base_role_byte + 3U);
  return spec;
}

} // namespace Render::GL
