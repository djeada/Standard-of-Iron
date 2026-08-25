#include "rpg_engagement_system.h"

#include "../../core/component.h"
#include "../../core/simulation_timing.h"
#include "../../core/world.h"
#include "rpg_combat_processor.h"

namespace Game::Systems {

void RpgEngagementSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }
  Engine::Core::Timing::ScopedAccumulator const scope(
      Engine::Core::Timing::commander_engagement());

  for (auto* entity :
       world->collect_entities_with<Engine::Core::CommanderComponent>()) {
    auto const* commander =
        entity != nullptr ? entity->get_component<Engine::Core::CommanderComponent>()
                          : nullptr;
    if (commander == nullptr || !commander->fpv_controlled) {
      continue;
    }
    auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }
    RpgCombat::tick_rpg_combat(world, entity->get_id(), delta_time);
  }
}

} // namespace Game::Systems
