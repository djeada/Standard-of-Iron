#pragma once

#include <array>

#include "render/gl/backend/prop_parts.h"

namespace Render::GL::BackendPipelines::MagicShrineParts {

inline constexpr std::array<PropBoxPart, 16> k_magic_shrine_boxes{{
    {{-0.86F, -0.02F, -0.86F}, {0.86F, 0.08F, 0.86F}},
    {{-0.68F, 0.08F, -0.68F}, {0.68F, 0.16F, 0.68F}},
    {{-0.82F, 0.08F, -0.24F}, {0.82F, 0.15F, 0.24F}},
    {{-0.24F, 0.08F, -0.82F}, {0.24F, 0.15F, 0.82F}},
    {{-0.48F, 0.16F, -0.48F}, {0.48F, 0.24F, 0.48F}},
    {{-0.26F, 0.24F, -0.26F}, {0.26F, 0.72F, 0.26F}},
    {{-0.32F, 0.72F, -0.32F}, {0.32F, 0.80F, 0.32F}},
    {{-0.26F, 0.80F, -0.26F}, {-0.08F, 0.92F, 0.26F}},
    {{0.08F, 0.80F, -0.26F}, {0.26F, 0.92F, 0.26F}},
    {{-0.08F, 0.80F, -0.26F}, {0.08F, 0.92F, -0.08F}},
    {{-0.08F, 0.80F, 0.08F}, {0.08F, 0.92F, 0.26F}},
    {{-0.16F, 0.92F, -0.16F}, {0.16F, 1.00F, 0.16F}},
    {{-0.58F, 1.12F, -0.60F}, {0.58F, 1.20F, -0.44F}},
    {{-0.58F, 1.12F, 0.44F}, {0.58F, 1.20F, 0.60F}},
    {{-0.60F, 1.12F, -0.58F}, {-0.44F, 1.20F, 0.58F}},
    {{0.44F, 1.12F, -0.58F}, {0.60F, 1.20F, 0.58F}},
}};

inline constexpr std::array<PropVertPrismPart, 2> k_magic_shrine_prisms{{
    {0.0F, 0.96F, 0.0F, 0.10F, 0.82F, 6},
    {0.0F, 1.78F, 0.0F, 0.065F, 0.24F, 5},
}};

inline constexpr std::array<PropOrientedBoxPart, 8> k_magic_shrine_oriented_boxes{{
    {{-0.05F, 1.52F, 0.0F}, {-0.42F, 1.76F, 0.0F}, 0.055F, 0.065F},
    {{0.05F, 1.52F, 0.0F}, {0.42F, 1.76F, 0.0F}, 0.055F, 0.065F},
    {{-0.42F, 1.76F, 0.0F}, {-0.52F, 1.96F, 0.0F}, 0.045F, 0.055F},
    {{0.42F, 1.76F, 0.0F}, {0.52F, 1.96F, 0.0F}, 0.045F, 0.055F},
    {{-0.58F, 0.16F, -0.12F}, {-0.26F, 0.60F, -0.12F}, 0.055F, 0.06F},
    {{0.58F, 0.16F, 0.12F}, {0.26F, 0.60F, 0.12F}, 0.055F, 0.06F},
    {{-0.12F, 0.16F, -0.58F}, {-0.12F, 0.58F, -0.24F}, 0.055F, 0.07F},
    {{0.12F, 0.16F, 0.58F}, {0.12F, 0.58F, 0.24F}, 0.055F, 0.07F},
}};

} // namespace Render::GL::BackendPipelines::MagicShrineParts
