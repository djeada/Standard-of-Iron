#include "healing_rules.h"

#include <algorithm>
#include <cstdint>

#include "../core/component.h"
#include "../core/entity.h"

namespace Game::Systems::HealingRules {

auto maximum_recoverable_health(const Engine::Core::Entity& target) -> int {
  auto const* unit = target.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || unit->health <= 0) {
    return 0;
  }

  int const safe_max_health = std::max(1, unit->max_health);
  int recoverable = safe_max_health;
  auto const* roster =
      target.get_component<Engine::Core::FormationRosterPresentationComponent>();
  bool const valid_roster =
      roster != nullptr && roster->total_count > 0U &&
      roster->alive.size() == static_cast<std::size_t>(roster->total_count);
  if (valid_roster) {
    int const living_soldiers = static_cast<int>(
        std::count(roster->alive.begin(), roster->alive.end(), std::uint8_t{1U}));
    if (living_soldiers > 0 &&
        living_soldiers < static_cast<int>(roster->total_count)) {
      auto const scaled_health =
          (static_cast<std::int64_t>(safe_max_health) * living_soldiers) /
          static_cast<int>(roster->total_count);
      recoverable = std::max(1, static_cast<int>(scaled_health));
    }
  }

  return std::clamp(std::max(unit->health, recoverable), 1, safe_max_health);
}

auto can_receive_healing(const Engine::Core::Entity& target) -> bool {
  auto const* unit = target.get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && unit->health > 0 &&
         unit->health < maximum_recoverable_health(target);
}

} // namespace Game::Systems::HealingRules
