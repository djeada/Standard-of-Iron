#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/building_ornaments.h"

namespace Render::GL {

struct PantileRoof {
  float x0{-1.0F};
  float x1{1.0F};
  float eave_y{1.0F};
  float front_z{1.0F};
  float rear_z{-1.0F};
  float rise{0.4F};
  QVector3D tile{0.68F, 0.32F, 0.20F};
  QVector3D tile_light{0.81F, 0.43F, 0.26F};
  QVector3D tile_dark{0.43F, 0.27F, 0.21F};
  float cover_pitch{0.21F};

  bool lean_to{false};
};

inline void add_pantile_roof(BuildingArchetypeDesc& desc, const PantileRoof& roof) {
  constexpr float k_to_degrees = 180.0F / 3.14159265F;
  constexpr int k_courses = 4;
  BuildingPartMaterial const tiles(desc, k_building_material_ceramic);
  const float ridge_z =
      roof.lean_to ? roof.rear_z : (roof.front_z + roof.rear_z) * 0.5F;
  const float half_depth = (roof.front_z - roof.rear_z) * (roof.lean_to ? 1.0F : 0.5F);
  const float slope = std::sqrt((half_depth * half_depth) + (roof.rise * roof.rise));
  const float angle = std::atan2(roof.rise, half_depth) * k_to_degrees;
  const float centre_x = (roof.x0 + roof.x1) * 0.5F;
  const float half_x = (roof.x1 - roof.x0) * 0.5F;
  const int covers = std::max(
      2, static_cast<int>(std::lround((roof.x1 - roof.x0) / roof.cover_pitch)));
  for (const float side : {-1.0F, 1.0F}) {
    if (roof.lean_to && side < 0.0F) {
      continue;
    }
    for (int course = 0; course < k_courses; ++course) {
      const float mid =
          (static_cast<float>(course) + 0.5F) / static_cast<float>(k_courses);
      desc.add_rotated_box(
          QVector3D(centre_x,
                    roof.eave_y + (roof.rise * (1.0F - mid)) + 0.010F,
                    ridge_z + (side * half_depth * mid)),
          QVector3D(
              half_x, 0.014F, (slope / static_cast<float>(k_courses) * 0.5F) + 0.014F),
          QVector3D(side * (angle - 2.4F), 0.0F, 0.0F),
          weathered(roof.tile, course + (side < 0.0F ? 3 : 9), 0.05F));
    }
    for (int cover = 0; cover <= covers; ++cover) {
      const float x = roof.x0 + 0.03F +
                      ((roof.x1 - roof.x0 - 0.06F) * static_cast<float>(cover) /
                       static_cast<float>(covers));
      const QVector3D foot(
          x, roof.eave_y + 0.034F, ridge_z + (side * (half_depth - 0.012F)));
      desc.add_cylinder(
          foot,
          QVector3D(x, roof.eave_y + roof.rise + 0.030F, ridge_z),
          0.028F,
          weathered(roof.tile_light, cover + (side < 0.0F ? 17 : 53), 0.075F));
      desc.add_box(foot + QVector3D(0.0F, 0.004F, side * 0.006F),
                   QVector3D(0.032F, 0.034F, 0.010F),
                   weathered(roof.tile_light, cover + 5, 0.05F));
    }
    desc.add_box(QVector3D(centre_x,
                           roof.eave_y - 0.006F,
                           ridge_z + (side * (half_depth - 0.02F))),
                 QVector3D(half_x, 0.016F, 0.022F),
                 roof.tile_dark);
  }
  if (roof.lean_to) {
    return;
  }
  desc.add_cylinder(QVector3D(roof.x0, roof.eave_y + roof.rise + 0.034F, ridge_z),
                    QVector3D(roof.x1, roof.eave_y + roof.rise + 0.034F, ridge_z),
                    0.038F,
                    roof.tile_dark);
}

} // namespace Render::GL
