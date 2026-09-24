#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

inline constexpr char k_structure_damage_stages_id[] = "structure_damage_stages";
inline constexpr char k_structure_repair_id[] = "structure_repair";
inline constexpr char k_structure_dismantle_id[] = "structure_dismantle";
inline constexpr char k_structure_construction_id[] = "structure_construction";

// Every stage a structure passes through, one scenario each, so a capture of
// any of them shows the state reading on its own at gameplay zoom:
// healthy -> damaged -> critical -> collapse -> rubble, repair under
// scaffolding, dismantling into stacked materials, and construction from
// foundation through framing to completion.
[[nodiscard]] auto
build_structure_lifecycle_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
