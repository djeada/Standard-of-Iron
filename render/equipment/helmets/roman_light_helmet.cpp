#include "roman_light_helmet.h"

#include "../attachment_builder.h"
#include "../generated_equipment.h"

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

auto roman_light_palette(const HumanoidPalette &palette)
    -> std::array<QVector3D, 3> {
  QVector3D const metal =
      saturate_color(palette.metal * QVector3D(1.15F, 0.92F, 0.68F));
  return {metal, saturate_color(metal * 1.14F), QVector3D(0.88F, 0.18F, 0.18F)};
}

constexpr QVector3D k_authored_local_offset(0.0F, 0.05F, 0.0F);

} // namespace

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

auto roman_light_helmet_fill_role_colors(const HumanoidPalette &palette,
                                         QVector3D *out,
                                         std::size_t max) -> std::uint32_t {
  if (max < kRomanLightHelmetRoleCount) {
    return 0;
  }
  auto const colors = roman_light_palette(palette);
  out[0] = colors[0];
  out[1] = colors[1];
  out[2] = colors[2];
  return kRomanLightHelmetRoleCount;
}

auto roman_light_helmet_make_static_attachment(
    std::uint16_t socket_bone_index, std::uint8_t base_role_byte,
    const QMatrix4x4 &bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {

  constexpr float kHeadSocketRadius = 0.16F;
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &roman_light_helmet_archetype(),
      .socket_bone_index = socket_bone_index,
      .authored_local_offset = k_authored_local_offset,
      .bind_radius = kHeadSocketRadius,
      .bind_socket_transform = bind_palette_socket_bone,
  });
  spec.palette_role_remap[k_metal_slot] = base_role_byte;
  spec.palette_role_remap[k_metal_accent_slot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[k_crest_slot] =
      static_cast<std::uint8_t>(base_role_byte + 2U);
  return spec;
}

} // namespace Render::GL
