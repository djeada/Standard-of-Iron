#pragma once

#include <string>

#include "../ai_types.h"

namespace Game::Systems::AI {

inline constexpr const char* BUILDING_TYPE_HOME = "home";
inline constexpr const char* BUILDING_TYPE_DEFENSE_TOWER = "defense_tower";
inline constexpr const char* BUILDING_TYPE_WALL_SEGMENT = "wall_segment";
inline constexpr const char* BUILDING_TYPE_WALL_GATE = "wall_gate";
inline constexpr const char* BUILDING_TYPE_BARRACKS = "barracks";
inline constexpr const char* BUILDING_TYPE_MARKETPLACE = "marketplace";
inline constexpr const char* BUILDING_TYPE_CATAPULT = "catapult";
inline constexpr const char* BUILDING_TYPE_BALLISTA = "ballista";
inline constexpr const char* BUILDING_TYPE_FARM = "farm";
inline constexpr const char* HARVEST_TREE = "cut_tree";
inline constexpr const char* HARVEST_STONE = "collect_stone";
inline constexpr const char* HARVEST_IRON = "collect_iron_ore";
inline constexpr const char* HARVEST_GRAIN = "harvest_grain";

inline constexpr int MAX_HOMES = 20;
inline constexpr int MAX_DEFENSE_TOWERS = 12;
inline constexpr int MAX_WALL_SEGMENTS = 128;
inline constexpr int MAX_WALL_GATES = 4;
inline constexpr int MAX_BARRACKS = 6;
inline constexpr int MAX_FARMS = 8;
inline constexpr int MAX_MARKETPLACES = 2;
inline constexpr int MAX_CATAPULTS = 5;

[[nodiscard]] auto building_type_name(const std::string& name) -> const char*;

[[nodiscard]] auto preferred_siege_engine(const AIContext& context) -> const char*;

[[nodiscard]] auto is_fortification(const char* building_type) -> bool;
[[nodiscard]] auto is_fortification_or_tower(const std::string& building) -> bool;

} // namespace Game::Systems::AI
