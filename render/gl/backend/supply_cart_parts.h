#pragma once

#include <array>

#include "render/gl/backend/prop_parts.h"

namespace Render::GL::BackendPipelines::SupplyCartParts {

// The bed, frame and shafts of the supply cart. Wheels, barrels and the plank
// courses are generated in loops in the pipeline.

inline constexpr std::array<PropBoxPart, 21> k_supply_cart_boxes{{
    {{-0.58F, 0.36F, -0.60F}, {-0.44F, 0.45F, 0.56F}},
    {{0.44F, 0.36F, -0.60F}, {0.58F, 0.45F, 0.56F}},
    {{-0.56F, 0.30F, -0.58F}, {0.56F, 0.39F, -0.42F}},
    {{-0.60F, 0.34F, 0.34F}, {0.60F, 0.43F, 0.52F}},
    {{-0.86F, 0.225F, -0.555F}, {-0.72F, 0.295F, -0.445F}},
    {{0.72F, 0.225F, -0.555F}, {0.86F, 0.295F, -0.445F}},
    {{-0.90F, 0.30F, 0.385F}, {-0.75F, 0.38F, 0.495F}},
    {{0.75F, 0.30F, 0.385F}, {0.90F, 0.38F, 0.495F}},
    {{-0.56F, 0.45F, -0.56F}, {0.56F, 0.54F, 0.54F}},
    {{-0.64F, 0.45F, -0.60F}, {-0.52F, 0.94F, 0.58F}},
    {{0.52F, 0.45F, -0.60F}, {0.64F, 0.94F, 0.58F}},
    {{-0.56F, 0.45F, -0.64F}, {0.56F, 0.88F, -0.52F}},
    {{-0.56F, 0.45F, 0.48F}, {0.56F, 0.78F, 0.60F}},
    {{-0.66F, 0.88F, -0.62F}, {-0.50F, 0.98F, 0.60F}},
    {{0.50F, 0.88F, -0.62F}, {0.66F, 0.98F, 0.60F}},
    {{-0.20F, 0.32F, -0.62F}, {-0.09F, 0.41F, -1.34F}},
    {{0.09F, 0.32F, -0.62F}, {0.20F, 0.41F, -1.34F}},
    {{-0.22F, 0.30F, -1.24F}, {0.22F, 0.39F, -1.14F}},
    {{-0.46F, 0.31F, -1.40F}, {0.46F, 0.40F, -1.30F}},
    {{-0.50F, 0.36F, -1.42F}, {-0.36F, 0.46F, -1.28F}},
    {{0.36F, 0.36F, -1.42F}, {0.50F, 0.46F, -1.28F}},
}};

inline constexpr std::array<PropBeamPart, 4> k_supply_cart_beams{{
    {{-0.16F, 0.40F, -1.30F}, {-0.16F, 0.56F, -1.06F}, 0.030F, 0.028F},
    {{0.16F, 0.40F, -1.30F}, {0.16F, 0.56F, -1.06F}, 0.030F, 0.028F},
    {{-0.50F, 1.010F, 0.10F}, {0.50F, 1.010F, 0.10F}, 0.090F, 0.085F},
    {{0.0F, 1.352F, 0.06F}, {0.0F, 1.352F, 0.48F}, 0.034F, 0.030F},
}};

inline constexpr std::array<PropTaperPart, 3> k_supply_cart_tapers{{
    {-0.06F, 0.556F, -0.42F, 0.075F, 0.135F, 0.20F, 10},
    {-0.06F, 0.756F, -0.42F, 0.135F, 0.060F, 0.24F, 10},
    {-0.06F, 0.996F, -0.42F, 0.052F, 0.072F, 0.10F, 10},
}};

} // namespace Render::GL::BackendPipelines::SupplyCartParts
