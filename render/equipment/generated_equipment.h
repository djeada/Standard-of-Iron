#pragma once

#include "../geom/transforms.h"
#include "../gl/primitives.h"
#include "../render_archetype.h"

#include <QVector3D>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace Render::GL {

enum class GeneratedEquipmentPrimitiveKind : std::uint8_t {
  Sphere,
  Cylinder,
  Cone,
  Box,
};

struct GeneratedEquipmentPrimitive {
  GeneratedEquipmentPrimitiveKind kind{GeneratedEquipmentPrimitiveKind::Sphere};
  QVector3D from{0.0F, 0.0F, 0.0F};
  QVector3D to{0.0F, 0.0F, 0.0F};
  QVector3D scale{1.0F, 1.0F, 1.0F};
  float radius{0.0F};
  std::uint8_t palette_slot{0U};
  float alpha{1.0F};
  int material_id{0};
};

inline auto
generated_sphere(const QVector3D &center, float radius,
                 std::uint8_t palette_slot, float alpha = 1.0F,
                 int material_id = 0) -> GeneratedEquipmentPrimitive {
  return {GeneratedEquipmentPrimitiveKind::Sphere,
          center,
          {},
          {},
          radius,
          palette_slot,
          alpha,
          material_id};
}

inline auto
generated_cylinder(const QVector3D &start, const QVector3D &end, float radius,
                   std::uint8_t palette_slot, float alpha = 1.0F,
                   int material_id = 0) -> GeneratedEquipmentPrimitive {
  return {GeneratedEquipmentPrimitiveKind::Cylinder,
          start,
          end,
          {},
          radius,
          palette_slot,
          alpha,
          material_id};
}

inline auto generated_cone(const QVector3D &base, const QVector3D &tip,
                           float base_radius, std::uint8_t palette_slot,
                           float alpha = 1.0F,
                           int material_id = 0) -> GeneratedEquipmentPrimitive {
  return {GeneratedEquipmentPrimitiveKind::Cone,
          base,
          tip,
          {},
          base_radius,
          palette_slot,
          alpha,
          material_id};
}

inline auto generated_box(const QVector3D &center, const QVector3D &scale,
                          std::uint8_t palette_slot, float alpha = 1.0F,
                          int material_id = 0) -> GeneratedEquipmentPrimitive {
  return {GeneratedEquipmentPrimitiveKind::Box,
          center,
          {},
          scale,
          0.0F,
          palette_slot,
          alpha,
          material_id};
}

inline void add_generated_equipment_primitive(
    RenderArchetypeBuilder &builder,
    const GeneratedEquipmentPrimitive &primitive) {
  switch (primitive.kind) {
  case GeneratedEquipmentPrimitiveKind::Sphere:
    builder.add_palette_mesh(
        get_unit_sphere(),
        Render::Geom::sphere_at(primitive.from, primitive.radius),
        primitive.palette_slot, nullptr, primitive.alpha,
        primitive.material_id);
    break;
  case GeneratedEquipmentPrimitiveKind::Cylinder:
    builder.add_palette_mesh(get_unit_cylinder(),
                             Render::Geom::cylinder_between(primitive.from,
                                                            primitive.to,
                                                            primitive.radius),
                             primitive.palette_slot, nullptr, primitive.alpha,
                             primitive.material_id);
    break;
  case GeneratedEquipmentPrimitiveKind::Cone:
    builder.add_palette_mesh(get_unit_cone(),
                             Render::Geom::cone_from_to(primitive.from,
                                                        primitive.to,
                                                        primitive.radius),
                             primitive.palette_slot, nullptr, primitive.alpha,
                             primitive.material_id);
    break;
  case GeneratedEquipmentPrimitiveKind::Box:
    builder.add_palette_box(primitive.from, primitive.scale,
                            primitive.palette_slot, nullptr, primitive.alpha,
                            primitive.material_id);
    break;
  }
}

inline auto build_generated_equipment_archetype(
    std::string_view debug_name,
    std::span<const GeneratedEquipmentPrimitive> primitives)
    -> RenderArchetype {
  RenderArchetypeBuilder builder{std::string(debug_name)};
  for (const auto &primitive : primitives) {
    add_generated_equipment_primitive(builder, primitive);
  }
  return std::move(builder).build();
}

template <std::size_t Count>
inline auto build_generated_equipment_archetype(
    std::string_view debug_name,
    const std::array<GeneratedEquipmentPrimitive, Count> &primitives)
    -> RenderArchetype {
  return build_generated_equipment_archetype(
      debug_name, std::span<const GeneratedEquipmentPrimitive>(
                      primitives.data(), primitives.size()));
}

} // namespace Render::GL
