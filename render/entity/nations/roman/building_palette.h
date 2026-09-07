#pragma once

#include <QVector3D>

namespace Render::GL::Roman::BuildingPalette {

// Shared architectural materials keep civilian and military buildings cohesive.
// Player colors belong on banners and trim, never on the roof materials.
inline constexpr QVector3D k_limestone{0.96F, 0.94F, 0.88F};
inline constexpr QVector3D k_limestone_shade{0.88F, 0.85F, 0.78F};
inline constexpr QVector3D k_limestone_dark{0.66F, 0.62F, 0.55F};
inline constexpr QVector3D k_plaster{0.93F, 0.90F, 0.82F};
inline constexpr QVector3D k_plaster_shade{0.84F, 0.80F, 0.71F};
inline constexpr QVector3D k_marble{0.98F, 0.97F, 0.95F};
inline constexpr QVector3D k_marble_shade{0.88F, 0.85F, 0.78F};
inline constexpr QVector3D k_cedar{0.52F, 0.38F, 0.26F};
inline constexpr QVector3D k_cedar_light{0.62F, 0.48F, 0.34F};
inline constexpr QVector3D k_cedar_dark{0.38F, 0.26F, 0.16F};
inline constexpr QVector3D k_terracotta{0.76F, 0.32F, 0.18F};
inline constexpr QVector3D k_terracotta_dark{0.46F, 0.12F, 0.07F};
inline constexpr QVector3D k_terracotta_light{0.84F, 0.42F, 0.25F};
inline constexpr QVector3D k_blue_accent{0.28F, 0.48F, 0.68F};
inline constexpr QVector3D k_blue_light{0.40F, 0.60F, 0.80F};
inline constexpr QVector3D k_gold{0.85F, 0.72F, 0.35F};
inline constexpr QVector3D k_bronze{0.62F, 0.46F, 0.24F};

} // namespace Render::GL::Roman::BuildingPalette
