#pragma once

#include "generated_equipment.h"

#include <array>
#include <cmath>
#include <deque>
#include <string>
#include <string_view>

namespace Render::GL {

inline auto
single_cylinder_archetype(float radius, int material_id,
                          std::string_view debug_prefix = "segment_cylinder")
    -> const RenderArchetype & {
  struct CachedArchetype {
    std::string debug_prefix;
    int radius_key{0};
    int material_id{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const radius_key = std::lround(radius * 1000.0F);
  for (const auto &entry : cache) {
    if (entry.debug_prefix == debug_prefix && entry.radius_key == radius_key &&
        entry.material_id == material_id) {
      return entry.archetype;
    }
  }

  std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
      generated_cylinder(QVector3D(0.0F, 0.0F, 0.0F),
                         QVector3D(0.0F, 1.0F, 0.0F), radius, 0U, 1.0F,
                         material_id),
  }};
  cache.push_back(
      {std::string(debug_prefix), radius_key, material_id,
       build_generated_equipment_archetype(
           std::string(debug_prefix) + "_" + std::to_string(radius_key) + "_" +
               std::to_string(material_id),
           primitives)});
  return cache.back().archetype;
}

inline auto
single_sphere_archetype(int material_id,
                        std::string_view debug_prefix = "segment_sphere")
    -> const RenderArchetype & {
  struct CachedArchetype {
    std::string debug_prefix;
    int material_id{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  for (const auto &entry : cache) {
    if (entry.debug_prefix == debug_prefix &&
        entry.material_id == material_id) {
      return entry.archetype;
    }
  }

  std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
      generated_sphere(QVector3D(0.0F, 0.0F, 0.0F), 1.0F, 0U, 1.0F,
                       material_id),
  }};
  cache.push_back(
      {std::string(debug_prefix), material_id,
       build_generated_equipment_archetype(std::string(debug_prefix) + "_" +
                                               std::to_string(material_id),
                                           primitives)});
  return cache.back().archetype;
}

inline auto single_cone_archetype(
    float base_radius, int material_id,
    std::string_view debug_prefix = "segment_cone") -> const RenderArchetype & {
  struct CachedArchetype {
    std::string debug_prefix;
    int radius_key{0};
    int material_id{0};
    RenderArchetype archetype;
  };

  static std::deque<CachedArchetype> cache;
  int const radius_key = std::lround(base_radius * 1000.0F);
  for (const auto &entry : cache) {
    if (entry.debug_prefix == debug_prefix && entry.radius_key == radius_key &&
        entry.material_id == material_id) {
      return entry.archetype;
    }
  }

  std::array<GeneratedEquipmentPrimitive, 1> const primitives{{
      generated_cone(QVector3D(0.0F, 0.0F, 0.0F), QVector3D(0.0F, 1.0F, 0.0F),
                     base_radius, 0U, 1.0F, material_id),
  }};
  cache.push_back(
      {std::string(debug_prefix), radius_key, material_id,
       build_generated_equipment_archetype(
           std::string(debug_prefix) + "_" + std::to_string(radius_key) + "_" +
               std::to_string(material_id),
           primitives)});
  return cache.back().archetype;
}

} // namespace Render::GL
