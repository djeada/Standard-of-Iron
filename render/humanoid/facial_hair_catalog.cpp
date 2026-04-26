#include "facial_hair_catalog.h"

#include "../equipment/attachment_builder.h"
#include "../equipment/generated_equipment.h"
#include "humanoid_spec.h"

#include <QVector3D>
#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace Render::Humanoid {

namespace {

enum FacialHairPaletteSlot : std::uint8_t {
  kHairDarkSlot = 0U,
  kHairBaseSlot = 1U,
  kHairHighlightSlot = 2U,
};

auto saturate_color(const QVector3D &color) -> QVector3D {
  return {std::clamp(color.x(), 0.0F, 1.0F),
          std::clamp(color.y(), 0.0F, 1.0F),
          std::clamp(color.z(), 0.0F, 1.0F)};
}

auto resolved_hair_color(const Render::GL::FacialHairParams &params)
    -> QVector3D {
  return saturate_color(params.color * (1.0F - params.greyness) +
                        QVector3D(0.75F, 0.75F, 0.75F) * params.greyness);
}

auto facial_hair_debug_name(Render::GL::FacialHairStyle style)
    -> std::string_view {
  using Render::GL::FacialHairStyle;
  switch (style) {
  case FacialHairStyle::Stubble:
    return "humanoid/facial_hair/stubble";
  case FacialHairStyle::ShortBeard:
    return "humanoid/facial_hair/short_beard";
  case FacialHairStyle::FullBeard:
    return "humanoid/facial_hair/full_beard";
  case FacialHairStyle::LongBeard:
    return "humanoid/facial_hair/long_beard";
  case FacialHairStyle::Goatee:
    return "humanoid/facial_hair/goatee";
  case FacialHairStyle::Mustache:
    return "humanoid/facial_hair/mustache";
  case FacialHairStyle::MustacheAndBeard:
    return "humanoid/facial_hair/mustache_and_beard";
  case FacialHairStyle::None:
  default:
    return "humanoid/facial_hair/none";
  }
}

auto stubble_archetype() -> const Render::GL::RenderArchetype & {
  static const auto archetype = [] {
    std::array<Render::GL::GeneratedEquipmentPrimitive, 8> const primitives{{
        Render::GL::generated_cone(QVector3D(-0.30F, -0.18F, 0.86F),
                                   QVector3D(-0.33F, -0.28F, 0.93F), 0.032F,
                                   kHairDarkSlot),
        Render::GL::generated_cone(QVector3D(-0.12F, -0.28F, 0.95F),
                                   QVector3D(-0.10F, -0.37F, 1.00F), 0.028F,
                                   kHairDarkSlot),
        Render::GL::generated_cone(QVector3D(0.00F, -0.34F, 0.98F),
                                   QVector3D(0.00F, -0.45F, 1.05F), 0.030F,
                                   kHairBaseSlot),
        Render::GL::generated_cone(QVector3D(0.12F, -0.28F, 0.95F),
                                   QVector3D(0.10F, -0.37F, 1.00F), 0.028F,
                                   kHairDarkSlot),
        Render::GL::generated_cone(QVector3D(0.30F, -0.18F, 0.86F),
                                   QVector3D(0.33F, -0.28F, 0.93F), 0.032F,
                                   kHairDarkSlot),
        Render::GL::generated_cone(QVector3D(-0.20F, -0.08F, 0.91F),
                                   QVector3D(-0.39F, -0.06F, 0.88F), 0.026F,
                                   kHairBaseSlot),
        Render::GL::generated_cone(QVector3D(0.20F, -0.08F, 0.91F),
                                   QVector3D(0.39F, -0.06F, 0.88F), 0.026F,
                                   kHairBaseSlot),
        Render::GL::generated_cone(QVector3D(0.00F, -0.12F, 0.94F),
                                   QVector3D(0.00F, -0.16F, 0.98F), 0.022F,
                                   kHairHighlightSlot),
    }};
    return Render::GL::build_generated_equipment_archetype(
        facial_hair_debug_name(Render::GL::FacialHairStyle::Stubble),
        primitives);
  }();
  return archetype;
}

auto short_beard_archetype() -> const Render::GL::RenderArchetype & {
  static const auto archetype = [] {
    std::array<Render::GL::GeneratedEquipmentPrimitive, 13> const primitives{{
        Render::GL::generated_sphere(QVector3D(-0.24F, -0.06F, 0.90F), 0.050F,
                                     kHairDarkSlot),
        Render::GL::generated_sphere(QVector3D(0.24F, -0.06F, 0.90F), 0.050F,
                                     kHairDarkSlot),
        Render::GL::generated_cylinder(QVector3D(-0.24F, -0.06F, 0.90F),
                                       QVector3D(-0.42F, -0.04F, 0.84F), 0.040F,
                                       kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.24F, -0.06F, 0.90F),
                                       QVector3D(0.42F, -0.04F, 0.84F), 0.040F,
                                       kHairBaseSlot),
        Render::GL::generated_sphere(QVector3D(-0.28F, -0.26F, 0.86F), 0.060F,
                                     kHairDarkSlot),
        Render::GL::generated_sphere(QVector3D(0.28F, -0.26F, 0.86F), 0.060F,
                                     kHairDarkSlot),
        Render::GL::generated_cylinder(QVector3D(-0.28F, -0.26F, 0.86F),
                                       QVector3D(-0.18F, -0.58F, 0.98F), 0.056F,
                                       kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.28F, -0.26F, 0.86F),
                                       QVector3D(0.18F, -0.58F, 0.98F), 0.056F,
                                       kHairBaseSlot),
        Render::GL::generated_sphere(QVector3D(0.00F, -0.34F, 0.96F), 0.070F,
                                     kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.00F, -0.34F, 0.96F),
                                       QVector3D(0.00F, -0.74F, 1.08F), 0.070F,
                                       kHairBaseSlot),
        Render::GL::generated_cone(QVector3D(0.00F, -0.56F, 1.02F),
                                   QVector3D(0.00F, -0.92F, 1.14F), 0.058F,
                                   kHairHighlightSlot),
        Render::GL::generated_cone(QVector3D(-0.16F, -0.46F, 0.98F),
                                   QVector3D(-0.12F, -0.76F, 1.08F), 0.046F,
                                   kHairHighlightSlot),
        Render::GL::generated_cone(QVector3D(0.16F, -0.46F, 0.98F),
                                   QVector3D(0.12F, -0.76F, 1.08F), 0.046F,
                                   kHairHighlightSlot),
    }};
    return Render::GL::build_generated_equipment_archetype(
        facial_hair_debug_name(Render::GL::FacialHairStyle::ShortBeard),
        primitives);
  }();
  return archetype;
}

