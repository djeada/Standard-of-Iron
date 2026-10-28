#pragma once

#include <string>
#include <string_view>

#include "../ai_types.h"

namespace Game::Systems::AI {

inline constexpr char BUILDING_TYPE_HOME[] = "home";
inline constexpr char BUILDING_TYPE_DEFENSE_TOWER[] = "defense_tower";
inline constexpr char BUILDING_TYPE_WALL_SEGMENT[] = "wall_segment";
inline constexpr char BUILDING_TYPE_WALL_GATE[] = "wall_gate";
inline constexpr char BUILDING_TYPE_BARRACKS[] = "barracks";
inline constexpr char BUILDING_TYPE_MARKETPLACE[] = "marketplace";
inline constexpr char BUILDING_TYPE_CATAPULT[] = "catapult";
inline constexpr char BUILDING_TYPE_BALLISTA[] = "ballista";
inline constexpr char BUILDING_TYPE_RAM[] = "ram";
inline constexpr char BUILDING_TYPE_SIEGE_TOWER[] = "siege_tower";
inline constexpr char BUILDING_TYPE_FARM[] = "farm";
inline constexpr char HARVEST_TREE[] = "cut_tree";
inline constexpr char HARVEST_STONE[] = "collect_stone";
inline constexpr char HARVEST_IRON[] = "collect_iron_ore";
inline constexpr char HARVEST_GRAIN[] = "harvest_grain";

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

[[nodiscard]] auto is_siege_engine_building(const char* building_type) -> bool;

[[nodiscard]] auto is_assault_engine_building(const char* building_type) -> bool;

[[nodiscard]] auto is_fortification(const char* building_type) -> bool;
[[nodiscard]] auto is_fortification_or_tower(const std::string& building) -> bool;

} // namespace Game::Systems::AI
