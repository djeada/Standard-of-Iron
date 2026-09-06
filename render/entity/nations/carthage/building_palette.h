#pragma once

#include <QVector3D>

namespace Render::GL::Carthage::BuildingPalette {

// Shared architectural materials keep civilian and military buildings cohesive.
// Player colors belong on banners and trim, never on the roof materials.
inline constexpr QVector3D k_sandstone{0.82F, 0.70F, 0.52F};
inline constexpr QVector3D k_sandstone_light{0.92F, 0.84F, 0.68F};
inline constexpr QVector3D k_sandstone_dark{0.55F, 0.44F, 0.31F};
inline constexpr QVector3D k_sandstone_shade{0.70F, 0.59F, 0.44F};
inline constexpr QVector3D k_plaster{0.90F, 0.83F, 0.68F};
inline constexpr QVector3D k_plaster_shade{0.80F, 0.73F, 0.58F};
inline constexpr QVector3D k_brick{0.71F, 0.43F, 0.27F};
inline constexpr QVector3D k_brick_dark{0.40F, 0.18F, 0.11F};
inline constexpr QVector3D k_tile_red{0.57F, 0.21F, 0.12F};
inline constexpr QVector3D k_tile_dark{0.43F, 0.18F, 0.11F};
inline constexpr QVector3D k_wood{0.46F, 0.30F, 0.16F};
inline constexpr QVector3D k_wood_dark{0.23F, 0.16F, 0.09F};
inline constexpr QVector3D k_wood_light{0.60F, 0.42F, 0.21F};
inline constexpr QVector3D k_bronze{0.72F, 0.43F, 0.12F};
inline constexpr QVector3D k_indigo{0.21F, 0.25F, 0.45F};
inline constexpr QVector3D k_oxblood{0.47F, 0.10F, 0.08F};
inline constexpr QVector3D k_saffron{0.76F, 0.36F, 0.035F};

} // namespace Render::GL::Carthage::BuildingPalette
