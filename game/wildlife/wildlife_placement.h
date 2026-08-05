#pragma once

#include "wildlife_config.h"

namespace Game::Map {
struct MapDefinition;
}

namespace Game::Wildlife {

[[nodiscard]] auto
default_settings_for_map(const Game::Map::MapDefinition& map) -> WildlifeSettings;

void populate_missing_spawn_areas(const Game::Map::MapDefinition& map,
                                  WildlifeSettings& settings);

} // namespace Game::Wildlife
