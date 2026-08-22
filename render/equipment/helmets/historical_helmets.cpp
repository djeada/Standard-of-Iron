#include "historical_helmets.h"

#include <array>

#include "helmet_alignment.h"
#include "render/equipment/attachment_builder.h"
#include "render/equipment/equipment_archetype_helpers.h"
#include "render/equipment/generated_equipment.h"
#include "render/humanoid/runtime/style_palette.h"

namespace Render::GL {
namespace {

enum PaletteSlot : std::uint8_t {
  k_metal = 0U,
  k_dark = 1U,
  k_leather = 2U,
  k_crest = 3U,
};

auto montefortino() -> const RenderArchetype& {
  static const RenderArchetype value = [] {
    std::array<GeneratedEquipmentPrimitive, 35> const parts{{

        generated_ellipsoid(
            {0.0F, 0.34F, -0.06F}, {1.46F, 1.34F, 1.58F}, k_metal, 1.0F, 2),
        generated_ellipsoid(
            {0.0F, 0.86F, -0.08F}, {1.10F, 0.84F, 1.18F}, k_metal, 1.0F, 2),

        generated_cylinder(
            {0.0F, 1.48F, -0.08F}, {0.0F, 1.72F, -0.08F}, 0.22F, k_dark, 1.0F, 2),
        generated_sphere({0.0F, 1.34F, 1.34F}, 0.20F, k_metal, 1.0F, 2),

        generated_ellipsoid(
            {0.0F, -0.28F, 1.10F}, {1.12F, 0.13F, 0.36F}, k_metal, 1.0F, 2),
        generated_ellipsoid(
            {-0.70F, -0.30F, 1.24F}, {0.42F, 0.09F, 0.18F}, k_dark, 1.0F, 2),
        generated_ellipsoid(
            {0.70F, -0.30F, 1.24F}, {0.42F, 0.09F, 0.18F}, k_dark, 1.0F, 2),

        generated_cylinder(
            {-0.76F, -0.30F, 1.16F}, {-1.18F, -0.46F, 0.28F}, 0.11F, k_dark, 1.0F, 2),
        generated_cylinder(
            {0.76F, -0.30F, 1.16F}, {1.18F, -0.46F, 0.28F}, 0.11F, k_dark, 1.0F, 2),

        generated_ellipsoid(
            {0.0F, -0.40F, -1.30F}, {1.22F, 0.22F, 0.44F}, k_metal, 1.0F, 2),
        generated_ellipsoid(
            {0.0F, -0.68F, -1.56F}, {1.24F, 0.20F, 0.42F}, k_metal, 1.0F, 2),
        generated_ellipsoid(
            {0.0F, -0.94F, -1.74F}, {1.14F, 0.18F, 0.38F}, k_metal, 1.0F, 2),
        generated_ellipsoid(
            {0.0F, -1.10F, -1.80F}, {1.00F, 0.11F, 0.26F}, k_dark, 1.0F, 2),

        generated_cylinder(
            {-1.14F, -0.34F, 0.34F}, {-1.32F, -0.39F, 0.29F}, 0.60F, k_metal, 1.0F, 2),
        generated_cylinder(
            {-1.00F, -0.96F, 0.50F}, {-1.18F, -1.01F, 0.45F}, 0.46F, k_metal, 1.0F, 2),
        generated_cylinder(
            {-0.78F, -1.40F, 0.60F}, {-0.94F, -1.45F, 0.55F}, 0.28F, k_metal, 1.0F, 2),
        generated_cylinder(
            {1.14F, -0.34F, 0.34F}, {1.32F, -0.39F, 0.29F}, 0.60F, k_metal, 1.0F, 2),
        generated_cylinder(
            {1.00F, -0.96F, 0.50F}, {1.18F, -1.01F, 0.45F}, 0.46F, k_metal, 1.0F, 2),
        generated_cylinder(
            {0.78F, -1.40F, 0.60F}, {0.94F, -1.45F, 0.55F}, 0.28F, k_metal, 1.0F, 2),

        generated_sphere({-1.36F, -0.22F, 0.28F}, 0.15F, k_dark, 1.0F, 2),
        generated_sphere({1.36F, -0.22F, 0.28F}, 0.15F, k_dark, 1.0F, 2),

        generated_cylinder({-0.80F, -1.46F, 0.54F},
                           {-0.30F, -1.74F, 0.40F},
                           0.09F,
                           k_leather,
                           1.0F,
                           0),
        generated_cylinder(
            {0.80F, -1.46F, 0.54F}, {0.30F, -1.74F, 0.40F}, 0.09F, k_leather, 1.0F, 0),

        generated_box({0.0F, 1.40F, -0.06F}, {0.13F, 0.30F, 1.05F}, k_dark, 1.0F, 2),

        generated_ellipsoid(
            {0.00F, 1.377F, 1.30F}, {0.18F, 0.62F, 0.52F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 1.802F, 0.95F}, {0.18F, 0.62F, 0.52F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 2.042F, 0.60F}, {0.18F, 0.62F, 0.52F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 2.168F, 0.25F}, {0.18F, 0.62F, 0.52F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 2.199F, -0.10F}, {0.18F, 0.62F, 0.52F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 2.139F, -0.45F}, {0.18F, 0.62F, 0.52F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 1.981F, -0.80F}, {0.18F, 0.62F, 0.52F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 1.701F, -1.15F}, {0.18F, 0.62F, 0.52F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 1.191F, -1.50F}, {0.18F, 0.62F, 0.52F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.0F, 1.00F, 1.58F}, {0.19F, 0.50F, 0.34F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.0F, 0.80F, -1.80F}, {0.18F, 0.56F, 0.38F}, k_crest, 1.0F, 0),
    }};
    return build_generated_equipment_archetype("roman_montefortino_helmet", parts);
  }();
  return value;
}

auto boeotian() -> const RenderArchetype& {
  static const RenderArchetype value = [] {
    std::array<GeneratedEquipmentPrimitive, 31> const parts{{
        generated_ellipsoid(
            {0.0F, 0.42F, -0.06F}, {1.42F, 1.36F, 1.52F}, k_metal, 1.0F, 2),
        generated_ellipsoid(
            {0.0F, 0.92F, -0.08F}, {1.06F, 0.80F, 1.14F}, k_metal, 1.0F, 2),

        generated_cylinder({0.0F, -0.149F, -0.090F},
                           {0.0F, -0.051F, -0.110F},
                           1.95F,
                           k_metal,
                           1.0F,
                           2),
        generated_cylinder(
            {0.0F, -0.176F, -0.085F}, {0.0F, -0.147F, -0.091F}, 1.99F, k_dark, 1.0F, 2),
        generated_ellipsoid(
            {0.0F, 0.24F, 1.72F}, {0.70F, 0.07F, 0.18F}, k_dark, 1.0F, 2),

        generated_ellipsoid(
            {0.0F, -0.46F, -1.24F}, {1.10F, 0.20F, 0.40F}, k_metal, 1.0F, 2),
        generated_ellipsoid(
            {0.0F, -0.72F, -1.48F}, {1.10F, 0.18F, 0.38F}, k_metal, 1.0F, 2),
        generated_ellipsoid(
            {0.0F, -0.94F, -1.62F}, {0.98F, 0.16F, 0.32F}, k_metal, 1.0F, 2),
        generated_ellipsoid(
            {0.0F, -1.08F, -1.66F}, {0.86F, 0.10F, 0.22F}, k_dark, 1.0F, 2),

        generated_cylinder(
            {-1.30F, -0.60F, 0.18F}, {-1.44F, -0.64F, 0.14F}, 0.36F, k_dark, 1.0F, 2),
        generated_cylinder(
            {1.30F, -0.60F, 0.18F}, {1.44F, -0.64F, 0.14F}, 0.36F, k_dark, 1.0F, 2),
        generated_sphere({-1.32F, -0.20F, 0.24F}, 0.13F, k_dark, 1.0F, 2),
        generated_sphere({1.32F, -0.20F, 0.24F}, 0.13F, k_dark, 1.0F, 2),
        generated_ellipsoid(
            {-0.62F, 0.30F, 1.18F}, {0.44F, 0.09F, 0.18F}, k_dark, 1.0F, 2),
        generated_ellipsoid(
            {0.62F, 0.30F, 1.18F}, {0.44F, 0.09F, 0.18F}, k_dark, 1.0F, 2),
        generated_sphere({-1.10F, 0.34F, 0.58F}, 0.12F, k_dark, 1.0F, 2),
        generated_sphere({1.10F, 0.34F, 0.58F}, 0.12F, k_dark, 1.0F, 2),

        generated_cylinder({-0.74F, -1.06F, 0.40F},
                           {-0.26F, -1.42F, 0.30F},
                           0.08F,
                           k_leather,
                           1.0F,
                           0),
        generated_cylinder(
            {0.74F, -1.06F, 0.40F}, {0.26F, -1.42F, 0.30F}, 0.08F, k_leather, 1.0F, 0),

        generated_box({0.0F, 1.46F, -0.06F}, {0.12F, 0.28F, 1.00F}, k_dark, 1.0F, 2),
        generated_ellipsoid(
            {0.00F, 1.490F, 1.20F}, {0.17F, 0.58F, 0.48F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 1.859F, 0.90F}, {0.17F, 0.58F, 0.48F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 2.082F, 0.60F}, {0.17F, 0.58F, 0.48F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 2.211F, 0.30F}, {0.17F, 0.58F, 0.48F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 2.259F, 0.00F}, {0.17F, 0.58F, 0.48F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 2.232F, -0.30F}, {0.17F, 0.58F, 0.48F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 2.129F, -0.60F}, {0.17F, 0.58F, 0.48F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 1.938F, -0.90F}, {0.17F, 0.58F, 0.48F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.00F, 1.629F, -1.20F}, {0.17F, 0.58F, 0.48F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.0F, 1.16F, 1.50F}, {0.18F, 0.46F, 0.32F}, k_crest, 1.0F, 0),
        generated_ellipsoid(
            {0.0F, 0.98F, -1.52F}, {0.17F, 0.52F, 0.36F}, k_crest, 1.0F, 0),
    }};
    return build_generated_equipment_archetype("roman_boeotian_cavalry_helmet", parts);
  }();
  return value;
}

auto punic_conical() -> const RenderArchetype& {
  static const RenderArchetype value = [] {
    std::array<GeneratedEquipmentPrimitive, 10> const parts{{
        generated_cylinder(
            {0.0F, -0.20F, 0.0F}, {0.0F, 0.54F, 0.0F}, 1.40F, k_metal, 1.0F, 2),
        generated_cone(
            {0.0F, 0.48F, 0.0F}, {0.0F, 2.62F, -0.08F}, 1.38F, k_metal, 1.0F, 2),
        generated_cylinder(
            {0.0F, -0.24F, 0.0F}, {0.0F, -0.08F, 0.0F}, 1.48F, k_dark, 1.0F, 2),
        generated_box({0.0F, -0.24F, -0.92F}, {1.34F, 0.18F, 0.38F}, k_metal, 1.0F, 2),
        generated_cylinder(
            {-1.16F, 0.06F, 0.16F}, {-1.02F, -0.58F, 0.34F}, 0.19F, k_dark, 1.0F, 2),
        generated_cylinder(
            {1.16F, 0.06F, 0.16F}, {1.02F, -0.58F, 0.34F}, 0.19F, k_dark, 1.0F, 2),
        generated_cylinder(
            {0.0F, -0.12F, 0.92F}, {0.0F, 0.48F, 0.76F}, 0.08F, k_metal, 1.0F, 2),
        generated_sphere({-0.78F, -0.04F, 0.72F}, 0.07F, k_metal, 1.0F, 2),
        generated_sphere({0.78F, -0.04F, 0.72F}, 0.07F, k_metal, 1.0F, 2),
        generated_sphere({0.0F, 2.64F, -0.08F}, 0.13F, k_dark, 1.0F, 2),
    }};
    return build_generated_equipment_archetype("carthage_punic_conical_helmet", parts);
  }();
  return value;
}

auto thracian() -> const RenderArchetype& {
  static const RenderArchetype value = [] {
    std::array<GeneratedEquipmentPrimitive, 14> const parts{{
        generated_sphere({0.0F, 0.54F, 0.0F}, 1.48F, k_metal, 1.0F, 2),
        generated_cone(
            {0.0F, 1.34F, -0.04F}, {0.0F, 2.16F, -0.12F}, 0.92F, k_metal, 1.0F, 2),
        generated_box({0.0F, 0.12F, 1.00F}, {1.14F, 0.13F, 0.36F}, k_dark, 1.0F, 2),
        generated_cylinder(
            {-1.20F, 0.08F, 0.18F}, {-0.96F, -0.66F, 0.42F}, 0.23F, k_dark, 1.0F, 2),
        generated_cylinder(
            {1.20F, 0.08F, 0.18F}, {0.96F, -0.66F, 0.42F}, 0.23F, k_dark, 1.0F, 2),
        generated_cylinder(
            {0.0F, 2.18F, 0.36F}, {0.0F, 2.02F, -0.66F}, 0.05F, k_metal, 1.0F, 2),
        generated_cylinder(
            {0.0F, 2.18F, 0.32F}, {0.0F, 3.02F, 0.12F}, 0.10F, k_crest, 1.0F, 0),
        generated_cylinder(
            {0.0F, 2.14F, 0.12F}, {0.0F, 2.92F, -0.08F}, 0.10F, k_crest, 1.0F, 0),
        generated_cylinder(
            {0.0F, 2.10F, -0.08F}, {0.0F, 2.78F, -0.30F}, 0.10F, k_crest, 1.0F, 0),
        generated_cylinder(
            {0.0F, 2.06F, -0.28F}, {0.0F, 2.62F, -0.54F}, 0.10F, k_crest, 1.0F, 0),
        generated_cylinder(
            {0.0F, 2.02F, -0.48F}, {0.0F, 2.44F, -0.78F}, 0.10F, k_crest, 1.0F, 0),
        generated_sphere({-0.84F, 0.12F, 0.76F}, 0.08F, k_metal, 1.0F, 2),
        generated_sphere({0.84F, 0.12F, 0.76F}, 0.08F, k_metal, 1.0F, 2),
        generated_box({0.0F, -0.24F, -0.92F}, {1.38F, 0.16F, 0.40F}, k_metal, 1.0F, 2),
    }};
    return build_generated_equipment_archetype("carthage_thracian_crested_helmet",
                                               parts);
  }();
  return value;
}

} // namespace

auto historical_helmet_archetype(HistoricalHelmet helmet) -> const RenderArchetype& {
  switch (helmet) {
  case HistoricalHelmet::RomanMontefortino:
    return montefortino();
  case HistoricalHelmet::RomanBoeotianCavalry:
    return boeotian();
  case HistoricalHelmet::CarthagePunicConical:
    return punic_conical();
  case HistoricalHelmet::CarthageThracianCrested:
    return thracian();
  }
  return montefortino();
}

auto historical_helmet_fill_role_colors(HistoricalHelmet helmet,
                                        const HumanoidPalette& palette,
                                        QVector3D* out,
                                        std::size_t max) -> std::uint32_t {
  if (max < k_historical_helmet_role_count) {
    return 0U;
  }
  using Render::GL::Humanoid::saturate_color;
  bool const carthaginian = helmet == HistoricalHelmet::CarthagePunicConical ||
                            helmet == HistoricalHelmet::CarthageThracianCrested;
  QVector3D const metal =
      carthaginian ? saturate_color(palette.metal * QVector3D(1.38F, 1.12F, 0.62F))
                   : saturate_color(palette.metal * QVector3D(1.24F, 1.05F, 0.66F));
  out[0] = metal;
  out[1] = metal * 0.70F;
  out[2] = saturate_color(palette.leather_dark);
  out[3] =
      carthaginian ? QVector3D(0.50F, 0.10F, 0.34F) : QVector3D(0.88F, 0.13F, 0.10F);
  return k_historical_helmet_role_count;
}

auto historical_helmet_make_static_attachment(
    HistoricalHelmet helmet,
    std::uint16_t socket_bone_index,
    std::uint8_t base_role_byte,
    const QMatrix4x4& bind_palette_socket_bone)
    -> Render::Creature::StaticAttachmentSpec {
  auto spec = Render::Equipment::build_static_attachment({
      .archetype = &historical_helmet_archetype(helmet),
      .socket_bone_index = socket_bone_index,
      .uniform_scale = k_helmet_uniform_scale,
      .authored_local_offset = k_helmet_local_offset * k_helmet_uniform_scale,
      .bind_radius = 0.16F,
      .bind_socket_transform = bind_palette_socket_bone,
  });
  fill_sequential_role_remap(spec, base_role_byte, k_historical_helmet_role_count);
  return spec;
}

} // namespace Render::GL