auto full_beard_archetype() -> const Render::GL::RenderArchetype & {
  static const auto archetype = [] {
    std::array<Render::GL::GeneratedEquipmentPrimitive, 16> const primitives{{
        Render::GL::generated_sphere(QVector3D(-0.26F, -0.08F, 0.90F), 0.058F,
                                     kHairDarkSlot),
        Render::GL::generated_sphere(QVector3D(0.26F, -0.08F, 0.90F), 0.058F,
                                     kHairDarkSlot),
        Render::GL::generated_cylinder(QVector3D(-0.26F, -0.08F, 0.90F),
                                       QVector3D(-0.48F, -0.02F, 0.82F), 0.046F,
                                       kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.26F, -0.08F, 0.90F),
                                       QVector3D(0.48F, -0.02F, 0.82F), 0.046F,
                                       kHairBaseSlot),
        Render::GL::generated_sphere(QVector3D(-0.36F, -0.30F, 0.84F), 0.075F,
                                     kHairDarkSlot),
        Render::GL::generated_sphere(QVector3D(0.36F, -0.30F, 0.84F), 0.075F,
                                     kHairDarkSlot),
        Render::GL::generated_cylinder(QVector3D(-0.36F, -0.30F, 0.84F),
                                       QVector3D(-0.22F, -0.80F, 1.05F), 0.068F,
                                       kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.36F, -0.30F, 0.84F),
                                       QVector3D(0.22F, -0.80F, 1.05F), 0.068F,
                                       kHairBaseSlot),
        Render::GL::generated_sphere(QVector3D(0.00F, -0.38F, 0.98F), 0.082F,
                                     kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.00F, -0.38F, 0.98F),
                                       QVector3D(0.00F, -0.96F, 1.16F), 0.080F,
                                       kHairBaseSlot),
        Render::GL::generated_cone(QVector3D(0.00F, -0.66F, 1.08F),
                                   QVector3D(0.00F, -1.20F, 1.24F), 0.068F,
                                   kHairHighlightSlot),
        Render::GL::generated_cone(QVector3D(-0.20F, -0.60F, 1.00F),
                                   QVector3D(-0.12F, -1.00F, 1.14F), 0.056F,
                                   kHairHighlightSlot),
        Render::GL::generated_cone(QVector3D(0.20F, -0.60F, 1.00F),
                                   QVector3D(0.12F, -1.00F, 1.14F), 0.056F,
                                   kHairHighlightSlot),
        Render::GL::generated_cylinder(QVector3D(-0.12F, -0.44F, 1.00F),
                                       QVector3D(-0.08F, -0.92F, 1.12F), 0.050F,
                                       kHairDarkSlot),
        Render::GL::generated_cylinder(QVector3D(0.12F, -0.44F, 1.00F),
                                       QVector3D(0.08F, -0.92F, 1.12F), 0.050F,
                                       kHairDarkSlot),
        Render::GL::generated_sphere(QVector3D(0.00F, -0.92F, 1.18F), 0.050F,
                                     kHairHighlightSlot),
    }};
    return Render::GL::build_generated_equipment_archetype(
        facial_hair_debug_name(Render::GL::FacialHairStyle::FullBeard),
        primitives);
  }();
  return archetype;
}

