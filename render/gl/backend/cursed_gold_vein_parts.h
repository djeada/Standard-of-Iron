#pragma once

#include <array>

#include "render/gl/backend/prop_parts.h"

// A cursed gold vein is a low crag of dark rock split open by a seam of ore, with
// gold crystals growing out of the crack. Rock parts stay below y = 0.70 so the
// fragment shader can key the crystal shading on height; the crystals rise above.
namespace Render::GL::BackendPipelines::CursedGoldVeinParts {

inline constexpr std::array<PropBoxPart, 2> k_cursed_gold_vein_boxes{{
    {{-0.92F, -0.02F, -0.70F}, {0.40F, 0.07F, 0.90F}},
    {{-0.40F, -0.02F, -0.92F}, {0.92F, 0.07F, 0.55F}},
}};

inline constexpr std::array<PropVertPrismPart, 7> k_cursed_gold_vein_prisms{{
    {-0.06F, 0.62F, 0.02F, 0.13F, 0.78F, 6},
    {0.20F, 0.54F, -0.18F, 0.10F, 0.58F, 5},
    {-0.30F, 0.50F, 0.18F, 0.085F, 0.46F, 6},
    {0.04F, 0.66F, 0.26F, 0.065F, 0.34F, 5},
    {-0.22F, 0.26F, -0.44F, 0.07F, 0.30F, 5},
    {0.58F, 0.22F, 0.52F, 0.06F, 0.24F, 6},
    {-0.64F, 0.16F, -0.62F, 0.055F, 0.22F, 5},
}};

// The crag is three rock slabs stacked at different yaws (a -> b runs along
// each slab, half_width is its breadth, half_depth its thickness), plus two
// outlying rocks; the last six entries are the gold shards leaning out of it.
inline constexpr std::array<PropOrientedBoxPart, 11> k_cursed_gold_vein_oriented_boxes{{
    {{-0.56F, 0.16F, -0.26F}, {0.48F, 0.20F, 0.40F}, 0.32F, 0.11F},
    {{-0.40F, 0.36F, 0.30F}, {0.45F, 0.40F, -0.35F}, 0.28F, 0.10F},
    {{-0.28F, 0.56F, -0.10F}, {0.30F, 0.60F, 0.18F}, 0.20F, 0.09F},
    {{0.42F, 0.12F, 0.34F}, {0.76F, 0.16F, 0.70F}, 0.12F, 0.07F},
    {{-0.80F, 0.10F, -0.46F}, {-0.48F, 0.14F, -0.78F}, 0.11F, 0.06F},

    {{0.10F, 0.60F, -0.06F}, {0.48F, 1.10F, -0.30F}, 0.07F, 0.06F},
    {{-0.18F, 0.58F, 0.06F}, {-0.56F, 1.02F, 0.30F}, 0.065F, 0.06F},
    {{0.02F, 0.64F, 0.10F}, {0.16F, 1.22F, 0.46F}, 0.05F, 0.05F},
    {{-0.10F, 0.56F, -0.14F}, {-0.36F, 0.92F, -0.52F}, 0.055F, 0.05F},
    {{0.60F, 0.20F, 0.44F}, {0.80F, 0.50F, 0.30F}, 0.045F, 0.04F},
    {{-0.66F, 0.16F, -0.56F}, {-0.80F, 0.46F, -0.72F}, 0.04F, 0.04F},
}};

} // namespace Render::GL::BackendPipelines::CursedGoldVeinParts
