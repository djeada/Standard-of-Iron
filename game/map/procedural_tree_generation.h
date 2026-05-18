#pragma once

#include <vector>

#include "map_definition.h"

namespace Game::Map {

class TerrainHeightMap;

[[nodiscard]] auto generate_procedural_world_props(
    const TerrainHeightMap& height_map,
    const BiomeSettings& biome_settings,
    CoordSystem coord_system,
    const std::vector<WorldProp>& anchor_world_props) -> std::vector<WorldProp>;

} // namespace Game::Map
