#pragma once

#include <QVector3D>

namespace Render {

inline constexpr int k_material_unclassified = 0;
inline constexpr int k_material_metal = 1;
inline constexpr int k_material_wood = 2;
inline constexpr int k_material_cloth = 3;
inline constexpr int k_material_leather = 4;

[[nodiscard]] inline auto classify_material_id(const QVector3D& color) -> int {
  const float average = (color.x() + color.y() + color.z()) / 3.0F;
  const bool wood = color.x() < color.y() * 2.5F && color.x() > color.z() * 1.45F &&
                    average > 0.18F && average < 0.50F;
  if (wood) {
    return k_material_wood;
  }
  if (average < 0.40F) {
    return k_material_metal;
  }
  if (average > 0.65F) {
    return k_material_cloth;
  }
  return k_material_leather;
}

[[nodiscard]] inline auto resolve_material_id(int material_id,
                                              const QVector3D& color) -> int {
  if (material_id % 10 != k_material_unclassified) {
    return material_id;
  }
  return material_id + classify_material_id(color);
}

} // namespace Render
