#include "farm_system.h"

#include <algorithm>

#include "../core/component.h"
#include "../core/ownership_constants.h"
#include "../core/system_context.h"
#include "../core/world.h"

namespace Game::Systems {

void FarmSystem::run(Engine::Core::SystemContext& context) {
  const float delta_time = context.delta_time();
  if (delta_time <= 0.0F) {
    return;
  }

  for (auto [entity_id, farm, unit] :
       context.view<Engine::Core::FarmComponent, Engine::Core::UnitComponent>()) {
    if (farm.ripe() || unit.health <= 0 ||
        Game::Core::is_neutral_owner(unit.owner_id)) {
      continue;
    }
    if (context.has<Engine::Core::PendingRemovalComponent>(entity_id) ||
        context.has<Engine::Core::DismantleSiteComponent>(entity_id)) {
      continue;
    }
    float const cycle = std::max(farm.cycle_seconds, 0.001F);
    farm.growth = std::min(1.0F, farm.growth + delta_time / cycle);
  }
}

auto FarmSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(
      Reads<UnitComponent, PendingRemovalComponent, DismantleSiteComponent>{},
      Writes<FarmComponent>{});
}

} // namespace Game::Systems