auto long_beard_archetype() -> const Render::GL::RenderArchetype & {
  static const auto archetype = [] {
    std::array<Render::GL::GeneratedEquipmentPrimitive, 18> const primitives{{
        Render::GL::generated_sphere(QVector3D(-0.28F, -0.08F, 0.90F), 0.060F,
                                     kHairDarkSlot),
        Render::GL::generated_sphere(QVector3D(0.28F, -0.08F, 0.90F), 0.060F,
                                     kHairDarkSlot),
        Render::GL::generated_cylinder(QVector3D(-0.28F, -0.08F, 0.90F),
                                       QVector3D(-0.48F, -0.02F, 0.82F), 0.046F,
                                       kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.28F, -0.08F, 0.90F),
                                       QVector3D(0.48F, -0.02F, 0.82F), 0.046F,
                                       kHairBaseSlot),
        Render::GL::generated_sphere(QVector3D(-0.38F, -0.32F, 0.84F), 0.078F,
                                     kHairDarkSlot),
        Render::GL::generated_sphere(QVector3D(0.38F, -0.32F, 0.84F), 0.078F,
                                     kHairDarkSlot),
        Render::GL::generated_cylinder(QVector3D(-0.38F, -0.32F, 0.84F),
                                       QVector3D(-0.22F, -0.98F, 1.10F), 0.070F,
                                       kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.38F, -0.32F, 0.84F),
                                       QVector3D(0.22F, -0.98F, 1.10F), 0.070F,
                                       kHairBaseSlot),
        Render::GL::generated_sphere(QVector3D(0.00F, -0.42F, 0.98F), 0.086F,
                                     kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.00F, -0.42F, 0.98F),
                                       QVector3D(0.00F, -1.14F, 1.22F), 0.082F,
                                       kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.00F, -1.14F, 1.22F),
                                       QVector3D(0.00F, -1.64F, 1.28F), 0.064F,
                                       kHairBaseSlot),
        Render::GL::generated_cone(QVector3D(0.00F, -1.34F, 1.25F),
                                   QVector3D(0.00F, -1.90F, 1.30F), 0.056F,
                                   kHairHighlightSlot),
        Render::GL::generated_cone(QVector3D(-0.20F, -0.70F, 1.04F),
                                   QVector3D(-0.10F, -1.24F, 1.16F), 0.054F,
                                   kHairHighlightSlot),
        Render::GL::generated_cone(QVector3D(0.20F, -0.70F, 1.04F),
                                   QVector3D(0.10F, -1.24F, 1.16F), 0.054F,
                                   kHairHighlightSlot),
        Render::GL::generated_cylinder(QVector3D(-0.10F, -0.52F, 1.02F),
                                       QVector3D(-0.06F, -1.18F, 1.16F), 0.048F,
                                       kHairDarkSlot),
        Render::GL::generated_cylinder(QVector3D(0.10F, -0.52F, 1.02F),
                                       QVector3D(0.06F, -1.18F, 1.16F), 0.048F,
                                       kHairDarkSlot),
        Render::GL::generated_sphere(QVector3D(0.00F, -1.62F, 1.28F), 0.040F,
                                     kHairHighlightSlot),
        Render::GL::generated_sphere(QVector3D(0.00F, -1.84F, 1.30F), 0.030F,
                                     kHairHighlightSlot),
    }};
    return Render::GL::build_generated_equipment_archetype(
        facial_hair_debug_name(Render::GL::FacialHairStyle::LongBeard),
        primitives);
  }();
  return archetype;
}

