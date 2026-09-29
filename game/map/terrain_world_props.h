#pragma once

#include <vector>

#include "map_definition.h"

namespace Game::Map {

class TerrainHeightMap;

[[nodiscard]] auto build_runtime_world_props(
    const TerrainHeightMap& height_map,
    const BiomeSettings& biome_settings,
    CoordSystem coord_system,
    const std::vector<WorldProp>& authored_world_props) -> std::vector<WorldProp>;

[[nodiscard]] auto world_props_match(const std::vector<WorldProp>& lhs,
                                     const std::vector<WorldProp>& rhs) -> bool;

[[nodiscard]] auto
has_runtime_harvest_props(const std::vector<WorldProp>& world_props) -> bool;

} // namespace Game::Map
