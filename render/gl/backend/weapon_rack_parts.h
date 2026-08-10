#pragma once

#include <array>

#include "render/gl/backend/prop_parts.h"

namespace Render::GL::BackendPipelines::WeaponRackParts {

// The frame and shelves of the weapon rack. The blades themselves are built by
// the sword and spear helpers in the pipeline.

inline constexpr std::array<PropBoxPart, 15> k_weapon_rack_boxes{{
    {{-0.78F, 0.00F, -0.13F}, {-0.64F, 1.42F, 0.03F}},
    {{0.64F, 0.00F, -0.13F}, {0.78F, 1.42F, 0.03F}},
    {{-0.88F, 0.00F, 0.13F}, {-0.66F, 0.18F, 0.33F}},
    {{0.66F, 0.00F, 0.13F}, {0.88F, 0.18F, 0.33F}},
    {{-0.86F, 0.34F, -0.10F}, {0.86F, 0.48F, 0.08F}},
    {{-0.84F, 0.96F, -0.12F}, {0.84F, 1.10F, 0.06F}},
    {{-0.74F, 1.28F, -0.10F}, {0.74F, 1.40F, 0.04F}},
    {{-0.675F, 0.00F, 0.13F}, {-0.595F, 0.12F, 0.21F}},
    {{-0.335F, 0.02F, 0.12F}, {-0.210F, 0.15F, 0.24F}},
    {{0.030F, 0.03F, 0.14F}, {0.140F, 0.14F, 0.25F}},
    {{0.505F, 0.00F, 0.12F}, {0.585F, 0.12F, 0.20F}},
    {{0.520F, 0.86F, -0.29F}, {0.625F, 1.12F, -0.16F}},
    {{-0.70F, 0.48F, -0.115F}, {0.70F, 0.94F, -0.055F}},
    {{-0.72F, 0.46F, -0.130F}, {-0.66F, 0.96F, -0.050F}},
    {{0.66F, 0.46F, -0.130F}, {0.72F, 0.96F, -0.050F}},
}};

inline constexpr std::array<PropBeamPart, 11> k_weapon_rack_beams{{
    {{-0.70F, 0.16F, -0.08F}, {-0.18F, 0.96F, -0.08F}, 0.045F, 0.055F},
    {{0.70F, 0.16F, -0.08F}, {0.18F, 0.96F, -0.08F}, 0.045F, 0.055F},
    {{-0.64F, 0.05F, 0.17F}, {-0.50F, 1.72F, 0.09F}, 0.032F, 0.034F},
    {{-0.515F, 1.54F, 0.099F}, {-0.503F, 1.68F, 0.092F}, 0.046F, 0.047F},
    {{-0.280F, 0.12F, 0.17F}, {-0.235F, 0.38F, 0.12F}, 0.044F, 0.034F},
    {{-0.525F, 0.34F, 0.13F}, {0.030F, 0.41F, 0.13F}, 0.042F, 0.048F},
    {{0.085F, 0.12F, 0.18F}, {0.080F, 0.34F, 0.13F}, 0.036F, 0.030F},
    {{-0.075F, 0.30F, 0.14F}, {0.285F, 0.37F, 0.14F}, 0.038F, 0.044F},
    {{0.54F, 0.05F, 0.16F}, {0.68F, 1.70F, 0.08F}, 0.030F, 0.032F},
    {{0.665F, 1.52F, 0.089F}, {0.677F, 1.66F, 0.082F}, 0.044F, 0.045F},
    {{0.225F, 0.10F, -0.20F}, {0.205F, 1.88F, -0.19F}, 0.006F, 0.006F},
}};

inline constexpr std::array<PropTaperPart, 2> k_weapon_rack_tapers{{
    {0.315F, 0.020F, 0.400F, 0.135F, 0.115F, 0.360F, 10},
    {0.315F, 0.380F, 0.400F, 0.115F, 0.128F, 0.030F, 10},
}};

} // namespace Render::GL::BackendPipelines::WeaponRackParts