auto goatee_archetype() -> const Render::GL::RenderArchetype & {
  static const auto archetype = [] {
    std::array<Render::GL::GeneratedEquipmentPrimitive, 10> const primitives{{
        Render::GL::generated_sphere(QVector3D(-0.18F, -0.08F, 0.90F), 0.046F,
                                     kHairDarkSlot),
        Render::GL::generated_sphere(QVector3D(0.18F, -0.08F, 0.90F), 0.046F,
                                     kHairDarkSlot),
        Render::GL::generated_cylinder(QVector3D(-0.18F, -0.08F, 0.90F),
                                       QVector3D(-0.36F, -0.04F, 0.86F), 0.036F,
                                       kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.18F, -0.08F, 0.90F),
                                       QVector3D(0.36F, -0.04F, 0.86F), 0.036F,
                                       kHairBaseSlot),
        Render::GL::generated_sphere(QVector3D(0.00F, -0.34F, 0.98F), 0.072F,
                                     kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.00F, -0.34F, 0.98F),
                                       QVector3D(0.00F, -0.94F, 1.20F), 0.068F,
                                       kHairBaseSlot),
        Render::GL::generated_cone(QVector3D(0.00F, -0.60F, 1.06F),
                                   QVector3D(0.00F, -1.20F, 1.24F), 0.056F,
                                   kHairHighlightSlot),
        Render::GL::generated_cone(QVector3D(-0.10F, -0.46F, 1.02F),
                                   QVector3D(-0.05F, -0.92F, 1.16F), 0.040F,
                                   kHairDarkSlot),
        Render::GL::generated_cone(QVector3D(0.10F, -0.46F, 1.02F),
                                   QVector3D(0.05F, -0.92F, 1.16F), 0.040F,
                                   kHairDarkSlot),
        Render::GL::generated_sphere(QVector3D(0.00F, -1.00F, 1.22F), 0.034F,
                                     kHairHighlightSlot),
    }};
    return Render::GL::build_generated_equipment_archetype(
        facial_hair_debug_name(Render::GL::FacialHairStyle::Goatee), primitives);
  }();
  return archetype;
}

