#pragma once

#include <array>

#include "render/gl/backend/prop_parts.h"

namespace Render::GL::BackendPipelines::WeaponRackParts {

inline constexpr float k_upright_x = 0.74F;
inline constexpr float k_top_cap_front_z = 0.06F;
inline constexpr float k_top_cap_y = 1.53F;
inline constexpr float k_mid_rail_front_z = 0.035F;
inline constexpr float k_tray_top_y = 0.035F;

inline constexpr std::array<PropBoxPart, 13> k_weapon_rack_boxes{{
    {{-0.80F, 0.00F, -0.40F}, {-0.68F, 0.10F, 0.40F}},
    {{0.68F, 0.00F, -0.40F}, {0.80F, 0.10F, 0.40F}},
    {{-0.79F, 0.08F, -0.06F}, {-0.69F, 1.50F, 0.04F}},
    {{0.69F, 0.08F, -0.06F}, {0.79F, 1.50F, 0.04F}},
    {{-0.84F, 1.48F, -0.08F}, {0.84F, 1.58F, 0.06F}},
    {{-0.70F, 0.98F, -0.045F}, {0.70F, 1.06F, 0.035F}},
    {{-0.71F, 0.20F, 0.17F}, {0.71F, 0.28F, 0.25F}},
    {{-0.785F, 0.08F, 0.165F}, {-0.695F, 0.33F, 0.255F}},
    {{0.695F, 0.08F, 0.165F}, {0.785F, 0.33F, 0.255F}},
    {{-0.66F, 0.00F, 0.12F}, {0.66F, 0.035F, 0.34F}},
    {{-0.665F, 0.00F, 0.33F}, {0.665F, 0.07F, 0.36F}},
    {{-0.665F, 0.00F, 0.105F}, {0.665F, 0.06F, 0.13F}},
    {{-0.70F, 0.18F, -0.05F}, {0.70F, 0.25F, 0.03F}},
}};

inline constexpr std::array<PropBeamPart, 6> k_weapon_rack_beams{{
    {{-0.74F, 0.09F, 0.33F}, {-0.74F, 0.78F, 0.045F}, 0.028F, 0.034F},
    {{-0.74F, 0.09F, -0.33F}, {-0.74F, 0.78F, -0.055F}, 0.028F, 0.034F},
    {{0.74F, 0.09F, 0.33F}, {0.74F, 0.78F, 0.045F}, 0.028F, 0.034F},
    {{0.74F, 0.09F, -0.33F}, {0.74F, 0.78F, -0.055F}, 0.028F, 0.034F},
    {{-0.69F, 0.24F, -0.02F}, {0.69F, 0.99F, -0.02F}, 0.021F, 0.024F},
    {{0.69F, 0.24F, -0.02F}, {-0.69F, 0.99F, -0.02F}, 0.021F, 0.024F},
}};

inline constexpr std::array<PropTaperPart, 3> k_weapon_rack_tapers{{
    {0.74F, 1.58F, -0.01F, 0.040F, 0.030F, 0.040F, 12},
    {0.74F, 1.62F, -0.01F, 0.030F, 0.012F, 0.050F, 12},
    {-0.74F, 1.58F, -0.01F, 0.034F, 0.030F, 0.050F, 12},
}};

} // namespace Render::GL::BackendPipelines::WeaponRackParts
