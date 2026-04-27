#pragma once

#include "../generated_equipment.h"

#include <array>
#include <cmath>
#include <deque>
#include <string>

namespace Render::GL {

enum ArrowPaletteSlot : std::uint8_t {
  k_arrow_shaft_slot = 0U,
  k_arrow_head_slot = 1U,
  k_arrow_fletching_slot = 2U,
};

inline auto quantize_arrow_value(float value) -> int {
  return std::lround(value * 1000.0F);
}

inline auto arrow_shaft_archetype(float radius, int material_id,
                                  std::string_view debug_prefix = "arrow_shaft")
    -> const RenderArchetype & {
  struct CachedArchetype {
    int radius_key{0};
    int material_id{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const radius_key = quantize_arrow_value(radius);
  for (const auto &entry : cache) {
    if (entry.radius_key == radius_key && entry.material_id == material_id) {
      return entry.archetype;
    }
  }

  std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
      generated_cylinder(QVector3D(0.0F, 0.0F, 0.0F),
                         QVector3D(0.0F, 1.0F, 0.0F), radius,
                         k_arrow_shaft_slot, 1.0F, material_id),
  }};
  cache.push_back(
      {radius_key, material_id,
       build_generated_equipment_archetype(
           std::string(debug_prefix) + "_" + std::to_string(radius_key) + "_" +
               std::to_string(material_id),
           primitives)});
  return cache.back().archetype;
}

inline auto arrow_head_archetype(float base_radius, int material_id,
                                 std::string_view debug_prefix = "arrow_head")
    -> const RenderArchetype & {
  struct CachedArchetype {
    int radius_key{0};
    int material_id{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const radius_key = quantize_arrow_value(base_radius);
  for (const auto &entry : cache) {
    if (entry.radius_key == radius_key && entry.material_id == material_id) {
      return entry.archetype;
    }
  }

  std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
      generated_cone(QVector3D(0.0F, 0.0F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F),
                     base_radius, k_arrow_head_slot, 1.0F, material_id),
  }};
  cache.push_back(
      {radius_key, material_id,
       build_generated_equipment_archetype(
           std::string(debug_prefix) + "_" + std::to_string(radius_key) + "_" +
               std::to_string(material_id),
           primitives)});
  return cache.back().archetype;
}

inline auto
arrow_fletching_archetype(float base_radius, int material_id,
                          std::string_view debug_prefix = "arrow_fletching")
    -> const RenderArchetype & {
  struct CachedArchetype {
    int radius_key{0};
    int material_id{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const radius_key = quantize_arrow_value(base_radius);
  for (const auto &entry : cache) {
    if (entry.radius_key == radius_key && entry.material_id == material_id) {
      return entry.archetype;
    }
  }

  std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
      generated_cone(QVector3D(0.0F, 0.0F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F),
                     base_radius, k_arrow_fletching_slot, 1.0F, material_id),
  }};
  cache.push_back(
      {radius_key, material_id,
       build_generated_equipment_archetype(
           std::string(debug_prefix) + "_" + std::to_string(radius_key) + "_" +
               std::to_string(material_id),
           primitives)});
  return cache.back().archetype;
}

} // namespace Render::GL
