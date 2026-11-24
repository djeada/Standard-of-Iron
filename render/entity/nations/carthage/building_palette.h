#pragma once

#include <QVector3D>

#include "render/equipment/material_palette.h"

namespace Render::GL::Carthage::BuildingPalette {

inline constexpr QVector3D k_sandstone{0.86F, 0.80F, 0.68F};
inline constexpr QVector3D k_sandstone_light{0.94F, 0.90F, 0.80F};
inline constexpr QVector3D k_sandstone_dark{0.25F, 0.23F, 0.24F};
inline constexpr QVector3D k_sandstone_shade{0.70F, 0.64F, 0.54F};
inline constexpr QVector3D k_plaster{0.95F, 0.92F, 0.84F};
inline constexpr QVector3D k_plaster_shade{0.81F, 0.77F, 0.69F};
inline constexpr QVector3D k_brick{0.60F, 0.41F, 0.30F};
inline constexpr QVector3D k_brick_dark{0.40F, 0.28F, 0.21F};
inline constexpr QVector3D k_tile_red{0.56F, 0.32F, 0.24F};
inline constexpr QVector3D k_tile_dark{0.40F, 0.24F, 0.19F};
inline constexpr QVector3D k_wood = Nation::k_carthage_materials.timber;
inline constexpr QVector3D k_wood_dark{0.26F, 0.21F, 0.17F};
inline constexpr QVector3D k_wood_light{0.53F, 0.44F, 0.33F};

inline constexpr QVector3D k_palisade_light{0.46F, 0.27F, 0.20F};
inline constexpr QVector3D k_palisade{0.35F, 0.19F, 0.15F};
inline constexpr QVector3D k_palisade_dark{0.22F, 0.12F, 0.10F};
inline constexpr QVector3D k_bronze{0.62F, 0.46F, 0.21F};
inline constexpr QVector3D k_indigo = Nation::k_carthage_materials.cloth;
inline constexpr QVector3D k_oxblood{0.42F, 0.14F, 0.38F};
inline constexpr QVector3D k_saffron = Nation::k_carthage_materials.accent;
inline constexpr QVector3D k_earth_light{0.38F, 0.30F, 0.21F};
inline constexpr QVector3D k_earth_dark{0.25F, 0.19F, 0.14F};
inline constexpr QVector3D k_rope{0.48F, 0.37F, 0.23F};

} // namespace Render::GL::Carthage::BuildingPalette
