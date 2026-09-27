#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

inline constexpr char k_structure_damage_stages_id[] = "structure_damage_stages";
inline constexpr char k_structure_repair_id[] = "structure_repair";
inline constexpr char k_structure_dismantle_id[] = "structure_dismantle";
inline constexpr char k_structure_construction_id[] = "structure_construction";

[[nodiscard]] auto
build_structure_lifecycle_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
