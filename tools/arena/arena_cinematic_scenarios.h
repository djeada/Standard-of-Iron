#pragma once

#include <vector>

#include "arena_scenario.h"

namespace Arena::Scenarios {

inline constexpr char k_cine_field_id[] = "cine_field";
inline constexpr char k_cine_siege_id[] = "cine_siege";
inline constexpr char k_cine_sepulcher_id[] = "cine_sepulcher";

[[nodiscard]] auto
build_cinematic_definitions() -> std::vector<ArenaScenarioDefinition>;

} // namespace Arena::Scenarios
