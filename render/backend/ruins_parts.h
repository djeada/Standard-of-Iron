#pragma once

#include <array>

#include "render/backend/prop_parts.h"

namespace Render::GL::BackendPipelines::RuinsParts {

inline constexpr std::array<PropBoxPart, 19> k_ruins_boxes{{
    // The two foundation courses retain the authored ground footprint.
    {{-0.92F, -0.02F, -0.70F}, {0.90F, 0.06F, 0.68F}},
    {{-0.76F, 0.06F, -0.56F}, {0.76F, 0.12F, 0.54F}},

    // Bases, capitals and the surviving length of entablature.
    {{-0.69F, 0.12F, -0.39F}, {-0.31F, 0.24F, -0.01F}},
    {{0.23F, 0.12F, -0.39F}, {0.61F, 0.24F, -0.01F}},
    {{-0.67F, 1.55F, -0.37F}, {-0.33F, 1.64F, -0.03F}},
    {{0.25F, 1.55F, -0.37F}, {0.59F, 1.64F, -0.03F}},
    {{-0.71F, 1.64F, -0.40F}, {-0.29F, 1.73F, 0.00F}},
    {{0.21F, 1.64F, -0.40F}, {0.63F, 1.73F, 0.00F}},
    {{-0.73F, 1.73F, -0.36F}, {0.67F, 1.90F, -0.04F}},
    {{-0.76F, 1.90F, -0.39F}, {0.18F, 1.97F, -0.01F}},

    // A broken return wall with staggered joints and a low column stump.
    {{-0.72F, 0.12F, 0.02F}, {-0.48F, 0.36F, 0.27F}},
    {{-0.71F, 0.12F, 0.28F}, {-0.47F, 0.36F, 0.53F}},
    {{-0.70F, 0.36F, 0.02F}, {-0.47F, 0.56F, 0.37F}},
    {{-0.71F, 0.56F, 0.01F}, {-0.49F, 0.71F, 0.19F}},
    {{0.21F, 0.12F, 0.16F}, {0.59F, 0.23F, 0.54F}},

    // Loose blocks settle on the lower step and against the broken wall.
    {{-0.85F, 0.06F, 0.33F}, {-0.69F, 0.19F, 0.56F}},
    {{-0.42F, 0.12F, 0.29F}, {-0.20F, 0.24F, 0.49F}},
    {{-0.37F, 0.06F, -0.67F}, {-0.08F, 0.17F, -0.54F}},
    {{0.64F, 0.06F, -0.58F}, {0.83F, 0.20F, -0.39F}},
}};

inline constexpr std::array<PropVertPrismPart, 5> k_ruins_prisms{{
    {-0.50F, 0.24F, -0.20F, 0.16F, 0.12F, 10},
    {-0.50F, 0.36F, -0.20F, 0.13F, 1.19F, 10},
    {0.42F, 0.24F, -0.20F, 0.16F, 0.12F, 10},
    {0.42F, 0.36F, -0.20F, 0.13F, 1.19F, 10},
    {0.40F, 0.23F, 0.35F, 0.14F, 0.32F, 10},
}};

inline constexpr std::array<PropOrientedBoxPart, 3> k_ruins_oriented_boxes{{
    {{-0.38F, 0.30F, 0.36F}, {0.23F, 0.21F, 0.10F}, 0.12F, 0.09F},
    {{-0.08F, 0.17F, 0.47F}, {0.17F, 0.17F, 0.36F}, 0.07F, 0.05F},
    {{0.52F, 0.19F, 0.04F}, {0.73F, 0.19F, 0.19F}, 0.08F, 0.07F},
}};

} // namespace Render::GL::BackendPipelines::RuinsParts
