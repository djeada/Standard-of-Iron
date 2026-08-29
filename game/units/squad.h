#pragma once

#include <algorithm>

#include "../core/component.h"
#include "spawn_type.h"
#include "troop_config.h"

namespace Game::Units {

inline constexpr int k_minimum_squad_strength = 2;

[[nodiscard]] inline auto squad_establishment(SpawnType spawn_type) -> int {
  return std::max(1, TroopConfig::instance().get_individuals_per_unit(spawn_type));
}

[[nodiscard]] inline auto
squad_strength(const Engine::Core::UnitComponent& unit) -> int {
  const int establishment = squad_establishment(unit.spawn_type);
  if (unit.squad_strength <= 0) {
    return establishment;
  }
  return std::clamp(unit.squad_strength, 1, establishment);
}

[[nodiscard]] inline auto
squad_render_count(const Engine::Core::UnitComponent& unit) -> int {

  if (unit.render_individuals_per_unit_override > 0) {
    return unit.render_individuals_per_unit_override;
  }
  return squad_strength(unit);
}

[[nodiscard]] inline auto
squad_fraction(const Engine::Core::UnitComponent& unit) -> float {
  const int establishment = squad_establishment(unit.spawn_type);
  return static_cast<float>(squad_strength(unit)) / static_cast<float>(establishment);
}

[[nodiscard]] inline auto
squad_is_at_full_strength(const Engine::Core::UnitComponent& unit) -> bool {
  return squad_strength(unit) >= squad_establishment(unit.spawn_type);
}

[[nodiscard]] inline auto
squad_population_cost(const Engine::Core::UnitComponent& unit) -> int {
  const int full = TroopConfig::instance().get_population_cost(unit.spawn_type);
  if (squad_is_at_full_strength(unit)) {
    return full;
  }
  return std::max(1, static_cast<int>(static_cast<float>(full) * squad_fraction(unit)));
}

[[nodiscard]] inline auto
squad_can_divide(const Engine::Core::UnitComponent& unit) -> bool {
  return unit.health > 0 && !is_building_spawn(unit.spawn_type) &&
         squad_establishment(unit.spawn_type) >= k_minimum_squad_strength * 2 &&
         squad_strength(unit) >= k_minimum_squad_strength * 2;
}

} // namespace Game::Units
