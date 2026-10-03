#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

// The RPG commander control scenes: the aura pulse, exact melee and defence contact,
// projectile blocking, locomotion, motor curves and camera.
[[nodiscard]] auto
build_rpg_control_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
