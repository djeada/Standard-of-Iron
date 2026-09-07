#pragma once

#include <algorithm>
#include <cmath>

#include "building_palette.h"
#include "render/entity/building_ornaments.h"

namespace Render::GL::Roman {

inline void add_tiled_roof(BuildingArchetypeDesc& desc,
                           const QVector3D& eave_center,
                           float half_length,
                           float half_depth,
                           float rise,
                           bool ridge_along_z,
                           BuildingStateMask states = k_building_state_mask_intact,
                           bool close_gables = false) {
  const float theta = std::atan2(rise, half_depth);
  const float angle = theta * 180.0F / 3.14159265F;
  const float slope = std::sqrt(half_depth * half_depth + rise * rise);
  const float thickness = std::min(0.035F, half_depth * 0.08F);
  auto point = [&](float along, float y, float across) {
    return eave_center +
           (ridge_along_z ? QVector3D(across, y, along) : QVector3D(along, y, across));
  };
  auto size = [&](float along, float y, float across) {
    return ridge_along_z ? QVector3D(across, y, along) : QVector3D(along, y, across);
  };
  for (const float side : {-1.0F, 1.0F}) {
    desc.add_rotated_box(point(0.0F, rise * 0.5F, side * half_depth * 0.5F),
                         size(half_length, thickness, slope * 0.5F),
                         ridge_along_z ? QVector3D(0.0F, 0.0F, -side * angle)
                                       : QVector3D(side * angle, 0.0F, 0.0F),
                         BuildingPalette::k_terracotta,
                         states);
    const int rolls = std::max(3, static_cast<int>(half_length * 2.0F / 0.22F));
    const float lift = thickness / std::cos(theta) + 0.012F;
    for (int i = 0; i <= rolls; ++i) {
      const float along = -half_length + 0.04F +
                          (2.0F * half_length - 0.08F) * static_cast<float>(i) /
                              static_cast<float>(rolls);
      desc.add_cylinder(point(along, lift, side * half_depth),
                        point(along, rise + lift, 0.0F),
                        thickness * 0.40F,
                        BuildingPalette::k_terracotta_light,
                        states);
    }
    desc.add_box(point(0.0F, -0.01F, side * half_depth),
                 size(half_length, thickness, thickness),
                 BuildingPalette::k_terracotta_dark,
                 states);
  }
  desc.add_cylinder(point(-half_length, rise + thickness, 0.0F),
                    point(half_length, rise + thickness, 0.0F),
                    thickness * 1.2F,
                    BuildingPalette::k_terracotta_dark,
                    states);

  if (close_gables) {

    constexpr int k_courses = 16;
    const float course_h = rise / static_cast<float>(k_courses);
    for (const float end : {-1.0F, 1.0F}) {
      for (int i = 0; i < k_courses; ++i) {
        const float top = static_cast<float>(i + 1) * course_h;
        const float width = half_depth * (1.0F - top / rise);
        if (width <= 0.0F) {
          continue;
        }
        desc.add_box(point(end * (half_length - 0.06F), top - course_h * 0.5F, 0.0F),
                     size(0.025F, course_h * 0.5F, width),
                     BuildingPalette::k_plaster,
                     states);
      }
    }
  }
}

} // namespace Render::GL::Roman
