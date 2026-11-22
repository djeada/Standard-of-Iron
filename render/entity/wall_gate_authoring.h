#pragma once

#include <string_view>

#include "render/entity/building_archetype_desc.h"
#include "render/entity/wall_authoring.h"

// The wall gate frame and its swinging leaf, authored per nation palette.
namespace Render::GL {

auto build_wall_gate_desc(std::string_view name_prefix,
                          const WallPalette& palette,
                          const WallGeometry& geometry) -> BuildingArchetypeDesc;

auto build_wall_gate_leaf_desc(std::string_view name_prefix,
                               const WallPalette& palette,
                               const WallGeometry& geometry) -> BuildingArchetypeDesc;

} // namespace Render::GL
