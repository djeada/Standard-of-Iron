#pragma once

#include <QVector3D>

#include "render/equipment/material_palette.h"

namespace Render::GL::Roman::BuildingPalette {

inline constexpr QVector3D k_limestone{0.84F, 0.77F, 0.63F};
inline constexpr QVector3D k_limestone_shade{0.71F, 0.63F, 0.50F};
inline constexpr QVector3D k_limestone_dark{0.37F, 0.33F, 0.29F};
inline constexpr QVector3D k_plaster{0.89F, 0.79F, 0.59F};
inline constexpr QVector3D k_plaster_shade{0.75F, 0.63F, 0.45F};
inline constexpr QVector3D k_marble{0.92F, 0.88F, 0.78F};
inline constexpr QVector3D k_marble_shade{0.79F, 0.74F, 0.64F};
inline constexpr QVector3D k_cedar = Nation::k_roman_materials.timber;
inline constexpr QVector3D k_cedar_light{0.50F, 0.40F, 0.29F};
inline constexpr QVector3D k_cedar_dark{0.25F, 0.20F, 0.16F};
inline constexpr QVector3D k_terracotta{0.62F, 0.27F, 0.17F};
inline constexpr QVector3D k_terracotta_dark{0.36F, 0.20F, 0.16F};
inline constexpr QVector3D k_terracotta_light{0.76F, 0.38F, 0.22F};
inline constexpr QVector3D k_blue_accent{0.52F, 0.16F, 0.13F};
inline constexpr QVector3D k_blue_light{0.68F, 0.26F, 0.19F};
inline constexpr QVector3D k_gold = Nation::k_roman_materials.accent;
inline constexpr QVector3D k_bronze{0.50F, 0.42F, 0.26F};
inline constexpr QVector3D k_earth_light{0.39F, 0.32F, 0.24F};
inline constexpr QVector3D k_earth_dark{0.27F, 0.22F, 0.17F};
inline constexpr QVector3D k_rope{0.49F, 0.40F, 0.27F};

inline constexpr QVector3D k_palisade_light{0.60F, 0.53F, 0.41F};
inline constexpr QVector3D k_palisade{0.47F, 0.41F, 0.32F};
inline constexpr QVector3D k_palisade_dark{0.29F, 0.25F, 0.20F};

} // namespace Render::GL::Roman::BuildingPalette
