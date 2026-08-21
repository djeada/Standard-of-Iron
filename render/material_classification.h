#pragma once

#include <QVector3D>

namespace Render {

// Explicit material identities carried by the low decimal digit of a draw's
// material id; the higher digits stay free for the damage tier the fragment
// shaders already decode as `material_id / 10`.
inline constexpr int k_material_unclassified = 0;
inline constexpr int k_material_metal = 1;
inline constexpr int k_material_wood = 2;
inline constexpr int k_material_cloth = 3;
inline constexpr int k_material_leather = 4;

// basic.frag used to re-derive this from base-colour relationships for every
// covered pixel. The colour is known when the draw is recorded, so the same
// classification runs once here and the shader only executes the named path.
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

// Leaves any identity the caller already chose alone, including a bare damage
// tier, and fills in only the material digit.
[[nodiscard]] inline auto resolve_material_id(int material_id,
                                              const QVector3D& color) -> int {
  if (material_id % 10 != k_material_unclassified) {
    return material_id;
  }
  return material_id + classify_material_id(color);
}

} // namespace Render
