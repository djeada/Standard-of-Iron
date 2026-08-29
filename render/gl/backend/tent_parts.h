#pragma once

#include <algorithm>

namespace Render::GL::BackendPipelines::TentParts {

inline constexpr float k_ridge_height = 0.88F;
inline constexpr float k_half_width = 0.62F;
inline constexpr float k_half_depth = 0.60F;

inline constexpr float k_skirt = 0.07F;

inline constexpr float k_awning_extent = 0.30F;

inline constexpr float k_ground_half_x = k_half_width + k_skirt;
inline constexpr float k_ground_half_z =
    std::max(k_half_depth + k_skirt, k_half_depth + k_awning_extent);

} // namespace Render::GL::BackendPipelines::TentParts