auto mustache_archetype() -> const Render::GL::RenderArchetype & {
  static const auto archetype = [] {
    std::array<Render::GL::GeneratedEquipmentPrimitive, 8> const primitives{{
        Render::GL::generated_sphere(QVector3D(-0.16F, -0.08F, 0.92F), 0.048F,
                                     kHairDarkSlot),
        Render::GL::generated_sphere(QVector3D(0.16F, -0.08F, 0.92F), 0.048F,
                                     kHairDarkSlot),
        Render::GL::generated_cylinder(QVector3D(-0.16F, -0.08F, 0.92F),
                                       QVector3D(-0.46F, -0.02F, 0.86F), 0.042F,
                                       kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.16F, -0.08F, 0.92F),
                                       QVector3D(0.46F, -0.02F, 0.86F), 0.042F,
                                       kHairBaseSlot),
        Render::GL::generated_cone(QVector3D(-0.30F, -0.05F, 0.88F),
                                   QVector3D(-0.54F, 0.02F, 0.76F), 0.038F,
                                   kHairHighlightSlot),
        Render::GL::generated_cone(QVector3D(0.30F, -0.05F, 0.88F),
                                   QVector3D(0.54F, 0.02F, 0.76F), 0.038F,
                                   kHairHighlightSlot),
        Render::GL::generated_cylinder(QVector3D(-0.02F, -0.08F, 0.94F),
                                       QVector3D(-0.26F, -0.04F, 0.90F), 0.036F,
                                       kHairDarkSlot),
        Render::GL::generated_cylinder(QVector3D(0.02F, -0.08F, 0.94F),
                                       QVector3D(0.26F, -0.04F, 0.90F), 0.036F,
                                       kHairDarkSlot),
    }};
    return Render::GL::build_generated_equipment_archetype(
        facial_hair_debug_name(Render::GL::FacialHairStyle::Mustache),
        primitives);
  }();
  return archetype;
}

auto mustache_and_beard_archetype() -> const Render::GL::RenderArchetype & {
  static const auto archetype = [] {
    std::array<Render::GL::GeneratedEquipmentPrimitive, 18> const primitives{{
        Render::GL::generated_sphere(QVector3D(-0.18F, -0.08F, 0.92F), 0.046F,
                                     kHairDarkSlot),
        Render::GL::generated_sphere(QVector3D(0.18F, -0.08F, 0.92F), 0.046F,
                                     kHairDarkSlot),
        Render::GL::generated_cylinder(QVector3D(-0.18F, -0.08F, 0.92F),
                                       QVector3D(-0.46F, -0.02F, 0.86F), 0.040F,
                                       kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.18F, -0.08F, 0.92F),
                                       QVector3D(0.46F, -0.02F, 0.86F), 0.040F,
                                       kHairBaseSlot),
        Render::GL::generated_cone(QVector3D(-0.30F, -0.05F, 0.88F),
                                   QVector3D(-0.52F, 0.00F, 0.78F), 0.036F,
                                   kHairHighlightSlot),
        Render::GL::generated_cone(QVector3D(0.30F, -0.05F, 0.88F),
                                   QVector3D(0.52F, 0.00F, 0.78F), 0.036F,
                                   kHairHighlightSlot),
        Render::GL::generated_sphere(QVector3D(-0.30F, -0.24F, 0.86F), 0.062F,
                                     kHairDarkSlot),
        Render::GL::generated_sphere(QVector3D(0.30F, -0.24F, 0.86F), 0.062F,
                                     kHairDarkSlot),
        Render::GL::generated_cylinder(QVector3D(-0.30F, -0.24F, 0.86F),
                                       QVector3D(-0.16F, -0.66F, 1.02F), 0.056F,
                                       kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.30F, -0.24F, 0.86F),
                                       QVector3D(0.16F, -0.66F, 1.02F), 0.056F,
                                       kHairBaseSlot),
        Render::GL::generated_sphere(QVector3D(0.00F, -0.34F, 0.98F), 0.074F,
                                     kHairBaseSlot),
        Render::GL::generated_cylinder(QVector3D(0.00F, -0.34F, 0.98F),
                                       QVector3D(0.00F, -0.86F, 1.14F), 0.070F,
                                       kHairBaseSlot),
        Render::GL::generated_cone(QVector3D(0.00F, -0.58F, 1.06F),
                                   QVector3D(0.00F, -1.08F, 1.18F), 0.060F,
                                   kHairHighlightSlot),
        Render::GL::generated_cone(QVector3D(-0.16F, -0.50F, 1.00F),
                                   QVector3D(-0.10F, -0.86F, 1.10F), 0.044F,
                                   kHairHighlightSlot),
        Render::GL::generated_cone(QVector3D(0.16F, -0.50F, 1.00F),
                                   QVector3D(0.10F, -0.86F, 1.10F), 0.044F,
                                   kHairHighlightSlot),
        Render::GL::generated_cylinder(QVector3D(-0.06F, -0.12F, 0.94F),
                                       QVector3D(-0.24F, -0.06F, 0.90F), 0.032F,
                                       kHairDarkSlot),
        Render::GL::generated_cylinder(QVector3D(0.06F, -0.12F, 0.94F),
                                       QVector3D(0.24F, -0.06F, 0.90F), 0.032F,
                                       kHairDarkSlot),
        Render::GL::generated_sphere(QVector3D(0.00F, -0.90F, 1.16F), 0.038F,
                                     kHairHighlightSlot),
    }};
    return Render::GL::build_generated_equipment_archetype(
        facial_hair_debug_name(Render::GL::FacialHairStyle::MustacheAndBeard),
        primitives);
  }();
  return archetype;
}

