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
inline constexpr const char* structures = Game::Map::JsonKeys::STRUCTURES;
inline constexpr const char* firecamps = Game::Map::JsonKeys::FIRECAMPS;
inline constexpr const char* terrain = Game::Map::JsonKeys::TERRAIN;
inline constexpr const char* rivers = Game::Map::JsonKeys::RIVERS;
inline constexpr const char* lakes = Game::Map::JsonKeys::LAKES;
inline constexpr const char* roads = Game::Map::JsonKeys::ROADS;
inline constexpr const char* bridges = Game::Map::JsonKeys::BRIDGES;
inline constexpr const char* victory = Game::Map::JsonKeys::VICTORY;
inline constexpr const char* rain = Game::Map::JsonKeys::RAIN;
inline constexpr const char* thumbnail = Game::Map::JsonKeys::THUMBNAIL;
inline constexpr const char* world_props = Game::Map::JsonKeys::WORLD_PROPS;
inline constexpr const char* time_of_day = Game::Map::JsonKeys::TIME_OF_DAY;
inline constexpr const char* environment = Game::Map::JsonKeys::ENVIRONMENT;
inline constexpr const char* start_time = Game::Map::JsonKeys::START_TIME;
inline constexpr const char* time_mode = Game::Map::JsonKeys::TIME_MODE;
inline constexpr const char* day_length_seconds =
    Game::Map::JsonKeys::DAY_LENGTH_SECONDS;
inline constexpr const char* lighting_profile = Game::Map::JsonKeys::LIGHTING_PROFILE;
inline constexpr const char* fog_density = Game::Map::JsonKeys::FOG_DENSITY;
inline constexpr const char* exposure = Game::Map::JsonKeys::EXPOSURE;
inline constexpr const char* undead_zones = Game::Map::JsonKeys::UNDEAD_ZONES;
inline constexpr const char* fog_zones = Game::Map::JsonKeys::FOG_ZONES;
inline constexpr const char* forests = Game::Map::JsonKeys::FORESTS;
inline constexpr const char* wildlife = Game::Map::JsonKeys::WILDLIFE;
inline constexpr const char* wildlife_enabled = Game::Map::JsonKeys::WILDLIFE_ENABLED;
inline constexpr const char* wildlife_groups = Game::Map::JsonKeys::WILDLIFE_GROUPS;
inline constexpr const char* wildlife_spawn_areas =
    Game::Map::JsonKeys::WILDLIFE_SPAWN_AREAS;
inline constexpr const char* wildlife_radius = Game::Map::JsonKeys::WILDLIFE_RADIUS;
inline constexpr const char* wildlife_sheep = Game::Map::JsonKeys::WILDLIFE_SHEEP;
inline constexpr const char* wildlife_wolves = Game::Map::JsonKeys::WILDLIFE_WOLVES;
inline constexpr const char* wildlife_birds = Game::Map::JsonKeys::WILDLIFE_BIRDS;

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
inline constexpr const char* behavior = "behavior";
inline constexpr const char* guard_radius = "guard_radius";
inline constexpr const char* patrol_waypoints = "patrol_waypoints";

inline constexpr const char* depth = "depth";
inline constexpr const char* entrances = "entrances";
inline constexpr const char* shape = "shape";
inline constexpr const char* thickness = "thickness";
inline constexpr const char* arc = "arc";
inline constexpr const char* arc_start = "arc_start";
inline constexpr const char* taper = "taper";
inline constexpr const char* points = "points";
inline constexpr const char* cells = "cells";
inline constexpr const char* waypoints = "waypoints";
} // namespace MapEditor::MapJsonKeys
