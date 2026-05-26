#pragma once

#include "game/map/json_keys.h"

namespace MapEditor::MapJsonKeys {

inline constexpr const char* name = Game::Map::JsonKeys::NAME;
inline constexpr const char* description = Game::Map::JsonKeys::DESCRIPTION;
inline constexpr const char* coord_system = Game::Map::JsonKeys::COORD_SYSTEM;
inline constexpr const char* max_troops_per_player =
    Game::Map::JsonKeys::MAX_TROOPS_PER_PLAYER;
inline constexpr const char* grid = Game::Map::JsonKeys::GRID;
inline constexpr const char* biome = Game::Map::JsonKeys::BIOME;
inline constexpr const char* camera = Game::Map::JsonKeys::CAMERA;
inline constexpr const char* spawns = Game::Map::JsonKeys::SPAWNS;
inline constexpr const char* firecamps = Game::Map::JsonKeys::FIRECAMPS;
inline constexpr const char* terrain = Game::Map::JsonKeys::TERRAIN;
inline constexpr const char* rivers = Game::Map::JsonKeys::RIVERS;
inline constexpr const char* roads = Game::Map::JsonKeys::ROADS;
inline constexpr const char* bridges = Game::Map::JsonKeys::BRIDGES;
inline constexpr const char* victory = Game::Map::JsonKeys::VICTORY;
inline constexpr const char* rain = Game::Map::JsonKeys::RAIN;
inline constexpr const char* thumbnail = Game::Map::JsonKeys::THUMBNAIL;
inline constexpr const char* world_props = Game::Map::JsonKeys::WORLD_PROPS;
inline constexpr const char* time_of_day = Game::Map::JsonKeys::TIME_OF_DAY;
inline constexpr const char* undead_zones = Game::Map::JsonKeys::UNDEAD_ZONES;
inline constexpr const char* fog_zones = Game::Map::JsonKeys::FOG_ZONES;

inline constexpr const char* type = Game::Map::JsonKeys::TYPE;
inline constexpr const char* x = Game::Map::JsonKeys::X;
inline constexpr const char* z = Game::Map::JsonKeys::Z;
inline constexpr const char* width = Game::Map::JsonKeys::WIDTH;
inline constexpr const char* height = Game::Map::JsonKeys::HEIGHT;
inline constexpr const char* tile_size = Game::Map::JsonKeys::TILE_SIZE;
inline constexpr const char* radius = Game::Map::JsonKeys::RADIUS;
inline constexpr const char* scale = Game::Map::JsonKeys::SCALE;
inline constexpr const char* rotation = Game::Map::JsonKeys::ROTATION;
inline constexpr const char* intensity = Game::Map::JsonKeys::INTENSITY;
inline constexpr const char* persistent = Game::Map::JsonKeys::PERSISTENT;
inline constexpr const char* start = Game::Map::JsonKeys::START;
inline constexpr const char* end = Game::Map::JsonKeys::END;
inline constexpr const char* style = Game::Map::JsonKeys::ROAD_STYLE;
inline constexpr const char* player_id = Game::Map::JsonKeys::PLAYER_ID;
inline constexpr const char* max_population = Game::Map::JsonKeys::MAX_POPULATION;
inline constexpr const char* nation = Game::Map::JsonKeys::NATION;

inline constexpr const char* depth = "depth";
inline constexpr const char* entrances = "entrances";
inline constexpr const char* waypoints = "waypoints";
inline constexpr const char* buildings = Game::Map::JsonKeys::BUILDINGS;
inline constexpr const char* walls = Game::Map::JsonKeys::WALLS;

} // namespace MapEditor::MapJsonKeys