auto facial_hair_archetype(Render::GL::FacialHairStyle style)
    -> const Render::GL::RenderArchetype * {
  using Render::GL::FacialHairStyle;
  switch (style) {
  case FacialHairStyle::Stubble:
    return &stubble_archetype();
  case FacialHairStyle::ShortBeard:
    return &short_beard_archetype();
  case FacialHairStyle::FullBeard:
    return &full_beard_archetype();
  case FacialHairStyle::LongBeard:
    return &long_beard_archetype();
  case FacialHairStyle::Goatee:
    return &goatee_archetype();
  case FacialHairStyle::Mustache:
    return &mustache_archetype();
  case FacialHairStyle::MustacheAndBeard:
    return &mustache_and_beard_archetype();
  case FacialHairStyle::None:
  default:
    return nullptr;
  }
}

auto facial_hair_make_static_attachment(Render::GL::FacialHairStyle style,
                                        std::uint8_t base_role_byte)
    -> Render::Creature::StaticAttachmentSpec {
  Render::Creature::StaticAttachmentSpec spec{};
  auto const *archetype = facial_hair_archetype(style);
  if (archetype == nullptr) {
    return spec;
  }

  auto const socket_bone_index =
      static_cast<std::uint16_t>(Render::Humanoid::HumanoidBone::Head);
  auto const bind_palette = humanoid_bind_palette();
  auto const &bind_frames = humanoid_bind_body_frames();

  spec = Render::Equipment::build_static_attachment({
      .archetype = archetype,
      .socket_bone_index = socket_bone_index,
      .bind_radius = bind_frames.head.radius,
      .bind_socket_transform = bind_palette[static_cast<std::size_t>(
          Render::Humanoid::HumanoidBone::Head)],
  });
  spec.palette_role_remap[kHairDarkSlot] = base_role_byte;
  spec.palette_role_remap[kHairBaseSlot] =
      static_cast<std::uint8_t>(base_role_byte + 1U);
  spec.palette_role_remap[kHairHighlightSlot] =
      static_cast<std::uint8_t>(base_role_byte + 2U);
  return spec;
}

struct FacialHairKey {
  Render::Creature::ArchetypeId base_archetype{Render::Creature::kInvalidArchetype};
  Render::GL::FacialHairStyle style{Render::GL::FacialHairStyle::None};

