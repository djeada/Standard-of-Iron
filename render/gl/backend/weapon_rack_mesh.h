#pragma once

#include <QVector4D>

#include <vector>

#include "render/gl/backend/prop_mesh_builder.h"

namespace Render::GL::BackendPipelines {

namespace WeaponRackMaterial {
inline constexpr float k_oak = 0.0F;
inline constexpr float k_ash = 1.0F;
inline constexpr float k_steel = 2.0F;
inline constexpr float k_iron = 3.0F;
inline constexpr float k_bronze = 4.0F;
inline constexpr float k_leather = 5.0F;
inline constexpr float k_yew = 6.0F;
inline constexpr float k_linen = 7.0F;
inline constexpr float k_shield_paint = 8.0F;
inline constexpr float k_shield_back = 9.0F;
inline constexpr float k_bone = 10.0F;
inline constexpr float k_feather = 11.0F;
} // namespace WeaponRackMaterial

struct WeaponRackMeshData {
  PropMeshVerts vertices;
  std::vector<QVector4D> surface;
  PropMeshIndices indices;
};

[[nodiscard]] auto build_weapon_rack_mesh() -> WeaponRackMeshData;

} // namespace Render::GL::BackendPipelines
