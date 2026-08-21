#include "farm_system.h"

#include <algorithm>

#include "../core/component.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"

namespace Game::Systems {

void FarmSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr || delta_time <= 0.0F) {
    return;
  }

  for (auto* entity : world->collect_entities_with<Engine::Core::FarmComponent>()) {
    auto* farm = entity->get_component<Engine::Core::FarmComponent>();
    if (farm == nullptr || farm->ripe()) {
      continue;
    }
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0 ||
        Game::Core::is_neutral_owner(unit->owner_id)) {
      continue;
    }
    if (entity->has_component<Engine::Core::PendingRemovalComponent>() ||
        entity->has_component<Engine::Core::DismantleSiteComponent>()) {
      continue;
    }
    float const cycle = std::max(farm->cycle_seconds, 0.001F);
    farm->growth = std::min(1.0F, farm->growth + delta_time / cycle);
  }
}

} // namespace Game::Systems