  auto operator==(const FacialHairKey &other) const noexcept -> bool = default;
};

struct FacialHairKeyHash {
  auto operator()(const FacialHairKey &key) const noexcept -> std::size_t {
    return (static_cast<std::size_t>(key.base_archetype) << 8U) ^
           static_cast<std::size_t>(key.style);
  }
};

auto facial_hair_cache() -> std::unordered_map<FacialHairKey,
                                               Render::Creature::ArchetypeId,
                                               FacialHairKeyHash> & {
  static std::unordered_map<FacialHairKey, Render::Creature::ArchetypeId,
                            FacialHairKeyHash>
      cache;
  return cache;
}

auto facial_hair_cache_mutex() -> std::mutex & {
  static std::mutex mutex;
  return mutex;
}

} // namespace

auto facial_hair_role_colors(const Render::GL::HumanoidVariant &variant,
                             QVector3D *out, std::uint32_t base_count,
                             std::size_t max_count) -> std::uint32_t {
  if (variant.facial_hair.style == Render::GL::FacialHairStyle::None ||
      variant.facial_hair.coverage < 0.01F || out == nullptr ||
      max_count < static_cast<std::size_t>(base_count + kFacialHairRoleCount)) {
    return base_count;
  }

  QVector3D const hair = resolved_hair_color(variant.facial_hair);
  out[base_count] = saturate_color(hair * 0.72F);
  out[base_count + 1U] = saturate_color(hair);
  out[base_count + 2U] =
      saturate_color(hair * 1.08F + QVector3D(0.03F, 0.03F, 0.03F));
  return base_count + kFacialHairRoleCount;
}

auto resolve_facial_hair_archetype(Render::Creature::ArchetypeId base_archetype,
                                   const Render::GL::HumanoidVariant &variant)
    -> Render::Creature::ArchetypeId {
  if (variant.facial_hair.style == Render::GL::FacialHairStyle::None ||
      variant.facial_hair.coverage < 0.01F) {
    return base_archetype;
  }

  if (base_archetype == Render::Creature::kInvalidArchetype) {
    base_archetype = Render::Creature::ArchetypeRegistry::kHumanoidBase;
  }

  FacialHairKey const key{base_archetype, variant.facial_hair.style};
  {
    std::lock_guard<std::mutex> lock(facial_hair_cache_mutex());
    auto it = facial_hair_cache().find(key);
    if (it != facial_hair_cache().end()) {
      return it->second;
    }
  }

  auto &registry = Render::Creature::ArchetypeRegistry::instance();
  auto const *base_desc = registry.get(base_archetype);
  if (base_desc == nullptr ||
      base_desc->bake_attachment_count >=
          Render::Creature::ArchetypeDescriptor::kMaxBakeAttachments) {
    return base_archetype;
  }

  Render::Creature::ArchetypeDescriptor desc = *base_desc;
  desc.debug_name = facial_hair_debug_name(variant.facial_hair.style);
  desc.bake_attachments[desc.bake_attachment_count++] = facial_hair_make_static_attachment(
      variant.facial_hair.style, desc.role_count);
  desc.role_count =
      static_cast<std::uint8_t>(desc.role_count + kFacialHairRoleCount);
  desc.append_extra_role_colors_fn(+[](const void *variant_void, QVector3D *out,
                                       std::uint32_t base_count,
                                       std::size_t max_count)
                                       -> std::uint32_t {
    if (variant_void == nullptr) {
      return base_count;
    }
    return facial_hair_role_colors(
        *static_cast<const Render::GL::HumanoidVariant *>(variant_void), out,
        base_count, max_count);
  });

  auto const id = registry.register_archetype(desc);
  if (id == Render::Creature::kInvalidArchetype) {
    return base_archetype;
  }

  {
    std::lock_guard<std::mutex> lock(facial_hair_cache_mutex());
    facial_hair_cache().emplace(key, id);
  }
  return id;
}

} // namespace Render::Humanoid
